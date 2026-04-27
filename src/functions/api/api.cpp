#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <math.h>
#include <string.h>

#include "functions/api/api.h"
#include "functions/core/state.h"
#include "functions/config/pins.h"
#include "functions/storage/storage.h"
#include "functions/storage/filelog.h"
#include "functions/canview/canview.h"
#include "functions/can/can_id.h"
#include "functions/net/update.h"
#include "functions/tasks/tasks.h"

extern bool wifiInternetOk();
extern void wifiApplySettings();

#ifndef OPENHALDEX_VERSION
#define OPENHALDEX_VERSION "dev"
#endif

// Keep runtime mode enum valid when upgrading from older stored state.
static void ensureDefaults() {
  if (state.mode >= openhaldex_mode_t_MAX) {
    state.mode = MODE_STOCK;
  }
}

// CAN View diagnostic capture mode forces a safe bridge state while collecting data.
static bool canview_capture_active = false;
static bool canview_capture_prev_valid = false;
static bool canview_capture_prev_disable_controller = false;
static bool canview_capture_prev_broadcast = true;
static bool canview_capture_prev_mode_trigger_suppressed = false;
static openhaldex_mode_t canview_capture_prev_mode = MODE_STOCK;

static void canviewCaptureApplySafeState() {
  disableController = true;
  state.mode = MODE_STOCK;
  broadcastOpenHaldexOverCAN = true;
  modeTriggerSuppressed = true;
  modeTriggerRuntimeReset();
}

static void sendJson(AsyncWebServerRequest* request, int code, const JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  request->send(code, "application/json", out);
}

static void sendError(AsyncWebServerRequest* request, int code, const char* msg) {
  JsonDocument doc;
  doc["error"] = msg;
  if (request) {
    filelogLogError("api", request->url() + String(" code=") + String(code) + String(" msg=") + String(msg));
  } else {
    filelogLogError("api", String("code=") + String(code) + String(" msg=") + String(msg));
  }
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

static const char* modeName(openhaldex_mode_t mode) {
  return get_openhaldex_mode_string(mode);
}

static bool parseModeName(const String& mode_raw, openhaldex_mode_t& out_mode) {
  String mode = mode_raw;
  mode.toUpperCase();
  if (mode == "STOCK" || mode == "PASSTHRU") {
    out_mode = MODE_STOCK;
    return true;
  }
  if (mode == "FWD" || mode == "100:0") {
    out_mode = MODE_FWD;
    return true;
  }
  if (mode == "5050" || mode == "50:50" || mode == "LOCK") {
    out_mode = MODE_5050;
    return true;
  }
  if (mode == "6040" || mode == "60:40") {
    out_mode = MODE_6040;
    return true;
  }
  if (mode == "7030" || mode == "70:30" || mode == "7525" || mode == "75:25") {
    out_mode = MODE_7030;
    return true;
  }
  if (mode == "8020" || mode == "80:20") {
    out_mode = MODE_8020;
    return true;
  }
  if (mode == "9010" || mode == "90:10") {
    out_mode = MODE_9010;
    return true;
  }
  if (mode == "SPEED") {
    out_mode = MODE_SPEED;
    return true;
  }
  if (mode == "THROTTLE") {
    out_mode = MODE_THROTTLE;
    return true;
  }
  if (mode == "RPM") {
    out_mode = MODE_RPM;
    return true;
  }
  if (mode == "MAP" || mode == "EXPERT") {
    out_mode = MODE_MAP;
    return true;
  }
  return false;
}

static String sanitizeMappedSignalKey(const String& raw) {
  String value = raw;
  value.trim();
  if (value.length() > 160) {
    value = value.substring(0, 160);
  }
  return value;
}

static String mappingKeySignalToken(const char* value) {
  String token = String(value ? value : "");
  token.replace("_", " ");
  token.trim();
  token.toLowerCase();
  return token;
}

static String mappingKeyUnitToken(const char* value) {
  String token = String(value ? value : "");
  token.trim();
  token.toLowerCase();
  return token;
}

static String buildMappedSignalKey(const char* bus, uint32_t frame_id, const char* signal_name, const char* unit) {
  String key = String(bus ? bus : "chassis");
  key.trim();
  key.toLowerCase();
  key += "|0x";
  key += String(frame_id, HEX);
  key += "|";
  key += mappingKeySignalToken(signal_name);
  key += "|";
  key += mappingKeyUnitToken(unit);
  return key;
}

static bool getRecommendedInputMappingsForGeneration(uint8_t generation, String& speed, String& throttle, String& rpm) {
  switch (generation) {
  case 1:
  case 2:
  case 4:
    speed = buildMappedSignalKey("chassis", BRAKES1_ID, "BR1_Wheel_Speed_kmh", "km/h");
    throttle = buildMappedSignalKey("chassis", MOTOR1_ID, "Pedal_Value_or_Throttle_Plate", "%");
    rpm = buildMappedSignalKey("chassis", MOTOR1_ID, "Engine_RPM", "rpm");
    return true;
  case 5:
    speed = buildMappedSignalKey("chassis", ESP_21, "ESP_v_Signal", "Unit_KiloMeterPerHour");
    throttle = buildMappedSignalKey("chassis", MOTOR_20, "MO_Fahrpedalrohwert_01", "Unit_PerCent");
    rpm = buildMappedSignalKey("chassis", MOTOR_12, "MO_Drehzahl_01", "Unit_MinutInver");
    return true;
  default:
    speed = "";
    throttle = "";
    rpm = "";
    return false;
  }
}

static bool getRecommendedModeTriggerForGeneration(uint8_t generation, mode_trigger_config_t& trigger) {
  trigger.enabled = false;
  trigger.op = MODE_TRIGGER_GTE;
  trigger.value = 1.0f;
  trigger.mode = MODE_MAP;
  trigger.broadcastOpenHaldexOverCAN = broadcastOpenHaldexOverCAN;

  switch (generation) {
  case 1:
  case 2:
  case 4:
    trigger.signal = buildMappedSignalKey("chassis", BRAKES1_ID, "BR1_ESP_ASR_Passive", "");
    return true;
  case 5:
    trigger.signal = buildMappedSignalKey("chassis", ESP_21, "ESP_Tastung_passiv", "");
    return true;
  default:
    trigger.signal = "";
    return false;
  }
}

static bool isRecommendedModeTriggerSignal(const String& signal) {
  mode_trigger_config_t pq = {};
  mode_trigger_config_t mqb = {};
  (void)getRecommendedModeTriggerForGeneration(2, pq);
  (void)getRecommendedModeTriggerForGeneration(5, mqb);
  return signal == pq.signal || signal == mqb.signal;
}

static bool seedModeTriggerForGeneration(uint8_t generation, mode_trigger_config_t& trigger,
                                         bool overwrite_known_default) {
  mode_trigger_config_t recommended = {};
  if (!getRecommendedModeTriggerForGeneration(generation, recommended)) {
    return false;
  }

  const bool should_seed =
    trigger.signal.length() == 0 || (overwrite_known_default && isRecommendedModeTriggerSignal(trigger.signal));
  if (!should_seed) {
    return false;
  }

  const bool changed = trigger.signal != recommended.signal;
  trigger.signal = recommended.signal;
  if (trigger.op >= mode_trigger_operator_t_MAX) {
    trigger.op = recommended.op;
  }
  if (!isfinite(trigger.value)) {
    trigger.value = recommended.value;
  }
  if (trigger.mode >= openhaldex_mode_t_MAX) {
    trigger.mode = recommended.mode;
  }
  return changed;
}

static bool seedMissingInputMappingsForGeneration(uint8_t generation, String& speed, String& throttle, String& rpm) {
  String default_speed;
  String default_throttle;
  String default_rpm;
  if (!getRecommendedInputMappingsForGeneration(generation, default_speed, default_throttle, default_rpm)) {
    return false;
  }

  bool changed = false;
  if (!speed.length()) {
    speed = default_speed;
    changed = true;
  }
  if (!throttle.length()) {
    throttle = default_throttle;
    changed = true;
  }
  if (!rpm.length()) {
    rpm = default_rpm;
    changed = true;
  }
  return changed;
}

static bool inputsMappedForControl() {
  return mappedInputSignalsConfigured();
}

static void getMappedInputSnapshot(String& speed, String& throttle, String& rpm) {
  if (!mappedInputSignalsGet(speed, throttle, rpm, 2)) {
    speed = "";
    throttle = "";
    rpm = "";
  }
}

static void getDashboardSignalSnapshot(String* slots) {
  if (!slots || !dashboardSignalsGet(slots, DASHBOARD_SIGNAL_SLOT_COUNT, 2)) {
    for (size_t i = 0; i < DASHBOARD_SIGNAL_SLOT_COUNT; i++) {
      slots[i] = "";
    }
  }
}

static String dashboardSignalLabelFromKey(const String& raw) {
  String value = raw;
  value.trim();
  const int first = value.indexOf('|');
  const int second = (first >= 0) ? value.indexOf('|', first + 1) : -1;
  const int third = (second >= 0) ? value.indexOf('|', second + 1) : -1;
  if (second >= 0 && third > second) {
    value = value.substring(second + 1, third);
  }
  value.replace("_", " ");
  value.trim();
  return value;
}

static String formatDashboardNumericValue(float value, const String& unit, const String& signal_name) {
  float display_value = value;
  String normalized_unit = unit;
  normalized_unit.trim();
  normalized_unit.toLowerCase();
  String normalized_name = signal_name;
  normalized_name.trim();
  normalized_name.toLowerCase();

  if ((normalized_unit == "rpm" || normalized_name.indexOf("rpm") >= 0) && fabsf(display_value) > 0.0f &&
      fabsf(display_value) < 20.0f) {
    const float scaled = display_value * 100.0f;
    if (fabsf(scaled) >= 100.0f && fabsf(scaled) <= 12000.0f) {
      display_value = scaled;
    }
  }

  String rendered;
  const float magnitude = fabsf(display_value);
  if (normalized_unit == "rpm") {
    rendered = (magnitude >= 20.0f) ? String(lroundf(display_value)) : String(display_value, 2);
  } else if (magnitude >= 1000.0f) {
    rendered = String(lroundf(display_value));
  } else if (magnitude >= 100.0f) {
    rendered = String(display_value, 1);
  } else {
    rendered = String(display_value, 2);
  }

  while (rendered.endsWith("0")) {
    rendered.remove(rendered.length() - 1);
  }
  if (rendered.endsWith(".")) {
    rendered.remove(rendered.length() - 1);
  }
  if (!unit.length()) {
    return rendered;
  }
  return rendered + " " + unit;
}

static void applyRuntimeMode(openhaldex_mode_t mode) {
  state.mode = mode;
  disableController = (mode == MODE_STOCK);
  lastMode = (uint8_t)mode;
}
// Aggregated status endpoint used by Home and Diagnostics pages.
static void handleStatus(AsyncWebServerRequest* request) {
  JsonDocument doc;

  doc["version"] = OPENHALDEX_VERSION;
  doc["mode"] = modeName(state.mode);
  doc["effectiveMode"] = modeName(openhaldexEffectiveMode());
  doc["disableController"] = disableController;
  doc["broadcastOpenHaldexOverCAN"] = broadcastOpenHaldexOverCAN;
  doc["effectiveBroadcastOpenHaldexOverCAN"] = openhaldexEffectiveBroadcastOpenHaldexOverCAN();
  doc["canviewCaptureMode"] = canview_capture_active;
  doc["haldexGeneration"] = haldexGeneration;
  doc["disableThrottle"] = disableThrottle;
  doc["disableSpeed"] = disableSpeed;
  doc["lockReleaseRatePctPerSec"] = lockReleaseRatePctPerSec;
  doc["uptimeMs"] = millis();

  JsonObject logging = doc["logging"].to<JsonObject>();
  logging["masterEnabled"] = logToFileEnabled || logSerialEnabled || logCanToFileEnabled || logErrorToFileEnabled ||
                             logDebugFirmwareEnabled || logDebugNetworkEnabled || logDebugCanEnabled;
  logging["enabled"] = logToFileEnabled;
  logging["canEnabled"] = logCanToFileEnabled;
  logging["errorEnabled"] = logErrorToFileEnabled;
  logging["serialEnabled"] = logSerialEnabled;
  logging["debugFirmwareEnabled"] = logDebugFirmwareEnabled;
  logging["debugNetworkEnabled"] = logDebugNetworkEnabled;
  logging["debugCanEnabled"] = logDebugCanEnabled;
  logging["debugCaptureActive"] = loggingDebugCaptureActive();

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
  telemetry["inputsMapped"] = inputsMappedForControl();

  JsonObject frameDiag = doc["frameDiag"].to<JsonObject>();
  frameDiag["lockTarget"] = lock_target;
  frameDiag["haldexGen"] = haldexGeneration;

  JsonObject learn = doc["learn"].to<JsonObject>();
  learn["active"] = (bool)haldexLearnActive;
  learn["tableValid"] = haldexLearnTableValid;
  learn["progress"] = (uint8_t)haldexLearnStep;
  learn["currentCF"] = (uint8_t)haldexLearnCF;

  String mapped_speed;
  String mapped_throttle;
  String mapped_rpm;
  getMappedInputSnapshot(mapped_speed, mapped_throttle, mapped_rpm);

  JsonObject inputMappings = doc["inputMappings"].to<JsonObject>();
  inputMappings["speed"] = mapped_speed;
  inputMappings["throttle"] = mapped_throttle;
  inputMappings["rpm"] = mapped_rpm;

  mode_trigger_config_t mode_trigger_config = {};
  if (!modeTriggerConfigGet(mode_trigger_config, 2)) {
    (void)getRecommendedModeTriggerForGeneration(haldexGeneration, mode_trigger_config);
  }
  mode_trigger_config_t default_trigger = {};
  const bool has_default_trigger = getRecommendedModeTriggerForGeneration(haldexGeneration, default_trigger);
  const String mode_trigger_signal = mode_trigger_config.signal.length() > 0
                                       ? mode_trigger_config.signal
                                       : (has_default_trigger ? default_trigger.signal : "");
  mode_trigger_runtime_t mode_trigger_runtime = {};
  modeTriggerRuntimeGet(mode_trigger_runtime);
  JsonObject modeTrigger = doc["modeTrigger"].to<JsonObject>();
  modeTrigger["enabled"] = mode_trigger_config.enabled;
  modeTrigger["signal"] = mode_trigger_signal;
  modeTrigger["defaultSignal"] = has_default_trigger ? default_trigger.signal : "";
  modeTrigger["operator"] = modeTriggerOperatorName(mode_trigger_config.op);
  modeTrigger["value"] = mode_trigger_config.value;
  modeTrigger["mode"] = modeName(mode_trigger_config.mode);
  modeTrigger["broadcastOpenHaldexOverCAN"] = mode_trigger_config.broadcastOpenHaldexOverCAN;
  modeTrigger["active"] = mode_trigger_runtime.active;
  modeTrigger["seen"] = mode_trigger_runtime.seen;
  modeTrigger["lastValue"] = mode_trigger_runtime.lastValue;
  modeTrigger["ageMs"] = mode_trigger_runtime.ageMs;
  modeTrigger["effectiveMode"] = modeName(openhaldexEffectiveMode());
  modeTrigger["effectiveBroadcastOpenHaldexOverCAN"] = openhaldexEffectiveBroadcastOpenHaldexOverCAN();

  String dashboard_slots[DASHBOARD_SIGNAL_SLOT_COUNT];
  getDashboardSignalSnapshot(dashboard_slots);

  JsonObject dashMappings = doc["dashMappings"].to<JsonObject>();
  JsonArray dashboardSignals = doc["dashboardSignals"].to<JsonArray>();
  for (size_t i = 0; i < DASHBOARD_SIGNAL_SLOT_COUNT; i++) {
    const String slot_key = String("dash_") + String(i + 1);
    dashMappings[slot_key] = dashboard_slots[i];

    JsonObject slot = dashboardSignals.add<JsonObject>();
    slot["slot"] = slot_key;
    slot["signalId"] = dashboard_slots[i];
    const bool mapped = dashboard_slots[i].length() > 0;
    slot["mapped"] = mapped;

    canview_resolved_signal_t resolved = {};
    const bool found = mapped && canviewResolveMappedSignal(dashboard_slots[i], resolved);
    const String label = found ? String(resolved.name) : dashboardSignalLabelFromKey(dashboard_slots[i]);
    slot["found"] = found;
    slot["label"] = label;
    slot["bus"] = found ? resolved.bus : "";
    slot["dir"] = found ? resolved.dir : "";
    slot["unit"] = found ? resolved.unit : "";
    slot["generated"] = found ? resolved.generated : false;
    slot["ageMs"] = found ? resolved.ageMs : 0;
    if (found && resolved.numeric) {
      slot["numeric"] = true;
      slot["value"] = resolved.numericValue;
      slot["display"] = formatDashboardNumericValue(resolved.numericValue, resolved.unit, resolved.name);
    } else if (found) {
      slot["numeric"] = false;
      slot["display"] = resolved.textValue;
    } else {
      slot["numeric"] = false;
      slot["display"] = "--";
    }
  }

  JsonObject disengage = doc["disengageUnderSpeed"].to<JsonObject>();
  disengage["map"] = disengageUnderSpeedMap;
  disengage["speed"] = disengageUnderSpeedSpeedMode;
  disengage["throttle"] = disengageUnderSpeedThrottleMode;
  disengage["rpm"] = disengageUnderSpeedRpmMode;

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

static void handleLearnStatus(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["active"] = (bool)haldexLearnActive;
  doc["progress"] = (uint8_t)haldexLearnStep;
  doc["tableValid"] = haldexLearnTableValid;
  doc["currentCF"] = (uint8_t)haldexLearnCF;
  doc["currentEng"] = received_haldex_engagement;

  if (haldexLearnTableValid) {
    JsonArray table = doc["table"].to<JsonArray>();
    for (uint8_t i = 0; i <= 100; i++) {
      table.add(haldexLearnTable[i]);
    }
  }

  sendJson(request, 200, doc);
}

static void sendLearnResponse(AsyncWebServerRequest* request, bool ok, const char* error = nullptr) {
  JsonDocument doc;
  doc["ok"] = ok;
  if (error) {
    doc["error"] = error;
  }
  sendJson(request, 200, doc);
}

static void handleLearnStart(AsyncWebServerRequest* request) {
  if (haldexLearnActive) {
    JsonDocument doc;
    doc["ok"] = true;
    doc["alreadyActive"] = true;
    sendJson(request, 200, doc);
    return;
  }
  if (canview_capture_active) {
    sendLearnResponse(request, false, "CAN View capture is active");
    return;
  }
  if (!hasCANHaldex) {
    sendLearnResponse(request, false, "No Haldex CAN data available");
    return;
  }

  if (disableController || state.mode == MODE_STOCK) {
    haldexLearnRestorePending = true;
    haldexLearnRestoreDisableController = disableController;
    haldexLearnRestoreMode = state.mode;
    haldexLearnRestoreLastMode = lastMode;
    disableController = false;
    state.mode = MODE_5050;
    lastMode = MODE_5050;
  }

  startHaldexLearn();
  sendLearnResponse(request, (bool)haldexLearnActive, haldexLearnActive ? nullptr : "Failed to start learn task");
}

static void handleLearnCancel(AsyncWebServerRequest* request) {
  haldexLearnCancel = true;
  sendLearnResponse(request, true);
}

static void handleLearnClear(AsyncWebServerRequest* request) {
  haldexLearnCancel = true;
  haldexLearnTableValid = false;
  haldexLearnStep = 0;
  haldexLearnCF = 0;
  memset(haldexLearnTable, 0, sizeof(haldexLearnTable));
  storageMarkDirty();
  sendLearnResponse(request, true);
}

static void handleModeJson(AsyncWebServerRequest* request, const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    sendError(request, 400, "invalid json");
    return;
  }

  openhaldex_mode_t target_mode = MODE_STOCK;
  if (!parseModeName(doc["mode"] | "", target_mode)) {
    sendError(request, 400, "invalid mode");
    return;
  }

  if (canview_capture_active && target_mode != MODE_STOCK) {
    sendError(request, 409, "canview capture mode active");
    return;
  }

  applyRuntimeMode(target_mode);
  storageMarkDirty();
  filelogLogEvent("mode", String("set to ") + modeName(state.mode));

  JsonDocument resp;
  resp["ok"] = true;
  resp["mode"] = modeName(state.mode);
  sendJson(request, 200, resp);
}

// Central settings mutator. This endpoint is authoritative for runtime toggles.
static void handleSettingsJson(AsyncWebServerRequest* request, const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    sendError(request, 400, "invalid json");
    return;
  }

  if (canview_capture_active) {
    if (doc.containsKey("disableController") && !(bool)doc["disableController"]) {
      sendError(request, 409, "canview capture mode active");
      return;
    }
    if (doc.containsKey("broadcastOpenHaldexOverCAN") && !(bool)doc["broadcastOpenHaldexOverCAN"]) {
      sendError(request, 409, "canview capture mode active");
      return;
    }
  }

  bool dirty = false;
  bool mappings_changed = false;
  String mapped_speed;
  String mapped_throttle;
  String mapped_rpm;
  getMappedInputSnapshot(mapped_speed, mapped_throttle, mapped_rpm);
  String dashboard_slots[DASHBOARD_SIGNAL_SLOT_COUNT];
  getDashboardSignalSnapshot(dashboard_slots);
  mode_trigger_config_t mode_trigger_config = {};
  if (!modeTriggerConfigGet(mode_trigger_config, 2)) {
    (void)getRecommendedModeTriggerForGeneration(haldexGeneration, mode_trigger_config);
  }

  bool disable_controller_set = false;
  bool next_disable_controller = disableController;
  bool broadcast_set = false;
  bool next_broadcast = broadcastOpenHaldexOverCAN;
  bool haldex_generation_set = false;
  uint8_t next_haldex_generation = haldexGeneration;
  bool disable_throttle_set = false;
  uint8_t next_disable_throttle = disableThrottle;
  bool disable_speed_set = false;
  uint16_t next_disable_speed = disableSpeed;
  bool log_to_file_set = false;
  bool next_log_to_file = logToFileEnabled;
  bool log_can_set = false;
  bool next_log_can = logCanToFileEnabled;
  bool log_error_set = false;
  bool next_log_error = logErrorToFileEnabled;
  bool log_serial_set = false;
  bool next_log_serial = logSerialEnabled;
  bool log_debug_firmware_set = false;
  bool next_log_debug_firmware = logDebugFirmwareEnabled;
  bool log_debug_network_set = false;
  bool next_log_debug_network = logDebugNetworkEnabled;
  bool log_debug_can_set = false;
  bool next_log_debug_can = logDebugCanEnabled;
  bool disengage_map_set = false;
  uint16_t next_disengage_map = disengageUnderSpeedMap;
  bool disengage_speed_mode_set = false;
  uint16_t next_disengage_speed_mode = disengageUnderSpeedSpeedMode;
  bool disengage_throttle_mode_set = false;
  uint16_t next_disengage_throttle_mode = disengageUnderSpeedThrottleMode;
  bool disengage_rpm_mode_set = false;
  uint16_t next_disengage_rpm_mode = disengageUnderSpeedRpmMode;
  bool lock_release_rate_set = false;
  float next_lock_release_rate = lockReleaseRatePctPerSec;
  bool dashboard_mappings_changed = false;
  bool mode_trigger_changed = false;
  mode_trigger_config_t next_mode_trigger = mode_trigger_config;

  if (doc.containsKey("inputMappings")) {
    JsonObject inputMappings = doc["inputMappings"].as<JsonObject>();
    if (inputMappings.isNull()) {
      sendError(request, 400, "invalid inputMappings");
      return;
    }

    if (inputMappings.containsKey("speed")) {
      mapped_speed = sanitizeMappedSignalKey(inputMappings["speed"] | "");
      mappings_changed = true;
    }
    if (inputMappings.containsKey("throttle")) {
      mapped_throttle = sanitizeMappedSignalKey(inputMappings["throttle"] | "");
      mappings_changed = true;
    }
    if (inputMappings.containsKey("rpm")) {
      mapped_rpm = sanitizeMappedSignalKey(inputMappings["rpm"] | "");
      mappings_changed = true;
    }
  }

  if (doc.containsKey("disableController")) {
    disable_controller_set = true;
    next_disable_controller = (bool)doc["disableController"];
  }

  if (doc.containsKey("broadcastOpenHaldexOverCAN")) {
    broadcast_set = true;
    next_broadcast = (bool)doc["broadcastOpenHaldexOverCAN"];
  }

  if (doc.containsKey("haldexGeneration")) {
    int g = doc["haldexGeneration"];
    if (g == 1 || g == 2 || g == 4 || g == 5) {
      haldex_generation_set = true;
      next_haldex_generation = (uint8_t)g;
    }
  }

  if (doc.containsKey("disableThrottle")) {
    int v = doc["disableThrottle"];
    if (v < 0)
      v = 0;
    if (v > 100)
      v = 100;
    disable_throttle_set = true;
    next_disable_throttle = (uint8_t)v;
  }

  if (doc.containsKey("disableSpeed")) {
    int v = doc["disableSpeed"];
    if (v < 0)
      v = 0;
    if (v > 300)
      v = 300;
    disable_speed_set = true;
    next_disable_speed = (uint16_t)v;
  }

  if (doc.containsKey("logToFileEnabled")) {
    log_to_file_set = true;
    next_log_to_file = (bool)doc["logToFileEnabled"];
  }

  if (doc.containsKey("logCanToFileEnabled")) {
    log_can_set = true;
    next_log_can = (bool)doc["logCanToFileEnabled"];
  }

  if (doc.containsKey("logErrorToFileEnabled")) {
    log_error_set = true;
    next_log_error = (bool)doc["logErrorToFileEnabled"];
  }

  if (doc.containsKey("logSerialEnabled")) {
    log_serial_set = true;
    next_log_serial = (bool)doc["logSerialEnabled"];
  }

  if (doc.containsKey("logDebugFirmwareEnabled")) {
    log_debug_firmware_set = true;
    next_log_debug_firmware = (bool)doc["logDebugFirmwareEnabled"];
  }

  if (doc.containsKey("logDebugNetworkEnabled")) {
    log_debug_network_set = true;
    next_log_debug_network = (bool)doc["logDebugNetworkEnabled"];
  }

  if (doc.containsKey("logDebugCanEnabled")) {
    log_debug_can_set = true;
    next_log_debug_can = (bool)doc["logDebugCanEnabled"];
  }

  if (doc.containsKey("lockReleaseRatePctPerSec")) {
    float v = doc["lockReleaseRatePctPerSec"];
    if (!isfinite(v)) {
      sendError(request, 400, "invalid lockReleaseRatePctPerSec");
      return;
    }
    if (v < 0.0f)
      v = 0.0f;
    if (v > 1000.0f)
      v = 1000.0f;
    lock_release_rate_set = true;
    next_lock_release_rate = v;
  }
  if (doc.containsKey("disengageUnderSpeed")) {
    JsonObject disengage = doc["disengageUnderSpeed"].as<JsonObject>();
    if (disengage.isNull()) {
      sendError(request, 400, "invalid disengageUnderSpeed");
      return;
    }

    if (disengage.containsKey("map")) {
      int v = disengage["map"] | 0;
      if (v < 0)
        v = 0;
      if (v > 300)
        v = 300;
      disengage_map_set = true;
      next_disengage_map = (uint16_t)v;
    }
    if (disengage.containsKey("speed")) {
      int v = disengage["speed"] | 0;
      if (v < 0)
        v = 0;
      if (v > 300)
        v = 300;
      disengage_speed_mode_set = true;
      next_disengage_speed_mode = (uint16_t)v;
    }
    if (disengage.containsKey("throttle")) {
      int v = disengage["throttle"] | 0;
      if (v < 0)
        v = 0;
      if (v > 300)
        v = 300;
      disengage_throttle_mode_set = true;
      next_disengage_throttle_mode = (uint16_t)v;
    }
    if (disengage.containsKey("rpm")) {
      int v = disengage["rpm"] | 0;
      if (v < 0)
        v = 0;
      if (v > 300)
        v = 300;
      disengage_rpm_mode_set = true;
      next_disengage_rpm_mode = (uint16_t)v;
    }
  }

  if (doc.containsKey("dashMappings")) {
    JsonObject dashMappings = doc["dashMappings"].as<JsonObject>();
    if (dashMappings.isNull()) {
      sendError(request, 400, "invalid dashMappings");
      return;
    }

    for (size_t i = 0; i < DASHBOARD_SIGNAL_SLOT_COUNT; i++) {
      const String slot_key = String("dash_") + String(i + 1);
      if (dashMappings.containsKey(slot_key)) {
        dashboard_slots[i] = sanitizeMappedSignalKey(dashMappings[slot_key] | "");
        dashboard_mappings_changed = true;
      }
    }
  }

  if (doc.containsKey("modeTrigger")) {
    JsonObject trigger = doc["modeTrigger"].as<JsonObject>();
    if (trigger.isNull()) {
      sendError(request, 400, "invalid modeTrigger");
      return;
    }

    if (trigger.containsKey("enabled")) {
      next_mode_trigger.enabled = (bool)trigger["enabled"];
      mode_trigger_changed = true;
    }
    if (trigger.containsKey("signal")) {
      next_mode_trigger.signal = sanitizeMappedSignalKey(trigger["signal"] | "");
      mode_trigger_changed = true;
    }
    if (trigger.containsKey("operator")) {
      mode_trigger_operator_t op = MODE_TRIGGER_GTE;
      if (!modeTriggerOperatorFromString(String(trigger["operator"] | ""), op)) {
        sendError(request, 400, "invalid modeTrigger.operator");
        return;
      }
      next_mode_trigger.op = op;
      mode_trigger_changed = true;
    }
    if (trigger.containsKey("value")) {
      float value = trigger["value"] | 0.0f;
      if (!isfinite(value)) {
        sendError(request, 400, "invalid modeTrigger.value");
        return;
      }
      if (value < -1000000.0f)
        value = -1000000.0f;
      if (value > 1000000.0f)
        value = 1000000.0f;
      next_mode_trigger.value = value;
      mode_trigger_changed = true;
    }
    if (trigger.containsKey("mode")) {
      openhaldex_mode_t mode = MODE_MAP;
      if (!parseModeName(String(trigger["mode"] | ""), mode)) {
        sendError(request, 400, "invalid modeTrigger.mode");
        return;
      }
      next_mode_trigger.mode = mode;
      mode_trigger_changed = true;
    }
    if (trigger.containsKey("broadcastOpenHaldexOverCAN")) {
      next_mode_trigger.broadcastOpenHaldexOverCAN = (bool)trigger["broadcastOpenHaldexOverCAN"];
      mode_trigger_changed = true;
    }
  }

  const uint8_t effective_haldex_generation = haldex_generation_set ? next_haldex_generation : haldexGeneration;
  if ((mappings_changed || haldex_generation_set) &&
      seedMissingInputMappingsForGeneration(effective_haldex_generation, mapped_speed, mapped_throttle, mapped_rpm)) {
    mappings_changed = true;
  }
  if (haldex_generation_set &&
      seedModeTriggerForGeneration(effective_haldex_generation, next_mode_trigger, !mode_trigger_changed)) {
    mode_trigger_changed = true;
  } else if (mode_trigger_changed && next_mode_trigger.signal.length() == 0) {
    (void)seedModeTriggerForGeneration(effective_haldex_generation, next_mode_trigger, false);
  }

  if (mappings_changed) {
    if (!mappedInputSignalsSet(mapped_speed, mapped_throttle, mapped_rpm, 20)) {
      sendError(request, 503, "mapping update busy");
      return;
    }
    dirty = true;
  }

  if (dashboard_mappings_changed) {
    if (!dashboardSignalsSet(dashboard_slots, DASHBOARD_SIGNAL_SLOT_COUNT, 20)) {
      sendError(request, 503, "dashboard mapping update busy");
      return;
    }
    dirty = true;
  }

  if (mode_trigger_changed) {
    if (!modeTriggerConfigSet(next_mode_trigger, 20)) {
      sendError(request, 503, "mode trigger update busy");
      return;
    }
    modeTriggerRuntimeReset();
    mode_trigger_config = next_mode_trigger;
    dirty = true;
  }

  if (disable_controller_set) {
    if (next_disable_controller) {
      applyRuntimeMode(MODE_STOCK);
    } else if (state.mode == MODE_STOCK) {
      applyRuntimeMode(MODE_MAP);
    } else {
      disableController = false;
    }
    dirty = true;
  }

  if (broadcast_set) {
    broadcastOpenHaldexOverCAN = next_broadcast;
    dirty = true;
  }
  if (haldex_generation_set) {
    haldexGeneration = next_haldex_generation;
    dirty = true;
  }
  if (disable_throttle_set) {
    disableThrottle = next_disable_throttle;
    state.pedal_threshold = disableThrottle;
    dirty = true;
  }
  if (disable_speed_set) {
    disableSpeed = next_disable_speed;
    dirty = true;
  }
  if (log_to_file_set) {
    logToFileEnabled = next_log_to_file;
    dirty = true;
  }
  if (log_can_set) {
    logCanToFileEnabled = next_log_can;
    dirty = true;
  }
  if (log_error_set) {
    logErrorToFileEnabled = next_log_error;
    dirty = true;
  }
  if (log_serial_set) {
    logSerialEnabled = next_log_serial;
    dirty = true;
  }
  if (log_debug_firmware_set) {
    logDebugFirmwareEnabled = next_log_debug_firmware;
    dirty = true;
  }
  if (log_debug_network_set) {
    logDebugNetworkEnabled = next_log_debug_network;
    dirty = true;
  }
  if (log_debug_can_set) {
    logDebugCanEnabled = next_log_debug_can;
    dirty = true;
  }
  if (disengage_map_set) {
    disengageUnderSpeedMap = next_disengage_map;
    dirty = true;
  }
  if (disengage_speed_mode_set) {
    disengageUnderSpeedSpeedMode = next_disengage_speed_mode;
    dirty = true;
  }
  if (disengage_throttle_mode_set) {
    disengageUnderSpeedThrottleMode = next_disengage_throttle_mode;
    dirty = true;
  }
  if (disengage_rpm_mode_set) {
    disengageUnderSpeedRpmMode = next_disengage_rpm_mode;
    dirty = true;
  }
  if (lock_release_rate_set) {
    lockReleaseRatePctPerSec = next_lock_release_rate;
    dirty = true;
  }
  const bool debug_profile_enabled = logDebugFirmwareEnabled || logDebugNetworkEnabled || logDebugCanEnabled;
  if ((debug_profile_enabled || logCanToFileEnabled) && !logToFileEnabled) {
    logToFileEnabled = true;
    dirty = true;
    filelogLogWarn("settings", "forcing logToFileEnabled=1 while debug/can capture is active");
  }
  if (debug_profile_enabled && !logErrorToFileEnabled) {
    logErrorToFileEnabled = true;
    dirty = true;
    filelogLogWarn("settings", "forcing logErrorToFileEnabled=1 while debug capture is active");
  }

  if (loggingDebugCaptureActive()) {
    if (!disableController || state.mode != MODE_STOCK) {
      applyRuntimeMode(MODE_STOCK);
      filelogLogWarn("settings", "debug capture profile active; forcing STOCK controller-off");
      dirty = true;
    }
  }

  if (dirty) {
    storageMarkDirty();
    String msg;
    msg.reserve(96);
    msg += "updated";
    msg += " disableController=";
    msg += disableController ? "1" : "0";
    msg += " logToFile=";
    msg += logToFileEnabled ? "1" : "0";
    msg += " logCan=";
    msg += logCanToFileEnabled ? "1" : "0";
    msg += " logError=";
    msg += logErrorToFileEnabled ? "1" : "0";
    msg += " logSerial=";
    msg += logSerialEnabled ? "1" : "0";
    msg += " dbgFw=";
    msg += logDebugFirmwareEnabled ? "1" : "0";
    msg += " dbgNet=";
    msg += logDebugNetworkEnabled ? "1" : "0";
    msg += " dbgCan=";
    msg += logDebugCanEnabled ? "1" : "0";
    filelogLogEvent("settings", msg);
  }

  JsonDocument resp;
  resp["ok"] = true;
  resp["debugCaptureActive"] = loggingDebugCaptureActive();
  resp["disableController"] = disableController;
  resp["broadcastOpenHaldexOverCAN"] = broadcastOpenHaldexOverCAN;
  resp["effectiveBroadcastOpenHaldexOverCAN"] = openhaldexEffectiveBroadcastOpenHaldexOverCAN();
  resp["haldexGeneration"] = haldexGeneration;
  JsonObject respMappings = resp["inputMappings"].to<JsonObject>();
  respMappings["speed"] = mapped_speed;
  respMappings["throttle"] = mapped_throttle;
  respMappings["rpm"] = mapped_rpm;
  mode_trigger_config_t resp_default_trigger = {};
  const bool resp_has_default_trigger = getRecommendedModeTriggerForGeneration(haldexGeneration, resp_default_trigger);
  const String resp_mode_trigger_signal = mode_trigger_config.signal.length() > 0
                                            ? mode_trigger_config.signal
                                            : (resp_has_default_trigger ? resp_default_trigger.signal : "");
  mode_trigger_runtime_t resp_mode_trigger_runtime = {};
  modeTriggerRuntimeGet(resp_mode_trigger_runtime);
  JsonObject respTrigger = resp["modeTrigger"].to<JsonObject>();
  respTrigger["enabled"] = mode_trigger_config.enabled;
  respTrigger["signal"] = resp_mode_trigger_signal;
  respTrigger["defaultSignal"] = resp_has_default_trigger ? resp_default_trigger.signal : "";
  respTrigger["operator"] = modeTriggerOperatorName(mode_trigger_config.op);
  respTrigger["value"] = mode_trigger_config.value;
  respTrigger["mode"] = modeName(mode_trigger_config.mode);
  respTrigger["broadcastOpenHaldexOverCAN"] = mode_trigger_config.broadcastOpenHaldexOverCAN;
  respTrigger["active"] = resp_mode_trigger_runtime.active;
  respTrigger["seen"] = resp_mode_trigger_runtime.seen;
  respTrigger["lastValue"] = resp_mode_trigger_runtime.lastValue;
  respTrigger["ageMs"] = resp_mode_trigger_runtime.ageMs;
  respTrigger["effectiveMode"] = modeName(openhaldexEffectiveMode());
  respTrigger["effectiveBroadcastOpenHaldexOverCAN"] = openhaldexEffectiveBroadcastOpenHaldexOverCAN();
  sendJson(request, 200, resp);
}

static void handleCanviewCaptureGet(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["active"] = canview_capture_active;
  doc["disableController"] = disableController;
  doc["broadcastOpenHaldexOverCAN"] = broadcastOpenHaldexOverCAN;
  sendJson(request, 200, doc);
}

// Capture mode enters a safe bridge state while user records diagnostics.
static void handleCanviewCapturePost(AsyncWebServerRequest* request, const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    sendError(request, 400, "invalid json");
    return;
  }

  bool want_active = doc["active"] | false;
  bool changed = false;

  if (want_active && !canview_capture_active) {
    canview_capture_prev_disable_controller = disableController;
    canview_capture_prev_broadcast = broadcastOpenHaldexOverCAN;
    canview_capture_prev_mode_trigger_suppressed = modeTriggerSuppressed;
    canview_capture_prev_mode = state.mode;
    canview_capture_prev_valid = true;
    canview_capture_active = true;
    canviewCaptureApplySafeState();
    changed = true;
  } else if (!want_active && canview_capture_active) {
    canview_capture_active = false;
    if (canview_capture_prev_valid) {
      disableController = canview_capture_prev_disable_controller;
      state.mode = canview_capture_prev_mode;
      broadcastOpenHaldexOverCAN = canview_capture_prev_broadcast;
      modeTriggerSuppressed = canview_capture_prev_mode_trigger_suppressed;
      modeTriggerRuntimeReset();
      lastMode = (uint8_t)state.mode;
    }
    canview_capture_prev_valid = false;
    changed = true;
  }

  if (changed) {
    storageMarkDirty();
    filelogLogEvent("canview", String("capture mode ") + (canview_capture_active ? "enabled" : "disabled"));
  }

  JsonDocument resp;
  resp["ok"] = true;
  resp["active"] = canview_capture_active;
  resp["disableController"] = disableController;
  resp["broadcastOpenHaldexOverCAN"] = broadcastOpenHaldexOverCAN;
  sendJson(request, 200, resp);
}
// Active in-memory map payload (what the controller is currently using).
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

// Replace active in-memory map from UI/editor payload.
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
      filelogLogError("map", String("invalid lock table row size row=") + String(t) + " size=" + String(row.size()));
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
  filelogLogEvent("map", "active map updated");

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
  String ap_pass;
  storageGetWifiCreds(ssid, pass);
  doc["ssid"] = ssid;
  doc["staEnabled"] = storageGetWifiStaEnabled();
  doc["apPasswordSet"] = storageGetWifiApPassword(ap_pass) && ap_pass.length() >= 8 && ap_pass.length() <= 63;
  sendJson(request, 200, doc);
}

// Save hotspot credentials and STA enable policy, then re-apply Wi-Fi mode.
static void handleWifiPost(AsyncWebServerRequest* request, const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    sendError(request, 400, "invalid json");
    return;
  }

  bool changed = false;

  if (doc.containsKey("ssid") || doc.containsKey("password")) {
    String existing_ssid;
    String existing_pass;
    (void)storageGetWifiCreds(existing_ssid, existing_pass);

    const bool has_ssid = doc.containsKey("ssid");
    const bool has_password = doc.containsKey("password");

    String ssid = has_ssid ? String(doc["ssid"] | "") : existing_ssid;
    String pass = has_password ? String(doc["password"] | "") : existing_pass;

    if (ssid.length() == 0) {
      storageClearWifiCreds();
    } else {
      storageSetWifiCreds(ssid, pass);
    }
    changed = true;
  }

  if (doc.containsKey("apPassword")) {
    String ap_pass = doc["apPassword"] | "";
    if (ap_pass.length() == 0) {
      storageClearWifiApPassword();
    } else {
      if (ap_pass.length() < 8 || ap_pass.length() > 63) {
        sendError(request, 400, "ap password must be 8..63 chars or empty");
        return;
      }
      storageSetWifiApPassword(ap_pass);
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

// OTA state snapshot used by OTA page polling.
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

static void handleLogsList(AsyncWebServerRequest* request) {
  JsonDocument doc;
  JsonArray files = doc["files"].to<JsonArray>();
  filelogList(files);
  sendJson(request, 200, doc);
}

static void handleLogsRead(AsyncWebServerRequest* request) {
  if (!request->hasParam("path")) {
    sendError(request, 400, "missing path");
    return;
  }

  String path = request->getParam("path")->value();
  size_t max_bytes = 32768;
  if (request->hasParam("max")) {
    int max = request->getParam("max")->value().toInt();
    if (max > 0 && max <= 262144) {
      max_bytes = (size_t)max;
    }
  }

  String out;
  if (!filelogRead(path, out, max_bytes)) {
    sendError(request, 404, "log read failed");
    return;
  }

  AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", out);
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

static void handleLogsDelete(AsyncWebServerRequest* request, const String& body) {
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

  if (!filelogDelete(path)) {
    sendError(request, 400, "delete failed");
    return;
  }

  JsonDocument resp;
  resp["ok"] = true;
  resp["inputsMapped"] = inputsMappedForControl();
  sendJson(request, 200, resp);
}

static bool curvePointsStrictlyAscending(const uint16_t* bins, uint8_t count) {
  if (count == 0) {
    return false;
  }
  for (uint8_t i = 1; i < count; i++) {
    if (bins[i] <= bins[i - 1]) {
      return false;
    }
  }
  return true;
}

static void handleSpeedCurveGet(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["count"] = speed_curve_count;
  JsonArray points = doc["points"].to<JsonArray>();
  for (uint8_t i = 0; i < speed_curve_count && i < CURVE_POINTS_MAX; i++) {
    JsonObject p = points.add<JsonObject>();
    p["x"] = speed_curve_bins[i];
    p["lock"] = speed_curve_lock[i];
  }
  sendJson(request, 200, doc);
}

static void handleSpeedCurvePost(AsyncWebServerRequest* request, const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    sendError(request, 400, "invalid json");
    return;
  }

  JsonArray points = doc["points"].as<JsonArray>();
  if (points.isNull() || points.size() == 0 || points.size() > CURVE_POINTS_MAX) {
    sendError(request, 400, "invalid points");
    return;
  }

  uint8_t count = (uint8_t)points.size();
  uint16_t bins[CURVE_POINTS_MAX] = {};
  uint8_t locks[CURVE_POINTS_MAX] = {};

  for (uint8_t i = 0; i < count; i++) {
    int x = points[i]["x"] | -1;
    int lock = points[i]["lock"] | -1;
    if (x < 0 || x > 300 || lock < 0 || lock > 100) {
      filelogLogError("curve/speed",
                      String("point out of range idx=") + String(i) + " x=" + String(x) + " lock=" + String(lock));
      sendError(request, 400, "point out of range");
      return;
    }
    bins[i] = (uint16_t)x;
    locks[i] = (uint8_t)lock;
  }

  if (!curvePointsStrictlyAscending(bins, count)) {
    filelogLogError("curve/speed", String("points not strictly ascending count=") + String(count));
    sendError(request, 400, "points must ascend");
    return;
  }

  speed_curve_count = count;
  for (uint8_t i = 0; i < CURVE_POINTS_MAX; i++) {
    speed_curve_bins[i] = (i < count) ? bins[i] : 0;
    speed_curve_lock[i] = (i < count) ? locks[i] : 0;
  }

  storageMarkDirty();
  filelogLogEvent("curve/speed", String("saved count=") + String(count));

  JsonDocument resp;
  resp["ok"] = true;
  resp["debugCaptureActive"] = loggingDebugCaptureActive();
  resp["disableController"] = disableController;
  sendJson(request, 200, resp);
}

static void handleThrottleCurveGet(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["count"] = throttle_curve_count;
  JsonArray points = doc["points"].to<JsonArray>();
  for (uint8_t i = 0; i < throttle_curve_count && i < CURVE_POINTS_MAX; i++) {
    JsonObject p = points.add<JsonObject>();
    p["x"] = throttle_curve_bins[i];
    p["lock"] = throttle_curve_lock[i];
  }
  sendJson(request, 200, doc);
}

static void handleThrottleCurvePost(AsyncWebServerRequest* request, const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    sendError(request, 400, "invalid json");
    return;
  }

  JsonArray points = doc["points"].as<JsonArray>();
  if (points.isNull() || points.size() == 0 || points.size() > CURVE_POINTS_MAX) {
    sendError(request, 400, "invalid points");
    return;
  }

  uint8_t count = (uint8_t)points.size();
  uint16_t bins_u16[CURVE_POINTS_MAX] = {};
  uint8_t bins[CURVE_POINTS_MAX] = {};
  uint8_t locks[CURVE_POINTS_MAX] = {};

  for (uint8_t i = 0; i < count; i++) {
    int x = points[i]["x"] | -1;
    int lock = points[i]["lock"] | -1;
    if (x < 0 || x > 100 || lock < 0 || lock > 100) {
      filelogLogError("curve/throttle",
                      String("point out of range idx=") + String(i) + " x=" + String(x) + " lock=" + String(lock));
      sendError(request, 400, "point out of range");
      return;
    }
    bins[i] = (uint8_t)x;
    bins_u16[i] = (uint16_t)x;
    locks[i] = (uint8_t)lock;
  }

  if (!curvePointsStrictlyAscending(bins_u16, count)) {
    filelogLogError("curve/throttle", String("points not strictly ascending count=") + String(count));
    sendError(request, 400, "points must ascend");
    return;
  }

  throttle_curve_count = count;
  for (uint8_t i = 0; i < CURVE_POINTS_MAX; i++) {
    throttle_curve_bins[i] = (i < count) ? bins[i] : 0;
    throttle_curve_lock[i] = (i < count) ? locks[i] : 0;
  }

  storageMarkDirty();
  filelogLogEvent("curve/throttle", String("saved count=") + String(count));

  JsonDocument resp;
  resp["ok"] = true;
  resp["debugCaptureActive"] = loggingDebugCaptureActive();
  resp["disableController"] = disableController;
  sendJson(request, 200, resp);
}

static void handleRpmCurveGet(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["count"] = rpm_curve_count;
  JsonArray points = doc["points"].to<JsonArray>();
  for (uint8_t i = 0; i < rpm_curve_count && i < CURVE_POINTS_MAX; i++) {
    JsonObject p = points.add<JsonObject>();
    p["x"] = rpm_curve_bins[i];
    p["lock"] = rpm_curve_lock[i];
  }
  sendJson(request, 200, doc);
}

static void handleRpmCurvePost(AsyncWebServerRequest* request, const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    sendError(request, 400, "invalid json");
    return;
  }

  JsonArray points = doc["points"].as<JsonArray>();
  if (points.isNull() || points.size() == 0 || points.size() > CURVE_POINTS_MAX) {
    sendError(request, 400, "invalid points");
    return;
  }

  uint8_t count = (uint8_t)points.size();
  uint16_t bins[CURVE_POINTS_MAX] = {};
  uint8_t locks[CURVE_POINTS_MAX] = {};

  for (uint8_t i = 0; i < count; i++) {
    int x = points[i]["x"] | -1;
    int lock = points[i]["lock"] | -1;
    if (x < 0 || x > 10000 || lock < 0 || lock > 100) {
      filelogLogError("curve/rpm",
                      String("point out of range idx=") + String(i) + " x=" + String(x) + " lock=" + String(lock));
      sendError(request, 400, "point out of range");
      return;
    }
    bins[i] = (uint16_t)x;
    locks[i] = (uint8_t)lock;
  }

  if (!curvePointsStrictlyAscending(bins, count)) {
    filelogLogError("curve/rpm", String("points not strictly ascending count=") + String(count));
    sendError(request, 400, "points must ascend");
    return;
  }

  rpm_curve_count = count;
  for (uint8_t i = 0; i < CURVE_POINTS_MAX; i++) {
    rpm_curve_bins[i] = (i < count) ? bins[i] : 0;
    rpm_curve_lock[i] = (i < count) ? locks[i] : 0;
  }

  storageMarkDirty();
  filelogLogEvent("curve/rpm", String("saved count=") + String(count));

  JsonDocument resp;
  resp["ok"] = true;
  resp["debugCaptureActive"] = loggingDebugCaptureActive();
  resp["disableController"] = disableController;
  sendJson(request, 200, resp);
}

static void handleLogsClear(AsyncWebServerRequest* request, const String& body) {
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    sendError(request, 400, "invalid json");
    return;
  }

  String scope = doc["scope"] | "everything";
  if (!filelogClearScope(scope)) {
    sendError(request, 400, "clear failed");
    return;
  }

  JsonDocument resp;
  resp["ok"] = true;
  resp["debugCaptureActive"] = loggingDebugCaptureActive();
  resp["disableController"] = disableController;
  sendJson(request, 200, resp);
}

// Live CAN view endpoint (decoded + raw) with server-side limits and bus filter.
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

// One-shot text dump used for support/debug captures.
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
  server.on("/api/learn/status", HTTP_GET, [](AsyncWebServerRequest* request) { handleLearnStatus(request); });
  server.on("/api/learn/start", HTTP_POST, [](AsyncWebServerRequest* request) { handleLearnStart(request); });
  server.on("/api/learn/cancel", HTTP_POST, [](AsyncWebServerRequest* request) { handleLearnCancel(request); });
  server.on("/api/learn/clear", HTTP_POST, [](AsyncWebServerRequest* request) { handleLearnClear(request); });

  server.on(
    "/api/mode", HTTP_POST, [](AsyncWebServerRequest* request) { (void)request; }, nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      onJsonBody(request, data, len, index, total, handleModeJson);
    });

  server.on(
    "/api/settings", HTTP_POST, [](AsyncWebServerRequest* request) { (void)request; }, nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      onJsonBody(request, data, len, index, total, handleSettingsJson);
    });

  server.on("/api/map", HTTP_GET, [](AsyncWebServerRequest* request) { handleMapGet(request); });
  server.on("/api/curve/speed", HTTP_GET, [](AsyncWebServerRequest* request) { handleSpeedCurveGet(request); });
  server.on("/api/curve/throttle", HTTP_GET, [](AsyncWebServerRequest* request) { handleThrottleCurveGet(request); });
  server.on("/api/curve/rpm", HTTP_GET, [](AsyncWebServerRequest* request) { handleRpmCurveGet(request); });

  server.on(
    "/api/map", HTTP_POST, [](AsyncWebServerRequest* request) { (void)request; }, nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      onJsonBody(request, data, len, index, total, handleMapPost);
    });

  server.on(
    "/api/curve/speed", HTTP_POST, [](AsyncWebServerRequest* request) { (void)request; }, nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      onJsonBody(request, data, len, index, total, handleSpeedCurvePost);
    });

  server.on(
    "/api/curve/throttle", HTTP_POST, [](AsyncWebServerRequest* request) { (void)request; }, nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      onJsonBody(request, data, len, index, total, handleThrottleCurvePost);
    });

  server.on(
    "/api/curve/rpm", HTTP_POST, [](AsyncWebServerRequest* request) { (void)request; }, nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      onJsonBody(request, data, len, index, total, handleRpmCurvePost);
    });

  server.on("/api/maps", HTTP_GET, [](AsyncWebServerRequest* request) { handleMapsList(request); });

  server.on(
    "/api/maps/load", HTTP_POST, [](AsyncWebServerRequest* request) { (void)request; }, nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      onJsonBody(request, data, len, index, total, handleMapLoad);
    });

  server.on(
    "/api/maps/save", HTTP_POST, [](AsyncWebServerRequest* request) { (void)request; }, nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      onJsonBody(request, data, len, index, total, handleMapSave);
    });

  server.on(
    "/api/maps/delete", HTTP_POST, [](AsyncWebServerRequest* request) { (void)request; }, nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      onJsonBody(request, data, len, index, total, handleMapDelete);
    });

  server.on("/api/wifi", HTTP_GET, [](AsyncWebServerRequest* request) { handleWifiGet(request); });

  server.on(
    "/api/wifi", HTTP_POST, [](AsyncWebServerRequest* request) { (void)request; }, nullptr,
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

  server.on("/api/canview/dump", HTTP_GET, [](AsyncWebServerRequest* request) { handleCanviewDump(request); });
  server.on("/api/canview", HTTP_GET, [](AsyncWebServerRequest* request) { handleCanview(request); });
  server.on("/api/logs/read", HTTP_GET, [](AsyncWebServerRequest* request) { handleLogsRead(request); });
  server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest* request) { handleLogsList(request); });
  server.on(
    "/api/logs/delete", HTTP_POST, [](AsyncWebServerRequest* request) { (void)request; }, nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      onJsonBody(request, data, len, index, total, handleLogsDelete);
    });
  server.on(
    "/api/logs/clear", HTTP_POST, [](AsyncWebServerRequest* request) { (void)request; }, nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      onJsonBody(request, data, len, index, total, handleLogsClear);
    });
  server.on("/api/canview/capture", HTTP_GET, [](AsyncWebServerRequest* request) { handleCanviewCaptureGet(request); });
  server.on(
    "/api/canview/capture", HTTP_POST, [](AsyncWebServerRequest* request) { (void)request; }, nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      onJsonBody(request, data, len, index, total, handleCanviewCapturePost);
    });
}
