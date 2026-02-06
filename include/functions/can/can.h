#pragma once

#include <Arduino.h>
#include <driver/twai.h>

bool chassis_can_send(const twai_message_t& msg, TickType_t timeout_ticks);
bool haldex_can_send(const twai_message_t& msg, TickType_t timeout_ticks, bool generated = false);

bool chassis_can_receive(twai_message_t& msg);
bool haldex_can_receive(twai_message_t& msg);

void broadcastOpenHaldex(void* arg);
void parseCAN_chs(void* arg);
void parseCAN_hdx(void* arg);

void canInit();
void canRecoverIfBusFailure();
void canPoll();