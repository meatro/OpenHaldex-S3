#include "functions/power/power.h"

#include <Preferences.h>
#include <WiFi.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

#include "functions/can/can.h"
#include "functions/can/can_id.h"
#include "functions/canview/dbc_common.h"
#include "functions/config/config.h"
#include "functions/config/pins.h"
#include "functions/core/state.h"
#include "functions/storage/storage.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const gpio_num_t k_boot_override_pin = GPIO_NUM_0;
static const gpio_num_t k_chassis_can_rx_pin = GPIO_NUM_6;

static const uint32_t k_zas_kl15_id = 0x3C0;
static const uint32_t k_nm_gateway_id = 0x1B000010;

static volatile bool g_maintenance_override = false;
static volatile bool g_entering_sleep = false;
static volatile bool g_ignition_seen = false;
static volatile bool g_ignition_on = false;
static volatile uint32_t g_last_awake_ms = 0;
static volatile uint32_t g_last_chassis_any_ms = 0;
static volatile uint32_t g_last_ignition_on_ms = 0;
static volatile uint32_t g_last_ignition_off_ms = 0;
static const char* volatile g_ignition_source = "";
static esp_sleep_wakeup_cause_t g_wake_cause = ESP_SLEEP_WAKEUP_UNDEFINED;
static TaskHandle_t g_power_monitor_handle = nullptr;

RTC_DATA_ATTR static uint32_t g_deep_sleep_entries = 0;

static uint32_t clamp_u32(uint32_t value, uint32_t min_value, uint32_t max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

const char* powerWakeCauseName() {
  switch (g_wake_cause) {
  case ESP_SLEEP_WAKEUP_EXT0:
    return "can_rx";
  case ESP_SLEEP_WAKEUP_EXT1:
    return "gpio";
  case ESP_SLEEP_WAKEUP_TIMER:
    return "timer";
  case ESP_SLEEP_WAKEUP_UNDEFINED:
    return "power_on_or_reset";
  default:
    break;
  }
  return "other";
}

void powerInitBootState() {
  g_wake_cause = esp_sleep_get_wakeup_cause();
  pinMode((uint8_t)k_boot_override_pin, INPUT_PULLUP);
  delay(2);
  g_maintenance_override = (digitalRead((uint8_t)k_boot_override_pin) == LOW);
  const uint32_t now = millis();
  g_last_awake_ms = now;
  g_last_chassis_any_ms = 0;
  g_last_ignition_on_ms = 0;
  g_last_ignition_off_ms = 0;
  g_ignition_seen = false;
  g_ignition_on = false;
  g_ignition_source = "";
}

void powerLoadBootSettings() {
  Preferences boot_pref;
  if (!boot_pref.begin("openhaldex", true)) {
    return;
  }

  const uint8_t stored_generation = boot_pref.getUChar("haldexGen", 255);
  if (stored_generation == 1 || stored_generation == 2 || stored_generation == 4 || stored_generation == 5) {
    haldexGeneration = stored_generation;
  }

  const bool stored_low_power = boot_pref.isKey("lpSleepOn")
                                  ? boot_pref.getBool("lpSleepOn", lowPowerSleepEnabled)
                                  : (haldexGeneration == 5);
  lowPowerSleepEnabled = stored_low_power;
  lowPowerSleepDelayMs = clamp_u32(boot_pref.getUInt("lpDelayMs", lowPowerSleepDelayMs), 5000, 600000);
  lowPowerWakeTimerSeconds = clamp_u32(boot_pref.getUInt("lpWakeSec", lowPowerWakeTimerSeconds), 30, 86400);
  lowPowerProbeDurationMs = clamp_u32(boot_pref.getUInt("lpProbeMs", lowPowerProbeDurationMs), 100, 5000);
  boot_pref.end();
}

bool powerLowPowerActive() {
  return lowPowerSleepEnabled && haldexGeneration == 5 && !g_maintenance_override;
}

static bool decode_ignition_frame(const twai_message_t& frame, bool& ignition_on, const char*& source) {
  const uint32_t id = frame.identifier & 0x1FFFFFFF;
  if (!frame.extd && id == k_zas_kl15_id && frame.data_length_code >= 3) {
    ignition_on = dbc_extract_raw(frame.data, 17, 1, 1) != 0;
    source = "ZAS_Kl_15";
    return true;
  }
  if (frame.extd && id == k_nm_gateway_id && frame.data_length_code >= 5) {
    ignition_on = dbc_extract_raw(frame.data, 32, 1, 1) != 0;
    source = "NM_Gateway_KL15";
    return true;
  }
  return false;
}

void powerTrackChassisFrame(const twai_message_t& frame, uint32_t now_ms) {
  g_last_chassis_any_ms = now_ms;
  if (haldexGeneration != 5) {
    return;
  }

  bool ignition_on = false;
  const char* source = "";
  if (!decode_ignition_frame(frame, ignition_on, source)) {
    return;
  }

  g_ignition_seen = true;
  g_ignition_on = ignition_on;
  g_ignition_source = source;
  if (ignition_on) {
    g_last_ignition_on_ms = now_ms;
    g_last_awake_ms = now_ms;
  } else {
    g_last_ignition_off_ms = now_ms;
  }
}

static bool probe_chassis_ignition(uint32_t duration_ms) {
  if (!canInitChassisOnly(TWAI_MODE_LISTEN_ONLY)) {
    return true;
  }

  const uint32_t started_ms = millis();
  bool ignition_on = false;
  while ((millis() - started_ms) < duration_ms) {
    twai_message_t frame = {};
    bool got_frame = false;
    while (chassis_can_receive(frame)) {
      got_frame = true;
      const uint32_t now = millis();
      powerTrackChassisFrame(frame, now);
      bool decoded_ignition_on = false;
      const char* source = "";
      if (decode_ignition_frame(frame, decoded_ignition_on, source) && decoded_ignition_on) {
        ignition_on = true;
        break;
      }
    }
    if (ignition_on) {
      break;
    }
    if (!got_frame) {
      delay(1);
    } else {
      taskYIELD();
    }
  }

  canDeinit();
  return ignition_on;
}

static void shutdown_wifi_for_sleep() {
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  WiFi.setSleep(true);
}

static void configure_can_rx_wakeup() {
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  rtc_gpio_deinit(k_chassis_can_rx_pin);
  pinMode(CAN1_RX, INPUT);
  rtc_gpio_pullup_dis(k_chassis_can_rx_pin);
  rtc_gpio_pulldown_dis(k_chassis_can_rx_pin);

  esp_err_t err = esp_sleep_enable_ext0_wakeup(k_chassis_can_rx_pin, 0);
  if (err != ESP_OK) {
    LOG_ERROR("power", "CAN RX wake setup failed err=%s", esp_err_to_name(err));
  }

  if (lowPowerWakeTimerSeconds > 0) {
    const uint64_t wake_us = (uint64_t)lowPowerWakeTimerSeconds * 1000000ULL;
    err = esp_sleep_enable_timer_wakeup(wake_us);
    if (err != ESP_OK) {
      LOG_ERROR("power", "timer wake setup failed err=%s", esp_err_to_name(err));
    }
  }
}

static void enter_parked_sleep(const char* reason) {
  if (g_entering_sleep) {
    return;
  }
  g_entering_sleep = true;

  LOG_INFO("power", "Entering parked sleep reason=%s wakeCause=%s", reason ? reason : "unknown", powerWakeCauseName());
  disableController = true;
  state.mode = MODE_STOCK;
  lock_target = 0.0f;
  modeTriggerRuntimeReset();

  if (storageFsReady() && storageIsDirty()) {
    storageSave();
  }

  shutdown_wifi_for_sleep();
  haldexCanSleep();
  canDeinit();
  configure_can_rx_wakeup();
  g_deep_sleep_entries++;
  delay(20);
  esp_deep_sleep_start();
}

bool powerBootProbeOrSleep() {
  if (!powerLowPowerActive()) {
    if (g_maintenance_override && lowPowerSleepEnabled && haldexGeneration == 5) {
      LOG_WARN("power", "Parked sleep bypassed by BOOT override");
    }
    return true;
  }

  LOG_INFO("power", "Gen5 parked sleep armed wakeCause=%s probeMs=%lu", powerWakeCauseName(),
           (unsigned long)lowPowerProbeDurationMs);
  const bool ignition_on = probe_chassis_ignition(lowPowerProbeDurationMs);
  if (ignition_on) {
    g_ignition_seen = true;
    g_ignition_on = true;
    g_last_awake_ms = millis();
    LOG_INFO("power", "Ignition present; continuing active boot");
    return true;
  }

  enter_parked_sleep("boot_probe_no_ignition");
  return false;
}

static bool ignition_recent(uint32_t now_ms) {
  if (!g_ignition_on || g_last_ignition_on_ms == 0) {
    return false;
  }
  return (now_ms - g_last_ignition_on_ms) <= lowPowerSleepDelayMs;
}

static void powerMonitorTask(void* arg) {
  (void)arg;
  for (;;) {
    const uint32_t now = millis();
    if (!powerLowPowerActive()) {
      g_last_awake_ms = now;
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    if (haldexLearnActive || loggingDebugCaptureActive()) {
      g_last_awake_ms = now;
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    if (ignition_recent(now)) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    if ((now - g_last_awake_ms) >= lowPowerSleepDelayMs) {
      enter_parked_sleep(g_ignition_seen ? "ignition_off" : "ignition_unknown");
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void powerStartMonitor() {
  if (g_power_monitor_handle) {
    return;
  }
  xTaskCreatePinnedToCore(powerMonitorTask, "powerMonitor", 4096, nullptr, 1, &g_power_monitor_handle,
                          OH_APP_TASK_CORE);
}

void powerWriteStatusJson(JsonObject out) {
  const uint32_t now = millis();
  const uint32_t last_chassis_age = g_last_chassis_any_ms > 0 ? now - g_last_chassis_any_ms : 0;
  const uint32_t last_ignition_on_age = g_last_ignition_on_ms > 0 ? now - g_last_ignition_on_ms : 0;
  const uint32_t last_ignition_off_age = g_last_ignition_off_ms > 0 ? now - g_last_ignition_off_ms : 0;

  out["lowPowerSleepEnabled"] = lowPowerSleepEnabled;
  out["active"] = powerLowPowerActive();
  out["maintenanceOverride"] = (bool)g_maintenance_override;
  out["ignitionSeen"] = (bool)g_ignition_seen;
  out["ignitionOn"] = ignition_recent(now);
  out["ignitionRawOn"] = (bool)g_ignition_on;
  out["ignitionSource"] = g_ignition_source ? g_ignition_source : "";
  out["lastChassisAgeMs"] = last_chassis_age;
  out["lastIgnitionOnAgeMs"] = last_ignition_on_age;
  out["lastIgnitionOffAgeMs"] = last_ignition_off_age;
  out["sleepDelayMs"] = lowPowerSleepDelayMs;
  out["wakeTimerSeconds"] = lowPowerWakeTimerSeconds;
  out["probeDurationMs"] = lowPowerProbeDurationMs;
  out["wakeCause"] = powerWakeCauseName();
  out["deepSleepEntries"] = g_deep_sleep_entries;
}
