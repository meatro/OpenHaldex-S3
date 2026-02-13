#include <cmath>
#include <cstdlib>
#include <cstring>
#include "functions/can/can.h"

#include "functions/config/pins.h"
#include "functions/config/config.h"
#include "functions/core/state.h"
#include "functions/core/calcs.h"
#include "functions/can/can_id.h"
#include "functions/canview/canview.h"
#include "functions/canview/vw_pq_chassis_dbc.h"
#include "functions/can/can_state.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Frame equality helper used to mark generated (mutated) traffic for CAN View.
static bool can_messages_equal(const twai_message_t& a, const twai_message_t& b) {
  if (a.identifier != b.identifier)
    return false;
  if (a.extd != b.extd)
    return false;
  if (a.rtr != b.rtr)
    return false;
  if (a.data_length_code != b.data_length_code)
    return false;
  for (uint8_t i = 0; i < a.data_length_code && i < 8; i++) {
    if (a.data[i] != b.data[i])
      return false;
  }
  return true;
}

enum mapped_bus_t { MAPPED_BUS_UNKNOWN = 0, MAPPED_BUS_ANY, MAPPED_BUS_CHASSIS, MAPPED_BUS_HALDEX };

struct mapped_signal_binding_t {
  String source_key;
  mapped_bus_t bus = MAPPED_BUS_UNKNOWN;
  uint32_t frame_id = 0;
  const dbc_signal_t* signal = nullptr;
  bool ready = false;
};

static volatile uint32_t mapped_speed_tick_ms = 0;
static volatile uint32_t mapped_throttle_tick_ms = 0;
static volatile uint32_t mapped_rpm_tick_ms = 0;
static const uint32_t k_mapped_input_timeout_ms = 1000;

static String normalize_mapping_token(const String& input) {
  String out = input;
  out.trim();
  out.toLowerCase();
  return out;
}

static String normalize_signal_name(const char* name) {
  String out = String(name ? name : "");
  out.replace("_", " ");
  out.trim();
  out.toLowerCase();
  return out;
}

static String normalize_signal_unit(const char* unit) {
  String out = String(unit ? unit : "");
  out.trim();
  out.toLowerCase();
  return out;
}

static bool split_mapping_key(const String& key, String& bus, String& frame, String& signal, String& unit) {
  int p1 = key.indexOf('|');
  int p2 = (p1 >= 0) ? key.indexOf('|', p1 + 1) : -1;
  int p3 = (p2 >= 0) ? key.indexOf('|', p2 + 1) : -1;
  if (p1 < 0 || p2 < 0 || p3 < 0) {
    return false;
  }
  bus = key.substring(0, p1);
  frame = key.substring(p1 + 1, p2);
  signal = key.substring(p2 + 1, p3);
  unit = key.substring(p3 + 1);
  bus = normalize_mapping_token(bus);
  frame = normalize_mapping_token(frame);
  signal = normalize_mapping_token(signal);
  unit = normalize_mapping_token(unit);
  return true;
}

static mapped_bus_t parse_mapping_bus(const String& bus) {
  if (bus == "all" || bus.length() == 0) {
    return MAPPED_BUS_ANY;
  }
  if (bus == "chassis" || bus == "chs") {
    return MAPPED_BUS_CHASSIS;
  }
  if (bus == "haldex" || bus == "hdx") {
    return MAPPED_BUS_HALDEX;
  }
  return MAPPED_BUS_UNKNOWN;
}

static bool parse_mapping_frame_id(const String& frame, uint32_t& out_id) {
  if (frame.length() == 0) {
    return false;
  }
  char* endptr = nullptr;
  const unsigned long parsed = strtoul(frame.c_str(), &endptr, 0);
  if (endptr == frame.c_str() || *endptr != '\0') {
    return false;
  }
  out_id = (uint32_t)parsed;
  return true;
}

static const dbc_signal_t* find_dbc_signal(uint32_t id, const String& signal_name, const String& signal_unit) {
  const dbc_signal_t* name_match = nullptr;
  for (uint16_t i = 0; i < k_vw_pq_chassis_signal_count; i++) {
    const dbc_signal_t* sig = &k_vw_pq_chassis_signals[i];
    if (sig->id != id) {
      continue;
    }
    if (normalize_signal_name(sig->name) != signal_name) {
      continue;
    }
    if (normalize_signal_unit(sig->unit) == signal_unit) {
      return sig;
    }
    if (!name_match) {
      name_match = sig;
    }
  }
  return name_match;
}

static const dbc_signal_t* find_mux_signal(uint32_t id) {
  for (uint16_t i = 0; i < k_vw_pq_chassis_signal_count; i++) {
    const dbc_signal_t* sig = &k_vw_pq_chassis_signals[i];
    if (sig->id == id && sig->mux == -2) {
      return sig;
    }
  }
  return nullptr;
}

static bool frame_mux_matches(const dbc_signal_t* signal, const twai_message_t& frame) {
  if (!signal || signal->mux < 0) {
    return true;
  }
  const dbc_signal_t* mux_sig = find_mux_signal(signal->id);
  if (!mux_sig) {
    return false;
  }
  const uint64_t raw =
    vw_pq_dbc_extract_raw(frame.data, mux_sig->start_bit, mux_sig->length, mux_sig->is_little_endian);
  return ((int)raw) == signal->mux;
}

static bool mapping_bus_matches(mapped_bus_t bus, uint8_t bus_index) {
  if (bus == MAPPED_BUS_ANY) {
    return true;
  }
  if (bus == MAPPED_BUS_CHASSIS) {
    return bus_index == 0;
  }
  if (bus == MAPPED_BUS_HALDEX) {
    return bus_index != 0;
  }
  return false;
}

static void refresh_binding(mapped_signal_binding_t& binding, const String& source_key_raw) {
  const String source_key = normalize_mapping_token(source_key_raw);
  if (binding.source_key == source_key) {
    return;
  }

  binding.source_key = source_key;
  binding.bus = MAPPED_BUS_UNKNOWN;
  binding.frame_id = 0;
  binding.signal = nullptr;
  binding.ready = false;

  if (source_key.length() == 0) {
    return;
  }

  String bus;
  String frame;
  String signal;
  String unit;
  if (!split_mapping_key(source_key, bus, frame, signal, unit)) {
    return;
  }

  binding.bus = parse_mapping_bus(bus);
  if (binding.bus == MAPPED_BUS_UNKNOWN) {
    return;
  }

  if (!parse_mapping_frame_id(frame, binding.frame_id)) {
    return;
  }

  binding.signal = find_dbc_signal(binding.frame_id, signal, unit);
  binding.ready = binding.signal != nullptr;
}

static bool apply_binding_from_frame(const mapped_signal_binding_t& binding, const twai_message_t& frame,
                                     uint8_t bus_index, float& out_value) {
  if (!binding.ready || !binding.signal) {
    return false;
  }
  if (!mapping_bus_matches(binding.bus, bus_index)) {
    return false;
  }
  if ((frame.identifier & 0x1FFFFFFF) != (binding.frame_id & 0x1FFFFFFF)) {
    return false;
  }
  if (!frame_mux_matches(binding.signal, frame)) {
    return false;
  }
  const float value = vw_pq_dbc_decode_signal(binding.signal, frame.data);
  if (!std::isfinite(value)) {
    return false;
  }
  out_value = value;
  return true;
}

// Chassis-side receive loop:
// - caches incoming chassis traffic for CAN View
// - updates core telemetry (throttle/rpm/speed)
// - mutates pass-through frames when controller is enabled
// - forwards toward Haldex bus
void parseCAN_chs(void* arg) {
  static uint32_t last_abs_speed_ms = 0;
  static bool abs_speed_valid = false;
  static const uint32_t k_abs_speed_timeout_ms = 500;
  static const uint16_t k_rx_burst_yield_frames = 64;
  static mapped_signal_binding_t mapped_speed_binding = {};
  static mapped_signal_binding_t mapped_throttle_binding = {};
  static mapped_signal_binding_t mapped_rpm_binding = {};
  static String mapped_speed_source = "";
  static String mapped_throttle_source = "";
  static String mapped_rpm_source = "";

  while (1) {
#if detailedDebugStack
    stackCHS = uxTaskGetStackHighWaterMark(NULL);
#endif
    (void)mappedInputSignalsGet(mapped_speed_source, mapped_throttle_source, mapped_rpm_source, 0);
    refresh_binding(mapped_speed_binding, mapped_speed_source);
    refresh_binding(mapped_throttle_binding, mapped_throttle_source);
    refresh_binding(mapped_rpm_binding, mapped_rpm_source);

    uint16_t burst_frames = 0;
    while (chassis_can_receive(rx_msg_chs())) {
      lastCANChassisTick = millis();
      canviewCacheFrame(rx_msg_chs(), 0);
      const uint32_t now_ms = millis();

      float mapped_value = 0.0f;
      if (apply_binding_from_frame(mapped_throttle_binding, rx_msg_chs(), 0, mapped_value)) {
        if (mapped_value < 0.0f) {
          mapped_value = 0.0f;
        }
        received_pedal_value = mapped_value;
        vehicle_state.throttle = received_pedal_value;
        mapped_throttle_tick_ms = now_ms;
      }
      if (apply_binding_from_frame(mapped_rpm_binding, rx_msg_chs(), 0, mapped_value)) {
        if (mapped_value < 0.0f) {
          mapped_value = 0.0f;
        }
        received_vehicle_rpm = (uint16_t)lroundf(mapped_value);
        mapped_rpm_tick_ms = now_ms;
      }
      if (apply_binding_from_frame(mapped_speed_binding, rx_msg_chs(), 0, mapped_value)) {
        if (mapped_value < 0.0f) {
          mapped_value = 0.0f;
        }
        received_vehicle_speed = (uint16_t)lroundf(mapped_value);
        vehicle_state.speed = received_vehicle_speed;
        mapped_speed_tick_ms = now_ms;
      }

      const bool speed_mapped_recent =
        (mapped_speed_source.length() > 0) && ((now_ms - (uint32_t)mapped_speed_tick_ms) <= k_mapped_input_timeout_ms);
      const bool throttle_mapped_recent = (mapped_throttle_source.length() > 0) &&
                                          ((now_ms - (uint32_t)mapped_throttle_tick_ms) <= k_mapped_input_timeout_ms);
      const bool rpm_mapped_recent =
        (mapped_rpm_source.length() > 0) && ((now_ms - (uint32_t)mapped_rpm_tick_ms) <= k_mapped_input_timeout_ms);

      tx_msg_hdx().identifier = rx_msg_chs().identifier;

      if (isGen1Standalone || isGen2Standalone || isGen4Standalone) {
        switch (rx_msg_chs().identifier) {
        case diagnostics_1_ID:
        case diagnostics_2_ID:
        case diagnostics_3_ID:
        case diagnostics_4_ID:
        case diagnostics_5_ID:
          // Standalone mode keeps diagnostic gateway traffic alive across buses.
          tx_msg_hdx() = rx_msg_chs();
          tx_msg_hdx().extd = rx_msg_chs().extd;
          tx_msg_hdx().rtr = rx_msg_chs().rtr;
          tx_msg_hdx().data_length_code = rx_msg_chs().data_length_code;
          haldex_can_send(tx_msg_hdx(), (10 / portTICK_PERIOD_MS), false);
          break;
        }
      }

      if (!isGen1Standalone && !isGen2Standalone && !isGen4Standalone) {
        switch (rx_msg_chs().identifier) {
        case MOTOR1_ID:
          if (!throttle_mapped_recent) {
            received_pedal_value = rx_msg_chs().data[5] * 0.4f;
            vehicle_state.throttle = received_pedal_value;
          }
          if (!rpm_mapped_recent) {
            received_vehicle_rpm = ((rx_msg_chs().data[3] << 8) | rx_msg_chs().data[2]) * 0.25f;
          }
          break;

        case BRAKES1_ID: {
          // ABS aggregate vehicle speed (BR1_Wheel_Speed_kmh, 0.01 km/h units).
          if (!speed_mapped_recent) {
            uint64_t raw = vw_pq_dbc_extract_raw(rx_msg_chs().data, 17, 15, 1);
            received_vehicle_speed = (uint16_t)((raw + 50) / 100);
            vehicle_state.speed = received_vehicle_speed;
            last_abs_speed_ms = millis();
            abs_speed_valid = true;
          }
          break;
        }

        case MOTOR2_ID:
          // Fallback only when ABS speed is missing/stale.
          if (!speed_mapped_recent && (!abs_speed_valid || (millis() - last_abs_speed_ms) > k_abs_speed_timeout_ms)) {
            received_vehicle_speed = rx_msg_chs().data[3] * 128 / 100;
            vehicle_state.speed = received_vehicle_speed;
          }
          break;

        case OPENHALDEX_EXTERNAL_CONTROL_ID:
          // External mode messages are intentionally ignored for mode selection.
          // Main UI/API configuration is authoritative.
          break;
        }

        bool generatedFrame = false;
        twai_message_t original = rx_msg_chs();

        // STOCK: bridge unchanged. MAP: mutate known control frames for selected generation.
        if (state.mode != MODE_STOCK) {
          if (haldexGeneration == 1 || haldexGeneration == 2 || haldexGeneration == 4) {
            getLockData(rx_msg_chs());
            generatedFrame = !can_messages_equal(original, rx_msg_chs());
          }
        } else {
          lock_target = 0;
        }

        tx_msg_hdx() = rx_msg_chs();
        tx_msg_hdx().extd = rx_msg_chs().extd;
        tx_msg_hdx().rtr = rx_msg_chs().rtr;
        tx_msg_hdx().data_length_code = rx_msg_chs().data_length_code;
        haldex_can_send(tx_msg_hdx(), (10 / portTICK_PERIOD_MS), generatedFrame);
      }

      if (++burst_frames >= k_rx_burst_yield_frames) {
        burst_frames = 0;
        taskYIELD();
      }
    }

    vTaskDelay(1);
  }
}

// Haldex-side receive loop:
// - caches incoming Haldex traffic for CAN View
// - derives telemetry/status flags used by diagnostics UI
// - optionally rebroadcasts Haldex frames onto chassis bus
void parseCAN_hdx(void* arg) {
  static const uint16_t k_rx_burst_yield_frames = 64;
  static mapped_signal_binding_t mapped_speed_binding = {};
  static mapped_signal_binding_t mapped_throttle_binding = {};
  static mapped_signal_binding_t mapped_rpm_binding = {};
  static String mapped_speed_source = "";
  static String mapped_throttle_source = "";
  static String mapped_rpm_source = "";

  while (1) {
#if detailedDebugStack
    stackHDX = uxTaskGetStackHighWaterMark(NULL);
#endif
    (void)mappedInputSignalsGet(mapped_speed_source, mapped_throttle_source, mapped_rpm_source, 0);
    refresh_binding(mapped_speed_binding, mapped_speed_source);
    refresh_binding(mapped_throttle_binding, mapped_throttle_source);
    refresh_binding(mapped_rpm_binding, mapped_rpm_source);

    uint16_t burst_frames = 0;
    while (haldex_can_receive(rx_msg_hdx())) {
      lastCANHaldexTick = millis();
      canviewCacheFrame(rx_msg_hdx(), 1);
      const uint32_t now_ms = millis();

      float mapped_value = 0.0f;
      if (apply_binding_from_frame(mapped_throttle_binding, rx_msg_hdx(), 1, mapped_value)) {
        if (mapped_value < 0.0f) {
          mapped_value = 0.0f;
        }
        received_pedal_value = mapped_value;
        vehicle_state.throttle = received_pedal_value;
        mapped_throttle_tick_ms = now_ms;
      }
      if (apply_binding_from_frame(mapped_rpm_binding, rx_msg_hdx(), 1, mapped_value)) {
        if (mapped_value < 0.0f) {
          mapped_value = 0.0f;
        }
        received_vehicle_rpm = (uint16_t)lroundf(mapped_value);
        mapped_rpm_tick_ms = now_ms;
      }
      if (apply_binding_from_frame(mapped_speed_binding, rx_msg_hdx(), 1, mapped_value)) {
        if (mapped_value < 0.0f) {
          mapped_value = 0.0f;
        }
        received_vehicle_speed = (uint16_t)lroundf(mapped_value);
        vehicle_state.speed = received_vehicle_speed;
        mapped_speed_tick_ms = now_ms;
      }

      // Engagement extraction is generation-specific and follows legacy OpenHaldex behavior.
      if (haldexGeneration == 1) {
        received_haldex_engagement_raw = rx_msg_hdx().data[1];
        received_haldex_engagement = map(received_haldex_engagement_raw, 128, 198, 0, 100);
      }

      if (haldexGeneration == 2) {
        received_haldex_engagement_raw = rx_msg_hdx().data[1] + rx_msg_hdx().data[4];
        received_haldex_engagement = map(received_haldex_engagement_raw, 128, 255, 0, 100);
      }

      if (haldexGeneration == 4) {
        received_haldex_engagement_raw = rx_msg_hdx().data[1];
        received_haldex_engagement = map(received_haldex_engagement_raw, 128, 255, 0, 100);
      }
      received_haldex_state = rx_msg_hdx().data[0];
      awd_state.actual = received_haldex_engagement;

      received_report_clutch1 = (received_haldex_state & (1 << 0));
      received_temp_protection = (received_haldex_state & (1 << 1));
      received_report_clutch2 = (received_haldex_state & (1 << 2));
      received_coupling_open = (received_haldex_state & (1 << 3));
      received_speed_limit = (received_haldex_state & (1 << 6));

      // Forward Haldex traffic onto chassis CAN (bridge behavior).
      chassis_can_send(rx_msg_hdx(), (10 / portTICK_PERIOD_MS));

      if (++burst_frames >= k_rx_burst_yield_frames) {
        burst_frames = 0;
        taskYIELD();
      }
    }

    vTaskDelay(1);
  }
}
