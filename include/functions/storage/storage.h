#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

void storageInit();
void storageLoad();
void storageSave();

bool storageFsReady();

void storageMarkDirty();
bool storageIsDirty();
void storageClearDirty();

bool storageLoadMapPath(const String& path);
bool storageSaveMapName(const String& name, String& outPath);
bool storageDeleteMapPath(const String& path);
void storageListMaps(JsonArray out);
String storageGetCurrentMapPath();
void storageSetCurrentMapPath(const String& path);

bool storageGetWifiCreds(String& ssid, String& pass);
void storageSetWifiCreds(const String& ssid, const String& pass);
void storageClearWifiCreds();

bool storageGetWifiStaEnabled();
void storageSetWifiStaEnabled(bool enabled);
