#pragma once

// Board selection
#ifndef OH_BOARD_T2CAN
#define OH_BOARD_T2CAN 1
#endif

// MCP2515 support for T-2CAN Haldex bus.
// Chassis bus uses internal ESP32-S3 TWAI by default.
#ifndef OH_CAN_HALDEX_MCP2515
#define OH_CAN_HALDEX_MCP2515 1
#endif

#if !OH_BOARD_T2CAN
#error "This project targets the LilyGo T-2CAN (ESP32-S3)."
#endif

// LilyGo T-2CAN (ESP32-S3) pin map
#define CAN0_RS -1
#define CAN0_RX -1
#define CAN0_TX -1
#define CAN1_RS -1
#define CAN1_RX 6
#define CAN1_TX 7

#define MCP2515_CS 10
#define MCP2515_SCLK 12
#define MCP2515_MOSI 11
#define MCP2515_MISO 13
#define MCP2515_RST 9
