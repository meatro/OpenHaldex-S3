#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#include "functions/api/api.h"
#include "functions/core/state.h"
#include "functions/config/pins.h"
#include "functions/storage/storage.h"
#include "functions/canview/canview.h"
#include "functions/can/can_id.h"
#include "functions/net/update.h"

extern bool wifiInternetOk();
extern void wifiApplySettings();

#ifndef OPENHALDEX_VERSION
#define OPENHALDEX_VERSION "dev"
#endif

static void ensureDefaults() {
  if (state.mode >= openhaldex_mode_t_MAX) {
    state.mode = MODE_STOCK;
  }
}

static void sendJson(AsyncWebServerRequest* request, int code, const JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  request->send(code, "application/json", out);
}

static void sendError(AsyncWebServerRequest* request, int code, const char* msg) {
  JsonDocument doc;
  doc["error"] = msg;
  sendJson(request, code, doc);
}

static void onJsonBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total,
                       void (*done)(AsyncWebServerRequest*, const String&)) {
  if (index == 0) {
    request->_tempObject = new String();
    ((String*)request->_tempObject)->reserve(total);
  }

  String* body = (String*)request->_tempObject;
  body->concat((const char*)data, len);

  if (index + len == total) {
    String full = *body;
    delete body;
    request->_tempObject = nullptr;
    done(request, full);
  }
}


static String frameDataHex(const canview_last_tx_t& f) {
  String out;
  out.reserve(3 * f.dlc);
  for (uint8_t i = 0; i < f.dlc && i < 8; i++) {
    if (i > 0)
      out += " ";
    if (f.data[i] < 16)
      out += "0";
    out += String(f.data[i], HEX);
  }
  return out;
}
static void handleStatus(AsyncWebServerRequest* request) {
  JsonDocument doc;

  doc["version"] = OPENHALDEX_VERSION;
  doc["mode"] = disableController ? "STOCK" : "MAP";
  doc["disableController"] = disableController;
  doc["broadcastOpenHaldexOverCAN"] = broadcastOpenHaldexOverCAN;
  doc["haldexGeneration"] = haldexGeneration;
  doc["uptimeMs"] = millis();

  JsonObject can = doc["can"].to<JsonObject>();
  can["ready"] = can_ready;
  can["chassis"] = hasCANChassis;
  can["haldex"] = hasCANHaldex;
  can["busFailure"] = isBusFailure;
  can["lastChassisMs"] = lastCANChassisTick > 0 ? (millis() - lastCANChassisTick) : 0;
  can["lastHaldexMs"] = lastCANHaldexTick > 0 ? (millis() - lastCANHaldexTick) : 0;

  JsonObject telemetry = doc["telemetry"].to<JsonObject>();
  telemetry["speed"] = received_vehicle_speed;
  telemetry["rpm"] = received_vehicle_rpm;
  telemetry["boost"] = received_vehicle_boost;
  telemetry["throttle"] = received_pedal_value;
  telemetry["spec"] = lock_target;
  telemetry["act"] = received_haldex_engagement;
  telemetry["haldexState"] = received_haldex_state;
  telemetry["haldexEngagement"] = received_haldex_engagement;
  telemetry["haldexEngagementRaw"] = received_haldex_engagement_raw;
  telemetry["clutch1Report"] = received_report_clutch1;
  telemetry["clutch2Report"] = received_report_clutch2;
  telemetry["tempProtection"] = received_temp_protection;
  telemetry["couplingOpen"] = received_coupling_open;
  telemetry["speedLimit"] = received_speed_limit;

  JsonObject frameDiag = doc["frameDiag"].to<JsonObject>();
  frameDiag["lockTarget"] = lock_target;
  frameDiag["haldexGen"] = haldexGeneration;

  auto addFrameDiag = [&](const char* key, uint32_t id) {
    canview_last_tx_t frame;
    JsonObject f = frameDiag[key].to<JsonObject>();
    f["id"] = id;
    if (!canviewGetLastTxFrame(1, id, frame)) {
      f["ok"] = false;
      return;
    }
    f["ok"] = true;
    f["generated"] = frame.generated;
    f["ageMs"] = frame.ageMs;
    f["dlc"] = frame.dlc;
    f["data"] = frameDataHex(frame);
  };

  addFrameDiag("motor1", MOTOR1_ID);
  addFrameDiag("motor3", MOTOR3_ID);
  addFrameDiag("brakes1", BRAKES1_ID);
  addFrameDiag("brakes2", BRAKES2_ID);
  addFrameDiag("brakes3", BRAKES3_ID);

  sendJson(request, 200, doc);
}

static void handleModeJson(AsyncWebServerRequest* request, const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    sendError(request, 400, "invalid json");
    return;
  }

  String mode = doc["mode"] | "";
  mode.toUpperCase();

  if (mode == "MAP") {
    disableController = false;
    state.mode = MODE_MAP;
  } else if (mode == "STOCK") {
    disableController = true;
    state.mode = MODE_STOCK;
  } else {
    sendError(request, 400, "invalid mode");
    return;
  }

  storageMarkDirty();

  JsonDocument resp;
  resp["ok"] = true;
  resp["mode"] = disableController ? "STOCK" : "MAP";
  sendJson(request, 200, resp);
}

static void handleSettingsJson(AsyncWebServerRequest* request, const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    sendError(request, 400, "invalid json");
    return;
  }

  bool dirty = false;

  if (doc.containsKey("disableController")) {
    disableController = (bool)doc["disableController"];
    state.mode = disableController ? MODE_STOCK : MODE_MAP;
    dirty = true;
  }

  if (doc.containsKey("broadcastOpenHaldexOverCAN")) {
    broadcastOpenHaldexOverCAN = (bool)doc["broadcastOpenHaldexOverCAN"];
    dirty = true;
  }

  if (doc.containsKey("haldexGeneration")) {
    int g = doc["haldexGeneration"];
    if (g == 1 || g == 2 || g == 4) {
      haldexGeneration = (uint8_t)g;
      dirty = true;
    }
  }

  if (doc.containsKey("disableThrottle")) {
    int v = doc["disableThrottle"];
    if (v < 0)
      v = 0;
    if (v > 100)
      v = 100;
    disableThrottle = (uint8_t)v;
    state.pedal_threshold = disableThrottle;
    dirty = true;
  }

  if (doc.containsKey("disableSpeed")) {
    int v = doc["disableSpeed"];
    if (v < 0)
      v = 0;
    if (v > 300)
      v = 300;
    disableSpeed = (uint16_t)v;
    dirty = true;
  }

  if (dirty) {
    storageMarkDirty();
  }

  JsonDocument resp;
  resp["ok"] = true;
  sendJson(request, 200, resp);
}

static void handleMapGet(AsyncWebServerRequest* request) {
  JsonDocument doc;

  JsonArray speedBins = doc["speedBins"].to<JsonArray>();
  for (uint8_t i = 0; i < MAP_SPEED_BINS; i++) {
    speedBins.add(map_speed_bins[i]);
  }

  JsonArray throttleBins = doc["throttleBins"].to<JsonArray>();
  for (uint8_t i = 0; i < MAP_THROTTLE_BINS; i++) {
    throttleBins.add(map_throttle_bins[i]);
  }

  JsonArray lockTable = doc["lockTable"].to<JsonArray>();
  for (uint8_t t = 0; t < MAP_THROTTLE_BINS; t++) {
    JsonArray row = lockTable.add<JsonArray>();
    for (uint8_t s = 0; s < MAP_SPEED_BINS; s++) {
      row.add(map_lock_table[t][s]);
    }
  }

  sendJson(request, 200, doc);
}

static void handleMapPost(AsyncWebServerRequest* request, const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    sendError(request, 400, "invalid json");
    return;
  }

  JsonArray speedBins = doc["speedBins"].as<JsonArray>();
  JsonArray throttleBins = doc["throttleBins"].as<JsonArray>();
  JsonArray lockTable = doc["lockTable"].as<JsonArray>();

  if (speedBins.size() != MAP_SPEED_BINS || throttleBins.size() != MAP_THROTTLE_BINS ||
      lockTable.size() != MAP_THROTTLE_BINS) {
    sendError(request, 400, "invalid map sizes");
    return;
  }

  for (uint8_t i = 0; i < MAP_SPEED_BINS; i++) {
    map_speed_bins[i] = (uint16_t)(speedBins[i] | 0);
  }

  for (uint8_t i = 0; i < MAP_THROTTLE_BINS; i++) {
    map_throttle_bins[i] = (uint8_t)(throttleBins[i] | 0);
  }

  for (uint8_t t = 0; t < MAP_THROTTLE_BINS; t++) {
    JsonArray row = lockTable[t].as<JsonArray>();
    if (row.size() != MAP_SPEED_BINS) {
      sendError(request, 400, "invalid lock table");
      return;
    }
    for (uint8_t s = 0; s < MAP_SPEED_BINS; s++) {
      int v = row[s] | 0;
      if (v < 0)
        v = 0;
      if (v > 100)
        v = 100;
      map_lock_table[t][s] = (uint8_t)v;
    }
  }

  storageMarkDirty();

  JsonDocument resp;
  resp["ok"] = true;
  sendJson(request, 200, resp);
}

static void handleMapsList(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["current"] = storageGetCurrentMapPath();
  JsonArray maps = doc["maps"].to<JsonArray>();
  storageListMaps(maps);
  sendJson(request, 200, doc);
}

static void handleMapLoad(AsyncWebServerRequest* request, const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    sendError(request, 400, "invalid json");
    return;
  }

  String path = doc["path"] | "";
  if (path.length() == 0) {
    sendError(request, 400, "missing path");
    return;
  }

  if (!storageLoadMapPath(path)) {
    sendError(request, 400, "load failed");
    return;
  }

  JsonDocument resp;
  resp["ok"] = true;
  resp["path"] = path;
  sendJson(request, 200, resp);
}

static void handleMapSave(AsyncWebServerRequest* request, const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    sendError(request, 400, "invalid json");
    return;
  }

  String name = doc["name"] | "";
  if (name.length() == 0) {
    sendError(request, 400, "missing name");
    return;
  }

  String outPath;
  if (!storageSaveMapName(name, outPath)) {
    sendError(request, 400, "save failed");
    return;
  }

  JsonDocument resp;
  resp["ok"] = true;
  resp["path"] = outPath;
  sendJson(request, 200, resp);
}

static void handleMapDelete(AsyncWebServerRequest* request, const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    sendError(request, 400, "invalid json");
    return;
  }

  String path = doc["path"] | "";
  if (path.length() == 0) {
    sendError(request, 400, "missing path");
    return;
  }

  if (!storageDeleteMapPath(path)) {
    sendError(request, 400, "delete failed");
    return;
  }

  JsonDocument resp;
  resp["ok"] = true;
  sendJson(request, 200, resp);
}

static void handleWifiGet(AsyncWebServerRequest* request) {
  JsonDocument doc;
  String ssid;
  String pass;
  storageGetWifiCreds(ssid, pass);
  doc["ssid"] = ssid;
  doc["staEnabled"] = storageGetWifiStaEnabled();
  sendJson(request, 200, doc);
}

static void handleWifiPost(AsyncWebServerRequest* request, const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    sendError(request, 400, "invalid json");
    return;
  }

  bool changed = false;

  if (doc.containsKey("ssid") || doc.containsKey("password")) {
    String ssid = doc["ssid"] | "";
    String pass = doc["password"] | "";
    if (ssid.length() == 0) {
      storageClearWifiCreds();
    } else {
      storageSetWifiCreds(ssid, pass);
    }
    changed = true;
  }

  if (doc.containsKey("staEnabled")) {
    bool enabled = (bool)doc["staEnabled"];
    storageSetWifiStaEnabled(enabled);
    changed = true;
  }

  if (changed) {
    wifiApplySettings();
  }

  JsonDocument resp;
  resp["ok"] = true;
  sendJson(request, 200, resp);
}

static void handleNetworkGet(AsyncWebServerRequest* request) {
  JsonDocument doc;
  const bool staConnected = (WiFi.status() == WL_CONNECTED);
  const bool apEnabled = (WiFi.getMode() & WIFI_MODE_AP) != 0;

  doc["staConnected"] = staConnected;
  doc["staIp"] = staConnected ? WiFi.localIP().toString() : String("");
  doc["ap"] = apEnabled;
  doc["apIp"] = apEnabled ? WiFi.softAPIP().toString() : String("");
  doc["hostname"] = "openhaldex.local";
  doc["internet"] = wifiInternetOk();

  sendJson(request, 200, doc);
}

static void handleUpdateGet(AsyncWebServerRequest* request) {
  UpdateInfo info;
  updateGetInfo(info);

  JsonDocument doc;
  doc["current"] = info.current;
  doc["latest"] = info.latest;
  doc["available"] = info.available;
  doc["lastCheckMs"] = info.lastCheckMs;
  doc["error"] = info.error;
  doc["url"] = info.url;
  doc["firmwareUrl"] = info.firmwareUrl;
  doc["filesystemUrl"] = info.filesystemUrl;
  doc["installing"] = info.installing;
  doc["installError"] = info.installError;
  doc["bytesTotal"] = info.bytesTotal;
  doc["bytesDone"] = info.bytesDone;
  doc["speedBps"] = info.speedBps;
  doc["progressMs"] = info.progressMs;
  doc["stage"] = info.stage;

  sendJson(request, 200, doc);
}

static void handleCanview(AsyncWebServerRequest* request) {
  uint16_t decoded = 200;
  uint8_t raw = 20;

  if (request->hasParam("decoded")) {
    decoded = (uint16_t)request->getParam("decoded")->value().toInt();
  }
  if (request->hasParam("raw")) {
    raw = (uint8_t)request->getParam("raw")->value().toInt();
  }

  String bus = "all";
  if (request->hasParam("bus")) {
    bus = request->getParam("bus")->value();
    bus.toLowerCase();
  }

  String json = canviewBuildJson(decoded, raw, bus);
  request->send(200, "application/json", json);
}


static void handleCanviewDump(AsyncWebServerRequest* request) {
  uint32_t seconds = 30;
  if (request->hasParam("seconds")) {
    seconds = (uint32_t)request->getParam("seconds")->value().toInt();
  }
  if (seconds < 1)
    seconds = 1;
  if (seconds > 120)
    seconds = 120;

  String bus = "all";
  if (request->hasParam("bus")) {
    bus = request->getParam("bus")->value();
    bus.toLowerCase();
  }

  String text = canviewBuildDumpText(seconds * 1000UL, bus);
  AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", text);
  response->addHeader("Content-Disposition", "attachment; filename=openhaldex-can-dump.txt");
  request->send(response);
}

void setupApi(AsyncWebServer& server) {
  ensureDefaults();

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) { handleStatus(request); });

  server.on(
    "/api/mode", HTTP_POST,
    [](AsyncWebServerRequest* request) {
      // response sent in body handler
    },
    nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      onJsonBody(request, data, len, index, total, handleModeJson);
    });

  server.on(
    "/api/settings", HTTP_POST,
    [](AsyncWebServerRequest* request) {
      // response sent in body handler
    },
    nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      onJsonBody(request, data, len, index, total, handleSettingsJson);
    });

  server.on("/api/map", HTTP_GET, [](AsyncWebServerRequest* request) { handleMapGet(request); });

  server.on(
    "/api/map", HTTP_POST,
    [](AsyncWebServerRequest* request) {
      // response sent in body handler
    },
    nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      onJsonBody(request, data, len, index, total, handleMapPost);
    });

  server.on("/api/maps", HTTP_GET, [](AsyncWebServerRequest* request) { handleMapsList(request); });

  server.on(
    "/api/maps/load", HTTP_POST,
    [](AsyncWebServerRequest* request) {
      // response sent in body handler
    },
    nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      onJsonBody(request, data, len, index, total, handleMapLoad);
    });

  server.on(
    "/api/maps/save", HTTP_POST,
    [](AsyncWebServerRequest* request) {
      // response sent in body handler
    },
    nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      onJsonBody(request, data, len, index, total, handleMapSave);
    });

  server.on(
    "/api/maps/delete", HTTP_POST,
    [](AsyncWebServerRequest* request) {
      // response sent in body handler
    },
    nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      onJsonBody(request, data, len, index, total, handleMapDelete);
    });

  server.on("/api/wifi", HTTP_GET, [](AsyncWebServerRequest* request) { handleWifiGet(request); });

  server.on(
    "/api/wifi", HTTP_POST,
    [](AsyncWebServerRequest* request) {
      // response sent in body handler
    },
    nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      onJsonBody(request, data, len, index, total, handleWifiPost);
    });

  server.on("/api/network", HTTP_GET, [](AsyncWebServerRequest* request) { handleNetworkGet(request); });

  server.on("/api/update", HTTP_GET, [](AsyncWebServerRequest* request) { handleUpdateGet(request); });

  server.on("/api/update/check", HTTP_POST, [](AsyncWebServerRequest* request) {
    updateCheckNow();
    handleUpdateGet(request);
  });

  server.on("/api/update/install", HTTP_POST, [](AsyncWebServerRequest* request) {
    bool started = updateInstallStart();
    JsonDocument doc;
    doc["started"] = started;
    sendJson(request, started ? 200 : 409, doc);
  });

  server.on("/api/canview", HTTP_GET, [](AsyncWebServerRequest* request) { handleCanview(request); });
  server.on("/api/canview/dump", HTTP_GET, [](AsyncWebServerRequest* request) { handleCanviewDump(request); });
}



