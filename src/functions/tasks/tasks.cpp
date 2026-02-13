#include "functions/tasks/tasks.h"

#include <Arduino.h>
#include <driver/twai.h>

#include "functions/config/config.h"
#include "functions/config/pins.h"
#include "functions/core/state.h"
#include "functions/core/calcs.h"
#include "functions/can/can.h"
#include "functions/can/can_id.h"
#include "functions/io/frames.h"
#include "functions/can/standalone_can.h"
#include "functions/storage/storage.h"
#include "functions/storage/filelog.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static TaskHandle_t handle_frames1000 = nullptr;
static TaskHandle_t handle_frames200 = nullptr;
static TaskHandle_t handle_frames100 = nullptr;
static TaskHandle_t handle_frames25 = nullptr;
static TaskHandle_t handle_frames20 = nullptr;
static TaskHandle_t handle_frames10 = nullptr;

static void updateLabels(void* arg) {
  while (1) {
    stackupdateLabels = uxTaskGetStackHighWaterMark(NULL);
    const uint32_t now = millis();
    hasCANChassis = (lastCANChassisTick > 0) && ((now - lastCANChassisTick) <= 500);
    hasCANHaldex = (lastCANHaldexTick > 0) && ((now - lastCANHaldexTick) <= 500);
    vTaskDelay(OH_LABEL_REFRESH_MS / portTICK_PERIOD_MS);
  }
}

static void writeEEP(void* arg) {
  vTaskDelay(OH_EEP_START_DELAY_MS / portTICK_PERIOD_MS);
  while (1) {
    stackwriteEEP = uxTaskGetStackHighWaterMark(NULL);
    if (storageIsDirty()) {
      storageSave();
    }
    vTaskDelay(OH_EEP_REFRESH_MS / portTICK_PERIOD_MS);
  }
}
void tasksInit() {
  // Create FreeRTOS tasks for periodic operations
  // Tasks handle CAN frame transmission at different rates and EEPROM writes
  xTaskCreate(showHaldexState, "showHaldexState", 5000, NULL, 1, NULL);
  xTaskCreate(updateLabels, "updateLabels", 6000, NULL, 2, NULL);
#if OH_ENABLE_EEP_TASK
  xTaskCreate(writeEEP, "writeEEP", 4096, NULL, 3, NULL);
#endif
  xTaskCreate(updateTriggers, "updateTriggers", 2000, NULL, 4, NULL);

  if (can_ready) {
    xTaskCreate(frames1000, "frames1000", 8000, NULL, 5, &handle_frames1000);
    xTaskCreate(frames200, "frames200", 8000, NULL, 6, &handle_frames200);
    xTaskCreate(frames100, "frames100", 8000, NULL, 7, &handle_frames100);
    xTaskCreate(frames25, "frames25", 8000, NULL, 8, &handle_frames25);
    xTaskCreate(frames20, "frames20", 8000, NULL, 9, &handle_frames20);
    xTaskCreate(frames10, "frames10", 8000, NULL, 10, &handle_frames10);

    if (!isStandalone) {
      vTaskSuspend(handle_frames1000);
      vTaskSuspend(handle_frames200);
      vTaskSuspend(handle_frames100);
      vTaskSuspend(handle_frames25);
      vTaskSuspend(handle_frames20);
      vTaskSuspend(handle_frames10);
      filelogLogInfo("tasks", "Standalone disabled; generated frame tasks suspended");
    }

    xTaskCreate(broadcastOpenHaldex, "broadcastOpenHaldex", 4096, NULL, 6, NULL);
    xTaskCreate(parseCAN_hdx, "parseHaldex", 4096, NULL, 6, NULL);
    xTaskCreate(parseCAN_chs, "parseChassis", 4096, NULL, 7, NULL);
    filelogLogInfo("tasks", "CAN tasks initialized");
  } else {
    filelogLogError("tasks", "CAN not ready; frame/parser tasks not started");
  }
}

void updateTriggers(void* arg) {
  bool last_bus_failure = isBusFailure;
  while (1) {
    if (isBusFailure != last_bus_failure) {
      if (isBusFailure) {
        filelogLogError("can", "bus failure detected");
      } else {
        filelogLogEvent("can", "bus recovered");
      }
      last_bus_failure = isBusFailure;
    }

    canRecoverIfBusFailure();

    vTaskDelay(updateTriggersRefresh / portTICK_PERIOD_MS);
  }
}

void showHaldexState(void* arg) {
  while (1) {
    stackshowHaldexState = uxTaskGetStackHighWaterMark(NULL);

    DEBUG("Mode: %s", get_openhaldex_mode_string(state.mode));
    DEBUG("    Req:Act: %d:%d", int(lock_target), received_haldex_engagement);

#if detailedDebug
    DEBUG("    Raw haldexEngagement: " BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(received_haldex_state));
    DEBUG("    reportClutch1: %d", received_report_clutch1);
    DEBUG("    reportClutch2: %d", received_report_clutch2);
    DEBUG("    couplingOpen: %d", received_coupling_open);
    DEBUG("    speedLimit: %d", received_speed_limit);
    DEBUG("    tempCounter2: %d", state.pedal_threshold);

    DEBUG("    hasChassisCAN: %d", hasCANChassis);
    DEBUG("    hasHaldexCAN: %d", hasCANHaldex);
    DEBUG("    lastCANChassis: %d", lastCANChassisTick);
    DEBUG("    lastHaldexChassis: %d", lastCANHaldexTick);

    DEBUG("    currentMode: %d", lastMode);
    DEBUG("    isStandalone: %d", isStandalone);
    DEBUG("    haldexGen: %d", haldexGeneration);

    if (isBusFailure) {
      DEBUG("    Bus Failure: True");
    }
#endif

#if detailedDebugStack
    DEBUG("Stack Sizes:");
    DEBUG("    stackCHS: %d", stackCHS);
    DEBUG("    stackHDX: %d", stackHDX);

    DEBUG("    stackframes10: %d", stackframes10);
    DEBUG("    stackframes20: %d", stackframes20);
    DEBUG("    stackframes25: %d", stackframes25);
    DEBUG("    stackframes100: %d", stackframes100);
    DEBUG("    stackframes200: %d", stackframes200);
    DEBUG("    stackframes1000: %d", stackframes1000);

    DEBUG("    stackbroadcastOpenHaldex: %d", stackbroadcastOpenHaldex);
    DEBUG("    stackupdateLabels: %d", stackupdateLabels);
    DEBUG("    stackshowHaldexState: %d", stackshowHaldexState);
    DEBUG("    stackwriteEEP: %d", stackwriteEEP);
#endif

#if detailedDebugRuntimeStats
    char buffer2[2048] = {0};
    vTaskGetRunTimeStats(buffer2);
    if (filelogShouldSerialEmit("DEBUG", "debug")) {
      Serial.println(buffer2);
    }
#endif

#if detailedDebugCAN
    uint32_t alerts_triggered;
    twai_read_alerts(&alerts_triggered, pdMS_TO_TICKS(0));
    twai_status_info_t twaistatus;
    twai_get_status_info(&twaistatus);
    DEBUG("");
    DEBUG("CAN-BUS Details:");
    DEBUG("    RX buffered: %lu\t", twaistatus.msgs_to_rx);
    DEBUG("    RX missed: %lu\t", twaistatus.rx_missed_count);
    DEBUG("    RX overrun %lu", twaistatus.rx_overrun_count);

    if (alerts_triggered & TWAI_ALERT_ERR_ACTIVE) {
      isBusFailure = true;
    }
    if (alerts_triggered & TWAI_ALERT_RECOVERY_IN_PROGRESS) {
      isBusFailure = true;
    }
    if (alerts_triggered & TWAI_ALERT_ABOVE_ERR_WARN) {
      isBusFailure = true;
    }
    if (alerts_triggered & TWAI_ALERT_BUS_ERROR) {
      isBusFailure = true;
    }
    if (alerts_triggered & TWAI_ALERT_TX_FAILED) {
      isBusFailure = true;
    }
    if (alerts_triggered & TWAI_ALERT_RX_QUEUE_FULL) {
      isBusFailure = true;
    }
    if (alerts_triggered & TWAI_ALERT_ERR_PASS) {
      isBusFailure = true;
    }
    if (alerts_triggered & TWAI_ALERT_BUS_OFF) {
      isBusFailure = true;
    }
    if (alerts_triggered & TWAI_ALERT_RX_FIFO_OVERRUN) {
      isBusFailure = true;
    }
#endif

    vTaskDelay(serialMonitorRefresh / portTICK_PERIOD_MS);
  }
}

void frames10(void* arg) {
  while (1) {
    if (isStandalone) {
      switch (haldexGeneration) {
      case 1:
        Gen1_frames10();
        break;
      case 2:
        Gen2_frames10();
        break;
      case 4:
        Gen4_frames10();
        break;
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void frames20(void* arg) {
  while (1) {
    if (isStandalone) {
      switch (haldexGeneration) {
      case 1:
        Gen1_frames20();
        break;
      case 2:
        Gen2_frames20();
        break;
      case 4:
        Gen4_frames20();
        break;
      }
    }
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

void frames25(void* arg) {
  while (1) {
    if (isStandalone) {
      switch (haldexGeneration) {
      case 1:
        Gen1_frames25();
        break;
      case 2:
        Gen2_frames25();
        break;
      case 4:
        Gen4_frames25();
        break;
      }
    }
    vTaskDelay(25 / portTICK_PERIOD_MS);
  }
}

void frames100(void* arg) {
  while (1) {
    if (isStandalone) {
      lock_target = get_lock_target_adjustment();
      switch (haldexGeneration) {
      case 1:
        Gen1_frames100();
        break;
      case 2:
        Gen2_frames100();
        break;
      case 4:
        Gen4_frames100();
        break;
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void frames200(void* arg) {
  while (1) {
    if (isStandalone) {
      switch (haldexGeneration) {
      case 1:
        Gen1_frames200();
        break;
      case 2:
        Gen2_frames200();
        break;
      case 4:
        Gen4_frames200();
        break;
      }
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

void frames1000(void* arg) {
  while (1) {
    if (isStandalone) {
      switch (haldexGeneration) {
      case 1:
        Gen1_frames1000();
        break;
      case 2:
        Gen2_frames1000();
        break;
      case 4:
        Gen4_frames1000();
        break;
      }
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
