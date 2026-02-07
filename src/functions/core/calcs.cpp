#include "functions/core/calcs.h"

#include <Arduino.h>

#include "functions/core/state.h"
#include "functions/can/can_id.h"
#include "functions/can/standalone_can.h"

// Gate for any lock request generation.
// - MODE_MAP always uses the map table.
// - Preset/custom modes apply pedal/speed threshold rules.
static inline bool lock_enabled() {
  if (state.mode == MODE_MAP) {
    return true;
  }
  bool throttle_ok = false;
  bool speed_ok = false;

  if (state.mode != MODE_CUSTOM) {
    throttle_ok = (state.pedal_threshold == 0) || (int(received_pedal_value) >= state.pedal_threshold);
    speed_ok = (disableSpeed == 0) || (received_vehicle_speed <= disableSpeed);
    return throttle_ok && speed_ok;
  }

  if (state.mode == MODE_CUSTOM) {
    if (customSpeed && !customThrottle) {
      speed_ok = (speedArray[0] == 0) || (received_vehicle_speed <= speedArray[0]);
      throttle_ok = true;
    }
    if (customThrottle && !customSpeed) {
      throttle_ok = (throttleArray[0] == 0) || (int(received_pedal_value) >= throttleArray[0]);
      speed_ok = true;
    }
    if (customThrottle && customSpeed) {
      speed_ok = (speedArray[0] == 0) || (received_vehicle_speed <= speedArray[0]);
      throttle_ok = (throttleArray[0] == 0) || (int(received_pedal_value) >= throttleArray[0]);
    }

    return throttle_ok && speed_ok;
  }
  return false;
}

static float get_map_lock_target() {
  // 2D throttle/speed map with bilinear interpolation between bins.
  // Result is normalized to a lock percent in [0..100].
  if (!lock_enabled()) {
    return 0;
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

// Computes desired lock percent before generation-specific frame shaping.
// This is the single source of "requested lock" used by CAN frame mutation.
float get_lock_target_adjustment() {
  switch (state.mode) {
  case MODE_FWD:
    return 0;

  case MODE_5050:
    if (lock_enabled()) {
      return 100;
    }
    return 0;

  case MODE_6040:
    if (lock_enabled()) {
      return 40;
    }
    return 0;

  case MODE_7525:
    if (lock_enabled()) {
      return 30;
    }
    return 0;

  case MODE_CUSTOM:
    if (lock_enabled()) {
      return lockArray[0];
    }
    return 0;

  case MODE_MAP:
    return get_map_lock_target();

  default:
    return 0;
  }
}

// Converts a generation-specific control byte into a mode-adjusted byte.
// `invert=true` is used by frames where lower encoded values mean higher lock.
uint8_t get_lock_target_adjusted_value(uint8_t value, bool invert) {
  if (lock_target == 100) {
    if (lock_enabled()) {
      return (invert ? (0xFE - value) : value);
    }
    return (invert ? 0xFE : 0x00);
  }

  if (lock_target == 0) {
    return (invert ? 0xFE : 0x00);
  }

  float correction_factor = ((float)lock_target / 2) + 20;
  uint8_t corrected_value = value * (correction_factor / 100);
  if (lock_enabled()) {
    return (invert ? (0xFE - corrected_value) : corrected_value);
  }
  return (invert ? 0xFE : 0x00);
}