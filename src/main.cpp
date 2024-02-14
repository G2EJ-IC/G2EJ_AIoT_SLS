////////////////////////////////////////////////////////////////////////
/*
  Modificado: Ernesto José Guerrero González, Civil Engineering ud FJdC.
*/
////////////////////////////////////////////////////////////////////////
#include <Arduino.h>
#include "WiFi_AIoT.h"
#include "driver/gpio.h"

String endChar = String (char (0xff)) + String (char (0xff)) + String (char (0xff));

// Macros
#define LVGL_REFRESH_TIME (5u)      // 5 milliseconds

// Private Variables
static uint32_t lvgl_refresh_timestamp = 0u;

// Private functions
static void LVGL_TaskInit(void);
static void LVGL_TaskMng(void);

void setup()
{
    Serial.begin(115200); /* prepare for possible serial debug */

    String LVGL_Arduino = "Hello Arduino! ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

    Serial.println(LVGL_Arduino);
    Serial.println("I am LVGL_Arduino");

    LVGL_TaskInit();
    Display_Init();

    ui_init();

    Serial.println("Setup done");   
}

void loop()
{
    Display_Mng();
    LVGL_TaskMng();
}

// Private Function Definitions
static void LVGL_TaskInit(void)
{
  lv_init();
  lvgl_refresh_timestamp = millis();
}

static void LVGL_TaskMng(void)
{
  uint32_t now = millis();
  // LVGL Refresh Timed Task
  if((now - lvgl_refresh_timestamp) >= LVGL_REFRESH_TIME)
  {
    lvgl_refresh_timestamp = now;
    // let the GUI does work
    lv_timer_handler();
  }
}