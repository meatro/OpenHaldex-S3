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

struct canview_resolved_signal_t {
  bool found;
  bool mapped;
  bool numeric;
  bool generated;
  uint32_t id;
  uint32_t ageMs;
  String bus;
  String dir;
  String name;
  String unit;
  float numericValue;
  String textValue;
};

bool canviewResolveMappedSignal(const String& key, canview_resolved_signal_t& out);
