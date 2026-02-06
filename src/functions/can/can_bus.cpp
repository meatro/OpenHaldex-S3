#include "functions/can/can.h"

#include "functions/config/pins.h"
#include "functions/config/config.h"
#include "functions/core/state.h"
#include "functions/can/can_id.h"
#include "functions/can/can_state.h"
#include "functions/canview/canview.h"

#if OH_CAN_CHASSIS_MCP2515
#include <SPI.h>
#include <mcp2515.h>

static MCP2515 can_mcp(MCP2515_CS, 10000000, &SPI);
#endif

static bool can_init_bus(int bus_id, int tx_pin, int rx_pin, twai_handle_t* out_handle) {
  if (tx_pin < 0 || rx_pin < 0)
    return false;

  twai_general_config_t g_config =
    TWAI_GENERAL_CONFIG_DEFAULT(gpio_num_t(tx_pin), gpio_num_t(rx_pin), TWAI_MODE_NO_ACK);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  g_config.tx_queue_len = 1024;
  g_config.rx_queue_len = 2048;
  g_config.controller_id = bus_id;
  g_config.tx_io = gpio_num_t(tx_pin);
  g_config.rx_io = gpio_num_t(rx_pin);

  esp_err_t err = twai_driver_install_v2(&g_config, &t_config, &f_config, out_handle);
  if (err != ESP_OK) {
    return false;
  }
  err = twai_start_v2(*out_handle);
  if (err != ESP_OK) {
    return false;
  }
  return true;
}

bool chassis_can_send(const twai_message_t& msg, TickType_t timeout_ticks) {
#if OH_CAN_CHASSIS_MCP2515
  (void)timeout_ticks;
  struct can_frame frame = {};
  frame.can_id = msg.identifier & 0x1FFFFFFF;
  if (msg.extd)
    frame.can_id |= CAN_EFF_FLAG;
  if (msg.rtr)
    frame.can_id |= CAN_RTR_FLAG;
  frame.can_dlc = msg.data_length_code;
  for (uint8_t i = 0; i < frame.can_dlc && i < 8; i++) {
    frame.data[i] = msg.data[i];
  }
  return can_mcp.sendMessage(&frame) == MCP2515::ERROR_OK;
#else
  if (!can_bus_0())
    return false;
  bool ok = (twai_transmit_v2(can_bus_0(), &msg, timeout_ticks) == ESP_OK);
  if (ok) {
    canviewCacheFrameTx(msg, 0);
  }
  return ok;
#endif
}

bool haldex_can_send(const twai_message_t& msg, TickType_t timeout_ticks, bool generated) {
  if (!can_bus_1())
    return false;
  bool ok = (twai_transmit_v2(can_bus_1(), &msg, timeout_ticks) == ESP_OK);
  if (ok) {
    canviewCacheFrameTx(msg, 1, generated);
  }
  return ok;
}

bool chassis_can_receive(twai_message_t& msg) {
#if OH_CAN_CHASSIS_MCP2515
  struct can_frame frame = {};
  if (can_mcp.readMessage(&frame) != MCP2515::ERROR_OK) {
    return false;
  }
  msg.identifier = frame.can_id & CAN_EFF_MASK;
  msg.extd = (frame.can_id & CAN_EFF_FLAG) ? 1 : 0;
  msg.rtr = (frame.can_id & CAN_RTR_FLAG) ? 1 : 0;
  msg.data_length_code = frame.can_dlc;
  for (uint8_t i = 0; i < frame.can_dlc && i < 8; i++) {
    msg.data[i] = frame.data[i];
  }
  return true;
#else
  if (!can_bus_0())
    return false;
  return twai_receive_v2(can_bus_0(), &msg, 0) == ESP_OK;
#endif
}

bool haldex_can_receive(twai_message_t& msg) {
  if (!can_bus_1())
    return false;
  return twai_receive_v2(can_bus_1(), &msg, 0) == ESP_OK;
}

void canInit() {
  // Initialize CAN buses - T-CAN-S3 only has one built-in CAN controller (CAN1)
  // CAN0 is optional via MCP2515 SPI module
  can_ready = false;
  can0_ready = false;
  can1_ready = false;

#if OH_CAN_CHASSIS_MCP2515
  // CAN A (chassis) on MCP2515 (match lilygo-t2can-map-dev)
  pinMode(MCP2515_RST, OUTPUT);
  digitalWrite(MCP2515_RST, HIGH);
  delay(10);
  digitalWrite(MCP2515_RST, LOW);
  delay(10);
  digitalWrite(MCP2515_RST, HIGH);
  delay(10);

  SPI.begin(MCP2515_SCLK, MCP2515_MISO, MCP2515_MOSI, MCP2515_CS);

  if (can_mcp.reset() != MCP2515::ERROR_OK) {
    DEBUG("CAN A (MCP2515) reset failed");
    return;
  }
  if (can_mcp.setBitrate(CAN_500KBPS) != MCP2515::ERROR_OK) {
    DEBUG("CAN A (MCP2515) bitrate set failed");
    return;
  }
  if (can_mcp.setNormalMode() != MCP2515::ERROR_OK) {
    DEBUG("CAN A (MCP2515) normal mode failed");
    return;
  }
  DEBUG("CAN A (MCP2515) started");
  can0_ready = true;
#else
  can0_ready = can_init_bus(0, CAN0_TX, CAN0_RX, &can_bus_0());
  if (!can0_ready) {
    DEBUG("CAN - Driver 0 init failed");
    return;
  }
#endif

  // CAN B (TWAI) for Haldex
  {
    twai_general_config_t g_config =
      TWAI_GENERAL_CONFIG_DEFAULT(gpio_num_t(CAN1_TX), gpio_num_t(CAN1_RX), TWAI_MODE_NO_ACK);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    g_config.tx_queue_len = 1024;
    g_config.rx_queue_len = 2048;
    g_config.controller_id = OH_BOARD_T2CAN ? 0 : 1;
    g_config.tx_io = gpio_num_t(CAN1_TX);
    g_config.rx_io = gpio_num_t(CAN1_RX);

    esp_err_t err = twai_driver_install_v2(&g_config, &t_config, &f_config, &can_bus_1());
    if (err != ESP_OK) {
      DEBUG("CAN - Driver 1 Install Failed: %s", esp_err_to_name(err));
      return;
    }
    DEBUG("CAN - Driver 1 Installed");
    err = twai_start_v2(can_bus_1());
    if (err != ESP_OK) {
      DEBUG("CAN - Driver 1 Start Failed: %s", esp_err_to_name(err));
      return;
    }
    DEBUG("CAN - Driver 1 Started");
    can1_ready = true;
  }

  can_ready = can0_ready && can1_ready;

  if (!can1_ready) {
    DEBUG("CAN - Not ready; skipping alert configuration");
    return;
  }
  uint32_t alerts_to_enable =
    TWAI_ALERT_RX_DATA | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR | TWAI_ALERT_RX_QUEUE_FULL;
  if (twai_reconfigure_alerts(alerts_to_enable, NULL) == ESP_OK) {
    DEBUG("Reconfiguration of CAN alerts");
  } else {
    DEBUG("Failed to reconfigure CAN alerts!");
    return;
  }
}

void canRecoverIfBusFailure() {
#if OH_CAN_CHASSIS_MCP2515
  if (can1_ready && isBusFailure && can_bus_1()) {
    twai_initiate_recovery_v2(can_bus_1());
  }
#else
  if (can_ready && isBusFailure) {
    if (can_bus_0()) {
      twai_initiate_recovery_v2(can_bus_0());
    }
    if (can_bus_1()) {
      twai_initiate_recovery_v2(can_bus_1());
    }
  }
#endif
}

void canPoll() {}
