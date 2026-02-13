#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <driver/twai.h>

void filelogInit();

bool filelogShouldSerialEmit(const char* level, const char* tag);

void filelogPrintf(const char* level, const char* tag, const char* fmt, ...);

void filelogLogDebug(const String& tag, const String& message);
void filelogLogInfo(const String& tag, const String& message);
void filelogLogWarn(const String& tag, const String& message);
void filelogLogEvent(const String& tag, const String& message);
void filelogLogError(const String& tag, const String& message);
void filelogLogCanFrame(const twai_message_t& msg, uint8_t bus, uint8_t dir, bool generated);

void filelogList(JsonArray out);
bool filelogRead(const String& path, String& out, size_t max_bytes = 32768);
bool filelogDelete(const String& path);
bool filelogClearScope(const String& scope);
