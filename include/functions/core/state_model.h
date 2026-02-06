#pragma once

struct VehicleState {
  float speed;
  float throttle;
};

struct AWDState {
  float requested;
  float actual;
};

extern VehicleState vehicle_state;
extern AWDState awd_state;
