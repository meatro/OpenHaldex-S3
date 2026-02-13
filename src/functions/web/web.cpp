#include "functions/web/web.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <Update.h>

#include "functions/config/config.h"
#include "functions/storage/storage.h"

static bool fs_ok = false;

static void sendNotFound(AsyncWebServerRequest* request) {
  request->send(404, "text/plain", "Not found");
}

static void otaSendResult(AsyncWebServerRequest* request, bool ok) {
  AsyncWebServerResponse* response = request->beginResponse(ok ? 200 : 500, "text/plain", ok ? "OK" : "FAIL");
  response->addHeader("Connection", "close");
  request->send(response);

  if (ok) {
    delay(100);
    ESP.restart();
  }
}

void webInit(AsyncWebServer& server) {
  fs_ok = storageFsReady();
  if (!fs_ok) {
    DEBUG("LittleFS not mounted");
  }

  // Avoid stale UI assets after LittleFS updates.
  auto& staticHandler = server.serveStatic("/", LittleFS, "/");
  staticHandler.setDefaultFile("index.html");
  staticHandler.setCacheControl("no-cache, no-store, must-revalidate");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!fs_ok) {
      request->send(500, "text/plain", "LittleFS not mounted");
    } else if (LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      request->send(500, "text/plain", "Missing /index.html in LittleFS");
    }
  });

  server.on("/map", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!fs_ok) {
      request->send(500, "text/plain", "LittleFS not mounted");
    } else if (LittleFS.exists("/map.html")) {
      request->send(LittleFS, "/map.html", "text/html");
    } else {
      request->send(500, "text/plain", "Missing /map.html in LittleFS");
    }
  });

  server.on("/canview", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!fs_ok) {
      request->send(500, "text/plain", "LittleFS not mounted");
    } else if (LittleFS.exists("/canview.html")) {
      request->send(LittleFS, "/canview.html", "text/html");
    } else {
      request->send(500, "text/plain", "Missing /canview.html in LittleFS");
    }
  });

  server.on("/diag", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!fs_ok) {
      request->send(500, "text/plain", "LittleFS not mounted");
    } else if (LittleFS.exists("/diag.html")) {
      request->send(LittleFS, "/diag.html", "text/html");
    } else {
      request->send(500, "text/plain", "Missing /diag.html in LittleFS");
    }
  });

  server.on("/ota", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!fs_ok) {
      request->send(500, "text/plain", "LittleFS not mounted");
    } else if (LittleFS.exists("/ota.html")) {
      request->send(LittleFS, "/ota.html", "text/html");
    } else {
      request->send(500, "text/plain", "Missing /ota.html in LittleFS");
    }
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    String out;
    out += "fs_mounted=";
    out += (fs_ok ? "1" : "0");
    out += "\n";
    if (!fs_ok) {
      request->send(200, "text/plain", out);
      return;
    }
    File root = LittleFS.open("/");
    if (!root) {
      out += "files=OPEN_FAIL\n";
      request->send(200, "text/plain", out);
      return;
    }
    File file = root.openNextFile();
    while (file) {
      out += file.name();
      out += "\t";
      out += String(file.size());
      out += "\n";
      file = root.openNextFile();
    }
    request->send(200, "text/plain", out);
  });

  server.on(
    "/ota/update", HTTP_POST,
    [](AsyncWebServerRequest* request) {
      bool ok = !Update.hasError();
      otaSendResult(request, ok);
    },
    [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
      // Handle OTA firmware upload chunk by chunk.
      if (index == 0) {
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
#if enableDebug
          Update.printError(Serial);
#endif
        }
      }

      if (!Update.hasError()) {
        if (Update.write(data, len) != len) {
#if enableDebug
          Update.printError(Serial);
#endif
        }
      }

      if (final) {
        if (!Update.end(true)) {
#if enableDebug
          Update.printError(Serial);
#endif
        }
      }
    });

  server.onNotFound([](AsyncWebServerRequest* request) { sendNotFound(request); });
}

void disconnectWifi() {
#if enableDebug
  DEBUG("WiFi disconnect requested (not implemented yet)");
#endif
}
