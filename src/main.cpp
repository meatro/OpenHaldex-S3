#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>

#include "functions/api/api.h"
#include "functions/can/can.h"
#include "functions/tasks/tasks.h"
#include "functions/io/io.h"
#include "functions/storage/storage.h"
#include "functions/web/web.h"
#include "functions/net/update.h"

static AsyncWebServer server(80);

static const char* AP_SSID = "OpenHaldex-S3";
static const char* MDNS_NAME = "openhaldex";
static const uint32_t WIFI_STA_TIMEOUT_MS = 15000;
static volatile bool g_internet_ok = false;

bool wifiInternetOk() {
  return g_internet_ok;
}

static void internetCheckTask(void* arg) {
  (void)arg;
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      IPAddress ip;
      g_internet_ok = WiFi.hostByName("example.com", ip);
    } else {
      g_internet_ok = false;
    }
    vTaskDelay(pdMS_TO_TICKS(30000));
  }
}

static void startMdns() {
  MDNS.end();
  delay(50);
  for (uint8_t i = 0; i < 3; i++) {
    if (MDNS.begin(MDNS_NAME)) {
      MDNS.addService("http", "tcp", 80);
      Serial.println("mDNS: http://openhaldex.local");
      return;
    }
    delay(200);
  }
  Serial.println("mDNS start failed");
}

static void startApOnly() {
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);
  WiFi.softAP(AP_SSID, nullptr, 1, false, 4);
  Serial.print("AP started, IP: ");
  Serial.println(WiFi.softAPIP());
  startMdns();
}

static int findSsidChannel(const String& ssid) {
  int count = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/true);
  if (count <= 0) {
    Serial.println("STA scan: no networks found");
    return 0;
  }

  for (int i = 0; i < count; i++) {
    String found = WiFi.SSID(i);
    if (found == ssid) {
      int ch = WiFi.channel(i);
      Serial.print("STA scan: found SSID on channel ");
      Serial.println(ch);
      return ch;
    }
  }

  Serial.println("STA scan: SSID not found");
  return 0;
}

static void wifiStart() {
  String ssid;
  String pass;
  bool staEnabled = storageGetWifiStaEnabled();
  bool haveCreds = storageGetWifiCreds(ssid, pass) && ssid.length() > 0;

  if (!haveCreds) {
    Serial.println("No STA credentials, AP only");
    startApOnly();
    return;
  }

  // If STA was disabled, auto-enable only when the saved hotspot is actually present.
  if (!staEnabled) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(false);

    int probeChannel = findSsidChannel(ssid);

    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(50);

    if (probeChannel > 0) {
      Serial.println("Saved hotspot detected, auto-enabling STA");
      storageSetWifiStaEnabled(true);
      staEnabled = true;
    }
  }

  if (!staEnabled) {
    Serial.println("STA disabled, AP only");
    startApOnly();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);
  // Force DNS while keeping DHCP
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, IPAddress(8, 8, 8, 8), IPAddress(1, 1, 1, 1));
  Serial.print("STA connecting to SSID: ");
  Serial.println(ssid);

  int channel = findSsidChannel(ssid);
  if (channel > 0) {
    WiFi.begin(ssid.c_str(), pass.c_str(), channel);
  } else {
    WiFi.begin(ssid.c_str(), pass.c_str());
  }

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_STA_TIMEOUT_MS) {
    delay(200);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("STA connected, IP: ");
    Serial.println(WiFi.localIP());

    uint8_t apChannel = WiFi.channel();
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, nullptr, apChannel, false, 4);
    Serial.print("AP started (STA+AP ch ");
    Serial.print(apChannel);
    Serial.print("), IP: ");
    Serial.println(WiFi.softAPIP());
    startMdns();
  } else {
    Serial.println("STA connect failed, disabling STA until re-enabled");
    storageSetWifiStaEnabled(false);
    startApOnly();
  }
}

static void wifiApplyTask(void* arg) {
  (void)arg;
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  delay(200);
  wifiStart();
  vTaskDelete(nullptr);
}

void wifiApplySettings() {
  xTaskCreate(wifiApplyTask, "wifiApply", 4096, nullptr, 1, nullptr);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n=== OpenHaldex S3 Starting ===");

  storageInit();
  storageLoad();

  setupIO();
  setupButtons();

  wifiStart();
  updateInit();
  xTaskCreatePinnedToCore(internetCheckTask, "internetCheck", 4096, nullptr, 1, nullptr, 1);

  canInit();
  tasksInit();
  webInit(server);

  setupApi(server);
  server.begin();
}

void loop() {
  delay(10);
}
