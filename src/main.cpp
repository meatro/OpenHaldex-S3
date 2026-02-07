#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>

#include "functions/api/api.h"
#include "functions/can/can.h"
#include "functions/tasks/tasks.h"
#include "functions/storage/storage.h"
#include "functions/web/web.h"
#include "functions/net/update.h"

static AsyncWebServer server(80);

static const char* AP_SSID = "OpenHaldex-S3";
static const char* MDNS_NAME = "openhaldex";
static const uint32_t WIFI_STA_TIMEOUT_MS = 15000;
static const char* OTA_VERSION_URL = "https://www.springfieldvw.com/openhaldex-s3/version.json";
static volatile bool g_internet_ok = false;

bool wifiInternetOk() {
  return g_internet_ok;
}

// Lightweight connectivity probe for OTA status.
// Only runs DNS lookup when STA is connected.
static void internetCheckTask(void* arg) {
  (void)arg;
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClientSecure client;
      client.setTimeout(8000);
      client.setHandshakeTimeout(8000);
      client.setInsecure();
      HTTPClient http;
      http.setTimeout(8000);
      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      http.setRedirectLimit(3);

      if (http.begin(client, OTA_VERSION_URL)) {
        int code = http.GET();
        g_internet_ok = (code == 200);
        http.end();
      } else {
        g_internet_ok = false;
      }
    } else {
      g_internet_ok = false;
    }
    vTaskDelay(pdMS_TO_TICKS(30000));
  }
}

// Advertise openhaldex.local on the currently active network interface(s).
static void startMdns() {
  MDNS.end();
  delay(50);
  for (uint8_t i = 0; i < 3; i++) {
    if (MDNS.begin(MDNS_NAME)) {
      MDNS.addService("http", "tcp", 80);
      Serial.println("mDNS: http://openhaldex.local");
      return;
    }
  }
  Serial.println("mDNS start failed");
}

// AP fallback path used when STA is disabled or hotspot connection fails.
static void startApOnly() {
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);
  WiFi.softAP(AP_SSID, nullptr, 1, false, 4);
  Serial.print("AP started, IP: ");
  Serial.println(WiFi.softAPIP());
  startMdns();
}

// Scan helper to locate saved hotspot channel for faster/cleaner association.
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

// Boot-time network policy:
// - If no saved creds => AP only
// - If STA enabled and hotspot reachable => STA+AP
// - Else fallback to AP only
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
  // Keep DHCP but force resolver selection for hotspot edge cases.
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

// Runtime apply path used by /api/wifi settings updates.
static void wifiApplyTask(void* arg) {
  (void)arg;
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  wifiStart();
  vTaskDelete(nullptr);
}

void wifiApplySettings() {
  xTaskCreate(wifiApplyTask, "wifiApply", 4096, nullptr, 1, nullptr);
}

void setup() {

  // Bring CAN online as early as possible (after transceiver enable pins).
  canInit();

  Serial.begin(115200);
  Serial.println("\n\n=== OpenHaldex S3 Starting ===");
  
  storageInit();
  storageLoad();

  tasksInit();

  wifiStart();
  updateInit();
  xTaskCreatePinnedToCore(internetCheckTask, "internetCheck", 4096, nullptr, 1, nullptr, 1);

  webInit(server);
  setupApi(server);
  server.begin();
}

void loop() {
  delay(10);
}
