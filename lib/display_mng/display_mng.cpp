/*
 * display_mng.cpp
 *
 *  Created on: 25-Nov-2023
 *      Author: G2EJ-IC.Esp
 */
//#include <Arduino.h>
#include <lvgl.h>
#include "ClassILI9488.h"
#include <WiFi.h>

#include "display_mng.h"
#include "ui.h"
#include "WiFi_AIoT.h"

/************************************Macros************************************/
#define DISP_STATE_INIT_WAIT_TIME                     (2000u)
#define DISP_STATE_WIFI_SSID_SCANNED_DONE_TIME        (1000u)
#define WIFI_CONNECT_MAX_RETRY                        (5u)

/*---------------------------Private Variables--------------------------------*/
/*Change to your screen resolution*/
static const uint16_t screenWidth  = 480;
static const uint16_t screenHeight = 320;

// LVGL related stuff
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ screenWidth * screenHeight / 10 ]; // Falla por desbordamiento  de Pila.
// static lv_color_t buf[ screenWidth * 10 ]; // (4800)
//static lv_color_t buf[ screenWidth * screenHeight / 30 ]; // (5120)

// LVGL related stuff
//static lv_disp_draw_buf_t draw_buf;
// Declare a buffer for 1/10 screen size
//static lv_color_t buf[ screenWidth * 10 ];

// TFT Instance
//TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */
static LGFX tft;

// Display State to track what to display
static Display_State_e disp_state = DISP_STATE_INIT;
static uint32_t display_timestamp = 0u;
static uint8_t wifi_connect_retry = 0;

// LVGL Objects
static lv_obj_t *power_up_bar;
static lv_anim_t power_up_bar_anim;

/*--------------------------Private Function Prototypes-----------------------*/
#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char * buffer);
#endif
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void my_touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data);

// display state machine related functions
static void Display_StateInit(void);
static void Display_StateWiFiSSIDScanned(void);
static void Display_StateWiFiLogin(void);
static void Display_StateConnectingMenuWait(void);
static void Display_StateConnectingFailed(void);
static void Display_StateConnected(void);
static void PowerUp_BarAnimation(void *bar, int32_t bar_value);

/*---------------------------Public Function Definitions----------------------*/
void Display_Init(void)
{
#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

    tft.begin();          /* TFT init */
    tft.setRotation(3); /* Landscape orientation, flipped */

    ////////////////////////////////////////////////////////////////////////////////////
    //tft.setBrightness(255);
    //uint16_t calData[] = { 120, 3120, 170, 170, 4880, 3030, 4770, 50};
    uint16_t calData[] = { 239, 3926, 233, 265, 3856, 3896, 3714, 308};
    tft.setTouchCalibrate(calData);
    lv_init();
    ////////////////////////////////////////////////////////////////////////////////////

    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

    /*Initialize the display*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    /*Change the following line to your display resolution*/
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /*Initialize the (dummy) input device driver*/
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
}

/**
 * @brief This function handles all the display related stuff, this function is
 *        called continously inside the super loop, and all timings related 
 *        stuff is handled here itself.
 * @param  none
 */
void Display_Mng(void)
{
  uint32_t now = millis();
  switch(disp_state)
  {
    case DISP_STATE_INIT:
      Display_StateInit();
      Display_ChangeState(DISP_STATE_INIT_WAIT);
    break;
    case DISP_STATE_INIT_WAIT:
      // display state will move to next state in progress bar callback
    break;
    case DISP_STATE_SCAN_SSID:
      // This is blocking function, an can take upto 5 seconds
      WiFi_ScanSSID();
      // after this call we will move the progress bar to 100% and then display
      // the next login or WiFi connect screen
      Display_StateWiFiSSIDScanned();
      Display_ChangeState(DISP_STATE_SCAN_SSID_WAIT);
    break;
    case DISP_STATE_SCAN_SSID_WAIT:
      // display state will move to next state in progress bar callback
    break;
    case DISP_STATE_CONNECT_MENU:
      // show login screen
      Display_StateWiFiLogin();
      Display_ChangeState(DISP_STATE_CONNECT_MENU_WAIT);
    break;
    case DISP_STATE_CONNECT_MENU_WAIT:
      // Wait here for User Inputs
      // there are two options which should change the state, first is the
      // connect button press and second is the re-scanning
      // so display state will be changed in these functions
    break;
    case DISP_STATE_CONNECTING_MENU_WAIT:
      // connect button is pressed and now waiting for successfull connection
      Display_StateConnectingMenuWait();
    break;
    case DISP_STATE_CONNECT_FAIL:
      // connecting is failed
      Display_StateConnectingFailed();
    break;
    case DISP_STATE_CONNECTED:
      // connecting is passed and system will move to next screen, where IP
      // address assigned to ESP32 is displayed
      Display_StateConnected();
    break;
    case DISP_STATE_CONNECTED_WAIT:
      // Will wait here, until next project 😊
    break;
    case DISP_STATE_MAX:
    default:
      Display_ChangeState(DISP_STATE_INIT);
    break;
  };
}
/**
 * @brief Update Display State
 * @param state Display State to Change
 */
void Display_ChangeState(Display_State_e state)
{
  Display_State_e prev_state = disp_state;
  disp_state = state;
  LV_LOG_USER("Prev. Disp State = %d, Current State = %d", prev_state, disp_state);
}

/**
 * @brief Get the Display State
 * @param  none
 * @return current display state
 */
Display_State_e Display_GetDispState(void)
{
  return disp_state;
}

/*************************Private Function Definition**************************/
static void Display_StateInit()
{
  // Selecting theme, this code is taken from ui_init function, and this is done
  // to make theme same, at power-up and when square line studio auto generated
  // code is executed
  lv_disp_t * dispp = lv_disp_get_default();
  lv_theme_t * theme = lv_theme_basic_init(dispp);
  lv_disp_set_theme(dispp, theme);

  power_up_bar = lv_bar_create(lv_scr_act());

  lv_obj_set_size(power_up_bar, LV_PCT(100), LV_PCT(10));
  lv_obj_align(power_up_bar, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_bar_set_range(power_up_bar, 0, 100);
  lv_bar_set_value(power_up_bar, 0, LV_ANIM_OFF);

  // Progress Bar Animation
  lv_anim_init(&power_up_bar_anim);
  lv_anim_set_exec_cb(&power_up_bar_anim, PowerUp_BarAnimation);
  lv_anim_set_time(&power_up_bar_anim, DISP_STATE_INIT_WAIT_TIME);
  // lv_anim_set_playback_time(&power_up_bar_anim, DISP_STATE_INIT_WAIT_TIME);
  lv_anim_set_var(&power_up_bar_anim, power_up_bar);
  lv_anim_set_values(&power_up_bar_anim, 0, 50);
  lv_anim_set_repeat_count(&power_up_bar_anim, 0);
  lv_anim_start(&power_up_bar_anim);

  // make sure no delay after configuring station mode
  WiFi_Init();
}

static void Display_StateWiFiSSIDScanned(void)
{
  lv_bar_set_value(power_up_bar, 50, LV_ANIM_OFF);

  // Progress Bar animation
  lv_anim_set_exec_cb(&power_up_bar_anim, PowerUp_BarAnimation);
  lv_anim_set_time(&power_up_bar_anim, DISP_STATE_WIFI_SSID_SCANNED_DONE_TIME);
  lv_anim_set_var(&power_up_bar_anim, power_up_bar);
  lv_anim_set_values(&power_up_bar_anim, 50, 100);
  lv_anim_set_repeat_count(&power_up_bar_anim, 0);
  lv_anim_start(&power_up_bar_anim);
}
static void Display_StateWiFiLogin(void)
{
  lv_obj_clean(lv_scr_act());
  ui_init();
  // update drop down list
  lv_dropdown_set_options(ui_DropDownSSID, Get_WiFiSSID_DD_List());
  // Hide the keyboard at power-up can be done by adding the hidden flag
  lv_obj_add_flag(ui_Keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void Display_StateConnectingMenuWait(void)
{
  uint32_t now = millis();
  if((now - display_timestamp) >= 1000u)
  {
    display_timestamp = now;
    if(WiFi.status() == WL_CONNECTED)
    {
      Display_ChangeState(DISP_STATE_CONNECTED);
      // Clear the screen
      lv_obj_clean(lv_scr_act());
      // Load the second screen
      //lv_disp_load_scr(ui_InitScreen);
      //lv_disp_load_scr(ui_MainScreen);
      lv_disp_load_scr(ui_ConnectedScreen);
    }
    else
    {
      wifi_connect_retry++;
      if(wifi_connect_retry >= WIFI_CONNECT_MAX_RETRY)
      {
        wifi_connect_retry = 0;
        WiFi.disconnect();
        lv_label_set_text(ui_ConnectingLabel, "Connecting Failed......");
        Display_ChangeState(DISP_STATE_CONNECT_FAIL);
      }
    }
  }
}

/**
 * @brief This function waits for 1 second, as connecting failed is displayed in
 *        in the previous state, and after 1 second it switches to connect menu
 * @param  none
 */
static void Display_StateConnectingFailed(void)
{
  uint32_t now = millis();
  if((now - display_timestamp) >= 1000u)
  {
    display_timestamp = now;
    lv_obj_add_flag(ui_ConnectingLabel, LV_OBJ_FLAG_HIDDEN);
    Display_ChangeState(DISP_STATE_CONNECT_MENU);
  }
}

/**
 * @brief Display Connected State,here we only update the ESP32 address on label
 * @param  
 */
static void Display_StateConnected(void)
{
  lv_label_set_text(ui_WiFiSSIDLabel, WiFi.SSID().c_str());
  lv_label_set_text(ui_WiFiLocalIPLabel, WiFi.localIP().toString().c_str());
  lv_label_set_text(ui_WiFidnsIPLabel, WiFi.dnsIP().toString().c_str());
  lv_label_set_text(ui_WiFimacAddressLabel, WiFi.macAddress().c_str());
  Display_ChangeState(DISP_STATE_CONNECTED_WAIT);
}

/**
 * @brief Callback function to animate the progress bar at power-up
 * @param bar pointer to bar object
 * @param bar_value progress bar value
 */
static void PowerUp_BarAnimation(void *bar, int32_t bar_value)
{
  int32_t final_value = 0;
  if(Display_GetDispState() == DISP_STATE_INIT_WAIT)
  {
    final_value = 50;
  }
  else if(Display_GetDispState() == DISP_STATE_SCAN_SSID_WAIT)
  {
    final_value = 100;
  }
  LV_LOG_USER("bar_value=%d, final_value=%d, disp_state=%d", bar_value, final_value, disp_state);
  if(bar_value == final_value)
  {
    lv_bar_set_value((lv_obj_t*)bar, bar_value, LV_ANIM_OFF);
    if(Display_GetDispState() == DISP_STATE_INIT_WAIT)
    {
      // when progress bar reaches 50% move to next state
      Display_ChangeState(DISP_STATE_SCAN_SSID);
    }
    else if(Display_GetDispState() == DISP_STATE_SCAN_SSID_WAIT)
    {
      Display_ChangeState(DISP_STATE_CONNECT_MENU);
    }
  }
  else
  {
    lv_bar_set_value((lv_obj_t*)bar, bar_value, LV_ANIM_ON);
  }
}

/*Read the touchpad*/
void my_touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data)
{
    uint16_t touchX = 0, touchY = 0;

    //bool touched = false;//tft.getTouch(&touchX, &touchY, 600);
    bool touched = tft.getTouch(&touchX, &touchY, 0);

    if(!touched)
    {
        data->state = LV_INDEV_STATE_REL;
    }
    else
    {
        data->state = LV_INDEV_STATE_PR;

        /*Set the coordinates*/
        data->point.x = touchX;
        data->point.y = touchY;

        Serial.print("Data x ");
        Serial.println(touchX);

        Serial.print("Data y ");
        Serial.println(touchY);
    }
}

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    //tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.writePixels((lgfx::rgb565_t *)&color_p->full, w * h);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char * buf)
{
    Serial.printf(buf);
    Serial.flush();
}
#endif