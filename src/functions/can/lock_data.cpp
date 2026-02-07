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
      switch (state.mode) {
      case MODE_FWD:
        appliedTorque = get_lock_target_adjusted_value(0xFE, true); // return 0xFE to disable
        break;
      case MODE_5050:
        appliedTorque = get_lock_target_adjusted_value(0x16, false); // return 0x16 to fully lock
        break;
      case MODE_6040:
        appliedTorque = get_lock_target_adjusted_value(0x22, false); // set to ~30% lock (0x96 = 15%, 0x56 = 27%)
        break;
      case MODE_7525:
        appliedTorque = get_lock_target_adjusted_value(0x50, false); // set to ~30% lock (0x96 = 15%, 0x56 = 27%)
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
}
