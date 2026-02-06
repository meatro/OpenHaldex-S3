#include "functions/io/io.h"

#include <Arduino.h>

#include "functions/config/pins.h"

void setupIO() {
#if CAN0_RS >= 0
  pinMode(CAN0_RS, OUTPUT);
  digitalWrite(CAN0_RS, LOW);
#endif
#if CAN1_RS >= 0
  pinMode(CAN1_RS, OUTPUT);
  digitalWrite(CAN1_RS, LOW);
#endif
}

void setupButtons() {
  return;
}
