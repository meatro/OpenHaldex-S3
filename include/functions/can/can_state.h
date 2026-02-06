#pragma once

#include <driver/twai.h>

twai_handle_t& can_bus_0();
twai_handle_t& can_bus_1();

twai_message_t& rx_msg_hdx();
twai_message_t& rx_msg_chs();
twai_message_t& tx_msg_hdx();
