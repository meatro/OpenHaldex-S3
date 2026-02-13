#include "functions/core/state.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Global state variables - shared across all modules
// Contains controller state, vehicle telemetry, and CAN bus status

openhaldex_state_t state = {};
VehicleState vehicle_state = {};
AWDState awd_state = {};
float lock_target = 0.0f;

uint8_t received_haldex_state = 0;
uint8_t received_haldex_engagement_raw = 0;
uint8_t received_haldex_engagement = 0;
uint8_t appliedTorque = 0;

float received_pedal_value = 0.0f;
uint16_t received_vehicle_speed = 0;
uint16_t received_vehicle_rpm = 0;
uint16_t received_vehicle_boost = 0;
uint8_t haldexGeneration = 0;

bool isStandalone = false;
bool isGen1Standalone = false;
bool isGen2Standalone = false;
bool isGen4Standalone = false;
bool isBusFailure = false;
bool hasCANChassis = false;
bool hasCANHaldex = false;
bool broadcastOpenHaldexOverCAN = true;
bool disableController = false;
bool logToFileEnabled = true;
bool logCanToFileEnabled = false;
bool logErrorToFileEnabled = true;
bool logSerialEnabled = false;
bool logDebugFirmwareEnabled = false;
bool logDebugNetworkEnabled = false;
bool logDebugCanEnabled = false;
bool received_report_clutch1 = false;
bool received_report_clutch2 = false;
bool received_temp_protection = false;
bool received_coupling_open = false;
bool received_speed_limit = false;

static SemaphoreHandle_t mapped_input_mutex = nullptr;
static String mapped_input_speed_signal = "";
static String mapped_input_throttle_signal = "";
static String mapped_input_rpm_signal = "";

long lastCANChassisTick = 0;
long lastCANHaldexTick = 0;
uint8_t lastMode = 0;

bool can_ready = false;
bool can0_ready = false;
bool can1_ready = false;

uint32_t stackCHS = 0;
uint32_t stackHDX = 0;
uint32_t stackframes10 = 0;
uint32_t stackframes20 = 0;
uint32_t stackframes25 = 0;
uint32_t stackframes100 = 0;
uint32_t stackframes200 = 0;
uint32_t stackframes1000 = 0;
uint32_t stackbroadcastOpenHaldex = 0;
uint32_t stackupdateLabels = 0;
uint32_t stackshowHaldexState = 0;
uint32_t stackwriteEEP = 0;

uint8_t disableThrottle = 0;
uint16_t disableSpeed = 0;
uint16_t disengageUnderSpeedMap = 0;
uint16_t disengageUnderSpeedSpeedMode = 0;
uint16_t disengageUnderSpeedThrottleMode = 0;
uint16_t disengageUnderSpeedRpmMode = 0;
float lockReleaseRatePctPerSec = 120.0f;

uint8_t speed_curve_count = 5;
uint16_t speed_curve_bins[CURVE_POINTS_MAX] = {0, 20, 40, 80, 140, 0, 0, 0, 0, 0, 0, 0};
uint8_t speed_curve_lock[CURVE_POINTS_MAX] = {50, 45, 35, 20, 0, 0, 0, 0, 0, 0, 0, 0};
uint8_t throttle_curve_count = 5;
uint8_t throttle_curve_bins[CURVE_POINTS_MAX] = {0, 10, 25, 50, 80, 0, 0, 0, 0, 0, 0, 0};
uint8_t throttle_curve_lock[CURVE_POINTS_MAX] = {0, 10, 25, 55, 80, 0, 0, 0, 0, 0, 0, 0};
uint8_t rpm_curve_count = 6;
uint16_t rpm_curve_bins[CURVE_POINTS_MAX] = {0, 1000, 2000, 3500, 5000, 6500, 0, 0, 0, 0, 0, 0};
uint8_t rpm_curve_lock[CURVE_POINTS_MAX] = {0, 10, 30, 55, 80, 100, 0, 0, 0, 0, 0, 0};

uint16_t map_speed_bins[MAP_SPEED_BINS] = {0, 5, 10, 20, 40, 60, 80, 100, 140};
uint8_t map_throttle_bins[MAP_THROTTLE_BINS] = {0, 5, 10, 20, 40, 60, 80};
uint8_t map_lock_table[MAP_THROTTLE_BINS][MAP_SPEED_BINS] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0},         {0, 0, 0, 0, 0, 0, 0, 0, 0},        {0, 0, 5, 5, 5, 5, 0, 0, 0},
  {0, 5, 10, 15, 15, 10, 5, 0, 0},     {5, 10, 20, 25, 25, 20, 15, 10, 5}, {10, 20, 30, 40, 40, 30, 25, 20, 15},
  {20, 30, 45, 60, 60, 50, 40, 30, 20}};

static SemaphoreHandle_t mappedInputMutexHandle() {
  if (!mapped_input_mutex) {
    mapped_input_mutex = xSemaphoreCreateMutex();
  }
  return mapped_input_mutex;
}

void mappedInputSignalsInit() {
  (void)mappedInputMutexHandle();
}

bool mappedInputSignalsGet(String& speed, String& throttle, String& rpm, uint32_t timeout_ms) {
  SemaphoreHandle_t mutex = mappedInputMutexHandle();
  if (!mutex) {
    return false;
  }

  TickType_t wait_ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
  if (xSemaphoreTake(mutex, wait_ticks) != pdTRUE) {
    return false;
  }

  speed = mapped_input_speed_signal;
  throttle = mapped_input_throttle_signal;
  rpm = mapped_input_rpm_signal;
  xSemaphoreGive(mutex);
  return true;
}

bool mappedInputSignalsSet(const String& speed, const String& throttle, const String& rpm, uint32_t timeout_ms) {
  SemaphoreHandle_t mutex = mappedInputMutexHandle();
  if (!mutex) {
    return false;
  }

  TickType_t wait_ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
  if (xSemaphoreTake(mutex, wait_ticks) != pdTRUE) {
    return false;
  }

  mapped_input_speed_signal = speed;
  mapped_input_throttle_signal = throttle;
  mapped_input_rpm_signal = rpm;
  xSemaphoreGive(mutex);
  return true;
}

bool mappedInputSignalsConfigured() {
  String speed;
  String throttle;
  String rpm;
  if (!mappedInputSignalsGet(speed, throttle, rpm, 1)) {
    return false;
  }
  return speed.length() > 0 && throttle.length() > 0 && rpm.length() > 0;
}

bool loggingDebugCaptureActive() {
  return logDebugFirmwareEnabled || logDebugNetworkEnabled || logDebugCanEnabled || logCanToFileEnabled;
}

const char* get_openhaldex_mode_string(openhaldex_mode_t mode) {
  switch (mode) {
  case MODE_STOCK:
    return "STOCK";
  case MODE_FWD:
    return "FWD";
  case MODE_5050:
    return "5050";
  case MODE_6040:
    return "6040";
  case MODE_7030:
    return "7030";
  case MODE_8020:
    return "8020";
  case MODE_9010:
    return "9010";
  case MODE_SPEED:
    return "SPEED";
  case MODE_THROTTLE:
    return "THROTTLE";
  case MODE_MAP:
    return "MAP";
  case MODE_RPM:
    return "RPM";
  default:
    break;
  }
  return "?";
}
