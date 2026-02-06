#include <cstring>
#include "functions/can/can.h"

#include "functions/config/pins.h"
#include "functions/config/config.h"
#include "functions/core/state.h"
#include "functions/core/calcs.h"
#include "functions/can/can_id.h"
#include "functions/canview/canview.h"
#include "functions/can/can_state.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static bool can_messages_equal(const twai_message_t& a, const twai_message_t& b) {
  if (a.identifier != b.identifier)
    return false;
  if (a.extd != b.extd)
    return false;
  if (a.rtr != b.rtr)
    return false;
  if (a.data_length_code != b.data_length_code)
    return false;
  for (uint8_t i = 0; i < a.data_length_code && i < 8; i++) {
    if (a.data[i] != b.data[i])
      return false;
  }
  return true;
}

void parseCAN_chs(void* arg) {
  // FreeRTOS task: Process chassis CAN bus messages
  // Extracts telemetry (speed, RPM, throttle) and forwards diagnostics to Haldex bus in standalone mode

  while (1) {
#if detailedDebugStack
    stackCHS = uxTaskGetStackHighWaterMark(NULL);
#endif
    while (chassis_can_receive(rx_msg_chs())) {
      lastCANChassisTick = millis();
      canviewCacheFrame(rx_msg_chs(), 0);

      tx_msg_hdx().identifier = rx_msg_chs().identifier;

      if (isGen1Standalone || isGen2Standalone || isGen4Standalone) {
        switch (rx_msg_chs().identifier) {
        case diagnostics_1_ID:
        case diagnostics_2_ID:
        case diagnostics_3_ID:
        case diagnostics_4_ID:
        case diagnostics_5_ID:
          // Forward diagnostic messages to Haldex CAN bus
          tx_msg_hdx() = rx_msg_chs();
          tx_msg_hdx().extd = rx_msg_chs().extd;
          tx_msg_hdx().rtr = rx_msg_chs().rtr;
          tx_msg_hdx().data_length_code = rx_msg_chs().data_length_code;
          haldex_can_send(tx_msg_hdx(), (10 / portTICK_PERIOD_MS), false);
          break;
        }
      }

      if (!isGen1Standalone && !isGen2Standalone && !isGen4Standalone) {
        switch (rx_msg_chs().identifier) {
        case MOTOR1_ID:
          received_pedal_value = rx_msg_chs().data[5] * 0.4f;
          vehicle_state.throttle = received_pedal_value;
          received_vehicle_rpm = ((rx_msg_chs().data[3] << 8) | rx_msg_chs().data[2]) * 0.25f;
          break;

        case MOTOR2_ID:
          received_vehicle_speed = rx_msg_chs().data[3] * 128 / 100;
          vehicle_state.speed = received_vehicle_speed;
          break;

        case OPENHALDEX_EXTERNAL_CONTROL_ID:
          // Only allow mode via controller enable toggle
          state.mode = disableController ? MODE_STOCK : MODE_MAP;
          break;
        }

        bool generatedFrame = false;
        twai_message_t original = rx_msg_chs();
        if (state.mode != MODE_STOCK) {
          if (haldexGeneration == 1 || haldexGeneration == 2 || haldexGeneration == 4) {
            getLockData(rx_msg_chs());
            generatedFrame = !can_messages_equal(original, rx_msg_chs());
          }
        } else {
          lock_target = 0;
        }

        tx_msg_hdx() = rx_msg_chs();
        tx_msg_hdx().extd = rx_msg_chs().extd;
        tx_msg_hdx().rtr = rx_msg_chs().rtr;
        tx_msg_hdx().data_length_code = rx_msg_chs().data_length_code;
        haldex_can_send(tx_msg_hdx(), (10 / portTICK_PERIOD_MS), generatedFrame);
      }
    }

    vTaskDelay(1);
  }
}

void parseCAN_hdx(void* arg) {
  while (1) {
#if detailedDebugStack
    stackHDX = uxTaskGetStackHighWaterMark(NULL);
#endif
    while (haldex_can_receive(rx_msg_hdx())) {
      lastCANHaldexTick = millis();
      canviewCacheFrame(rx_msg_hdx(), 1);

      if (haldexGeneration == 1) {
        received_haldex_engagement_raw = rx_msg_hdx().data[1];
        received_haldex_engagement = map(received_haldex_engagement_raw, 128, 198, 0, 100);
      }

      if (haldexGeneration == 2) {
        received_haldex_engagement_raw = rx_msg_hdx().data[1] + rx_msg_hdx().data[4];
        received_haldex_engagement = map(received_haldex_engagement_raw, 128, 255, 0, 100);
      }

      if (haldexGeneration == 4) {
        received_haldex_engagement_raw = rx_msg_hdx().data[1];
        received_haldex_engagement = map(received_haldex_engagement_raw, 128, 255, 0, 100);
      }
      received_haldex_state = rx_msg_hdx().data[0];
      awd_state.actual = received_haldex_engagement;

      received_report_clutch1 = (received_haldex_state & (1 << 0));
      received_temp_protection = (received_haldex_state & (1 << 1));
      received_report_clutch2 = (received_haldex_state & (1 << 2));
      received_coupling_open = (received_haldex_state & (1 << 3));
      received_speed_limit = (received_haldex_state & (1 << 6));

      chassis_can_send(rx_msg_hdx(), (10 / portTICK_PERIOD_MS));
    }

    vTaskDelay(1);
  }
}