#include "functions/core/calcs.h"

#include <Arduino.h>

#include "functions/core/state.h"
#include "functions/can/can_id.h"
#include "functions/can/standalone_can.h"

// Determines if AWD lock should be active based on current mode, speed, and throttle thresholds
// Only executed when in MODE_FWD/MODE_5050/MODE_CUSTOM
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
  // Calculate lock target from 2D map using bilinear interpolation
  // Maps throttle % and speed to desired lock percentage
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

// Only executed when in MODE_FWD/MODE_5050/MODE_CUSTOM
float get_lock_target_adjustment() {
  // Handle FWD and 5050 modes.
  // Handle FWD and 5050 modes.
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

  // Getting here means it's in not FWD or 5050/7525.

  // Check if locking is necessary.
  if (!lock_enabled()) {
    return 0;
  }

  // Find the pair of lockpoints between which the vehicle speed falls.
  lockpoint_t lp_lower = state.custom_mode.lockpoints[0];
  lockpoint_t lp_upper = state.custom_mode.lockpoints[state.custom_mode.lockpoint_count - 1];

  // Look for the lockpoint above the current vehicle speed.
  for (uint8_t i = 0; i < state.custom_mode.lockpoint_count; i++) {
    if (received_vehicle_speed <= state.custom_mode.lockpoints[i].speed) {
      lp_upper = state.custom_mode.lockpoints[i];
      lp_lower = state.custom_mode.lockpoints[(i == 0) ? 0 : (i - 1)];
      break;
    }
  }

  // Handle the case where the vehicle speed is lower than the lowest lockpoint.
  if (received_vehicle_speed <= lp_lower.speed) {
    return lp_lower.lock;
  }

  // Handle the case where the vehicle speed is higher than the highest lockpoint.
  if (received_vehicle_speed >= lp_upper.speed) {
    return lp_upper.lock;
  }

  // In all other cases, interpolation is necessary.
  float inter = (float)(lp_upper.speed - lp_lower.speed) / (float)(received_vehicle_speed - lp_lower.speed);

  // Calculate the target.
  float target = lp_lower.lock + ((float)(lp_upper.lock - lp_lower.lock) / inter);
  // DEBUG("lp_upper:%d@%d lp_lower:%d@%d speed:%d target:%0.2f", lp_upper.lock, lp_upper.speed, lp_lower.lock,
  // lp_lower.speed, received_vehicle_speed, target);
  return target;
}

// Only executed when in MODE_FWD/MODE_5050/MODE_CUSTOM
uint8_t get_lock_target_adjusted_value(uint8_t value, bool invert) {
  // Handle 5050 mode.
  if (lock_target == 100) {
    // is this needed?  Should be caught in get_lock_target_adjustment
    if (lock_enabled()) {
      return (invert ? (0xFE - value) : value);
    }
    return (invert ? 0xFE : 0x00);
  }

  // Handle FWD and CUSTOM modes.
  // No correction is necessary if the target is already 0.
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

// Only executed when in MODE_FWD/MODE_5050/MODE_CUSTOM
