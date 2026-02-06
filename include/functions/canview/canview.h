#pragma once

#include <Arduino.h>
#include <driver/twai.h>

void canviewCacheFrame(const twai_message_t& msg, uint8_t bus);
void canviewCacheFrameTx(const twai_message_t& msg, uint8_t bus, bool generated = false);
String canviewBuildJson(uint16_t decoded_limit, uint8_t raw_limit, const String& bus_filter);
String canviewBuildDumpText(uint32_t window_ms, const String& bus_filter);

struct canview_last_tx_t {
  bool found;
  bool generated;
  uint32_t id;
  uint8_t dlc;
  uint8_t data[8];
  uint32_t ageMs;
};

bool canviewGetLastTxFrame(uint8_t bus, uint32_t id, canview_last_tx_t& out);