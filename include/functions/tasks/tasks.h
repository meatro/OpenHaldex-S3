#pragma once

#include <Arduino.h>

void tasksInit();

// Individual task entry points (FreeRTOS)
void updateTriggers(void* arg);
void showHaldexState(void* arg);
void frames10(void* arg);
void frames20(void* arg);
void frames25(void* arg);
void frames100(void* arg);
void frames200(void* arg);
void frames1000(void* arg);
