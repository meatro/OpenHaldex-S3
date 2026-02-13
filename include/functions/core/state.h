#pragma once

#include "functions/core/state_model.h"

#include <Arduino.h>

enum openhaldex_mode_t {
  MODE_STOCK,
  MODE_FWD,
  MODE_5050,
  MODE_6040,
  MODE_7030,
  MODE_8020,
  MODE_9010,
  MODE_SPEED,
  MODE_THROTTLE,
  MODE_MAP,
  MODE_RPM,
  openhaldex_mode_t_MAX,
  MODE_CUSTOM = MODE_SPEED, // legacy alias
  MODE_7525 = MODE_7030     // legacy alias
};

struct lockpoint_t {
  uint8_t speed;
  uint8_t lock;
  uint8_t intensity;
};

#define CUSTOM_LOCK_POINTS_MAX_COUNT 10
struct openhaldex_custom_mode_t {
  lockpoint_t lockpoints[CUSTOM_LOCK_POINTS_MAX_COUNT];
  uint8_t lockpoint_bitfield_high_byte;
  uint8_t lockpoint_bitfield_low_byte;
  uint8_t lockpoint_count;
};

struct openhaldex_state_t {
  openhaldex_mode_t mode;
  openhaldex_custom_mode_t custom_mode;
  uint8_t pedal_threshold;
  bool mode_override;
};

const char* get_openhaldex_mode_string(openhaldex_mode_t mode);

// Global state
extern openhaldex_state_t state;
extern float lock_target;

// Telemetry (CAN)
extern uint8_t received_haldex_state;
extern uint8_t received_haldex_engagement_raw;
extern uint8_t received_haldex_engagement;
extern uint8_t appliedTorque;

extern float received_pedal_value;
extern uint16_t received_vehicle_speed;
extern uint16_t received_vehicle_rpm;
extern uint16_t received_vehicle_boost;
extern uint8_t haldexGeneration;

// Flags
extern bool isStandalone;
extern bool isGen1Standalone;
extern bool isGen2Standalone;
extern bool isGen4Standalone;
extern bool isBusFailure;
extern bool hasCANChassis;
extern bool hasCANHaldex;
extern bool broadcastOpenHaldexOverCAN;
extern bool disableController;
extern bool logToFileEnabled;
extern bool logCanToFileEnabled;
extern bool logErrorToFileEnabled;
extern bool logSerialEnabled;
extern bool logDebugFirmwareEnabled;
extern bool logDebugNetworkEnabled;
extern bool logDebugCanEnabled;
extern bool received_report_clutch1;
extern bool received_report_clutch2;
extern bool received_temp_protection;
extern bool received_coupling_open;
extern bool received_speed_limit;

bool loggingDebugCaptureActive();

void mappedInputSignalsInit();
bool mappedInputSignalsGet(String& speed, String& throttle, String& rpm, uint32_t timeout_ms = 0);
bool mappedInputSignalsSet(const String& speed, const String& throttle, const String& rpm, uint32_t timeout_ms = 50);
bool mappedInputSignalsConfigured();

extern long lastCANChassisTick;
extern long lastCANHaldexTick;
extern uint8_t lastMode;

extern bool can_ready;
extern bool can0_ready;
extern bool can1_ready;

// Settings
extern uint8_t disableThrottle;
extern uint16_t disableSpeed;
extern uint16_t disengageUnderSpeedMap;
extern uint16_t disengageUnderSpeedSpeedMode;
extern uint16_t disengageUnderSpeedThrottleMode;
extern uint16_t disengageUnderSpeedRpmMode;
extern float lockReleaseRatePctPerSec;

// 1D curve mode tables
#define CURVE_POINTS_MAX 12
extern uint8_t speed_curve_count;
extern uint16_t speed_curve_bins[CURVE_POINTS_MAX];
extern uint8_t speed_curve_lock[CURVE_POINTS_MAX];
extern uint8_t throttle_curve_count;
extern uint8_t throttle_curve_bins[CURVE_POINTS_MAX];
extern uint8_t throttle_curve_lock[CURVE_POINTS_MAX];
extern uint8_t rpm_curve_count;
extern uint16_t rpm_curve_bins[CURVE_POINTS_MAX];
extern uint8_t rpm_curve_lock[CURVE_POINTS_MAX];

// Map mode tables
#define MAP_SPEED_BINS 9
#define MAP_THROTTLE_BINS 7

extern uint16_t map_speed_bins[MAP_SPEED_BINS];
extern uint8_t map_throttle_bins[MAP_THROTTLE_BINS];
extern uint8_t map_lock_table[MAP_THROTTLE_BINS][MAP_SPEED_BINS];

// Debug stack markers
extern uint32_t stackCHS;
extern uint32_t stackHDX;
extern uint32_t stackframes10;
extern uint32_t stackframes20;
extern uint32_t stackframes25;
extern uint32_t stackframes100;
extern uint32_t stackframes200;
extern uint32_t stackframes1000;
extern uint32_t stackbroadcastOpenHaldex;
extern uint32_t stackupdateLabels;
extern uint32_t stackshowHaldexState;
extern uint32_t stackwriteEEP;
