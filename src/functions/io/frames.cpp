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
