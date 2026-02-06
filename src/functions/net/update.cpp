#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

#include "functions/config/config.h"
#include "functions/net/update.h"

#ifndef OPENHALDEX_VERSION
#define OPENHALDEX_VERSION "dev"
#endif

#ifndef OH_UPDATE_URL
#define OH_UPDATE_URL "https://www.springfieldvw.com/openhaldex-s3/version.json"
#endif

static portMUX_TYPE g_updateMux = portMUX_INITIALIZER_UNLOCKED;
static String g_latest;
static bool g_available = false;
static uint32_t g_lastCheckMs = 0;
static String g_error;
static String g_firmwareUrl;
static String g_filesystemUrl;
static bool g_installing = false;
static String g_installError;

static uint32_t g_progressTotal = 0;
static uint32_t g_progressDone = 0;
static float g_progressBps = 0.0f;
static uint32_t g_progressMs = 0;
static String g_progressStage;

static const uint32_t kOtaTimeoutMs = 60000;

static const char kGithubCaBundle[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIICOjCCAcGgAwIBAgIQQvLM2htpN0RfFf51KBC49DAKBggqhkjOPQQDAzBfMQswCQYDVQQGEwJH
QjEYMBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTYwNAYDVQQDEy1TZWN0aWdvIFB1YmxpYyBTZXJ2
ZXIgQXV0aGVudGljYXRpb24gUm9vdCBFNDYwHhcNMjEwMzIyMDAwMDAwWhcNNDYwMzIxMjM1OTU5
WjBfMQswCQYDVQQGEwJHQjEYMBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTYwNAYDVQQDEy1TZWN0
aWdvIFB1YmxpYyBTZXJ2ZXIgQXV0aGVudGljYXRpb24gUm9vdCBFNDYwdjAQBgcqhkjOPQIBBgUr
gQQAIgNiAAR2+pmpbiDt+dd34wc7qNs9Xzjoq1WmVk/WSOrsfy2qw7LFeeyZYX8QeccCWvkEN/U0
NSt3zn8gj1KjAIns1aeibVvjS5KToID1AZTc8GgHHs3u/iVStSBDHBv+6xnOQ6OjQjBAMB0GA1Ud
DgQWBBTRItpMWfFLXyY4qp3W7usNw/upYTAOBgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB
/zAKBggqhkjOPQQDAwNnADBkAjAn7qRaqCG76UeXlImldCBteU/IvZNeWBj7LRoAasm4PdCkT0RH
lAFWovgzJQxC36oCMB3q4S6ILuH5px0CMk7yn2xVdOOurvulGu7t0vzCAxHrRVxgED1cf5kDW21U
SAGKcw==
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
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
Q+6IHdfGjjxDah2nGN59PRbxYvnKkKj9
-----END CERTIFICATE-----
)EOF";

static void setupTls(WiFiClientSecure& client) {
  client.setTimeout(15000);
  client.setHandshakeTimeout(15000);
  client.setInsecure();
}

static void set_state(const String& latest, bool available, const String& error) {
  portENTER_CRITICAL(&g_updateMux);
  g_latest = latest;
  g_available = available;
  g_error = error;
  g_lastCheckMs = millis();
  portEXIT_CRITICAL(&g_updateMux);
}

static void set_urls(const String& fw, const String& fs) {
  portENTER_CRITICAL(&g_updateMux);
  g_firmwareUrl = fw;
  g_filesystemUrl = fs;
  portEXIT_CRITICAL(&g_updateMux);
}

static void set_install_state(bool installing, const String& error) {
  portENTER_CRITICAL(&g_updateMux);
  g_installing = installing;
  g_installError = error;
  if (!installing) {
    g_progressTotal = 0;
    g_progressDone = 0;
    g_progressBps = 0.0f;
    g_progressMs = 0;
    g_progressStage = "";
  }
  portEXIT_CRITICAL(&g_updateMux);
}

static void progressStart(const String& stage, uint32_t total) {
  portENTER_CRITICAL(&g_updateMux);
  g_progressStage = stage;
  g_progressTotal = total;
  g_progressDone = 0;
  g_progressBps = 0.0f;
  g_progressMs = millis();
  portEXIT_CRITICAL(&g_updateMux);
}

static void progressUpdate(uint32_t done, float bps) {
  portENTER_CRITICAL(&g_updateMux);
  g_progressDone = done;
  if (bps >= 0.0f) {
    g_progressBps = bps;
  }
  g_progressMs = millis();
  portEXIT_CRITICAL(&g_updateMux);
}

static void performCheck() {
  if (WiFi.status() != WL_CONNECTED) {
    set_state("", false, "offline");
    return;
  }

  WiFiClientSecure client;
  setupTls(client);
  HTTPClient http;
  http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setRedirectLimit(3);

  if (!http.begin(client, OH_UPDATE_URL)) {
    set_state("", false, "begin failed");
    return;
  }

  int code = http.GET();
  if (code == 200) {
    String body = http.getString();
    JsonDocument doc;
    auto err = deserializeJson(doc, body);
    if (!err) {
      String latest = doc["version"] | "";
      String fwName = doc["firmware"] | "";
      String fsName = doc["filesystem"] | "";

      String base = String(OH_UPDATE_URL);
      int slash = base.lastIndexOf("/");
      if (slash > 0)
        base = base.substring(0, slash);

      String fwUrl = fwName.length() ? (base + "/" + fwName) : "";
      String fsUrl = fsName.length() ? (base + "/" + fsName) : "";
      set_urls(fwUrl, fsUrl);

      bool available = latest.length() > 0 && latest != OPENHALDEX_VERSION;
      set_state(latest, available, "");
    } else {
      set_state("", false, "invalid json");
    }
  } else {
    set_state("", false, String("http ") + code);
  }

  http.end();
}

static bool downloadAndUpdate(const String& url, int updateCommand, const char* stage) {
  if (url.length() == 0) {
    Serial.println("OTA: missing URL");
    return false;
  }

  WiFiClientSecure client;
  setupTls(client);
  client.setTimeout(kOtaTimeoutMs);
  HTTPClient http;
  http.setTimeout(kOtaTimeoutMs);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setRedirectLimit(3);

  Serial.println(String("OTA: GET ") + url);
  if (!http.begin(client, url)) {
    Serial.println("OTA: http.begin failed");
    return false;
  }

  int code = http.GET();
  Serial.println(String("OTA: HTTP ") + code);
  if (code != 200) {
    Serial.println(String("OTA: HTTP error ") + http.errorToString(code));
    http.end();
    return false;
  }

  int len = http.getSize();
  uint32_t total = len > 0 ? (uint32_t)len : 0;
  if (len > 0) {
    Serial.println(String("OTA: size ") + len + " bytes");
  } else {
    Serial.println("OTA: size unknown");
  }

  if (!Update.begin(len > 0 ? (size_t)len : UPDATE_SIZE_UNKNOWN, updateCommand)) {
    Serial.println(String("OTA: Update.begin failed ") + Update.errorString());
    http.end();
    return false;
  }

  progressStart(stage ? String(stage) : String("update"), total);

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[1024];
  size_t written = 0;
  uint32_t lastSampleMs = millis();
  size_t lastSampleBytes = 0;
  uint32_t lastDataMs = millis();
  bool ok = true;

  while (http.connected() && (len < 0 || written < (size_t)len)) {
    size_t avail = stream->available();
    if (avail) {
      size_t toRead = avail > sizeof(buf) ? sizeof(buf) : avail;
      int read = stream->readBytes(buf, toRead);
      if (read <= 0) {
        continue;
      }
      size_t w = Update.write(buf, (size_t)read);
      if (w != (size_t)read) {
        Serial.println(String("OTA: write failed ") + Update.errorString());
        ok = false;
        break;
      }
      written += w;
      lastDataMs = millis();

      uint32_t now = millis();
      float bps = -1.0f;
      if (now - lastSampleMs >= 250) {
        bps = (float)(written - lastSampleBytes) * 1000.0f / (float)(now - lastSampleMs);
        lastSampleMs = now;
        lastSampleBytes = written;
      }
      progressUpdate((uint32_t)written, bps);
    } else {
      if (millis() - lastDataMs > kOtaTimeoutMs) {
        Serial.println("OTA: stream timeout");
        ok = false;
        break;
      }
      delay(1);
    }
  }

  progressUpdate((uint32_t)written, 0.0f);

  if (len > 0 && written < (size_t)len) {
    Serial.println("OTA: stream ended early");
    ok = false;
  }

  bool finished = Update.end();
  if (!finished) {
    Serial.println(String("OTA: Update.end failed ") + Update.errorString());
  }
  Serial.println(String("OTA: written ") + written + " bytes");
  http.end();

  return ok && finished && Update.isFinished();
}

static void updateInstallTask(void* arg) {
  (void)arg;

  if (WiFi.status() != WL_CONNECTED) {
    set_install_state(false, "offline");
    vTaskDelete(NULL);
    return;
  }

  String fwUrl;
  String fsUrl;
  portENTER_CRITICAL(&g_updateMux);
  fwUrl = g_firmwareUrl;
  fsUrl = g_filesystemUrl;
  portEXIT_CRITICAL(&g_updateMux);

  if (fwUrl.length() == 0) {
    set_install_state(false, "missing firmware url");
    vTaskDelete(NULL);
    return;
  }

  if (!downloadAndUpdate(fwUrl, U_FLASH, "firmware")) {
    set_install_state(false, "firmware update failed");
    vTaskDelete(NULL);
    return;
  }

  if (fsUrl.length() > 0) {
    if (!downloadAndUpdate(fsUrl, U_SPIFFS, "filesystem")) {
      set_install_state(false, "filesystem update failed");
      vTaskDelete(NULL);
      return;
    }
  }

  set_install_state(false, "");
  delay(500);
  ESP.restart();
}

void updateInit() {
  performCheck();
}

void updateCheckNow() {
  performCheck();
}

bool updateGetInfo(UpdateInfo& out) {
  portENTER_CRITICAL(&g_updateMux);
  out.current = OPENHALDEX_VERSION;
  out.latest = g_latest;
  out.available = g_available;
  out.lastCheckMs = g_lastCheckMs;
  out.error = g_error;
  out.url = OH_UPDATE_URL;
  out.firmwareUrl = g_firmwareUrl;
  out.filesystemUrl = g_filesystemUrl;
  out.installing = g_installing;
  out.installError = g_installError;
  out.bytesTotal = g_progressTotal;
  out.bytesDone = g_progressDone;
  out.speedBps = g_progressBps;
  out.progressMs = g_progressMs;
  out.stage = g_progressStage;
  portEXIT_CRITICAL(&g_updateMux);
  return true;
}

bool updateInstallStart() {
  portENTER_CRITICAL(&g_updateMux);
  if (g_installing) {
    portEXIT_CRITICAL(&g_updateMux);
    return false;
  }
  g_installing = true;
  g_installError = "";
  portEXIT_CRITICAL(&g_updateMux);

  xTaskCreate(updateInstallTask, "updateInstall", 8192, NULL, 1, NULL);
  return true;
}
