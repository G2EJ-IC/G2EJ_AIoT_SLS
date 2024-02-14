#ifndef __WIFI_AIOT_H__
#define __WIFI_AIOT_H__

// Load Arduino library
//#include <Arduino.h>
// Load Wi-Fi library
#include <WiFi.h>
//#include <esp_wifi.h>
// Load ClassILI9488 library
#include "ClassILI9488.h"
// Load display_mng library
#include "display_mng.h"

// LVGL
// https://github.com/lvgl/lvgl
#include <lvgl.h>
#include "ui.h"

// Public Functions
char *Get_WiFiSSID_DD_List(void);
void WiFi_Init(void);
void WiFi_ScanSSID(void);

#endif // __WIFI_AIOT_H__