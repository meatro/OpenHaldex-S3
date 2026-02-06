#pragma once

#include <driver/twai.h>

#include "functions/can/standalone_can.h"

float get_lock_target_adjustment();
uint8_t get_lock_target_adjusted_value(uint8_t value, bool invert);
void getLockData(twai_message_t& rx_message_chs);
