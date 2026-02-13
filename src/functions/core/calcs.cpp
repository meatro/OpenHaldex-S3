#include "functions/core/calcs.h"

#include <Arduino.h>

#include "functions/core/state.h"
#include "functions/can/can_id.h"
#include "functions/can/standalone_can.h"

// Gate for any lock request generation.
// - MODE_MAP / MODE_SPEED / MODE_THROTTLE / MODE_RPM always use their tables.
// - Preset modes apply pedal/speed threshold rules.
static inline bool lock_enabled() {
  if (state.mode == MODE_MAP || state.mode == MODE_SPEED || state.mode == MODE_THROTTLE || state.mode == MODE_RPM) {
    return true;
  }
  bool throttle_ok = (state.pedal_threshold == 0) || (int(received_pedal_value) >= state.pedal_threshold);
  bool speed_ok = (disableSpeed == 0) || (received_vehicle_speed <= disableSpeed);
  return throttle_ok && speed_ok;
}

static uint16_t current_dynamic_mode_disengage_speed() {
  switch (state.mode) {
  case MODE_MAP:
    return disengageUnderSpeedMap;
  case MODE_SPEED:
    return disengageUnderSpeedSpeedMode;
  case MODE_THROTTLE:
    return disengageUnderSpeedThrottleMode;
  case MODE_RPM:
    return disengageUnderSpeedRpmMode;
  default:
    return 0;
  }
}

static bool dynamic_mode_speed_gate_allows_lock() {
  const uint16_t disengage_under_speed = current_dynamic_mode_disengage_speed();
  if (disengage_under_speed == 0) {
    return true;
  }
  // Full-throttle launch override: allow request below cutoff when driver commands full pedal.
  if (received_pedal_value >= 99.0f) {
    return true;
  }
  return received_vehicle_speed >= disengage_under_speed;
}

static float interpolate_curve_u16(uint16_t x, const uint16_t* bins, const uint8_t* values, uint8_t count) {
  if (count == 0 || bins == nullptr || values == nullptr) {
    return 0.0f;
  }

  if (count == 1 || x <= bins[0]) {
    return (float)values[0];
  }

  for (uint8_t i = 0; i < (uint8_t)(count - 1); i++) {
    if (x <= bins[i + 1]) {
      float left_x = (float)bins[i];
      float right_x = (float)bins[i + 1];
      float left_y = (float)values[i];
      float right_y = (float)values[i + 1];
      float denom = right_x - left_x;
      float ratio = (denom > 0.0f) ? (((float)x - left_x) / denom) : 0.0f;
      return left_y + ((right_y - left_y) * ratio);
    }
  }

  return (float)values[count - 1];
}

static float get_speed_lock_target() {
  if (!lock_enabled()) {
    return 0.0f;
  }
  if (!dynamic_mode_speed_gate_allows_lock()) {
    return 0.0f;
  }
  float lock = interpolate_curve_u16(received_vehicle_speed, speed_curve_bins, speed_curve_lock, speed_curve_count);
  if (lock < 0.0f)
    lock = 0.0f;
  if (lock > 100.0f)
    lock = 100.0f;
  return lock;
}

static float get_throttle_lock_target() {
  if (!lock_enabled()) {
    return 0.0f;
  }
  if (!dynamic_mode_speed_gate_allows_lock()) {
    return 0.0f;
  }
  float throttle = received_pedal_value;
  if (throttle < 0.0f)
    throttle = 0.0f;
  if (throttle > 100.0f)
    throttle = 100.0f;

  uint16_t throttle_bins_u16[CURVE_POINTS_MAX] = {};
  for (uint8_t i = 0; i < throttle_curve_count && i < CURVE_POINTS_MAX; i++) {
    throttle_bins_u16[i] = throttle_curve_bins[i];
  }

  float lock = interpolate_curve_u16((uint16_t)throttle, throttle_bins_u16, throttle_curve_lock, throttle_curve_count);
  if (lock < 0.0f)
    lock = 0.0f;
  if (lock > 100.0f)
    lock = 100.0f;
  return lock;
}

static float get_map_lock_target() {
  // 2D throttle/speed map with bilinear interpolation between bins.
  // Result is normalized to a lock percent in [0..100].
  if (!lock_enabled()) {
    return 0;
  }
  if (!dynamic_mode_speed_gate_allows_lock()) {
    return 0.0f;
  }

  float throttle = received_pedal_value;
  if (throttle < 0) {
    throttle = 0;
  }
  if (throttle > 100) {
    throttle = 100;
  }

  float speed = received_vehicle_speed;
  if (speed < 0) {
    speed = 0;
  }

  uint8_t t0 = 0;
  uint8_t t1 = 0;
  float t_ratio = 0;

  if (throttle >= map_throttle_bins[MAP_THROTTLE_BINS - 1]) {
    t0 = MAP_THROTTLE_BINS - 1;
    t1 = t0;
  } else {
    for (uint8_t i = 0; i < MAP_THROTTLE_BINS - 1; i++) {
      if (throttle <= map_throttle_bins[i + 1]) {
        t0 = i;
        t1 = i + 1;
        float denom = (float)map_throttle_bins[t1] - (float)map_throttle_bins[t0];
        t_ratio = (denom > 0) ? ((throttle - map_throttle_bins[t0]) / denom) : 0;
        break;
      }
    }
  }

  uint8_t s0 = 0;
  uint8_t s1 = 0;
  float s_ratio = 0;

  if (speed >= map_speed_bins[MAP_SPEED_BINS - 1]) {
    s0 = MAP_SPEED_BINS - 1;
    s1 = s0;
  } else {
    for (uint8_t i = 0; i < MAP_SPEED_BINS - 1; i++) {
      if (speed <= map_speed_bins[i + 1]) {
        s0 = i;
        s1 = i + 1;
        float denom = (float)map_speed_bins[s1] - (float)map_speed_bins[s0];
        s_ratio = (denom > 0) ? ((speed - map_speed_bins[s0]) / denom) : 0;
        break;
      }
    }
  }

  float v00 = map_lock_table[t0][s0];
  float v01 = map_lock_table[t0][s1];
  float v10 = map_lock_table[t1][s0];
  float v11 = map_lock_table[t1][s1];

  float v0 = v00 + ((v01 - v00) * s_ratio);
  float v1 = v10 + ((v11 - v10) * s_ratio);
  float v = v0 + ((v1 - v0) * t_ratio);

  if (v < 0) {
    v = 0;
  }
  if (v > 100) {
    v = 100;
  }
  return v;
}

static float get_rpm_lock_target() {
  if (!lock_enabled()) {
    return 0.0f;
  }
  if (!dynamic_mode_speed_gate_allows_lock()) {
    return 0.0f;
  }
  float lock = interpolate_curve_u16(received_vehicle_rpm, rpm_curve_bins, rpm_curve_lock, rpm_curve_count);
  if (lock < 0.0f)
    lock = 0.0f;
  if (lock > 100.0f)
    lock = 100.0f;
  return lock;
}

// Down-ramp smoother for requested lock. Upshifts remain immediate; downshifts are rate-limited
// to reduce driveline clunk when throttle/load drops quickly.

static float smooth_lock_release(float raw_target) {
  static bool initialized = false;
  static float smoothed = 0.0f;
  static uint32_t last_ms = 0;

  if (raw_target < 0.0f) {
    raw_target = 0.0f;
  }
  if (raw_target > 100.0f) {
    raw_target = 100.0f;
  }

  const uint32_t now_ms = millis();
  if (!initialized) {
    initialized = true;
    smoothed = raw_target;
    last_ms = now_ms;
    return smoothed;
  }

  uint32_t dt_ms = now_ms - last_ms;
  last_ms = now_ms;

  // If timing jumps (boot/resume), avoid stale ramp behavior.
  if (dt_ms > 5000) {
    smoothed = raw_target;
    return smoothed;
  }

  if (raw_target >= smoothed) {
    smoothed = raw_target;
    return smoothed;
  }

  // Universal release rate (percent lock per second). Tune lower for softer release.
  // 0 disables smoothing and makes release immediate.
  float release_rate_pct_per_sec = lockReleaseRatePctPerSec;
  if (release_rate_pct_per_sec <= 0.0f) {
    smoothed = raw_target;
    return smoothed;
  }

  float step = (release_rate_pct_per_sec * (float)dt_ms) / 1000.0f;
  if (step < 0.1f) {
    step = 0.1f;
  }
  smoothed -= step;
  if (smoothed < raw_target) {
    smoothed = raw_target;
  }

  return smoothed;
}

// Computes desired lock percent before generation-specific frame shaping.
// This is the single source of "requested lock" used by CAN frame mutation.
float get_lock_target_adjustment() {
  float raw_target = 0.0f;
  switch (state.mode) {
  case MODE_FWD:
    raw_target = 0.0f;
    break;

  case MODE_5050:
    raw_target = lock_enabled() ? 100.0f : 0.0f;
    break;

  case MODE_6040:
    raw_target = lock_enabled() ? 40.0f : 0.0f;
    break;

  case MODE_7030:
    raw_target = lock_enabled() ? 30.0f : 0.0f;
    break;

  case MODE_8020:
    raw_target = lock_enabled() ? 20.0f : 0.0f;
    break;

  case MODE_9010:
    raw_target = lock_enabled() ? 10.0f : 0.0f;
    break;

  case MODE_SPEED:
    raw_target = get_speed_lock_target();
    break;

  case MODE_THROTTLE:
    raw_target = get_throttle_lock_target();
    break;

  case MODE_MAP:
    raw_target = get_map_lock_target();
    break;

  case MODE_RPM:
    raw_target = get_rpm_lock_target();
    break;

  default:
    raw_target = 0.0f;
    break;
  }

  return smooth_lock_release(raw_target);
}
// Converts a generation-specific control byte into a mode-adjusted byte.
// `invert=true` is used by frames where lower encoded values mean higher lock.
uint8_t get_lock_target_adjusted_value(uint8_t value, bool invert) {
  float requested_lock = lock_target;
  if (requested_lock < 0.0f) {
    requested_lock = 0.0f;
  }
  if (requested_lock > 100.0f) {
    requested_lock = 100.0f;
  }
  // Avoid a large discontinuity between 99.x and 100 by snapping near-full requests to full scale.
  if (requested_lock >= 99.5f) {
    requested_lock = 100.0f;
  }

  if (requested_lock >= 100.0f) {
    if (lock_enabled()) {
      return (invert ? (0xFE - value) : value);
    }
    return (invert ? 0xFE : 0x00);
  }

  if (requested_lock <= 0.0f) {
    return (invert ? 0xFE : 0x00);
  }

  float correction_factor = (requested_lock * 0.5f) + 20.0f;
  uint8_t corrected_value = value * (correction_factor / 100);
  if (lock_enabled()) {
    return (invert ? (0xFE - corrected_value) : corrected_value);
  }
  return (invert ? 0xFE : 0x00);
}
