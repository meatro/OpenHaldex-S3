#include "functions/storage/filelog.h"

#include <LittleFS.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "functions/core/state.h"
#include "functions/storage/storage.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char* LOG_ROOT = "/logs";
static const char* LOG_CAN_DIR = "/logs/can";
static const char* LOG_ERROR_DIR = "/logs/error";

static const char* LOG_ALL_FILE = "/logs/all.txt";
static const char* LOG_CAN_FILE = "/logs/can/can.txt";
static const char* LOG_ERROR_FILE = "/logs/error/error.txt";

static const size_t LOG_FILE_MAX_BYTES = 256 * 1024;
static const uint8_t LOG_FILE_ROTATIONS = 4;

static SemaphoreHandle_t filelog_mutex = nullptr;
static bool filelog_ready = false;

static bool filelog_level_equals(const char* level, const char* expected) {
  if (!level || !expected) {
    return false;
  }
  return strcasecmp(level, expected) == 0;
}

enum filelog_category_t {
  FILELOG_CATEGORY_FIRMWARE = 0,
  FILELOG_CATEGORY_NETWORK = 1,
  FILELOG_CATEGORY_CAN = 2,
};

static bool filelog_starts_with(const String& value, const char* prefix) {
  if (!prefix) {
    return false;
  }
  return value.startsWith(prefix);
}

static filelog_category_t filelog_category_from_tag(const char* tag) {
  String t = tag ? String(tag) : String("");
  t.toLowerCase();

  if (filelog_starts_with(t, "can")) {
    return FILELOG_CATEGORY_CAN;
  }
  if (filelog_starts_with(t, "wifi") || filelog_starts_with(t, "net") || filelog_starts_with(t, "ota") ||
      filelog_starts_with(t, "mdns")) {
    return FILELOG_CATEGORY_NETWORK;
  }
  return FILELOG_CATEGORY_FIRMWARE;
}

static bool filelog_category_enabled(filelog_category_t category) {
  switch (category) {
  case FILELOG_CATEGORY_NETWORK:
    return logDebugNetworkEnabled;
  case FILELOG_CATEGORY_CAN:
    return logDebugCanEnabled;
  case FILELOG_CATEGORY_FIRMWARE:
  default:
    return logDebugFirmwareEnabled;
  }
}

static bool filelog_is_error_level(const char* level) {
  return filelog_level_equals(level, "ERROR");
}

static bool filelog_is_can_level(const char* level) {
  return filelog_level_equals(level, "CAN");
}

static String filelog_level_name(const char* level) {
  if (!level || level[0] == '\0') {
    return String("EVENT");
  }
  String out = level;
  out.toUpperCase();
  return out;
}

static bool filelog_should_emit_core(const char* level, const char* tag) {
  if (filelog_is_error_level(level)) {
    return logErrorToFileEnabled;
  }

  const filelog_category_t category = filelog_category_from_tag(tag);
  if (!filelog_category_enabled(category)) {
    return false;
  }

  if (filelog_is_can_level(level) && !logCanToFileEnabled) {
    return false;
  }

  return true;
}

static bool filelog_should_emit_file(const char* level, const char* tag) {
  if (!logToFileEnabled) {
    return false;
  }
  return filelog_should_emit_core(level, tag);
}

bool filelogShouldSerialEmit(const char* level, const char* tag) {
  if (!logSerialEnabled) {
    return false;
  }
  return filelog_should_emit_core(level, tag);
}

static String filelog_sanitize_line(const String& in) {
  String out = in;
  out.replace("\r", " ");
  out.replace("\n", " ");
  out.replace("\t", " ");
  return out;
}

static bool filelog_is_valid_path(const String& path) {
  if (path.length() == 0)
    return false;
  if (!path.startsWith("/logs/") && path != LOG_ALL_FILE)
    return false;
  if (path.indexOf("..") >= 0)
    return false;
  return true;
}

static String filelog_rotated_path(const String& path, uint8_t idx) {
  int dot = path.lastIndexOf('.');
  if (dot < 0) {
    return path + "." + String(idx);
  }
  String out = path.substring(0, dot);
  out += ".";
  out += String(idx);
  out += path.substring(dot);
  return out;
}

static void filelog_rotate_if_needed(const String& path) {
  if (!LittleFS.exists(path))
    return;

  File f = LittleFS.open(path, "r");
  if (!f)
    return;
  size_t size = f.size();
  f.close();

  if (size < LOG_FILE_MAX_BYTES) {
    return;
  }

  String oldest = filelog_rotated_path(path, LOG_FILE_ROTATIONS);
  if (LittleFS.exists(oldest)) {
    LittleFS.remove(oldest);
  }

  for (int i = LOG_FILE_ROTATIONS - 1; i >= 1; i--) {
    String from = filelog_rotated_path(path, (uint8_t)i);
    String to = filelog_rotated_path(path, (uint8_t)(i + 1));
    if (!LittleFS.exists(from))
      continue;
    if (LittleFS.exists(to)) {
      LittleFS.remove(to);
    }
    LittleFS.rename(from, to);
  }

  String first = filelog_rotated_path(path, 1);
  if (LittleFS.exists(first)) {
    LittleFS.remove(first);
  }
  LittleFS.rename(path, first);
}

static bool filelog_append_line_unlocked(const String& path, const String& line) {
  if (!filelog_ready || !storageFsReady())
    return false;
  if (!filelog_is_valid_path(path))
    return false;

  filelog_rotate_if_needed(path);

  File f = LittleFS.open(path, "a");
  if (!f)
    return false;
  size_t written = f.print(line);
  f.close();
  return written == line.length();
}

static String filelog_level_line(const char* level, const String& tag, const String& message) {
  String out;
  out.reserve(64 + tag.length() + message.length());
  out += String(millis());
  out += "\t";
  out += level;
  out += "\t";
  out += filelog_sanitize_line(tag);
  out += "\t";
  out += filelog_sanitize_line(message);
  out += "\n";
  return out;
}

static bool filelog_append_level_unlocked(const char* level, const String& tag, const String& message) {
  if (!filelog_ready || !storageFsReady()) {
    return false;
  }
  if (!filelog_should_emit_file(level, tag.c_str())) {
    return false;
  }

  const String line = filelog_level_line(filelog_level_name(level).c_str(), tag, message);
  bool ok = filelog_append_line_unlocked(LOG_ALL_FILE, line);

  if (filelog_is_error_level(level) && logErrorToFileEnabled) {
    ok = filelog_append_line_unlocked(LOG_ERROR_FILE, line) && ok;
  }
  return ok;
}

static String filelog_format_message(const char* fmt, va_list args) {
  if (!fmt) {
    return String("");
  }

  char stack_buf[256];
  va_list args_copy;
  va_copy(args_copy, args);
  int needed = vsnprintf(stack_buf, sizeof(stack_buf), fmt, args_copy);
  va_end(args_copy);

  if (needed < 0) {
    return String("format-error");
  }

  if ((size_t)needed < sizeof(stack_buf)) {
    return String(stack_buf);
  }

  const size_t required = (size_t)needed + 1;
  char* heap_buf = (char*)malloc(required);
  if (!heap_buf) {
    return String(stack_buf);
  }

  va_list args_copy_heap;
  va_copy(args_copy_heap, args);
  (void)vsnprintf(heap_buf, required, fmt, args_copy_heap);
  va_end(args_copy_heap);
  String out = String(heap_buf);
  free(heap_buf);
  return out;
}

static void filelog_write_level(const char* level, const String& tag, const String& message) {
  if (!filelog_ready || !storageFsReady()) {
    return;
  }
  if (!filelog_should_emit_file(level, tag.c_str())) {
    return;
  }
  if (xSemaphoreTake(filelog_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
    return;
  }
  (void)filelog_append_level_unlocked(level, tag, message);
  xSemaphoreGive(filelog_mutex);
}

static void filelog_list_dir(JsonArray out, const String& dir_path, const char* scope) {
  File dir = LittleFS.open(dir_path);
  if (!dir || !dir.isDirectory()) {
    return;
  }

  File file = dir.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String path = file.name();
      if (!path.startsWith("/")) {
        path = dir_path + "/" + path;
      } else if (!path.startsWith(dir_path + "/")) {
        path = dir_path + path;
      }
      JsonObject obj = out.add<JsonObject>();
      obj["scope"] = scope;
      obj["path"] = path;
      obj["size"] = (uint32_t)file.size();
    }
    file = dir.openNextFile();
  }
}

static bool filelog_clear_dir_unlocked(const String& dir_path) {
  File dir = LittleFS.open(dir_path);
  if (!dir || !dir.isDirectory()) {
    return false;
  }
  bool removed_any = false;
  File file = dir.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String path = file.name();
      if (!path.startsWith("/")) {
        path = dir_path + "/" + path;
      } else if (!path.startsWith(dir_path + "/")) {
        path = dir_path + path;
      }
      if (LittleFS.remove(path)) {
        removed_any = true;
      }
    }
    file = dir.openNextFile();
  }
  return removed_any;
}

void filelogInit() {
  if (!storageFsReady()) {
    return;
  }

  if (!filelog_mutex) {
    filelog_mutex = xSemaphoreCreateMutex();
    if (!filelog_mutex) {
      return;
    }
  }

  if (xSemaphoreTake(filelog_mutex, pdMS_TO_TICKS(250)) != pdTRUE) {
    return;
  }

  LittleFS.mkdir(LOG_ROOT);
  LittleFS.mkdir(LOG_CAN_DIR);
  LittleFS.mkdir(LOG_ERROR_DIR);

  auto ensure_log_file = [](const char* path) {
    if (LittleFS.exists(path)) {
      return;
    }
    File f = LittleFS.open(path, "w");
    if (f) {
      f.close();
    }
  };

  ensure_log_file(LOG_ALL_FILE);
  ensure_log_file(LOG_CAN_FILE);
  ensure_log_file(LOG_ERROR_FILE);

  filelog_ready = true;

  xSemaphoreGive(filelog_mutex);

  filelogLogEvent("system", "logger initialized");
}

void filelogLogEvent(const String& tag, const String& message) {
  filelog_write_level("EVENT", tag, message);
}

void filelogLogDebug(const String& tag, const String& message) {
  filelog_write_level("DEBUG", tag, message);
}

void filelogLogInfo(const String& tag, const String& message) {
  filelog_write_level("INFO", tag, message);
}

void filelogLogWarn(const String& tag, const String& message) {
  filelog_write_level("WARN", tag, message);
}

void filelogLogError(const String& tag, const String& message) {
  filelog_write_level("ERROR", tag, message);
}

void filelogPrintf(const char* level, const char* tag, const char* fmt, ...) {
  if (!fmt || !tag) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  String message = filelog_format_message(fmt, args);
  va_end(args);
  if (message.length() == 0) {
    return;
  }

  filelog_write_level(level ? level : "EVENT", String(tag), message);
}

void filelogLogCanFrame(const twai_message_t& msg, uint8_t bus, uint8_t dir, bool generated) {
  if (!filelog_ready || !storageFsReady()) {
    return;
  }
  if (!filelog_should_emit_file("CAN", "can")) {
    return;
  }

  if (xSemaphoreTake(filelog_mutex, 0) != pdTRUE) {
    return;
  }

  String line;
  line.reserve(96);
  line += String(millis());
  line += "\tCAN\t";
  line += (bus == 0) ? "chassis" : "haldex";
  line += "\t";
  line += (dir == 0) ? "RX" : "TX";
  line += "\t";
  line += generated ? "GEN" : "-";
  line += "\t0x";
  line += String(msg.identifier, HEX);
  line += "\t";
  line += String(msg.data_length_code);
  line += "\t";

  for (uint8_t i = 0; i < msg.data_length_code && i < 8; i++) {
    if (i > 0)
      line += " ";
    if (msg.data[i] < 16)
      line += "0";
    line += String(msg.data[i], HEX);
  }
  line += "\n";

  filelog_append_line_unlocked(LOG_CAN_FILE, line);
  filelog_append_line_unlocked(LOG_ALL_FILE, line);
  xSemaphoreGive(filelog_mutex);
}

void filelogList(JsonArray out) {
  if (!filelog_ready || !storageFsReady()) {
    return;
  }
  if (xSemaphoreTake(filelog_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return;
  }

  filelog_list_dir(out, LOG_ROOT, "all");
  filelog_list_dir(out, LOG_CAN_DIR, "can");
  filelog_list_dir(out, LOG_ERROR_DIR, "error");

  xSemaphoreGive(filelog_mutex);
}

bool filelogRead(const String& path, String& out, size_t max_bytes) {
  out = "";
  if (!filelog_ready || !storageFsReady()) {
    return false;
  }
  if (!filelog_is_valid_path(path)) {
    return false;
  }
  if (!LittleFS.exists(path)) {
    return false;
  }

  if (xSemaphoreTake(filelog_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
    return false;
  }

  File f = LittleFS.open(path, "r");
  if (!f) {
    xSemaphoreGive(filelog_mutex);
    return false;
  }

  size_t size = f.size();
  size_t to_read = size;
  bool truncated = false;
  if (max_bytes > 0 && to_read > max_bytes) {
    to_read = max_bytes;
    truncated = true;
  }

  if (to_read < size) {
    f.seek(size - to_read, SeekSet);
  }

  out = f.readString();
  f.close();
  xSemaphoreGive(filelog_mutex);

  if (truncated) {
    out = String("[truncated to last ") + String(to_read) + " bytes]\n" + out;
  }

  return true;
}

bool filelogDelete(const String& path) {
  if (!filelog_ready || !storageFsReady()) {
    return false;
  }
  if (!filelog_is_valid_path(path)) {
    return false;
  }
  if (!LittleFS.exists(path)) {
    return false;
  }
  if (xSemaphoreTake(filelog_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return false;
  }
  bool ok = LittleFS.remove(path);
  xSemaphoreGive(filelog_mutex);
  return ok;
}

bool filelogClearScope(const String& scope) {
  if (!filelog_ready || !storageFsReady()) {
    return false;
  }
  String s = scope;
  s.toLowerCase();

  if (xSemaphoreTake(filelog_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
    return false;
  }

  bool valid_scope = false;
  if (s == "all" || s == "everything") {
    (void)filelog_clear_dir_unlocked(LOG_ROOT);
    valid_scope = true;
  }

  if (s == "can" || s == "everything") {
    (void)filelog_clear_dir_unlocked(LOG_CAN_DIR);
    valid_scope = true;
  }

  if (s == "error" || s == "everything") {
    (void)filelog_clear_dir_unlocked(LOG_ERROR_DIR);
    valid_scope = true;
  }

  xSemaphoreGive(filelog_mutex);
  return valid_scope;
}
