#pragma once

// Debug toggles (mirrors lilygo-t2can-map-dev)
#define enableDebug 1
#define detailedDebug 0
#define detailedDebugStack 0
#define detailedDebugRuntimeStats 0
#define detailedDebugCAN 0
#define detailedDebugWiFi 0
#define detailedDebugEEP 0
#define detailedDebugIO 0

// Feature toggles
#define OH_ENABLE_EEP_TASK 1
#define OH_EEP_START_DELAY_MS 15000

// Refresh rates (ms)
#define eepRefresh 2000
#define broadcastRefresh 200
#define serialMonitorRefresh 1000
#define labelRefresh 500
#define updateTriggersRefresh 500

// Back-compat aliases used in earlier ports
#define OH_DEBUG enableDebug
#define OH_DEBUG_CAN detailedDebugCAN
#define OH_DEBUG_WIFI detailedDebugWiFi
#define OH_DEBUG_IO detailedDebugIO

#define OH_EEP_REFRESH_MS eepRefresh
#define OH_BROADCAST_REFRESH_MS broadcastRefresh
#define OH_SERIAL_REFRESH_MS serialMonitorRefresh
#define OH_LABEL_REFRESH_MS labelRefresh
#define OH_TRIGGERS_REFRESH_MS updateTriggersRefresh

void filelogPrintf(const char* level, const char* tag, const char* fmt, ...);
bool filelogShouldSerialEmit(const char* level, const char* tag);

// Debug helpers
#if enableDebug
#define DEBUG(x, ...)                                                                                                 \
  do {                                                                                                                \
    if (filelogShouldSerialEmit("DEBUG", "debug")) {                                                                 \
      Serial.printf(x "\n", ##__VA_ARGS__);                                                                          \
    }                                                                                                                 \
    filelogPrintf("DEBUG", "debug", x, ##__VA_ARGS__);                                                               \
  } while (0)
#define DEBUG_(x, ...)                                                                                                \
  do {                                                                                                                \
    if (filelogShouldSerialEmit("DEBUG", "debug")) {                                                                 \
      Serial.printf(x, ##__VA_ARGS__);                                                                               \
    }                                                                                                                 \
    filelogPrintf("DEBUG", "debug", x, ##__VA_ARGS__);                                                               \
  } while (0)
#else
#define DEBUG(x, ...)
#define DEBUG_(x, ...)
#endif

#define LOG_INFO(tag, x, ...)                                                                                         \
  do {                                                                                                                \
    if (filelogShouldSerialEmit("INFO", tag)) {                                                                       \
      Serial.printf("[INFO][%s] " x "\n", tag, ##__VA_ARGS__);                                                       \
    }                                                                                                                 \
    filelogPrintf("INFO", tag, x, ##__VA_ARGS__);                                                                    \
  } while (0)

#define LOG_WARN(tag, x, ...)                                                                                         \
  do {                                                                                                                \
    if (filelogShouldSerialEmit("WARN", tag)) {                                                                       \
      Serial.printf("[WARN][%s] " x "\n", tag, ##__VA_ARGS__);                                                       \
    }                                                                                                                 \
    filelogPrintf("WARN", tag, x, ##__VA_ARGS__);                                                                    \
  } while (0)

#define LOG_ERROR(tag, x, ...)                                                                                        \
  do {                                                                                                                \
    if (filelogShouldSerialEmit("ERROR", tag)) {                                                                      \
      Serial.printf("[ERROR][%s] " x "\n", tag, ##__VA_ARGS__);                                                      \
    }                                                                                                                 \
    filelogPrintf("ERROR", tag, x, ##__VA_ARGS__);                                                                   \
  } while (0)

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)                                                                                           \
  ((byte) & 0x80 ? '1' : '0'), ((byte) & 0x40 ? '1' : '0'), ((byte) & 0x20 ? '1' : '0'), ((byte) & 0x10 ? '1' : '0'),  \
    ((byte) & 0x08 ? '1' : '0'), ((byte) & 0x04 ? '1' : '0'), ((byte) & 0x02 ? '1' : '0'), ((byte) & 0x01 ? '1' : '0')


