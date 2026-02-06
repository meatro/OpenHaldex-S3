#pragma once

#include <Arduino.h>

struct UpdateInfo {
  String current;
  String latest;
  bool available;
  uint32_t lastCheckMs;
  String error;
  String url;
  String firmwareUrl;
  String filesystemUrl;
  bool installing;
  String installError;
  uint32_t bytesTotal;
  uint32_t bytesDone;
  float speedBps;
  uint32_t progressMs;
  String stage;
};

void updateInit();
void updateCheckNow();
bool updateGetInfo(UpdateInfo& out);
bool updateInstallStart();
