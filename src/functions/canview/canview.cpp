#include "functions/canview/canview.h"
#include "functions/can/can_id.h"
#include "functions/canview/vw_mqb_chassis_dbc.h"
#include "functions/canview/vw_pq_chassis_dbc.h"
#include "functions/core/state.h"
#include "functions/storage/filelog.h"
#include <math.h>
#include <stdlib.h>

struct canview_frame_t {
  uint32_t key;
  uint32_t id;
  uint32_t ts;
  uint8_t dlc;
  uint8_t extd;
  uint8_t rtr;
  bool generated;
  uint8_t data[8];
};

#define CANVIEW_CHASSIS_CACHE_SIZE 96
#define CANVIEW_HALDEX_CACHE_SIZE 48
#define CANVIEW_RAW_HISTORY 32
#define CANVIEW_DUMP_HISTORY 512
#define CANVIEW_STALE_MS 1500

static canview_frame_t canview_chassis_cache[CANVIEW_CHASSIS_CACHE_SIZE];
static canview_frame_t canview_haldex_cache[CANVIEW_HALDEX_CACHE_SIZE];
static canview_frame_t canview_raw_chassis[CANVIEW_RAW_HISTORY];
static canview_frame_t canview_raw_haldex[CANVIEW_RAW_HISTORY];
static canview_frame_t canview_chassis_cache_tx[CANVIEW_CHASSIS_CACHE_SIZE];
static canview_frame_t canview_haldex_cache_tx[CANVIEW_HALDEX_CACHE_SIZE];
static canview_frame_t canview_raw_chassis_tx[CANVIEW_RAW_HISTORY];
static canview_frame_t canview_raw_haldex_tx[CANVIEW_RAW_HISTORY];
static uint8_t canview_raw_chassis_idx = 0;
static uint8_t canview_raw_haldex_idx = 0;
static uint8_t canview_raw_chassis_tx_idx = 0;
static uint8_t canview_raw_haldex_tx_idx = 0;

struct canview_dump_entry_t {
  uint32_t ts;
  uint32_t id;
  uint8_t dlc;
  uint8_t extd;
  uint8_t rtr;
  uint8_t bus;
  uint8_t dir;
  uint8_t generated;
  uint8_t data[8];
};

static canview_dump_entry_t canview_dump_ring[CANVIEW_DUMP_HISTORY];
static uint16_t canview_dump_idx = 0;
static String canview_escape_json(const String& input) {
  String out;
  out.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    if (c == '\"' || c == '\\') {
      out += '\\';
      out += c;
    } else if ((uint8_t)c < 0x20) {
      out += ' ';
    } else {
      out += c;
    }
  }
  return out;
}

static uint32_t canview_make_key(const twai_message_t& msg) {
  uint32_t id = msg.identifier & 0x1FFFFFFF;
  return (msg.extd ? 0x80000000u : 0) | id;
}

static void canview_copy_frame(canview_frame_t& dst, const twai_message_t& msg, bool generated) {
  dst.key = canview_make_key(msg);
  dst.id = msg.identifier;
  dst.ts = millis();
  dst.dlc = msg.data_length_code;
  dst.extd = msg.extd;
  dst.rtr = msg.rtr;
  dst.generated = generated;
  for (uint8_t i = 0; i < dst.dlc && i < 8; i++) {
    dst.data[i] = msg.data[i];
  }
}

static void canview_update_cache(canview_frame_t* cache, uint8_t cache_size, const twai_message_t& msg,
                                 bool generated) {
  uint32_t key = canview_make_key(msg);
  int oldest_index = 0;
  uint32_t oldest_ts = cache[0].ts;
  for (uint8_t i = 0; i < cache_size; i++) {
    if (cache[i].key == key) {
      canview_copy_frame(cache[i], msg, generated);
      return;
    }
    if (i == 0 || cache[i].ts < oldest_ts) {
      oldest_ts = cache[i].ts;
      oldest_index = i;
    }
  }
  canview_copy_frame(cache[oldest_index], msg, generated);
}

static void canview_push_raw(canview_frame_t* raw, uint8_t& idx, const twai_message_t& msg, bool generated) {
  canview_copy_frame(raw[idx], msg, generated);
  idx = (uint8_t)((idx + 1) % CANVIEW_RAW_HISTORY);
}

static bool canview_bus_filter_match(uint8_t bus, const String& bus_filter) {
  if (bus_filter.length() == 0 || bus_filter == "all")
    return true;
  if (bus_filter == "chassis")
    return bus == 0;
  if (bus_filter == "haldex")
    return bus != 0;
  return true;
}

static void canview_push_dump(const twai_message_t& msg, uint8_t bus, uint8_t dir, bool generated) {
  canview_dump_entry_t& e = canview_dump_ring[canview_dump_idx];
  e.ts = millis();
  e.id = msg.identifier;
  e.dlc = msg.data_length_code;
  e.extd = msg.extd;
  e.rtr = msg.rtr;
  e.bus = bus;
  e.dir = dir;
  e.generated = generated ? 1 : 0;
  for (uint8_t i = 0; i < 8; i++) {
    e.data[i] = (i < msg.data_length_code) ? msg.data[i] : 0;
  }
  canview_dump_idx = (uint16_t)((canview_dump_idx + 1) % CANVIEW_DUMP_HISTORY);

  filelogLogCanFrame(msg, bus, dir, generated);
}
void canviewCacheFrame(const twai_message_t& msg, uint8_t bus) {
  if (bus == 0) {
    canview_update_cache(canview_chassis_cache, CANVIEW_CHASSIS_CACHE_SIZE, msg, false);
    canview_push_raw(canview_raw_chassis, canview_raw_chassis_idx, msg, false);
    canview_push_dump(msg, 0, 0, false);
  } else {
    canview_update_cache(canview_haldex_cache, CANVIEW_HALDEX_CACHE_SIZE, msg, false);
    canview_push_raw(canview_raw_haldex, canview_raw_haldex_idx, msg, false);
    canview_push_dump(msg, 1, 0, false);
  }
}

void canviewCacheFrameTx(const twai_message_t& msg, uint8_t bus, bool generated) {
  if (bus == 0) {
    canview_update_cache(canview_chassis_cache_tx, CANVIEW_CHASSIS_CACHE_SIZE, msg, generated);
    canview_push_raw(canview_raw_chassis_tx, canview_raw_chassis_tx_idx, msg, generated);
    canview_push_dump(msg, 0, 1, generated);
  } else {
    canview_update_cache(canview_haldex_cache_tx, CANVIEW_HALDEX_CACHE_SIZE, msg, generated);
    canview_push_raw(canview_raw_haldex_tx, canview_raw_haldex_tx_idx, msg, generated);
    canview_push_dump(msg, 1, 1, generated);
  }
}

static bool canview_find_frame(uint32_t id, const canview_frame_t* cache, uint8_t cache_size, canview_frame_t& out) {
  uint32_t key = id & 0x1FFFFFFF;
  for (uint8_t i = 0; i < cache_size; i++) {
    if ((cache[i].key & 0x1FFFFFFF) == key) {
      out = cache[i];
      return true;
    }
  }
  return false;
}

static const dbc_signal_t* canview_active_chassis_signals() {
  return (haldexGeneration == 5) ? k_vw_mqb_chassis_signals : k_vw_pq_chassis_signals;
}

static uint16_t canview_active_chassis_signal_count() {
  return (haldexGeneration == 5) ? k_vw_mqb_chassis_signal_count : k_vw_pq_chassis_signal_count;
}

bool canviewGetLastTxFrame(uint8_t bus, uint32_t id, canview_last_tx_t& out) {
  out.found = false;
  out.generated = false;
  out.id = id;
  out.dlc = 0;
  out.ageMs = 0;
  for (uint8_t i = 0; i < 8; i++) {
    out.data[i] = 0;
  }

  const canview_frame_t* cache = (bus == 0) ? canview_chassis_cache_tx : canview_haldex_cache_tx;
  uint8_t cache_size = (bus == 0) ? CANVIEW_CHASSIS_CACHE_SIZE : CANVIEW_HALDEX_CACHE_SIZE;
  canview_frame_t frame;
  if (!canview_find_frame(id, cache, cache_size, frame)) {
    return false;
  }

  out.found = true;
  out.generated = frame.generated;
  out.id = frame.id;
  out.dlc = frame.dlc;
  out.ageMs = millis() - frame.ts;
  for (uint8_t i = 0; i < frame.dlc && i < 8; i++) {
    out.data[i] = frame.data[i];
  }
  return true;
}
static const dbc_signal_t* canview_find_mux_signal(uint32_t id) {
  const dbc_signal_t* active_signals = canview_active_chassis_signals();
  const uint16_t active_signal_count = canview_active_chassis_signal_count();
  for (uint16_t i = 0; i < active_signal_count; i++) {
    const dbc_signal_t* sig = &active_signals[i];
    if (sig->id == id && sig->mux == -2) {
      return sig;
    }
  }
  return nullptr;
}

static String canview_haldex_state_label(uint8_t v) {
  String out = String(v);
  String labels;
  if (v & (1 << 0))
    labels += "Clutch 1 report, ";
  if (v & (1 << 1))
    labels += "Temp protection, ";
  if (v & (1 << 2))
    labels += "Clutch 2 report, ";
  if (v & (1 << 3))
    labels += "Coupling open, ";
  if (v & (1 << 6))
    labels += "Speed limit, ";
  if (labels.length() == 0) {
    labels = "None";
  } else {
    labels = labels.substring(0, labels.length() - 2);
  }
  out += " (" + labels + ")";
  return out;
}

static int canview_get_mux_value(uint32_t id, const canview_frame_t& frame, bool& ok) {
  ok = false;
  const dbc_signal_t* mux_sig = canview_find_mux_signal(id);
  if (!mux_sig) {
    return 0;
  }
  uint64_t raw = dbc_extract_raw(frame.data, mux_sig->start_bit, mux_sig->length, mux_sig->is_little_endian);
  ok = true;
  return (int)raw;
}

static String canview_normalize_signal_token(const char* value) {
  String token = String(value ? value : "");
  token.replace("_", " ");
  token.trim();
  token.toLowerCase();
  return token;
}

static String canview_normalize_unit_token(const char* value) {
  String token = String(value ? value : "");
  token.trim();
  token.toLowerCase();
  return token;
}

static bool canview_split_mapping_key(const String& raw, String& bus, String& frame, String& signal, String& unit) {
  int first = raw.indexOf('|');
  if (first < 0) {
    return false;
  }
  int second = raw.indexOf('|', first + 1);
  if (second < 0) {
    return false;
  }
  int third = raw.indexOf('|', second + 1);
  if (third < 0) {
    return false;
  }

  bus = raw.substring(0, first);
  frame = raw.substring(first + 1, second);
  signal = raw.substring(second + 1, third);
  unit = raw.substring(third + 1);
  bus.trim();
  bus.toLowerCase();
  frame.trim();
  frame.toLowerCase();
  signal.trim();
  signal.toLowerCase();
  unit.trim();
  unit.toLowerCase();
  return bus.length() > 0 && frame.length() > 0 && signal.length() > 0;
}

static bool canview_parse_frame_id(const String& token, uint32_t& out_id) {
  String value = token;
  value.trim();
  if (!value.length()) {
    return false;
  }

  char* end = nullptr;
  unsigned long parsed = strtoul(value.c_str(), &end, 0);
  if (!end || *end != '\0') {
    return false;
  }
  out_id = (uint32_t)parsed;
  return true;
}

static const dbc_signal_t* canview_find_signal_definition(uint32_t id, const String& signal_name,
                                                          const String& signal_unit) {
  const dbc_signal_t* name_match = nullptr;
  const dbc_signal_t* active_signals = canview_active_chassis_signals();
  const uint16_t active_signal_count = canview_active_chassis_signal_count();
  for (uint16_t i = 0; i < active_signal_count; i++) {
    const dbc_signal_t* sig = &active_signals[i];
    if (sig->id != id) {
      continue;
    }
    if (canview_normalize_signal_token(sig->name) != signal_name) {
      continue;
    }
    if (canview_normalize_unit_token(sig->unit) == signal_unit) {
      return sig;
    }
    if (!name_match) {
      name_match = sig;
    }
  }
  return name_match;
}

static bool canview_frame_matches_signal(const dbc_signal_t* signal, const canview_frame_t& frame) {
  if (!signal) {
    return false;
  }
  if (signal->mux < 0) {
    return true;
  }

  bool mux_ok = false;
  const int mux_val = canview_get_mux_value(signal->id, frame, mux_ok);
  return mux_ok && mux_val == signal->mux;
}

static bool canview_find_latest_frame_for_id(const canview_frame_t* cache, uint8_t cache_size, uint32_t id,
                                             uint32_t now, canview_frame_t& out) {
  bool found = false;
  uint32_t newest_ts = 0;
  const uint32_t key = id & 0x1FFFFFFF;
  for (uint8_t i = 0; i < cache_size; i++) {
    if ((cache[i].key & 0x1FFFFFFF) != key) {
      continue;
    }
    if (!cache[i].ts || (now - cache[i].ts) > CANVIEW_STALE_MS) {
      continue;
    }
    if (!found || cache[i].ts > newest_ts) {
      out = cache[i];
      newest_ts = cache[i].ts;
      found = true;
    }
  }
  return found;
}

static bool canview_select_latest_frame(const String& bus, uint32_t id, canview_frame_t& out, String& out_dir) {
  const uint32_t now = millis();
  canview_frame_t rx_frame = {};
  canview_frame_t tx_frame = {};
  bool has_rx = false;
  bool has_tx = false;

  if (bus == "chassis") {
    has_rx = canview_find_latest_frame_for_id(canview_chassis_cache, CANVIEW_CHASSIS_CACHE_SIZE, id, now, rx_frame);
    has_tx = canview_find_latest_frame_for_id(canview_chassis_cache_tx, CANVIEW_CHASSIS_CACHE_SIZE, id, now, tx_frame);
  } else if (bus == "haldex") {
    has_rx = canview_find_latest_frame_for_id(canview_haldex_cache, CANVIEW_HALDEX_CACHE_SIZE, id, now, rx_frame);
    has_tx = canview_find_latest_frame_for_id(canview_haldex_cache_tx, CANVIEW_HALDEX_CACHE_SIZE, id, now, tx_frame);
  } else {
    return false;
  }

  if (!has_rx && !has_tx) {
    return false;
  }

  if (has_tx && (!has_rx || tx_frame.ts >= rx_frame.ts)) {
    out = tx_frame;
    out_dir = "TX";
    return true;
  }

  out = rx_frame;
  out_dir = "RX";
  return true;
}

bool canviewResolveMappedSignal(const String& key, canview_resolved_signal_t& out) {
  out.found = false;
  out.mapped = false;
  out.numeric = false;
  out.generated = false;
  out.id = 0;
  out.ageMs = 0;
  out.bus = "";
  out.dir = "";
  out.name = "";
  out.unit = "";
  out.numericValue = 0.0f;
  out.textValue = "";

  String normalized = key;
  normalized.trim();
  normalized.toLowerCase();
  out.mapped = normalized.length() > 0;
  if (!out.mapped) {
    return false;
  }

  String bus;
  String frame_token;
  String signal_name;
  String signal_unit;
  if (!canview_split_mapping_key(normalized, bus, frame_token, signal_name, signal_unit)) {
    return false;
  }

  uint32_t frame_id = 0;
  if (!canview_parse_frame_id(frame_token, frame_id)) {
    return false;
  }

  canview_frame_t frame = {};
  String dir;
  if (!canview_select_latest_frame(bus, frame_id, frame, dir)) {
    return false;
  }

  out.bus = bus;
  out.dir = dir;
  out.generated = frame.generated;
  out.id = frame.id;
  out.ageMs = millis() - frame.ts;

  if (signal_name == "haldex state") {
    uint8_t state = (haldexGeneration == 5) ? frame.data[3] : frame.data[0];
    out.found = true;
    out.name = "Haldex state";
    out.textValue = canview_haldex_state_label(state);
    return true;
  }

  if (signal_name == "engagement") {
    uint16_t raw = (haldexGeneration == 5) ? frame.data[2] : frame.data[1];
    if (haldexGeneration == 2) {
      raw = (uint16_t)(frame.data[1] + frame.data[4]);
    }
    out.found = true;
    out.numeric = true;
    out.name = "Engagement";
    out.numericValue = raw;
    return true;
  }

  const dbc_signal_t* sig = canview_find_signal_definition(frame_id, signal_name, signal_unit);
  if (!sig || !canview_frame_matches_signal(sig, frame)) {
    return false;
  }

  float value = dbc_decode_signal(sig, frame.data);
  if (!isfinite(value)) {
    return false;
  }

  out.found = true;
  out.numeric = true;
  out.name = String(sig->name ? sig->name : "");
  out.unit = String(sig->unit ? sig->unit : "");
  out.numericValue = value;
  return true;
}

String canviewBuildJson(uint16_t decoded_limit, uint8_t raw_limit, const String& bus_filter) {
  String json;
  json.reserve(12000);
  json += "{";

  json += "\"decoded\":[";
  uint16_t decoded_count = 0;
  uint32_t now = millis();

  String busFilter = bus_filter;
  busFilter.toLowerCase();
  bool want_chassis = (busFilter.length() == 0 || busFilter == "all" || busFilter == "chassis");
  bool want_haldex = (busFilter.length() == 0 || busFilter == "all" || busFilter == "haldex");
  const dbc_signal_t* active_signals = canview_active_chassis_signals();
  const uint16_t active_signal_count = canview_active_chassis_signal_count();

  auto append_chassis = [&](const canview_frame_t* cache, uint8_t cache_size, const char* bus, const char* dir) {
    for (uint16_t i = 0; i < active_signal_count && decoded_count < decoded_limit; i++) {
      const dbc_signal_t* sig = &active_signals[i];
      canview_frame_t frame;
      if (!canview_find_frame(sig->id, cache, cache_size, frame)) {
        continue;
      }
      if ((now - frame.ts) > CANVIEW_STALE_MS) {
        continue;
      }
      if (sig->mux >= 0) {
        bool mux_ok = false;
        int mux_val = canview_get_mux_value(sig->id, frame, mux_ok);
        if (!mux_ok || mux_val != sig->mux) {
          continue;
        }
      }

      float value = dbc_decode_signal(sig, frame.data);
      if (!isfinite(value))
        value = 0.0f;
      if (decoded_count > 0) {
        json += ",";
      }
      String busStr = canview_escape_json(String(bus));
      String dirStr = canview_escape_json(String(dir));
      String nameStr = canview_escape_json(String(sig->name));
      String unitStr = canview_escape_json(String(sig->unit));
      json += "{";
      json += "\"bus\":\"" + busStr + "\"";
      json += ",\"dir\":\"" + dirStr + "\"";
      json += ",\"id\":" + String(sig->id);
      json += ",\"name\":\"" + nameStr + "\"";
      json += ",\"value\":" + String(value, 3);
      json += ",\"unit\":\"" + unitStr + "\"";
      json += ",\"ts\":" + String(frame.ts);
      json += ",\"generated\":" + String(frame.generated ? "true" : "false");
      json += "}";
      decoded_count++;
    }
  };

  auto append_haldex_known = [&](const canview_frame_t* cache, uint8_t cache_size, const char* bus, const char* dir) {
    const uint32_t haldex_status_id = (haldexGeneration == 5) ? HALDEX_ID_GEN5 : 0x704;
    canview_frame_t hframe;
    if (!canview_find_frame(haldex_status_id, cache, cache_size, hframe)) {
      return;
    }
    if ((now - hframe.ts) > CANVIEW_STALE_MS) {
      return;
    }
    uint8_t state = (haldexGeneration == 5) ? hframe.data[3] : hframe.data[0];
    uint16_t raw = (haldexGeneration == 5) ? hframe.data[2] : hframe.data[1];
    if (haldexGeneration == 2) {
      raw = (uint16_t)(hframe.data[1] + hframe.data[4]);
    }

    String busStr = canview_escape_json(String(bus));
    String dirStr = canview_escape_json(String(dir));
    if (decoded_count > 0)
      json += ",";
    json += "{";
    json += "\"bus\":\"" + busStr + "\"";
    json += ",\"dir\":\"" + dirStr + "\"";
    json += ",\"id\":" + String(hframe.id);
    json += ",\"name\":\"Haldex state\"";
    json += ",\"value\":\"" + canview_escape_json(canview_haldex_state_label(state)) + "\"";
    json += ",\"unit\":\"\"";
    json += ",\"ts\":" + String(hframe.ts);
    json += ",\"generated\":" + String(hframe.generated ? "true" : "false");
    json += "}";
    decoded_count++;

    if (decoded_count < decoded_limit) {
      json += ",";
      json += "{";
      json += "\"bus\":\"" + busStr + "\"";
      json += ",\"dir\":\"" + dirStr + "\"";
      json += ",\"id\":" + String(hframe.id);
      json += ",\"name\":\"Engagement\"";
      json += ",\"value\":" + String(raw);
      json += ",\"unit\":\"\"";
      json += ",\"ts\":" + String(hframe.ts);
      json += ",\"generated\":" + String(hframe.generated ? "true" : "false");
      json += "}";
      decoded_count++;
    }
  };

  if (want_haldex) {
    append_haldex_known(canview_haldex_cache, CANVIEW_HALDEX_CACHE_SIZE, "haldex", "RX");
    append_haldex_known(canview_haldex_cache_tx, CANVIEW_HALDEX_CACHE_SIZE, "haldex", "TX");
    append_chassis(canview_haldex_cache, CANVIEW_HALDEX_CACHE_SIZE, "haldex", "RX");
    append_chassis(canview_haldex_cache_tx, CANVIEW_HALDEX_CACHE_SIZE, "haldex", "TX");
  }

  if (want_chassis) {
    append_chassis(canview_chassis_cache, CANVIEW_CHASSIS_CACHE_SIZE, "chassis", "RX");
    append_chassis(canview_chassis_cache_tx, CANVIEW_CHASSIS_CACHE_SIZE, "chassis", "TX");
  }

  json += "]";

  json += ",\"raw\":[";
  struct raw_item {
    const canview_frame_t* f;
    const char* bus;
    const char* dir;
  };
  raw_item items[CANVIEW_RAW_HISTORY * 4];
  uint16_t item_count = 0;

  auto add_history = [&](const canview_frame_t* raw, const char* bus, const char* dir) {
    for (uint8_t i = 0; i < CANVIEW_RAW_HISTORY; i++) {
      if (raw[i].ts == 0)
        continue;
      if (item_count >= (CANVIEW_RAW_HISTORY * 4)) {
        return;
      }
      items[item_count++] = {&raw[i], bus, dir};
    }
  };

  if (want_chassis) {
    add_history(canview_raw_chassis, "chassis", "RX");
    add_history(canview_raw_chassis_tx, "chassis", "TX");
  }

  if (want_haldex) {
    add_history(canview_raw_haldex, "haldex", "RX");
    add_history(canview_raw_haldex_tx, "haldex", "TX");
  }

  for (uint16_t i = 0; i + 1 < item_count; i++) {
    uint16_t max_i = i;
    for (uint16_t j = i + 1; j < item_count; j++) {
      if (items[j].f->ts > items[max_i].f->ts)
        max_i = j;
    }
    if (max_i != i) {
      raw_item tmp = items[i];
      items[i] = items[max_i];
      items[max_i] = tmp;
    }
  }

  uint8_t added = 0;
  for (uint16_t i = 0; i < item_count && added < raw_limit; i++) {
    const canview_frame_t& f = *items[i].f;
    if (added > 0)
      json += ",";
    json += "{";
    json += "\"bus\":\"" + String(items[i].bus) + "\"";
    json += ",\"dir\":\"" + String(items[i].dir) + "\"";
    json += ",\"id\":" + String(f.id);
    json += ",\"dlc\":" + String(f.dlc);
    json += ",\"data\":\"";
    for (uint8_t b = 0; b < f.dlc && b < 8; b++) {
      if (b > 0)
        json += " ";
      uint8_t v = f.data[b];
      if (v < 16)
        json += "0";
      json += String(v, HEX);
    }
    json += "\"";
    json += ",\"ts\":" + String(f.ts);
    json += ",\"generated\":" + String(f.generated ? "true" : "false");
    json += "}";
    added++;
  }

  json += "]";
  json += "}";

  return json;
}

String canviewBuildDumpText(uint32_t window_ms, const String& bus_filter) {
  String filter = bus_filter;
  filter.toLowerCase();

  uint32_t now = millis();
  String out;
  out.reserve(24000);
  out += "OpenHaldex CAN dump\n";
  out += "window_ms=" + String(window_ms) + " bus=" + (filter.length() ? filter : String("all")) + "\n";
  out += "ts_ms\tbus\tdir\tgen\tid\tdlc\tdata\n";

  for (uint16_t i = 0; i < CANVIEW_DUMP_HISTORY; i++) {
    uint16_t idx = (uint16_t)((canview_dump_idx + i) % CANVIEW_DUMP_HISTORY);
    const canview_dump_entry_t& e = canview_dump_ring[idx];

    if (e.ts == 0)
      continue;

    uint32_t age = now - e.ts;
    if (age > window_ms)
      continue;

    if (!canview_bus_filter_match(e.bus, filter))
      continue;

    out += String(e.ts);
    out += "\t";
    out += (e.bus == 0) ? "chassis" : "haldex";
    out += "\t";
    out += (e.dir == 0) ? "RX" : "TX";
    out += "\t";
    out += (e.generated != 0) ? "GEN" : "-";
    out += "\t0x";
    out += String(e.id, HEX);
    out += "\t";
    out += String(e.dlc);
    out += "\t";

    for (uint8_t b = 0; b < e.dlc && b < 8; b++) {
      if (b > 0)
        out += " ";
      if (e.data[b] < 16)
        out += "0";
      out += String(e.data[b], HEX);
    }

    out += "\n";
  }

  return out;
}
