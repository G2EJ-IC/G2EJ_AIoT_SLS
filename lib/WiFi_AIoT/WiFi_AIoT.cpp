#include "WiFi_AIoT.h"

#define WIFI_MAX_SSID                       (15u)

//static uint32_t lvgl_refresh_timestamp = 0u;
static char wifi_dd_list[WIFI_MAX_SSID*20] = { 0 };

//////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This function returns the WiFi SSID Names appended together which is 
 *        suitable for LVGL drop down widget
 * @param  none
 * @return pointer to wifi ssid drop down list
 */
char *Get_WiFiSSID_DD_List(void)
{
  return wifi_dd_list;
}

/**
 * @brief Initialize the WiFi mode to station mode and disconnect if connected
 * @param  none
 */
void WiFi_Init(void)
{
  WiFi.mode(WIFI_STA);
  
  WiFi.disconnect();
}

/**
 * @brief Scans for the SSID and store the results in a WiFi drop down list
 * "wifi_dd_list", this list is suitable to be used with the LVGL Drop Down
 * @param  none
 */
void WiFi_ScanSSID(void)
{
  String ssid_name;
  Serial.println("Start Scanning");
  int n = WiFi.scanNetworks();
  Serial.println("Scanning Done");
  if(n == 0)
  {
    Serial.println("No Networks Found");
  }
  else
  {
    // I am restricting n to max WIFI_MAX_SSID value
    n = n <= WIFI_MAX_SSID ? n : WIFI_MAX_SSID;
    for (int i = 0; i < n; i++) 
    {
      if(i == 0)
      {
        ssid_name = WiFi.SSID(i);
      }
      else
      {
        ssid_name = ssid_name + WiFi.SSID(i);
      }
      ssid_name = ssid_name + '\n';
      delay(10);
      //vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    // clear the array, it might be possible that we coming after rescanning
    memset(wifi_dd_list, 0x00, sizeof(wifi_dd_list));
    strcpy(wifi_dd_list, ssid_name.c_str());
    Serial.println(wifi_dd_list);
  }
  Serial.println("Scanning Completed");
}