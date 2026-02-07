#include "functions/can/can.h"

#include "functions/config/config.h"

#include "functions/core/state.h"
#include "functions/can/can_id.h"
#include "functions/can/can_state.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void broadcastOpenHaldex(void* arg) {
  while (1) {
#if detailedDebugStack
    stackbroadcastOpenHaldex = uxTaskGetStackHighWaterMark(NULL);
#endif
    twai_message_t broadcast_frame = {};
    broadcast_frame.identifier = OPENHALDEX_BROADCAST_ID;
    broadcast_frame.extd = 0;
    broadcast_frame.rtr = 0;
    broadcast_frame.data_length_code = 8;
    broadcast_frame.data[0] = 0;
    broadcast_frame.data[1] = isGen1Standalone + isGen2Standalone + isGen4Standalone;
    broadcast_frame.data[2] = (uint8_t)received_haldex_engagement_raw;
    broadcast_frame.data[3] = (uint8_t)lock_target;
    broadcast_frame.data[4] = received_vehicle_speed;
    broadcast_frame.data[5] = state.mode_override;
    broadcast_frame.data[6] = (uint8_t)state.mode;
    broadcast_frame.data[7] = (uint8_t)received_pedal_value;

    if (broadcastOpenHaldexOverCAN) {
      chassis_can_send(broadcast_frame, (10 / portTICK_PERIOD_MS));
    }
    vTaskDelay(OH_BROADCAST_REFRESH_MS / portTICK_PERIOD_MS);
  }
}
