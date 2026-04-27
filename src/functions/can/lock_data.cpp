#include "functions/core/calcs.h"

#include <Arduino.h>

#include "functions/core/state.h"
#include "functions/can/can_id.h"
#include "functions/can/standalone_can.h"

// Core frame mutation entry point used when controller is enabled.
// Input frame is chassis-origin traffic and may be modified in-place
// depending on Haldex generation and active mode.
void getLockData(twai_message_t& rx_message_chs) {
  // Requested lock value (0..100) computed from mode thresholds/map.
  lock_target = get_lock_target_adjustment();
  awd_state.requested = lock_target;

  // Gen1 strategy: overwrite key motor/brake signals that the AWD ECU expects.
  if (haldexGeneration == 1) {
    switch (rx_message_chs.identifier) {
    case MOTOR1_ID:
      rx_message_chs.data[0] = 0x00; // various individual bits ('space gas', driving pedal, kick down, clutch, timeout
                                     // brake, brake intervention, drinks-torque intervention?) was 0x01 - ignored
      rx_message_chs.data[1] = get_lock_target_adjusted_value(0xFE, false); // rpm low byte
      rx_message_chs.data[2] = 0x21;                                        // rpm high byte
      rx_message_chs.data[3] =
        get_lock_target_adjusted_value(0x4E, false); // set RPM to a value so the pre-charge pump runs
      rx_message_chs.data[4] =
        get_lock_target_adjusted_value(0xFE, false); // inner moment (%): 0.39*(0xF0) = 93.6%  (make FE?) - ignored
      rx_message_chs.data[5] =
        get_lock_target_adjusted_value(0xFE, false); // driving pedal (%): 0.39*(0xF0) = 93.6%  (make FE?) - ignored
      appliedTorque = rx_message_chs.data[6];
      // rx_message_chs.data[6] = get_lock_target_adjusted_value(0x16, false);  // set to a low value to control the
      // req. transfer torque.  Main control value for Gen1
      switch (openhaldexEffectiveMode()) {
      case MODE_FWD:
        appliedTorque = get_lock_target_adjusted_value(0xFE, true); // return 0xFE to disable
        break;
      case MODE_5050:
        appliedTorque = get_lock_target_adjusted_value(0x16, false); // return 0x16 to fully lock
        break;
      case MODE_6040:
        appliedTorque = get_lock_target_adjusted_value(0x22, false); // set to ~30% lock (0x96 = 15%, 0x56 = 27%)
        break;
      case MODE_7030:
        appliedTorque = get_lock_target_adjusted_value(0x50, false); // set to ~30% lock
        break;
      case MODE_8020:
        appliedTorque = get_lock_target_adjusted_value(0x50, false); // set to ~20% lock
        break;
      case MODE_9010:
        appliedTorque = get_lock_target_adjusted_value(0x50, false); // set to ~10% lock
        break;
      default:
        break;
      }

      rx_message_chs.data[6] = appliedTorque; // was 0x00
      rx_message_chs.data[7] = 0x00;          // these must play a factor - achieves ~169 without
      break;
    case MOTOR3_ID:
      rx_message_chs.data[2] = get_lock_target_adjusted_value(0xFE, false); // pedal - ignored
      rx_message_chs.data[7] = get_lock_target_adjusted_value(0xFE, false); // throttle angle (100%), ignored
      break;
    case BRAKES1_ID:
      rx_message_chs.data[1] =
        get_lock_target_adjusted_value(0x00, false); // also controlling slippage.  Brake force can add 20%
      rx_message_chs.data[2] = 0x00;                 //  ignored
      rx_message_chs.data[3] = get_lock_target_adjusted_value(0x0A, false); // 0xA ignored?
      break;
    case BRAKES3_ID:
      rx_message_chs.data[0] =
        get_lock_target_adjusted_value(0xFE, false); // low byte, LEFT Front // affects slightly +2
      rx_message_chs.data[1] = 0x0A;                 // high byte, LEFT Front big effect
      rx_message_chs.data[2] =
        get_lock_target_adjusted_value(0xFE, false); // low byte, RIGHT Front// affects slightly +2
      rx_message_chs.data[3] = 0x0A;                 // high byte, RIGHT Front big effect
      rx_message_chs.data[4] = 0x00;                 // low byte, LEFT Rear
      rx_message_chs.data[5] = 0x0A;                 // high byte, LEFT Rear // 254+10? (5050 returns 0xA)
      rx_message_chs.data[6] = 0x00;                 // low byte, RIGHT Rear
      rx_message_chs.data[7] = 0x0A;                 // high byte, RIGHT Rear  // 254+10?
      break;
    }
  }

  // Gen2 strategy: currently mirrors Gen4 baseline with byte-level differences noted.
  if (haldexGeneration == 2) { // Gen2 baseline currently mirrors Gen4 with noted byte differences.
    switch (rx_message_chs.identifier) {
    case MOTOR1_ID:
      rx_message_chs.data[1] = get_lock_target_adjusted_value(0xFE, false);
      rx_message_chs.data[2] = 0x21;
      rx_message_chs.data[3] = get_lock_target_adjusted_value(0x4E, false);
      rx_message_chs.data[6] = get_lock_target_adjusted_value(0xFE, false);
      break;
    case MOTOR3_ID:
      rx_message_chs.data[2] = get_lock_target_adjusted_value(0xFE, false);
      rx_message_chs.data[7] = get_lock_target_adjusted_value(0x01, false); // gen1 is 0xFE, gen4 is 0x01
      break;
    case MOTOR6_ID:
      break;
    case BRAKES1_ID:
      rx_message_chs.data[0] = get_lock_target_adjusted_value(0x80, false);
      rx_message_chs.data[1] = get_lock_target_adjusted_value(0x41, false);
      rx_message_chs.data[2] = get_lock_target_adjusted_value(0xFE, false); // gen1 is 0x00, gen4 is 0xFE
      rx_message_chs.data[3] = 0x0A;
      break;
    case BRAKES2_ID:
      rx_message_chs.data[4] = get_lock_target_adjusted_value(0x7F, false); // big affect(!) 0x7F is max
      rx_message_chs.data[5] = get_lock_target_adjusted_value(0xFE, false); // no effect.  Was 0x6E
      break;
    case BRAKES3_ID:
      rx_message_chs.data[0] = get_lock_target_adjusted_value(0xFE, false);
      rx_message_chs.data[1] = 0x0A;
      rx_message_chs.data[2] = get_lock_target_adjusted_value(0xFE, false);
      rx_message_chs.data[3] = 0x0A;
      rx_message_chs.data[4] = 0x00;
      rx_message_chs.data[5] = 0x0A;
      rx_message_chs.data[6] = 0x00;
      rx_message_chs.data[7] = 0x0A;
      break;
    }
  }
  // Gen4 strategy: full byte-level shaping across steering, motor, and brake frames.
  if (haldexGeneration == 4) {
    switch (rx_message_chs.identifier) {
    case mLW_1:
      rx_message_chs.data[0] = lws_2[mLW_1_counter][0]; // angle of turn (block 011) low byte
      rx_message_chs.data[1] = lws_2[mLW_1_counter][1]; // no effect B high byte
      rx_message_chs.data[2] = lws_2[mLW_1_counter][2]; // no effect C
      rx_message_chs.data[3] = lws_2[mLW_1_counter][3]; // no effect D
      rx_message_chs.data[4] = lws_2[mLW_1_counter][4]; // rate of change (block 010) was 0x00
      rx_message_chs.data[5] = lws_2[mLW_1_counter][5]; // no effect F
      rx_message_chs.data[6] = lws_2[mLW_1_counter][6]; // no effect F
      rx_message_chs.data[7] = lws_2[mLW_1_counter][7]; // no effect F
      mLW_1_counter++;
      if (mLW_1_counter > 15) {
        mLW_1_counter = 0;
      }
      break;
    case MOTOR1_ID:
      rx_message_chs.data[1] = get_lock_target_adjusted_value(0xFE, false); // has effect
      rx_message_chs.data[2] = get_lock_target_adjusted_value(0x20, false); // RPM low byte no effect was 0x20
      rx_message_chs.data[3] = get_lock_target_adjusted_value(
        0x4E, false); // RPM high byte.  Will disable pre-charge pump if 0x00.  Sets raw = 8, coupling open
      rx_message_chs.data[4] = get_lock_target_adjusted_value(0xFE, false); // MDNORM no effect
      rx_message_chs.data[5] = get_lock_target_adjusted_value(0xFE, false); // Pedal no effect
      rx_message_chs.data[6] = get_lock_target_adjusted_value(0x16, false); // idle adaptation?  Was slippage?
      rx_message_chs.data[7] = get_lock_target_adjusted_value(0xFE, false); // Fahrerwunschmoment req. torque?
      break;
    case MOTOR3_ID:
      // frame.data[2] = get_lock_target_adjusted_value(0xFE, false);
      // frame.data[7] = get_lock_target_adjusted_value(0x01, false);
      break;
    case BRAKES1_ID:
      rx_message_chs.data[0] = 0x20; // ASR 0x04 sets bit 4.  0x08 removes set.  Coupling open/closed
      rx_message_chs.data[1] = 0x40; // can use to disable (>130 dec).  Was 0x00; 0x41?  0x43?
      rx_message_chs.data[4] = get_lock_target_adjusted_value(0xFE, false); // was 0xFE miasrl no effect
      rx_message_chs.data[5] = get_lock_target_adjusted_value(0xFE, false); // was 0xFE miasrs no effect
      break;
    case BRAKES2_ID:
      rx_message_chs.data[4] = get_lock_target_adjusted_value(0x7F, false); // big affect(!) 0x7F is max
      break;
    case BRAKES3_ID:
      rx_message_chs.data[0] = get_lock_target_adjusted_value(0xB6, false); // front left low
      rx_message_chs.data[1] = 0x07;                                        // front left high
      rx_message_chs.data[2] = get_lock_target_adjusted_value(0xCC, false); // front right low
      rx_message_chs.data[3] = 0x07;                                        // front right high
      rx_message_chs.data[4] = get_lock_target_adjusted_value(0xD2, false); // rear left low
      rx_message_chs.data[5] = 0x07;                                        // rear left high
      rx_message_chs.data[6] = get_lock_target_adjusted_value(0xD2, false); // rear right low
      rx_message_chs.data[7] = 0x07;                                        // rear right high
      break;

    case BRAKES4_ID:
      rx_message_chs.data[0] =
        get_lock_target_adjusted_value(0xFE, false); // affects estimated torque AND vehicle mode(!)
      rx_message_chs.data[1] = 0x00;                 //
      rx_message_chs.data[2] = 0x00;                 //
      rx_message_chs.data[3] = 0x64;                 // 32605
      rx_message_chs.data[4] = 0x00;                 //
      rx_message_chs.data[5] = 0x00;                 //
      rx_message_chs.data[6] = BRAKES4_counter;      // checksum
      BRAKES4_crc = 0;
      for (uint8_t i = 0; i < 7; i++) {
        BRAKES4_crc ^= rx_message_chs.data[i];
      }
      rx_message_chs.data[7] = BRAKES4_crc;

      BRAKES4_counter = BRAKES4_counter + 16;
      if (BRAKES4_counter > 0xF0) {
        BRAKES4_counter = 0x00;
      }
      break;
    }
  }

  // Gen5 strategy: MQB byte shaping plus AUTOSAR checksum/counter maintenance.
  if (haldexGeneration == 5) {
    switch (rx_message_chs.identifier) {
    case ESP_19:
      rx_message_chs.data[0] = get_lock_target_adjusted_value(ESP_19_counter2, false);
      rx_message_chs.data[1] = get_lock_target_adjusted_value(ESP_19_counter, false);
      rx_message_chs.data[2] = get_lock_target_adjusted_value(ESP_19_counter2, false);
      rx_message_chs.data[3] = get_lock_target_adjusted_value(ESP_19_counter, false);
      rx_message_chs.data[4] = get_lock_target_adjusted_value((uint8_t)(ESP_19_counter2 + 0xCA), false);
      rx_message_chs.data[5] = get_lock_target_adjusted_value(ESP_19_counter, false);
      rx_message_chs.data[6] = get_lock_target_adjusted_value((uint8_t)(ESP_19_counter2 + 0xCA), false);
      rx_message_chs.data[7] = get_lock_target_adjusted_value(ESP_19_counter, false);
      ESP_19_counter++;
      ESP_19_counter2++;
      if (ESP_19_counter > 0x1A) {
        ESP_19_counter = 0x01;
      }
      if (ESP_19_counter2 > 0x0E) {
        ESP_19_counter2 = 0x00;
      }
      break;

    case MOTOR_12:
      rx_message_chs.data[0] = 0x00;
      rx_message_chs.data[1] = MOTOR_12_counter;
      rx_message_chs.data[2] = 0x00;
      rx_message_chs.data[3] = 0x00;
      rx_message_chs.data[4] = 0x00;
      rx_message_chs.data[5] = 0x64;
      rx_message_chs.data[6] = 0x0F;
      rx_message_chs.data[7] = get_lock_target_adjusted_value(MOTOR_12_counter, false);
      rx_message_chs.data[0] = calcChecksum(rx_message_chs.data, ID_SEQ_0A8);
      MOTOR_12_counter++;
      if (MOTOR_12_counter > 0x7F) {
        MOTOR_12_counter = 0x70;
      }
      break;

    case MOTOR_11:
      rx_message_chs.data[0] = 0x00;
      rx_message_chs.data[1] = MOTOR_11_counter;
      rx_message_chs.data[2] = 0xFA;
      rx_message_chs.data[3] = 0xFA;
      rx_message_chs.data[4] = 0x00;
      rx_message_chs.data[5] = 0xFA;
      rx_message_chs.data[6] = get_lock_target_adjusted_value(0xFA, false);
      rx_message_chs.data[7] = get_lock_target_adjusted_value(0xFA, false);
      rx_message_chs.data[0] = calcChecksum(rx_message_chs.data, ID_SEQ_0A7);
      MOTOR_11_counter++;
      if (MOTOR_11_counter > 0x4F) {
        MOTOR_11_counter = 0x40;
      }
      break;

    case ESP_14:
      rx_message_chs.data[0] = 0x00;
      rx_message_chs.data[1] = ESP_14_counter;
      rx_message_chs.data[2] = 0x00;
      rx_message_chs.data[3] = 0x00;
      rx_message_chs.data[4] = 0x00;
      rx_message_chs.data[5] = 0x00;
      rx_message_chs.data[6] = 0x00;
      rx_message_chs.data[7] = get_lock_target_adjusted_value(0xFE, false);
      rx_message_chs.data[0] = calcChecksum(rx_message_chs.data, ID_SEQ_08A);
      ESP_14_counter++;
      if (ESP_14_counter > 0x1F) {
        ESP_14_counter = 0x10;
      }
      break;

    case ESP_10:
      rx_message_chs.data_length_code = 8;
      rx_message_chs.data[0] = 0x00;
      rx_message_chs.data[1] = ESP_10_counter;
      rx_message_chs.data[2] = 0x01;
      rx_message_chs.data[3] = 0x04;
      rx_message_chs.data[4] = 0x00;
      rx_message_chs.data[5] = 0x40;
      rx_message_chs.data[6] = 0x00;
      rx_message_chs.data[7] = get_lock_target_adjusted_value(ESP_10_counter, false);
      rx_message_chs.data[0] = calcChecksum(rx_message_chs.data, ID_SEQ_116);
      ESP_10_counter++;
      if (ESP_10_counter > 0x0F) {
        ESP_10_counter = 0x00;
      }
      break;

    case ESP_05:
      rx_message_chs.data_length_code = 8;
      rx_message_chs.data[0] = 0x00;
      rx_message_chs.data[1] = ESP_05_counter;
      rx_message_chs.data[2] = 0x64;
      rx_message_chs.data[3] = 0xC0;
      rx_message_chs.data[4] = 0x00;
      rx_message_chs.data[5] = 0x00;
      rx_message_chs.data[6] = 0xFD;
      rx_message_chs.data[7] = 0x00;
      rx_message_chs.data[0] = calcChecksum(rx_message_chs.data, ID_SEQ_106);
      ESP_05_counter++;
      if (ESP_05_counter > 0x8F) {
        ESP_05_counter = 0x80;
      }
      break;

    case KOMBI_01:
      rx_message_chs.data[0] = 0x10;
      rx_message_chs.data[1] = 0x20;
      rx_message_chs.data[2] = 0x02;
      rx_message_chs.data[3] = 0x00;
      rx_message_chs.data[4] = 0x0C;
      rx_message_chs.data[5] = 0x00;
      rx_message_chs.data[6] = 0x00;
      rx_message_chs.data[7] = 0x24;
      break;

    case ESP_23:
      rx_message_chs.data[0] = 0x00;
      rx_message_chs.data[1] = ESP_23_counter;
      rx_message_chs.data[2] = 0xBF;
      rx_message_chs.data[3] = 0x7F;
      rx_message_chs.data[4] = 0x00;
      rx_message_chs.data[5] = 0x00;
      rx_message_chs.data[6] = 0x7C;
      rx_message_chs.data[7] = 0x78;
      rx_message_chs.data[0] = calcChecksum(rx_message_chs.data, ID_SEQ_5BE);
      ESP_23_counter++;
      if (ESP_23_counter > 0x1F) {
        ESP_23_counter = 0x00;
      }
      break;

    case Parkhilfe_04:
      rx_message_chs.data[0] = 0x00;
      rx_message_chs.data[1] = 0x00;
      rx_message_chs.data[2] = 0x00;
      rx_message_chs.data[3] = 0x00;
      rx_message_chs.data[4] = 0x00;
      rx_message_chs.data[5] = 0x00;
      rx_message_chs.data[6] = 0x00;
      rx_message_chs.data[7] = 0x24;
      break;

    case GATEWAY_72:
      rx_message_chs.data[0] = 0x50;
      rx_message_chs.data[1] = 0x80;
      rx_message_chs.data[2] = 0x00;
      rx_message_chs.data[3] = 0x00;
      rx_message_chs.data[4] = 0x05;
      rx_message_chs.data[5] = 0x10;
      rx_message_chs.data[6] = 0x01;
      rx_message_chs.data[7] = 0x78;
      break;

    case GETRIEBE_14:
      rx_message_chs.data[0] = 0x00;
      rx_message_chs.data[1] = 0x00;
      rx_message_chs.data[2] = 0x54;
      rx_message_chs.data[3] = 0x24;
      rx_message_chs.data[4] = 0x00;
      rx_message_chs.data[5] = 0x60;
      rx_message_chs.data[6] = 0x01;
      rx_message_chs.data[7] = 0x51;
      break;

    case MOTOR_14:
      rx_message_chs.data[0] = 0x00;
      rx_message_chs.data[1] = MOTOR_14_counter;
      rx_message_chs.data[2] = 0xE6;
      rx_message_chs.data[3] = 0x01;
      rx_message_chs.data[4] = 0xC8;
      rx_message_chs.data[5] = 0x80;
      rx_message_chs.data[6] = 0x00;
      rx_message_chs.data[7] = 0x80;
      rx_message_chs.data[0] = calcChecksum(rx_message_chs.data, ID_SEQ_3BE);
      MOTOR_14_counter++;
      if (MOTOR_14_counter > 0x1F) {
        MOTOR_14_counter = 0x10;
      }
      break;

    case ESP_07:
      rx_message_chs.data[0] = 0x00;
      rx_message_chs.data[1] = ESP_07_counter;
      rx_message_chs.data[2] = 0x00;
      rx_message_chs.data[3] = 0x00;
      rx_message_chs.data[4] = 0x00;
      rx_message_chs.data[5] = 0x00;
      rx_message_chs.data[6] = 0x00;
      rx_message_chs.data[7] = 0x00;
      rx_message_chs.data[0] = calcChecksum(rx_message_chs.data, ID_SEQ_392);
      ESP_07_counter++;
      if (ESP_07_counter > 0x1F) {
        ESP_07_counter = 0x00;
      }
      break;

    case ESP_29:
      rx_message_chs.data[0] = 0x00;
      rx_message_chs.data[1] = 0x20;
      rx_message_chs.data[2] = 0x59;
      rx_message_chs.data[3] = 0x00;
      rx_message_chs.data[4] = 0x00;
      rx_message_chs.data[5] = 0x00;
      rx_message_chs.data[6] = 0x00;
      rx_message_chs.data[7] = 0x00;
      break;

    case MOTOR_07:
      rx_message_chs.data[0] = 0xA0;
      rx_message_chs.data[1] = 0x5A;
      rx_message_chs.data[2] = 0x56;
      rx_message_chs.data[3] = 0xA3;
      rx_message_chs.data[4] = 0x80;
      rx_message_chs.data[5] = 0xA0;
      rx_message_chs.data[6] = 0x59;
      rx_message_chs.data[7] = 0x01;
      break;

    case CHARISMA_01:
      rx_message_chs.data[0] = 0x00;
      rx_message_chs.data[1] = 0x00;
      rx_message_chs.data[2] = 0x22;
      rx_message_chs.data[3] = 0x02;
      rx_message_chs.data[4] = 0x02;
      rx_message_chs.data[5] = 0x20;
      rx_message_chs.data[6] = 0x02;
      rx_message_chs.data[7] = 0x02;
      break;

    case SYSTEMINFO_01:
      rx_message_chs.data[0] = 0x84;
      rx_message_chs.data[1] = 0x3C;
      rx_message_chs.data[2] = 0x00;
      rx_message_chs.data[3] = 0x7F;
      rx_message_chs.data[4] = 0x14;
      rx_message_chs.data[5] = 0x00;
      rx_message_chs.data[6] = 0x00;
      rx_message_chs.data[7] = 0x00;
      break;

    case MOTOR_CODE_01:
      rx_message_chs.data[0] = 0x00;
      rx_message_chs.data[1] = MOTOR_CODE_01_counter;
      rx_message_chs.data[2] = 0x2B;
      rx_message_chs.data[3] = 0x53;
      rx_message_chs.data[4] = 0x14;
      rx_message_chs.data[5] = 0x14;
      rx_message_chs.data[6] = 0xD7;
      rx_message_chs.data[7] = 0x24;
      rx_message_chs.data[0] = calcChecksum(rx_message_chs.data, ID_SEQ_641);
      MOTOR_CODE_01_counter++;
      if (MOTOR_CODE_01_counter > 0x1F) {
        MOTOR_CODE_01_counter = 0x10;
      }
      break;

    case ESP_20:
      rx_message_chs.data[0] = 0x00;
      rx_message_chs.data[1] = ESP_20_counter;
      rx_message_chs.data[2] = 0x2B;
      rx_message_chs.data[3] = 0x10;
      rx_message_chs.data[4] = 0x00;
      rx_message_chs.data[5] = 0x00;
      rx_message_chs.data[6] = 0xE2;
      rx_message_chs.data[7] = 0x79;
      rx_message_chs.data[0] = calcChecksum(rx_message_chs.data, ID_SEQ_65D);
      ESP_20_counter++;
      if (ESP_20_counter > 0x3F) {
        ESP_20_counter = 0x30;
      }
      break;

    case DIAGNOSE_01:
      rx_message_chs.data[0] = 0x30;
      rx_message_chs.data[1] = 0x4D;
      rx_message_chs.data[2] = 0x58;
      rx_message_chs.data[3] = 0xA2;
      rx_message_chs.data[4] = 0x89;
      rx_message_chs.data[5] = 0x85;
      rx_message_chs.data[6] = 0x3F;
      rx_message_chs.data[7] = 0x30;
      break;

    case KOMBI_02:
      rx_message_chs.data[0] = 0x4D;
      rx_message_chs.data[1] = 0x58;
      rx_message_chs.data[2] = 0xF2;
      rx_message_chs.data[3] = 0xEE;
      rx_message_chs.data[4] = 0x04;
      rx_message_chs.data[5] = 0x2B;
      rx_message_chs.data[6] = 0x00;
      rx_message_chs.data[7] = 0x78;
      break;
    }
  }
}
