#include "functions/io/frames.h"

#include <Arduino.h>
#include <driver/twai.h>

#include "functions/config/config.h"
#include "functions/config/pins.h"
#include "functions/core/state.h"
#include "functions/core/calcs.h"
#include "functions/can/can.h"
#include "functions/can/can_id.h"
#include "functions/can/standalone_can.h"

void Gen1_frames10() {}

void Gen1_frames20() {
  twai_message_t frame = {};
  frame.identifier = MOTOR1_ID;
  frame.data_length_code = 8;
  frame.data[0] = 0x00;
  frame.data[1] = get_lock_target_adjusted_value(0xFE, false);
  frame.data[2] = 0x21;
  frame.data[3] = get_lock_target_adjusted_value(0x4E, false);
  frame.data[4] = get_lock_target_adjusted_value(0xFE, false);
  frame.data[5] = get_lock_target_adjusted_value(0xFE, false);

  switch (state.mode) {
  case MODE_FWD:
    appliedTorque = get_lock_target_adjusted_value(0xFE, true);
    break;
  case MODE_5050:
    appliedTorque = get_lock_target_adjusted_value(0x16, false);
    break;
  case MODE_6040:
    appliedTorque = get_lock_target_adjusted_value(0x22, false);
    break;
  case MODE_7030:
    appliedTorque = get_lock_target_adjusted_value(0x50, false);
    break;
  case MODE_8020:
    appliedTorque = get_lock_target_adjusted_value(0x50, false);
    break;
  case MODE_9010:
    appliedTorque = get_lock_target_adjusted_value(0x50, false);
    break;
  default:
    break;
  }

  frame.data[6] = appliedTorque;
  frame.data[7] = 0x00;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = MOTOR3_ID;
  frame.data_length_code = 8;
  frame.data[0] = 0x00;
  frame.data[1] = 0x50;
  frame.data[2] = 0x00;
  frame.data[3] = 0x00;
  frame.data[4] = 0x00;
  frame.data[5] = 0x00;
  frame.data[6] = 0x00;
  frame.data[7] = 0xFE;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = BRAKES1_ID;
  frame.data_length_code = 8;
  frame.data[0] = 0x80;
  frame.data[1] = get_lock_target_adjusted_value(0x00, false);
  frame.data[2] = 0x00;
  frame.data[3] = 0x0A;
  frame.data[4] = 0xFE;
  frame.data[5] = 0xFE;
  frame.data[6] = 0x00;
  frame.data[7] = BRAKES1_counter;
  if (++BRAKES1_counter > 0xF) {
    BRAKES1_counter = 0;
  }
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = BRAKES3_ID;
  frame.data_length_code = 8;
  frame.data[0] = get_lock_target_adjusted_value(0xFE, false);
  frame.data[1] = 0x0A;
  frame.data[2] = get_lock_target_adjusted_value(0xFE, false);
  frame.data[3] = 0x0A;
  frame.data[4] = 0x00;
  frame.data[5] = 0x0A;
  frame.data[6] = 0x00;
  frame.data[7] = 0x0A;
  haldex_can_send(frame, 0);
}

void Gen1_frames25() {}

void Gen1_frames100() {}

void Gen1_frames200() {}

void Gen1_frames1000() {}

void Gen2_frames10() {
  twai_message_t frame = {};
  frame.identifier = BRAKES1_ID;
  frame.data_length_code = 8;
  frame.data[0] = 0x00;
  frame.data[1] = 0x41;
  frame.data[2] = 0x00;
  frame.data[3] = 0xFE;
  frame.data[4] = 0xFE;
  frame.data[5] = 0xFE;
  frame.data[6] = 0x00;
  frame.data[7] = BRAKES1_counter;
  if (++BRAKES1_counter > 0xF) {
    BRAKES1_counter = 0;
  }
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = BRAKES2_ID;
  frame.data_length_code = 8;
  frame.data[0] = 0x7F;
  frame.data[1] = 0xAE;
  frame.data[2] = 0x3D;
  frame.data[3] = BRAKES2_counter;
  frame.data[4] = get_lock_target_adjusted_value(0x7F, false);
  frame.data[5] = get_lock_target_adjusted_value(0xFE, false);
  frame.data[6] = 0x5E;
  frame.data[7] = 0x2B;
  BRAKES2_counter = (uint8_t)(BRAKES2_counter + 10);
  if (BRAKES2_counter > 0xF7) {
    BRAKES2_counter = 7;
  }
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = BRAKES3_ID;
  frame.data_length_code = 8;
  frame.data[0] = get_lock_target_adjusted_value(0xFE, false);
  frame.data[1] = 0x0A;
  frame.data[2] = get_lock_target_adjusted_value(0xFE, false);
  frame.data[3] = 0x0A;
  frame.data[4] = 0x00;
  frame.data[5] = 0x0A;
  frame.data[6] = 0x00;
  frame.data[7] = 0x0A;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = BRAKES4_ID;
  frame.data_length_code = 8;
  frame.data[0] = 0x00;
  frame.data[1] = 0x00;
  frame.data[2] = 0x00;
  frame.data[3] = 0x00;
  frame.data[4] = 0x00;
  frame.data[5] = 0x00;
  frame.data[6] = BRAKES4_counter;
  frame.data[7] = BRAKES4_counter;
  BRAKES4_counter = (uint8_t)(BRAKES4_counter + 10);
  if (BRAKES4_counter > 0xF0) {
    BRAKES4_counter = 0;
  }
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = BRAKES5_ID;
  frame.data_length_code = 8;
  frame.data[0] = 0xFE;
  frame.data[1] = 0x7F;
  frame.data[2] = 0x03;
  frame.data[3] = 0x00;
  frame.data[4] = 0x00;
  frame.data[5] = 0x00;
  frame.data[6] = BRAKES5_counter;
  frame.data[7] = BRAKES5_counter2;
  BRAKES5_counter = (uint8_t)(BRAKES5_counter + 10);
  if (BRAKES5_counter > 0xF0) {
    BRAKES5_counter = 0;
  }
  BRAKES5_counter2 = (uint8_t)(BRAKES5_counter2 + 10);
  if (BRAKES5_counter2 > 0xF3) {
    BRAKES5_counter2 = 3;
  }
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = BRAKES9_ID;
  frame.data_length_code = 8;
  frame.data[0] = BRAKES9_counter;
  frame.data[1] = BRAKES9_counter2;
  frame.data[2] = 0x00;
  frame.data[3] = 0x00;
  frame.data[4] = 0x00;
  frame.data[5] = 0x00;
  frame.data[6] = 0x02;
  frame.data[7] = 0x00;
  haldex_can_send(frame, 0);
  BRAKES9_counter = (uint8_t)(BRAKES9_counter + 10);
  if (BRAKES9_counter > 0xF1) {
    BRAKES9_counter = 0x11;
  }
  BRAKES9_counter2 = (uint8_t)(BRAKES9_counter2 + 10);
  if (BRAKES9_counter2 > 0xF0) {
    BRAKES9_counter2 = 0x00;
  }

  frame = {};
  frame.identifier = mLW_1;
  frame.extd = 0;
  frame.rtr = 0;
  frame.data_length_code = 8;
  frame.data[0] = 0x20;
  frame.data[1] = 0x00;
  frame.data[2] = 0x00;
  frame.data[3] = 0x00;
  frame.data[4] = 0x80;
  frame.data[5] = mLW_1_counter;
  frame.data[6] = 0x00;
  mLW_1_crc = 255 - (frame.data[0] + frame.data[1] + frame.data[2] + frame.data[3] + frame.data[5]);
  frame.data[7] = mLW_1_crc;
  mLW_1_counter = (uint8_t)(mLW_1_counter + 16);
  if (mLW_1_counter >= 0xF0) {
    mLW_1_counter = 0;
  }
  haldex_can_send(frame, 0);
}

void Gen2_frames20() {
  twai_message_t frame = {};
  frame.identifier = MOTOR1_ID;
  frame.data_length_code = 8;
  frame.data[0] = 0x08;
  frame.data[1] = 0xFA;
  frame.data[2] = 0x20;
  frame.data[3] = get_lock_target_adjusted_value(0x4E, false);
  frame.data[4] = 0xFA;
  frame.data[5] = 0xFA;
  frame.data[6] = get_lock_target_adjusted_value(0x20, false);
  frame.data[7] = 0xFA;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = MOTOR2_ID;
  frame.data_length_code = 8;
  frame.data[0] = 0x00;
  frame.data[1] = 0x30;
  frame.data[2] = 0x00;
  frame.data[3] = 0x0A;
  frame.data[4] = 0x0A;
  frame.data[5] = 0x10;
  frame.data[6] = 0xFE;
  frame.data[7] = 0xFE;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = MOTOR5_ID;
  frame.data_length_code = 8;
  frame.data[0] = 0xFE;
  frame.data[1] = 0x00;
  frame.data[2] = 0x00;
  frame.data[3] = 0x00;
  frame.data[4] = 0x00;
  frame.data[5] = 0x00;
  frame.data[6] = 0x00;
  frame.data[7] = MOTOR5_counter;
  if (++BRAKES1_counter > 255) {
    BRAKES1_counter = 0;
  }
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = BRAKES10_ID;
  frame.data_length_code = 8;
  frame.data[0] = 0xA6;
  frame.data[1] = BRAKES10_counter;
  frame.data[2] = 0x75;
  frame.data[3] = 0xD4;
  frame.data[4] = 0x51;
  frame.data[5] = 0x47;
  frame.data[6] = 0x1D;
  frame.data[7] = 0x0F;
  BRAKES10_counter = (uint8_t)(BRAKES10_counter + 1);
  if (BRAKES10_counter > 0xF) {
    BRAKES10_counter = 0;
  }
  haldex_can_send(frame, 0);
}

void Gen2_frames25() {
  twai_message_t frame = {};
  frame.identifier = mKombi_1;
  frame.data_length_code = 8;
  frame.data[0] = 0x00;
  frame.data[1] = 0x02;
  frame.data[2] = 0x00;
  frame.data[3] = 0x00;
  frame.data[4] = 0x36;
  frame.data[5] = 0x00;
  frame.data[6] = 0x00;
  frame.data[7] = 0x00;
  haldex_can_send(frame, 0);
}

void Gen2_frames100() {}

void Gen2_frames200() {}

void Gen2_frames1000() {}

void Gen4_frames10() {
  twai_message_t frame = {};
  frame.identifier = mLW_1;
  frame.extd = 0;
  frame.rtr = 0;
  frame.data_length_code = 8;
  frame.data[0] = lws_2[mLW_1_counter][0];
  frame.data[1] = lws_2[mLW_1_counter][1];
  frame.data[2] = lws_2[mLW_1_counter][2];
  frame.data[3] = lws_2[mLW_1_counter][3];
  frame.data[4] = lws_2[mLW_1_counter][4];
  frame.data[5] = lws_2[mLW_1_counter][5];
  frame.data[6] = lws_2[mLW_1_counter][6];
  frame.data[7] = lws_2[mLW_1_counter][7];

  mLW_1_counter++;
  if (mLW_1_counter > 15) {
    mLW_1_counter = 0;
  }
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = BRAKES1_ID;
  frame.data_length_code = 8;
  frame.data[0] = 0x20;
  frame.data[1] = 0x40;
  frame.data[2] = 0xF0;
  frame.data[3] = 0x07;
  frame.data[4] = get_lock_target_adjusted_value(0xFE, false);
  frame.data[5] = get_lock_target_adjusted_value(0xFE, false);
  frame.data[6] = 0x00;
  frame.data[7] = BRAKES1_counter;
  if (++BRAKES1_counter > 0x1F) {
    BRAKES1_counter = 10;
  }
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = BRAKES3_ID;
  frame.data_length_code = 8;
  frame.data[0] = get_lock_target_adjusted_value(0xB6, false);
  frame.data[1] = 0x07;
  frame.data[2] = get_lock_target_adjusted_value(0xCC, false);
  frame.data[3] = 0x07;
  frame.data[4] = get_lock_target_adjusted_value(0xD2, false);
  frame.data[5] = 0x07;
  frame.data[6] = get_lock_target_adjusted_value(0xD2, false);
  frame.data[7] = 0x07;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = BRAKES4_ID;
  frame.data_length_code = 8;
  frame.data[0] = get_lock_target_adjusted_value(0xFE, false);
  frame.data[1] = 0x00;
  frame.data[2] = 0x00;
  frame.data[3] = 0x64;
  frame.data[4] = 0x00;
  frame.data[5] = 0x00;
  frame.data[6] = BRAKES4_counter;
  BRAKES4_crc = 0;
  for (uint8_t i = 0; i < 7; i++) {
    BRAKES4_crc ^= frame.data[i];
  }
  frame.data[7] = BRAKES4_crc;

  BRAKES4_counter = (uint8_t)(BRAKES4_counter + 16);
  if (BRAKES4_counter > 0xF0) {
    BRAKES4_counter = 0x00;
  }
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = BRAKES9_ID;
  frame.data_length_code = 8;
  frame.data[0] = BRAKES9_counter;
  frame.data[1] = BRAKES9_counter2;
  frame.data[2] = 0x00;
  frame.data[3] = 0x00;
  frame.data[4] = 0x00;
  frame.data[5] = 0x00;
  frame.data[6] = 0x03;
  frame.data[7] = 0x00;
  haldex_can_send(frame, 0);
  BRAKES9_counter = (uint8_t)(BRAKES9_counter + 16);
  if (BRAKES9_counter > 0xF3) {
    BRAKES9_counter = 0x03;
  }
  BRAKES9_counter2 = (uint8_t)(BRAKES9_counter2 + 16);
  if (BRAKES9_counter2 > 0xF0) {
    BRAKES9_counter2 = 0x00;
  }

  frame = {};
  frame.identifier = MOTOR1_ID;
  frame.data_length_code = 8;
  frame.data[1] = get_lock_target_adjusted_value(0xFE, false);
  frame.data[2] = get_lock_target_adjusted_value(0x20, false);
  frame.data[3] = get_lock_target_adjusted_value(0x4E, false);
  frame.data[4] = get_lock_target_adjusted_value(0xFE, false);
  frame.data[5] = get_lock_target_adjusted_value(0xFE, false);
  frame.data[6] = get_lock_target_adjusted_value(0x16, false);
  frame.data[7] = get_lock_target_adjusted_value(0xFE, false);
  haldex_can_send(frame, 0);
}

void Gen4_frames20() {
  twai_message_t frame = {};
  frame.identifier = BRAKES2_ID;
  frame.data_length_code = 8;
  frame.data[0] = 0x80;
  frame.data[1] = 0x7A;
  frame.data[2] = 0x05;
  frame.data[3] = BRAKES2_counter;
  frame.data[4] = get_lock_target_adjusted_value(0x7F, false);
  frame.data[5] = 0xCA;
  frame.data[6] = 0x1B;
  frame.data[7] = 0xAB;
  haldex_can_send(frame, 0);
  BRAKES2_counter = (uint8_t)(BRAKES2_counter + 16);
  if (BRAKES2_counter > 0xF0) {
    BRAKES2_counter = 0;
  }
}

void Gen4_frames25() {
  twai_message_t frame = {};
  frame.identifier = mKombi_1;
  frame.data_length_code = 8;
  frame.data[0] = 0x24;
  frame.data[1] = 0x00;
  frame.data[2] = 0x1D;
  frame.data[3] = 0xB9;
  frame.data[4] = 0x07;
  frame.data[5] = 0x42;
  frame.data[6] = 0x09;
  frame.data[7] = 0x81;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = mKombi_3;
  frame.data_length_code = 8;
  frame.data[0] = 0x60;
  frame.data[1] = 0x43;
  frame.data[2] = 0x01;
  frame.data[3] = 0x10;
  frame.data[4] = 0x66;
  frame.data[5] = 0xF1;
  frame.data[6] = 0x03;
  frame.data[7] = 0x02;
  haldex_can_send(frame, 0);
}

void Gen4_frames100() {
  twai_message_t frame = {};
  frame.identifier = mGate_Komf_1;
  frame.data_length_code = 8;
  frame.data[0] = 0x03;
  frame.data[1] = 0x11;
  frame.data[2] = 0x58;
  frame.data[3] = 0x00;
  frame.data[4] = 0x40;
  frame.data[5] = 0x00;
  frame.data[6] = 0x01;
  frame.data[7] = 0x08;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = mGate_Komf_2;
  frame.data_length_code = 8;
  frame.data[0] = 0x09;
  frame.data[1] = 0x01;
  frame.data[2] = 0x00;
  frame.data[3] = 0xA1;
  frame.data[4] = 0x00;
  frame.data[5] = 0x00;
  frame.data[6] = 0x00;
  frame.data[7] = 0x00;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = mSysteminfo_1;
  frame.data_length_code = 6;
  frame.data[0] = 0xC0;
  frame.data[1] = 0x03;
  frame.data[2] = 0x50;
  frame.data[3] = 0xBF;
  frame.data[4] = 0x37;
  frame.data[5] = 0x56;
  frame.data[6] = 0xC0;
  frame.data[7] = 0x00;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = mSoll_Verbauliste_neu;
  frame.data_length_code = 8;
  frame.data[0] = 0xF7;
  frame.data[1] = 0x42;
  frame.data[2] = 0x70;
  frame.data[3] = 0x3F;
  frame.data[4] = 0x1C;
  frame.data[5] = 0x08;
  frame.data[6] = 0x00;
  frame.data[7] = 0xC8;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = BRAKES11_ID;
  frame.data_length_code = 8;
  frame.data[0] = 0x00;
  frame.data[1] = 0xC0;
  frame.data[2] = 0x00;
  frame.data[3] = 0x00;
  frame.data[4] = 0x00;
  frame.data[5] = 0x00;
  frame.data[6] = 0x00;
  frame.data[7] = 0x00;
  haldex_can_send(frame, 0);
}

void Gen4_frames200() {
  twai_message_t frame = {};
  frame.identifier = mKombi_2;
  frame.data_length_code = 8;
  frame.data[0] = 0x4C;
  frame.data[1] = 0x86;
  frame.data[2] = 0x85;
  frame.data[3] = 0x00;
  frame.data[4] = 0x00;
  frame.data[5] = 0x30;
  frame.data[6] = 0xFF;
  frame.data[7] = 0x04;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = mKombi_3;
  frame.data_length_code = 8;
  frame.data[0] = 0xA6;
  frame.data[1] = 0x87;
  frame.data[2] = 0x01;
  frame.data[3] = 0x10;
  frame.data[4] = 0x66;
  frame.data[5] = 0xF2;
  frame.data[6] = 0x03;
  frame.data[7] = 0x02;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = NMH_Gateway;
  frame.data_length_code = 7;
  frame.data[0] = 0x04;
  frame.data[1] = 0x03;
  frame.data[2] = 0x01;
  frame.data[3] = 0x00;
  frame.data[4] = 0x02;
  frame.data[5] = 0x00;
  frame.data[6] = 0x00;
  haldex_can_send(frame, 0);
}

void Gen4_frames1000() {
  twai_message_t frame = {};
  frame.identifier = mDiagnose_1;
  frame.data_length_code = 8;
  frame.data[0] = 0x26;
  frame.data[1] = 0xF2;
  frame.data[2] = 0x03;
  frame.data[3] = 0x12;
  frame.data[4] = 0x70;
  frame.data[5] = 0x19;
  frame.data[6] = 0x25;
  frame.data[7] = mDiagnose_1_counter;
  haldex_can_send(frame, 0);
  mDiagnose_1_counter++;
  if (mDiagnose_1_counter > 0x1F) {
    mDiagnose_1_counter = 0;
  }
}

void Gen5_frames10() {
  twai_message_t frame = {};
  frame.identifier = ESP_18;
  frame.data_length_code = 8;
  frame.data[1] = 0xC0;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = ESP_19;
  frame.data_length_code = 8;
  frame.data[0] = get_lock_target_adjusted_value(ESP_19_counter2, false);
  frame.data[1] = get_lock_target_adjusted_value(ESP_19_counter, false);
  frame.data[2] = get_lock_target_adjusted_value(ESP_19_counter2, false);
  frame.data[3] = get_lock_target_adjusted_value(ESP_19_counter, false);
  frame.data[4] = get_lock_target_adjusted_value((uint8_t)(ESP_19_counter2 + 0xCA), false);
  frame.data[5] = get_lock_target_adjusted_value(ESP_19_counter, false);
  frame.data[6] = get_lock_target_adjusted_value((uint8_t)(ESP_19_counter2 + 0xCA), false);
  frame.data[7] = get_lock_target_adjusted_value(ESP_19_counter, false);
  haldex_can_send(frame, 0);
  ESP_19_counter++;
  ESP_19_counter2++;
  if (ESP_19_counter > 0x1A) {
    ESP_19_counter = 0x01;
  }
  if (ESP_19_counter2 > 0x0E) {
    ESP_19_counter2 = 0x00;
  }

  frame = {};
  frame.identifier = GETRIEBE_11;
  frame.data_length_code = 8;
  frame.data[1] = GETRIEBE_11_counter;
  frame.data[3] = 0xFE;
  frame.data[0] = calcChecksum(frame.data, ID_SEQ_0AD);
  GETRIEBE_11_counter++;
  if (GETRIEBE_11_counter > 0x0F) {
    GETRIEBE_11_counter = 0;
  }
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = MOTOR_12;
  frame.data_length_code = 8;
  frame.data[1] = MOTOR_12_counter;
  frame.data[5] = 0x64;
  frame.data[6] = 0x0F;
  frame.data[7] = get_lock_target_adjusted_value(MOTOR_12_counter, false);
  frame.data[0] = calcChecksum(frame.data, ID_SEQ_0A8);
  MOTOR_12_counter++;
  if (MOTOR_12_counter > 0x7F) {
    MOTOR_12_counter = 0x70;
  }
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = MOTOR_11;
  frame.data_length_code = 8;
  frame.data[1] = MOTOR_11_counter;
  frame.data[2] = 0xFA;
  frame.data[3] = 0xFA;
  frame.data[5] = 0xFA;
  frame.data[6] = get_lock_target_adjusted_value(0xFA, false);
  frame.data[7] = get_lock_target_adjusted_value(0xFA, false);
  frame.data[0] = calcChecksum(frame.data, ID_SEQ_0A7);
  MOTOR_11_counter++;
  if (MOTOR_11_counter > 0x4F) {
    MOTOR_11_counter = 0x40;
  }
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = ESP_14;
  frame.data_length_code = 8;
  frame.data[1] = ESP_14_counter;
  frame.data[7] = get_lock_target_adjusted_value(0xFE, false);
  frame.data[0] = calcChecksum(frame.data, ID_SEQ_08A);
  ESP_14_counter++;
  if (ESP_14_counter > 0x1F) {
    ESP_14_counter = 0x10;
  }
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = LWI_01;
  frame.data_length_code = 8;
  frame.data[1] = LWI_01_counter;
  frame.data[2] = 0x01;
  frame.data[0] = calcChecksum(frame.data, ID_SEQ_086);
  LWI_01_counter++;
  if (LWI_01_counter > 0x1F) {
    LWI_01_counter = 0x10;
  }
  haldex_can_send(frame, 0);
}

void Gen5_frames20() {
  twai_message_t frame = {};
  frame.identifier = MOTOR_20;
  frame.data_length_code = 8;
  frame.data[1] = MOTOR_20_counter;
  frame.data[2] = 0x40;
  frame.data[3] = 0x40;
  frame.data[4] = 0x19;
  frame.data[5] = 0x59;
  frame.data[6] = 0x7E;
  frame.data[7] = 0xFE;
  frame.data[0] = calcChecksum(frame.data, ID_SEQ_121);
  MOTOR_20_counter++;
  if (MOTOR_20_counter > 0x0F) {
    MOTOR_20_counter = 0x00;
  }
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = ESP_10;
  frame.data_length_code = 8;
  frame.data[1] = ESP_10_counter;
  frame.data[2] = 0x01;
  frame.data[3] = 0x04;
  frame.data[5] = 0x40;
  frame.data[7] = get_lock_target_adjusted_value(ESP_10_counter, false);
  frame.data[0] = calcChecksum(frame.data, ID_SEQ_116);
  ESP_10_counter++;
  if (ESP_10_counter > 0x0F) {
    ESP_10_counter = 0x00;
  }
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = ESP_05;
  frame.data_length_code = 8;
  frame.data[1] = ESP_05_counter;
  frame.data[2] = 0x64;
  frame.data[3] = 0xC0;
  frame.data[6] = 0xFD;
  frame.data[0] = calcChecksum(frame.data, ID_SEQ_106);
  ESP_05_counter++;
  if (ESP_05_counter > 0x8F) {
    ESP_05_counter = 0x80;
  }
  haldex_can_send(frame, 0);
}

void Gen5_frames25() {
  twai_message_t frame = {};
  frame.identifier = KOMBI_01;
  frame.data_length_code = 8;
  frame.data[0] = 0x10;
  frame.data[1] = 0x20;
  frame.data[2] = 0x02;
  frame.data[4] = 0x0C;
  frame.data[7] = 0x24;
  haldex_can_send(frame, 0);
}

void Gen5_frames100() {
  twai_message_t frame = {};
  frame.identifier = ESP_23;
  frame.data_length_code = 8;
  frame.data[1] = ESP_23_counter;
  frame.data[2] = 0xBF;
  frame.data[3] = 0x7F;
  frame.data[6] = 0x7C;
  frame.data[7] = 0x78;
  frame.data[0] = calcChecksum(frame.data, ID_SEQ_5BE);
  ESP_23_counter++;
  if (ESP_23_counter > 0x1F) {
    ESP_23_counter = 0x00;
  }
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = Parkhilfe_04;
  frame.data_length_code = 8;
  frame.data[7] = 0x24;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = GATEWAY_72;
  frame.data_length_code = 8;
  frame.data[0] = 0x50;
  frame.data[1] = 0x80;
  frame.data[4] = 0x05;
  frame.data[5] = 0x10;
  frame.data[6] = 0x01;
  frame.data[7] = 0x78;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = GETRIEBE_14;
  frame.data_length_code = 8;
  frame.data[2] = 0x54;
  frame.data[3] = 0x24;
  frame.data[5] = 0x60;
  frame.data[6] = 0x01;
  frame.data[7] = 0x51;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = MOTOR_14;
  frame.data_length_code = 8;
  frame.data[1] = MOTOR_14_counter;
  frame.data[2] = 0xE6;
  frame.data[3] = 0x01;
  frame.data[4] = 0xC8;
  frame.data[5] = 0x80;
  frame.data[7] = 0x80;
  frame.data[0] = calcChecksum(frame.data, ID_SEQ_3BE);
  MOTOR_14_counter++;
  if (MOTOR_14_counter > 0x1F) {
    MOTOR_14_counter = 0x10;
  }
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = ESP_07;
  frame.data_length_code = 8;
  frame.data[1] = ESP_07_counter;
  frame.data[0] = calcChecksum(frame.data, ID_SEQ_392);
  ESP_07_counter++;
  if (ESP_07_counter > 0x1F) {
    ESP_07_counter = 0x00;
  }
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = ESP_29;
  frame.data_length_code = 8;
  frame.data[1] = 0x20;
  frame.data[2] = 0x59;
  haldex_can_send(frame, 0);
}

void Gen5_frames200() {
  // Legacy C6 Gen5 standalone code kept the candidate 200 ms frame disabled.
}

void Gen5_frames1000() {
  twai_message_t frame = {};
  frame.identifier = MOTOR_07;
  frame.data_length_code = 8;
  frame.data[0] = 0xA0;
  frame.data[1] = 0x5A;
  frame.data[2] = 0x56;
  frame.data[3] = 0xA3;
  frame.data[4] = 0x80;
  frame.data[5] = 0xA0;
  frame.data[6] = 0x59;
  frame.data[7] = 0x01;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = CHARISMA_01;
  frame.data_length_code = 8;
  frame.data[2] = 0x22;
  frame.data[3] = 0x02;
  frame.data[4] = 0x02;
  frame.data[5] = 0x20;
  frame.data[6] = 0x02;
  frame.data[7] = 0x02;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = SYSTEMINFO_01;
  frame.data_length_code = 8;
  frame.data[0] = 0x84;
  frame.data[1] = 0x3C;
  frame.data[3] = 0x7F;
  frame.data[4] = 0x14;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = MOTOR_CODE_01;
  frame.data_length_code = 8;
  frame.data[1] = MOTOR_CODE_01_counter;
  frame.data[2] = 0x2B;
  frame.data[3] = 0x53;
  frame.data[4] = 0x14;
  frame.data[5] = 0x14;
  frame.data[6] = 0xD7;
  frame.data[7] = 0x24;
  frame.data[0] = calcChecksum(frame.data, ID_SEQ_641);
  MOTOR_CODE_01_counter++;
  if (MOTOR_CODE_01_counter > 0x1F) {
    MOTOR_CODE_01_counter = 0x10;
  }
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = ESP_20;
  frame.data_length_code = 8;
  frame.data[1] = ESP_20_counter;
  frame.data[2] = 0x2B;
  frame.data[3] = 0x10;
  frame.data[6] = 0xE2;
  frame.data[7] = 0x79;
  frame.data[0] = calcChecksum(frame.data, ID_SEQ_65D);
  ESP_20_counter++;
  if (ESP_20_counter > 0x3F) {
    ESP_20_counter = 0x30;
  }
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = DIAGNOSE_01;
  frame.data_length_code = 8;
  frame.data[0] = 0x30;
  frame.data[1] = 0x4D;
  frame.data[2] = 0x58;
  frame.data[3] = 0xA2;
  frame.data[4] = 0x89;
  frame.data[5] = 0x85;
  frame.data[6] = 0x3F;
  frame.data[7] = 0x30;
  haldex_can_send(frame, 0);

  frame = {};
  frame.identifier = KOMBI_02;
  frame.data_length_code = 8;
  frame.data[0] = 0x4D;
  frame.data[1] = 0x58;
  frame.data[2] = 0xF2;
  frame.data[3] = 0xEE;
  frame.data[4] = 0x04;
  frame.data[5] = 0x2B;
  frame.data[7] = 0x78;
  haldex_can_send(frame, 0);
}
