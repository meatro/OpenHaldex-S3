#include "functions/storage/storage.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>

#include "functions/config/config.h"
#include "functions/core/state.h"
#include "functions/config/pins.h"

static Preferences pref;
static bool fs_ready = false;
static volatile bool storage_dirty = false;

static const char* MAP_DIR = "/maps";
static const char* MAP_FILE = "/maps/current.json";
static const char* CURRENT_MAP_KEY = "currentMap";

static const char* WIFI_SSID_KEY = "wifiSsid";
static const char* WIFI_PASS_KEY = "wifiPass";
static const char* WIFI_STA_ENABLE_KEY = "wifiStaEnable";
static const char* WIFI_AP_PASS_KEY = "wifiApPass";
static const char* LOG_FILE_ENABLE_KEY = "logFileOn";
static const char* LOG_CAN_ENABLE_KEY = "logCanOn";
static const char* LOG_ERROR_ENABLE_KEY = "logErrorOn";
static const char* LOG_SERIAL_ENABLE_KEY = "logSerial";
static const char* LOG_DEBUG_FIRMWARE_ENABLE_KEY = "logDbgFw";
static const char* LOG_DEBUG_NETWORK_ENABLE_KEY = "logDbgNet";
static const char* LOG_DEBUG_CAN_ENABLE_KEY = "logDbgCan";
static const char* SPEED_CURVE_COUNT_KEY = "spCurveCnt";
static const char* THROTTLE_CURVE_COUNT_KEY = "thCurveCnt";
static const char* RPM_CURVE_COUNT_KEY = "rpmCurveCnt";
static const char* INPUT_MAP_SPEED_KEY = "inMapSpeed";
static const char* INPUT_MAP_THROTTLE_KEY = "inMapThr";
static const char* INPUT_MAP_RPM_KEY = "inMapRpm";
static const char* DISENGAGE_MAP_SPEED_KEY = "disMapSpd";
static const char* DISENGAGE_SPEED_MODE_SPEED_KEY = "disSpdSpd";
static const char* DISENGAGE_THROTTLE_MODE_SPEED_KEY = "disThrSpd";
static const char* DISENGAGE_RPM_MODE_SPEED_KEY = "disRpmSpd";
static const char* LOCK_RELEASE_RATE_KEY = "relRate";
static const char* MODE_SCHEMA_KEY = "modeSchema";
static const uint8_t MODE_SCHEMA_UNKNOWN = 0;
static const uint8_t MODE_SCHEMA_LEGACY = 1;
static const uint8_t MODE_SCHEMA_VERSION = 2;

static uint8_t normalize_stored_mode(uint8_t stored_mode, uint8_t schema_version) {
  if (schema_version < MODE_SCHEMA_VERSION) {
    switch (stored_mode) {
    case 0:
      return MODE_STOCK;
    case 1:
      return MODE_FWD;
    case 2:
      return MODE_5050;
    case 3:
      return MODE_6040;
    case 4:
      return MODE_7030; // legacy MODE_7525
    case 5:
      return MODE_SPEED; // legacy MODE_CUSTOM
    case 6:
      return MODE_MAP;
    default:
      return MODE_MAP;
    }
  }
  return stored_mode;
}

static void reset_map_defaults() {
  static const uint16_t map_speed_bins_default[MAP_SPEED_BINS] = {0, 5, 10, 20, 40, 60, 80, 100, 140};
  static const uint8_t map_throttle_bins_default[MAP_THROTTLE_BINS] = {0, 5, 10, 20, 40, 60, 80};
  static const uint8_t map_lock_default[MAP_THROTTLE_BINS][MAP_SPEED_BINS] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0},         {0, 0, 0, 0, 0, 0, 0, 0, 0},        {0, 0, 5, 5, 5, 5, 0, 0, 0},
    {0, 5, 10, 15, 15, 10, 5, 0, 0},     {5, 10, 20, 25, 25, 20, 15, 10, 5}, {10, 20, 30, 40, 40, 30, 25, 20, 15},
    {20, 30, 45, 60, 60, 50, 40, 30, 20}};

  for (uint8_t i = 0; i < MAP_SPEED_BINS; i++) {
    map_speed_bins[i] = map_speed_bins_default[i];
  }
  for (uint8_t i = 0; i < MAP_THROTTLE_BINS; i++) {
    map_throttle_bins[i] = map_throttle_bins_default[i];
    for (uint8_t j = 0; j < MAP_SPEED_BINS; j++) {
      map_lock_table[i][j] = map_lock_default[i][j];
    }
  }
}

static int parse_token_int(String token) {
  // Parse integer from CSV token, handling 'S' and 'T' prefixes for Speed/Throttle bins
  token.trim();
  if (token.length() == 0)
    return 0;
  if (token[0] == 'S' || token[0] == 'T') {
    token = token.substring(1);
  }
  if (token.endsWith("%")) {
    token = token.substring(0, token.length() - 1);
  }
  return token.toInt();
}

static int split_tabs(const String& line, String parts[], int max_parts) {
  int count = 0;
  int start = 0;
  int len = line.length();
  while (start <= len && count < max_parts) {
    int idx = line.indexOf('\t', start);
    if (idx < 0)
      idx = len;
    parts[count++] = line.substring(start, idx);
    start = idx + 1;
  }
  return count;
}

static bool parse_map_txt(const String& text) {
  // Parse tab-delimited map format: first row is speed bins, subsequent rows are throttle bins + lock percentages
  String parts[32];
  String line;
  int line_start = 0;
  int line_end = text.indexOf('\n', line_start);
  if (line_end < 0)
    return false;

  line = text.substring(line_start, line_end);
  line.replace("\r", "");
  int header_count = split_tabs(line, parts, 32);
  if (header_count < (2 + MAP_SPEED_BINS))
    return false;

  for (uint8_t i = 0; i < MAP_SPEED_BINS; i++) {
    map_speed_bins[i] = (uint16_t)parse_token_int(parts[i + 2]);
  }

  uint8_t row = 0;
  line_start = line_end + 1;
  while (line_start < text.length() && row < MAP_THROTTLE_BINS) {
    line_end = text.indexOf('\n', line_start);
    if (line_end < 0)
      line_end = text.length();
    line = text.substring(line_start, line_end);
    line.replace("\r", "");
    line.trim();
    line_start = line_end + 1;

    if (line.length() == 0)
      continue;
    int count = split_tabs(line, parts, 32);
    if (count < (2 + MAP_SPEED_BINS))
      return false;

    int throttle = parse_token_int(parts[0]);
    String throttle_id = parts[0];
    throttle_id.trim();
    if (throttle == 0 && throttle_id.length() == 0) {
      throttle = parse_token_int(parts[1]);
    }
    map_throttle_bins[row] = (uint8_t)throttle;

    for (uint8_t s = 0; s < MAP_SPEED_BINS; s++) {
      int v = parse_token_int(parts[s + 2]);
      if (v < 0)
        v = 0;
      if (v > 100)
        v = 100;
      map_lock_table[row][s] = (uint8_t)v;
    }

    row++;
  }

  return (row == MAP_THROTTLE_BINS);
}

static bool load_map_from_json_file(const char* path) {
  if (!fs_ready)
    return false;
  if (!LittleFS.exists(path))
    return false;

  File f = LittleFS.open(path, "r");
  if (!f)
    return false;

  String body = f.readString();
  f.close();

  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    return false;
  }

  JsonArray speedBins = doc["speedBins"].as<JsonArray>();
  JsonArray throttleBins = doc["throttleBins"].as<JsonArray>();
  JsonArray lockTable = doc["lockTable"].as<JsonArray>();

  if (speedBins.size() != MAP_SPEED_BINS || throttleBins.size() != MAP_THROTTLE_BINS ||
      lockTable.size() != MAP_THROTTLE_BINS) {
    return false;
  }

  for (uint8_t i = 0; i < MAP_SPEED_BINS; i++) {
    map_speed_bins[i] = (uint16_t)(speedBins[i] | 0);
  }

  for (uint8_t i = 0; i < MAP_THROTTLE_BINS; i++) {
    map_throttle_bins[i] = (uint8_t)(throttleBins[i] | 0);
  }

  for (uint8_t t = 0; t < MAP_THROTTLE_BINS; t++) {
    JsonArray row = lockTable[t].as<JsonArray>();
    if (row.size() != MAP_SPEED_BINS) {
      return false;
    }
    for (uint8_t s = 0; s < MAP_SPEED_BINS; s++) {
      int v = row[s] | 0;
      if (v < 0)
        v = 0;
      if (v > 100)
        v = 100;
      map_lock_table[t][s] = (uint8_t)v;
    }
  }

  return true;
}

static bool save_map_to_json_file(const char* path) {
  if (!fs_ready)
    return false;
  LittleFS.mkdir(MAP_DIR);

  File f = LittleFS.open(path, "w");
  if (!f)
    return false;

  JsonDocument doc;
  JsonArray speedBins = doc["speedBins"].to<JsonArray>();
  for (uint8_t i = 0; i < MAP_SPEED_BINS; i++) {
    speedBins.add(map_speed_bins[i]);
  }

  JsonArray throttleBins = doc["throttleBins"].to<JsonArray>();
  for (uint8_t i = 0; i < MAP_THROTTLE_BINS; i++) {
    throttleBins.add(map_throttle_bins[i]);
  }

  JsonArray lockTable = doc["lockTable"].to<JsonArray>();
  for (uint8_t t = 0; t < MAP_THROTTLE_BINS; t++) {
    JsonArray row = lockTable.add<JsonArray>();
    for (uint8_t s = 0; s < MAP_SPEED_BINS; s++) {
      row.add(map_lock_table[t][s]);
    }
  }

  serializeJson(doc, f);
  f.close();
  return true;
}

static bool load_map_from_txt_file(const char* path) {
  if (!fs_ready)
    return false;
  if (!LittleFS.exists(path))
    return false;

  File f = LittleFS.open(path, "r");
  if (!f)
    return false;

  String body = f.readString();
  f.close();

  return parse_map_txt(body);
}

static String sanitize_map_name(const String& name) {
  String out;
  out.reserve(name.length());
  for (size_t i = 0; i < name.length(); i++) {
    char c = name[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
      out += c;
    } else if (c == ' ' || c == '_' || c == '-' || c == '.') {
      out += c;
    } else {
      out += '_';
    }
  }
  out.trim();
  return out;
}

static bool map_entry_exists(JsonArray out, const String& path) {
  for (JsonVariant v : out) {
    JsonObject obj = v.as<JsonObject>();
    if (!obj.isNull()) {
      const char* p = obj["path"] | "";
      if (path == String(p)) {
        return true;
      }
    }
  }
  return false;
}

static void add_map_entry(JsonArray out, const String& name, const String& path, const char* format, bool readOnly) {
  if (map_entry_exists(out, path)) {
    return;
  }
  JsonObject obj = out.add<JsonObject>();
  obj["name"] = name;
  obj["path"] = path;
  obj["format"] = format;
  obj["readOnly"] = readOnly;
}

static bool load_map_from_fs() {
  return load_map_from_json_file(MAP_FILE);
}

static void save_map_to_fs() {
  save_map_to_json_file(MAP_FILE);

  String currentPath = storageGetCurrentMapPath();
  if (currentPath.length() > 0 && currentPath != MAP_FILE && currentPath.startsWith("/maps/") &&
      currentPath.endsWith(".json")) {
    save_map_to_json_file(currentPath.c_str());
  }
}
void storageInit() {
  // Initialize NVS (Preferences) and LittleFS filesystem
  // LittleFS is used for map storage and UI files
  pref.begin("openhaldex", false);

  LOG_INFO("storage", "Attempting to mount LittleFS");
  fs_ready = LittleFS.begin(false, "/littlefs", 10, "littlefs");
  if (!fs_ready) {
    LOG_WARN("storage", "LittleFS mount failed, formatting");
    fs_ready = LittleFS.begin(true, "/littlefs", 10, "littlefs");
  }

  if (fs_ready) {
    LOG_INFO("storage", "LittleFS mounted successfully");
  } else {
    LOG_ERROR("storage", "LittleFS mount failed completely");
  }
}

void storageLoad() {
  storage_dirty = false;
  LOG_INFO("storage", "Loading persisted settings");
#if detailedDebugEEP
  DEBUG("EEPROM initialising!");
#endif

  if (pref.getUChar("haldexGen", 255) == 255) {
    LOG_WARN("storage", "No persisted settings found; applying defaults");
    String mapped_speed;
    String mapped_throttle;
    String mapped_rpm;
    (void)mappedInputSignalsGet(mapped_speed, mapped_throttle, mapped_rpm, 0);

    pref.putBool("broadcastOpen", broadcastOpenHaldexOverCAN);
    pref.putBool("isStandalone", isStandalone);
    pref.putBool("disableControl", disableController);

    pref.putUChar("haldexGen", haldexGeneration);
    pref.putUChar("lastMode", (uint8_t)state.mode);
    pref.putUChar(MODE_SCHEMA_KEY, MODE_SCHEMA_VERSION);
    pref.putUChar("disableThrottle", disableThrottle);
    pref.putUShort("disableSpeed", disableSpeed);
    pref.putUShort(DISENGAGE_MAP_SPEED_KEY, disengageUnderSpeedMap);
    pref.putUShort(DISENGAGE_SPEED_MODE_SPEED_KEY, disengageUnderSpeedSpeedMode);
    pref.putUShort(DISENGAGE_THROTTLE_MODE_SPEED_KEY, disengageUnderSpeedThrottleMode);
    pref.putUShort(DISENGAGE_RPM_MODE_SPEED_KEY, disengageUnderSpeedRpmMode);
    pref.putFloat(LOCK_RELEASE_RATE_KEY, lockReleaseRatePctPerSec);
    pref.putBool(LOG_FILE_ENABLE_KEY, logToFileEnabled);
    pref.putBool(LOG_CAN_ENABLE_KEY, logCanToFileEnabled);
    pref.putBool(LOG_ERROR_ENABLE_KEY, logErrorToFileEnabled);
    pref.putBool(LOG_SERIAL_ENABLE_KEY, logSerialEnabled);
    pref.putBool(LOG_DEBUG_FIRMWARE_ENABLE_KEY, logDebugFirmwareEnabled);
    pref.putBool(LOG_DEBUG_NETWORK_ENABLE_KEY, logDebugNetworkEnabled);
    pref.putBool(LOG_DEBUG_CAN_ENABLE_KEY, logDebugCanEnabled);

    pref.putUChar(SPEED_CURVE_COUNT_KEY, speed_curve_count);
    pref.putBytes("spCurveBins", (byte*)(&speed_curve_bins), sizeof(speed_curve_bins));
    pref.putBytes("spCurveLock", (byte*)(&speed_curve_lock), sizeof(speed_curve_lock));
    pref.putUChar(THROTTLE_CURVE_COUNT_KEY, throttle_curve_count);
    pref.putBytes("thCurveBins", (byte*)(&throttle_curve_bins), sizeof(throttle_curve_bins));
    pref.putBytes("thCurveLock", (byte*)(&throttle_curve_lock), sizeof(throttle_curve_lock));
    pref.putUChar(RPM_CURVE_COUNT_KEY, rpm_curve_count);
    pref.putBytes("rpmCurveBins", (byte*)(&rpm_curve_bins), sizeof(rpm_curve_bins));
    pref.putBytes("rpmCurveLock", (byte*)(&rpm_curve_lock), sizeof(rpm_curve_lock));
    pref.putString(INPUT_MAP_SPEED_KEY, mapped_speed);
    pref.putString(INPUT_MAP_THROTTLE_KEY, mapped_throttle);
    pref.putString(INPUT_MAP_RPM_KEY, mapped_rpm);

    reset_map_defaults();
    save_map_to_fs();
  } else {
    broadcastOpenHaldexOverCAN = pref.getBool("broadcastOpen", broadcastOpenHaldexOverCAN);
    isStandalone = pref.getBool("isStandalone", isStandalone);
    disableController = pref.getBool("disableControl", disableController);

    haldexGeneration = pref.getUChar("haldexGen", haldexGeneration);
    uint8_t mode_schema = pref.getUChar(MODE_SCHEMA_KEY, MODE_SCHEMA_UNKNOWN);
    if (mode_schema == MODE_SCHEMA_UNKNOWN) {
      bool has_v2_keys = pref.isKey(SPEED_CURVE_COUNT_KEY) || pref.isKey(THROTTLE_CURVE_COUNT_KEY) ||
                         pref.isKey(RPM_CURVE_COUNT_KEY) || pref.isKey(INPUT_MAP_SPEED_KEY) ||
                         pref.isKey(INPUT_MAP_THROTTLE_KEY) || pref.isKey(INPUT_MAP_RPM_KEY);
      mode_schema = has_v2_keys ? MODE_SCHEMA_VERSION : MODE_SCHEMA_LEGACY;
    }
    lastMode = pref.getUChar("lastMode", lastMode);
    lastMode = normalize_stored_mode(lastMode, mode_schema);
    if (mode_schema < MODE_SCHEMA_VERSION) {
      pref.putUChar(MODE_SCHEMA_KEY, MODE_SCHEMA_VERSION);
    }
    disableThrottle = pref.getUChar("disableThrottle", disableThrottle);
    state.pedal_threshold = disableThrottle;
    disableSpeed = pref.getUShort("disableSpeed", disableSpeed);
    disengageUnderSpeedMap = pref.getUShort(DISENGAGE_MAP_SPEED_KEY, disengageUnderSpeedMap);
    disengageUnderSpeedSpeedMode = pref.getUShort(DISENGAGE_SPEED_MODE_SPEED_KEY, disengageUnderSpeedSpeedMode);
    disengageUnderSpeedThrottleMode =
      pref.getUShort(DISENGAGE_THROTTLE_MODE_SPEED_KEY, disengageUnderSpeedThrottleMode);
    disengageUnderSpeedRpmMode = pref.getUShort(DISENGAGE_RPM_MODE_SPEED_KEY, disengageUnderSpeedRpmMode);
    lockReleaseRatePctPerSec = pref.getFloat(LOCK_RELEASE_RATE_KEY, lockReleaseRatePctPerSec);
    if (disengageUnderSpeedMap > 300) {
      disengageUnderSpeedMap = 300;
    }
    if (disengageUnderSpeedSpeedMode > 300) {
      disengageUnderSpeedSpeedMode = 300;
    }
    if (disengageUnderSpeedThrottleMode > 300) {
      disengageUnderSpeedThrottleMode = 300;
    }
    if (disengageUnderSpeedRpmMode > 300) {
      disengageUnderSpeedRpmMode = 300;
    }
    if (lockReleaseRatePctPerSec < 0.0f) {
      lockReleaseRatePctPerSec = 0.0f;
    } else if (lockReleaseRatePctPerSec > 1000.0f) {
      lockReleaseRatePctPerSec = 1000.0f;
    }
    logToFileEnabled = pref.getBool(LOG_FILE_ENABLE_KEY, logToFileEnabled);
    logCanToFileEnabled = pref.getBool(LOG_CAN_ENABLE_KEY, logCanToFileEnabled);
    logErrorToFileEnabled = pref.getBool(LOG_ERROR_ENABLE_KEY, logErrorToFileEnabled);
    logSerialEnabled = pref.getBool(LOG_SERIAL_ENABLE_KEY, logSerialEnabled);
    logDebugFirmwareEnabled = pref.getBool(LOG_DEBUG_FIRMWARE_ENABLE_KEY, logDebugFirmwareEnabled);
    logDebugNetworkEnabled = pref.getBool(LOG_DEBUG_NETWORK_ENABLE_KEY, logDebugNetworkEnabled);
    logDebugCanEnabled = pref.getBool(LOG_DEBUG_CAN_ENABLE_KEY, logDebugCanEnabled);
    const bool debug_profile_enabled = logDebugFirmwareEnabled || logDebugNetworkEnabled || logDebugCanEnabled;
    if ((debug_profile_enabled || logCanToFileEnabled) && !logToFileEnabled) {
      logToFileEnabled = true;
      pref.putBool(LOG_FILE_ENABLE_KEY, true);
      LOG_WARN("storage", "forcing logToFileEnabled=1 while debug/can capture is active");
    }
    if (debug_profile_enabled && !logErrorToFileEnabled) {
      logErrorToFileEnabled = true;
      pref.putBool(LOG_ERROR_ENABLE_KEY, true);
      LOG_WARN("storage", "forcing logErrorToFileEnabled=1 while debug capture is active");
    }

    speed_curve_count = pref.getUChar(SPEED_CURVE_COUNT_KEY, speed_curve_count);
    throttle_curve_count = pref.getUChar(THROTTLE_CURVE_COUNT_KEY, throttle_curve_count);
    rpm_curve_count = pref.getUChar(RPM_CURVE_COUNT_KEY, rpm_curve_count);
    if (speed_curve_count == 0 || speed_curve_count > CURVE_POINTS_MAX) {
      speed_curve_count = 5;
    }
    if (throttle_curve_count == 0 || throttle_curve_count > CURVE_POINTS_MAX) {
      throttle_curve_count = 5;
    }
    if (rpm_curve_count == 0 || rpm_curve_count > CURVE_POINTS_MAX) {
      rpm_curve_count = 6;
    }
    pref.getBytes("spCurveBins", &speed_curve_bins, sizeof(speed_curve_bins));
    pref.getBytes("spCurveLock", &speed_curve_lock, sizeof(speed_curve_lock));
    pref.getBytes("thCurveBins", &throttle_curve_bins, sizeof(throttle_curve_bins));
    pref.getBytes("thCurveLock", &throttle_curve_lock, sizeof(throttle_curve_lock));
    pref.getBytes("rpmCurveBins", &rpm_curve_bins, sizeof(rpm_curve_bins));
    pref.getBytes("rpmCurveLock", &rpm_curve_lock, sizeof(rpm_curve_lock));
    String mapped_speed = pref.getString(INPUT_MAP_SPEED_KEY, "");
    String mapped_throttle = pref.getString(INPUT_MAP_THROTTLE_KEY, "");
    String mapped_rpm = pref.getString(INPUT_MAP_RPM_KEY, "");
    (void)mappedInputSignalsSet(mapped_speed, mapped_throttle, mapped_rpm, 0);

    String currentPath = storageGetCurrentMapPath();
    if (!storageLoadMapPath(currentPath)) {
      reset_map_defaults();
      save_map_to_fs();
      storageSetCurrentMapPath(MAP_FILE);
    }
    if (disableController) {
      state.mode = MODE_STOCK;
    } else {
      uint8_t restored_mode = lastMode;
      if (restored_mode >= openhaldex_mode_t_MAX || restored_mode == MODE_STOCK) {
        restored_mode = MODE_MAP;
      }
      state.mode = (openhaldex_mode_t)restored_mode;
    }
    if (loggingDebugCaptureActive()) {
      disableController = true;
      state.mode = MODE_STOCK;
      LOG_WARN("storage", "Verbose debug profile active at boot; forcing STOCK controller-off");
    }
    LOG_INFO("storage", "Settings loaded mode=%s gen=%d disableControl=%d", get_openhaldex_mode_string(state.mode),
             haldexGeneration, disableController ? 1 : 0);
  }

#if detailedDebugEEP
  DEBUG("EEPROM initialised with...");
  DEBUG("    Broadcast OpenHaldex over CAN: %s", broadcastOpenHaldexOverCAN ? "true" : "false");
  DEBUG("    Standalone mode: %s", isStandalone ? "true" : "false");
  DEBUG("    Haldex Generation: %d", haldexGeneration);
  DEBUG("    Last Mode: %d", lastMode);
  DEBUG("    Disable Below Throttle: %d", disableThrottle);
  DEBUG("    Disable Above Speed: %d", disableSpeed);
#endif
}

void storageSave() {
  storage_dirty = false;
  LOG_INFO("storage", "Persisting settings to NVS/FS");

#if detailedDebugEEP
  DEBUG("Writing EEPROM...");
#endif

  pref.putBool("broadcastOpen", broadcastOpenHaldexOverCAN);
  pref.putBool("isStandalone", isStandalone);
  pref.putBool("disableControl", disableController);

  lastMode = (uint8_t)state.mode;
  pref.putUChar("haldexGen", haldexGeneration);
  pref.putUChar("lastMode", lastMode);
  pref.putUChar(MODE_SCHEMA_KEY, MODE_SCHEMA_VERSION);
  pref.putUChar("disableThrottle", disableThrottle);
  pref.putUShort("disableSpeed", disableSpeed);
  pref.putUShort(DISENGAGE_MAP_SPEED_KEY, disengageUnderSpeedMap);
  pref.putUShort(DISENGAGE_SPEED_MODE_SPEED_KEY, disengageUnderSpeedSpeedMode);
  pref.putUShort(DISENGAGE_THROTTLE_MODE_SPEED_KEY, disengageUnderSpeedThrottleMode);
  pref.putUShort(DISENGAGE_RPM_MODE_SPEED_KEY, disengageUnderSpeedRpmMode);
  pref.putFloat(LOCK_RELEASE_RATE_KEY, lockReleaseRatePctPerSec);
  pref.putBool(LOG_FILE_ENABLE_KEY, logToFileEnabled);
  pref.putBool(LOG_CAN_ENABLE_KEY, logCanToFileEnabled);
  pref.putBool(LOG_ERROR_ENABLE_KEY, logErrorToFileEnabled);
  pref.putBool(LOG_SERIAL_ENABLE_KEY, logSerialEnabled);
  pref.putBool(LOG_DEBUG_FIRMWARE_ENABLE_KEY, logDebugFirmwareEnabled);
  pref.putBool(LOG_DEBUG_NETWORK_ENABLE_KEY, logDebugNetworkEnabled);
  pref.putBool(LOG_DEBUG_CAN_ENABLE_KEY, logDebugCanEnabled);

  pref.putUChar(SPEED_CURVE_COUNT_KEY, speed_curve_count);
  pref.putBytes("spCurveBins", (byte*)(&speed_curve_bins), sizeof(speed_curve_bins));
  pref.putBytes("spCurveLock", (byte*)(&speed_curve_lock), sizeof(speed_curve_lock));
  pref.putUChar(THROTTLE_CURVE_COUNT_KEY, throttle_curve_count);
  pref.putBytes("thCurveBins", (byte*)(&throttle_curve_bins), sizeof(throttle_curve_bins));
  pref.putBytes("thCurveLock", (byte*)(&throttle_curve_lock), sizeof(throttle_curve_lock));
  pref.putUChar(RPM_CURVE_COUNT_KEY, rpm_curve_count);
  pref.putBytes("rpmCurveBins", (byte*)(&rpm_curve_bins), sizeof(rpm_curve_bins));
  pref.putBytes("rpmCurveLock", (byte*)(&rpm_curve_lock), sizeof(rpm_curve_lock));
  String mapped_speed;
  String mapped_throttle;
  String mapped_rpm;
  (void)mappedInputSignalsGet(mapped_speed, mapped_throttle, mapped_rpm, 2);
  pref.putString(INPUT_MAP_SPEED_KEY, mapped_speed);
  pref.putString(INPUT_MAP_THROTTLE_KEY, mapped_throttle);
  pref.putString(INPUT_MAP_RPM_KEY, mapped_rpm);

  save_map_to_fs();

#if detailedDebugEEP
  DEBUG("Written EEPROM with data:");
  DEBUG("    Broadcast OpenHaldex over CAN: %s", broadcastOpenHaldexOverCAN ? "true" : "false");
  DEBUG("    Standalone mode: %s", isStandalone ? "true" : "false");
  DEBUG("    Haldex Generation: %d", haldexGeneration);
  DEBUG("    Last Mode: %d", lastMode);
  DEBUG("    Disable Below Throttle: %d", disableThrottle);
  DEBUG("    Disable Above Speed: %d", disableSpeed);
#endif
}

bool storageFsReady() {
  return fs_ready;
}

void storageMarkDirty() {
  storage_dirty = true;
}

bool storageIsDirty() {
  return storage_dirty;
}

void storageClearDirty() {
  storage_dirty = false;
}

bool storageLoadMapPath(const String& path) {
  if (!fs_ready) {
    LOG_ERROR("storage", "Map load failed: filesystem not ready path=%s", path.c_str());
    return false;
  }
  String local = path;
  if (!local.startsWith("/")) {
    local = "/" + local;
  }
  if (local.indexOf("..") >= 0) {
    LOG_ERROR("storage", "Map load rejected: invalid traversal path=%s", local.c_str());
    return false;
  }

  bool ok = false;
  if (local.endsWith(".json")) {
    ok = load_map_from_json_file(local.c_str());
    if (ok) {
      storageSetCurrentMapPath(local);
      save_map_to_fs();
    } else {
      LOG_ERROR("storage", "Map load failed: json parse/read error path=%s", local.c_str());
    }
  } else if (local.endsWith(".txt")) {
    // TXT maps are read-only presets from /maps. Support legacy root paths transparently.
    String txtPath = local;
    if (!LittleFS.exists(txtPath)) {
      String fallback = String(MAP_DIR) + "/" + txtPath;
      fallback.replace("//", "/");
      if (LittleFS.exists(fallback)) {
        txtPath = fallback;
      } else {
        LOG_ERROR("storage", "Map load failed: txt not found path=%s fallback=%s", local.c_str(), fallback.c_str());
      }
    }
    // Load into RAM and persist to current.json
    // so boot and "save current" continue to work predictably.
    ok = load_map_from_txt_file(txtPath.c_str());
    if (ok) {
      storageSetCurrentMapPath(MAP_FILE);
      save_map_to_fs();
    } else {
      LOG_ERROR("storage", "Map load failed: txt parse/read error path=%s", txtPath.c_str());
    }
  } else {
    LOG_ERROR("storage", "Map load failed: unsupported extension path=%s", local.c_str());
  }

  if (ok) {
    LOG_INFO("storage", "Map loaded path=%s", local.c_str());
  }

  return ok;
}
bool storageSaveMapName(const String& name, String& outPath) {
  if (!fs_ready) {
    LOG_ERROR("storage", "Map save failed: filesystem not ready name=%s", name.c_str());
    return false;
  }
  String safe = sanitize_map_name(name);
  if (safe.length() == 0) {
    LOG_ERROR("storage", "Map save failed: invalid name=%s", name.c_str());
    return false;
  }

  String path = String(MAP_DIR) + "/" + safe;
  if (!path.endsWith(".json")) {
    path += ".json";
  }

  if (!save_map_to_json_file(path.c_str())) {
    LOG_ERROR("storage", "Map save failed: write error path=%s", path.c_str());
    return false;
  }

  // Make the saved map the active writable map.
  storageSetCurrentMapPath(path);
  save_map_to_fs();

  outPath = path;
  LOG_INFO("storage", "Map saved path=%s", path.c_str());
  return true;
}
bool storageDeleteMapPath(const String& path) {
  if (!fs_ready) {
    LOG_ERROR("storage", "Map delete failed: filesystem not ready path=%s", path.c_str());
    return false;
  }
  if (!path.startsWith("/maps/")) {
    LOG_ERROR("storage", "Map delete rejected: path outside /maps path=%s", path.c_str());
    return false;
  }
  if (path == MAP_FILE) {
    LOG_ERROR("storage", "Map delete rejected: cannot delete current map file");
    return false;
  }
  if (!LittleFS.exists(path)) {
    LOG_ERROR("storage", "Map delete failed: file missing path=%s", path.c_str());
    return false;
  }
  const bool ok = LittleFS.remove(path);
  if (!ok) {
    LOG_ERROR("storage", "Map delete failed: remove returned false path=%s", path.c_str());
    return false;
  }
  LOG_INFO("storage", "Map deleted path=%s", path.c_str());
  return true;
}

String storageGetCurrentMapPath() {
  String path = pref.getString(CURRENT_MAP_KEY, MAP_FILE);
  if (path.length() == 0)
    return String(MAP_FILE);
  return path;
}

void storageSetCurrentMapPath(const String& path) {
  if (path.length() == 0)
    return;
  pref.putString(CURRENT_MAP_KEY, path);
}

void storageListMaps(JsonArray out) {
  if (!fs_ready)
    return;

  bool hasMapDirTxtPresets = false;
  File maps = LittleFS.open(MAP_DIR);
  if (maps && maps.isDirectory()) {
    File file = maps.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        String path = file.name();
        if (!path.startsWith("/")) {
          path = String(MAP_DIR) + "/" + path;
        } else if (!path.startsWith(String(MAP_DIR) + "/")) {
          path = String(MAP_DIR) + path;
        }
        if (path.endsWith(".json")) {
          String base = path.substring(path.lastIndexOf('/') + 1);
          String name = base.substring(0, base.length() - 5);
          if (path != MAP_FILE) {
            add_map_entry(out, name, path, "json", false);
          }
        } else if (path.endsWith(".txt")) {
          String base = path.substring(path.lastIndexOf('/') + 1);
          String name = base.substring(0, base.length() - 4);
          add_map_entry(out, name, path, "txt", true);
          hasMapDirTxtPresets = true;
        }
      }
      file = maps.openNextFile();
    }
  }

  if (!hasMapDirTxtPresets) {
    File root = LittleFS.open("/");
    if (root) {
      File file = root.openNextFile();
      while (file) {
        if (!file.isDirectory()) {
          String path = file.name();
          if (!path.startsWith("/")) {
            path = "/" + path;
          }
          if (path.endsWith(".txt")) {
            String base = path.substring(path.lastIndexOf('/') + 1);
            String name = base.substring(0, base.length() - 4);
            add_map_entry(out, name, path, "txt", true);
          }
        }
        file = root.openNextFile();
      }
    }
  }
}

// WiFi credentials management
bool storageGetWifiCreds(String& ssid, String& pass) {
  ssid = pref.getString(WIFI_SSID_KEY, "");
  pass = pref.getString(WIFI_PASS_KEY, "");
  return ssid.length() > 0;
}

void storageSetWifiCreds(const String& ssid, const String& pass) {
  pref.putString(WIFI_SSID_KEY, ssid);
  pref.putString(WIFI_PASS_KEY, pass);
  LOG_INFO("storage", "WiFi credentials updated ssid=%s", ssid.c_str());
}

void storageClearWifiCreds() {
  pref.remove(WIFI_SSID_KEY);
  pref.remove(WIFI_PASS_KEY);
  LOG_WARN("storage", "WiFi credentials cleared");
}

// WiFi STA enable flag
bool storageGetWifiStaEnabled() {
  return pref.getBool(WIFI_STA_ENABLE_KEY, true);
}

void storageSetWifiStaEnabled(bool enabled) {
  pref.putBool(WIFI_STA_ENABLE_KEY, enabled);
  LOG_INFO("storage", "WiFi STA flag set enabled=%d", enabled ? 1 : 0);
}

bool storageGetWifiApPassword(String& pass) {
  pass = pref.getString(WIFI_AP_PASS_KEY, "");
  return pass.length() > 0;
}

void storageSetWifiApPassword(const String& pass) {
  pref.putString(WIFI_AP_PASS_KEY, pass);
  LOG_INFO("storage", "WiFi AP password updated set=%d", pass.length() > 0 ? 1 : 0);
}

void storageClearWifiApPassword() {
  pref.remove(WIFI_AP_PASS_KEY);
  LOG_WARN("storage", "WiFi AP password cleared");
}
