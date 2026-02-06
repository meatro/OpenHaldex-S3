#include "functions/core/state.h"

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
bool received_report_clutch1 = false;
bool received_report_clutch2 = false;
bool received_temp_protection = false;
bool received_coupling_open = false;
bool received_speed_limit = false;

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

bool customSpeed = false;
bool customThrottle = false;
uint16_t speedArray[5] = {0, 0, 0, 0, 0};
uint8_t throttleArray[5] = {0, 0, 0, 0, 0};
uint8_t lockArray[5] = {0, 0, 0, 0, 0};

uint16_t map_speed_bins[MAP_SPEED_BINS] = {0, 5, 10, 20, 40, 60, 80, 100, 140};
uint8_t map_throttle_bins[MAP_THROTTLE_BINS] = {0, 5, 10, 20, 40, 60, 80};
uint8_t map_lock_table[MAP_THROTTLE_BINS][MAP_SPEED_BINS] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0},         {0, 0, 0, 0, 0, 0, 0, 0, 0},        {0, 0, 5, 5, 5, 5, 0, 0, 0},
  {0, 5, 10, 15, 15, 10, 5, 0, 0},     {5, 10, 20, 25, 25, 20, 15, 10, 5}, {10, 20, 30, 40, 40, 30, 25, 20, 15},
  {20, 30, 45, 60, 60, 50, 40, 30, 20}};

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
  case MODE_7525:
    return "7525";
  case MODE_CUSTOM:
    return "CUSTOM";
  case MODE_MAP:
    return "MAP";
  default:
    break;
  }
  return "?";
}
