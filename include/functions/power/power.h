#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <driver/twai.h>

void powerInitBootState();
void powerLoadBootSettings();
bool powerBootProbeOrSleep();
void powerStartMonitor();
void powerTrackChassisFrame(const twai_message_t& frame, uint32_t now_ms);
bool powerLowPowerActive();
void powerWriteStatusJson(JsonObject out);
const char* powerWakeCauseName();
