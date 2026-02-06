#include "functions/can/can_state.h"

static twai_handle_t twai_bus_0 = nullptr;
static twai_handle_t twai_bus_1 = nullptr;

static twai_message_t rx_message_hdx = {};
static twai_message_t rx_message_chs = {};
static twai_message_t tx_message_hdx = {};

twai_handle_t& can_bus_0() {
  return twai_bus_0;
}
twai_handle_t& can_bus_1() {
  return twai_bus_1;
}

twai_message_t& rx_msg_hdx() {
  return rx_message_hdx;
}
twai_message_t& rx_msg_chs() {
  return rx_message_chs;
}
twai_message_t& tx_msg_hdx() {
  return tx_message_hdx;
}
