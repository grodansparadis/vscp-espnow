/* Beta node

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <dirent.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <driver/ledc.h>
#include <driver/gpio.h>

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include <nvs_flash.h>
#include "esp_mac.h"
#include <wifi_provisioning/manager.h>

#include <espnow.h>
// #include <espnow_prov.h>
#include <espnow_log.h>
#include <espnow_log_flash.h>
#include <espnow_security.h>
#include <espnow_security_handshake.h>
#include <espnow_storage.h>
#include <espnow_utils.h>

#include "espnow_console.h"
#include "espnow_log.h"

#include "espnow_ota.h"

#include <iot_button.h>

// https://docs.espressif.com/projects/espressif-esp-iot-solution/en/latest/display/led_indicator.html
#include "led_indicator.h"
#include "vscp_led_indicator_blink.h"

#include <vscp.h>
#include <vscp-firmware-helper.h>
#include <vscp_class.h>
#include <vscp_type.h>

#include "beta.h"

#ifndef CONFIG_ESPNOW_VERSION
#define ESPNOW_VERSION 2
#else
#define ESPNOW_VERSION CONFIG_ESPNOW_VERSION
#endif

static espnow_addr_t ESPNOW_ADDR_SELF = { 0 };

static const char *TAG = "app";
static const char *POP = CONFIG_APP_ESPNOW_SESSION_POP;

static beta_node_states_t s_stateNode = BETA_STATE_VIRGIN;

// Handle for nvs storage
static nvs_handle_t s_nvsHandle = 0;

uint32_t time_key_exchange; // Timer for key exchange state clear
uint32_t time_ota;          // Timer for OTA state clear

static led_indicator_handle_t s_led_handle_red;
static led_indicator_handle_t s_led_handle_green;

// Handle for the security task
static TaskHandle_t s_sec_task;

/*
 *
 */
static EventGroupHandle_t s_node_event_group;

#define KEY_SET_BIT     BIT0 // Key is set by alpha node
#define CHANNEL_SET_BIT BIT1 // Channel set by alpha node

///////////////////////////////////////////////////////////
//                   P E R S I S T A N T
///////////////////////////////////////////////////////////

// Set default configuration

node_persistent_config_t g_persistent = {

  // General
  .nodeName   = "Beta Node",
  .pmk        = { 0 },  // Currently not used
  .lmk        = { 0 },  // Currently not used
  .keyOrigin  = { 0 },
  .startDelay = 2,
  .bootCnt    = 0,
  .queueSize  = CONFIG_APP_ESPNOW_QUEUE_SIZE,

  // espnow
  .espnowLongRange             = false,
  .espnowChannel               = 0, // Use wifi channel
  .espnowTtl                   = 32,
  .espnowSizeQueue             = 32,    // Size fo input queue
  .espnowForwardEnable         = true,  // Forward when packets are received
  .espnowFilterAdjacentChannel = true,  // Don't receive if from other channel
  .espnowForwardSwitchChannel  = false, // Allow switchin gchannel on forward
  .espnowFilterWeakSignal      = -65,   // Filter onm RSSI (zero is no rssi filtering)

  // beta
  .logLevelUart   = ESP_LOG_INFO,
  .logLevelEspNow = ESP_LOG_INFO,
  .logLevelFlash  = ESP_LOG_INFO,

};

#define CONTROL_KEY_GPIO GPIO_NUM_0

typedef enum { APP_ESPNOW_CTRL_INIT, APP_ESPNOW_CTRL_BOUND, APP_ESPNOW_CTRL_MAX } app_espnow_ctrl_status_t;

static app_espnow_ctrl_status_t s_espnow_ctrl_status = APP_ESPNOW_CTRL_INIT;

#define WIFI_PROV_KEY_GPIO GPIO_NUM_0


///////////////////////////////////////////////////////////
//        F O R W A R D  D E C L A R A T I O N S
///////////////////////////////////////////////////////////

void
sec_probe_task(void *arg);

///////////////////////////////////////////////////////////////////////////////
// read_onboard_temperature
//

float
read_onboard_temperature(void)
{
  // TODO
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// getMilliSeconds
//

uint32_t
getMilliSeconds(void)
{
  return (esp_timer_get_time() / 1000);
};

///////////////////////////////////////////////////////////////////////////////
// app_led_init
//

static void
app_led_init(void)
{
  led_indicator_gpio_config_t led_indicator_gpio_red_config = {
    .gpio_num             = CONFIG_APP_VSCP_GPIO_OPERATION_LED, /**< num of GPIO */
    .is_active_level_high = 1,
  };

  // Initialize red LED indicator
  led_indicator_config_t indicator_config_red = {
    .mode                      = LED_GPIO_MODE,
    .led_indicator_gpio_config = &led_indicator_gpio_red_config,
    .blink_lists               = vscp_led_indicator_blink_lists,
    .blink_list_num            = VSCP_BLINK_LIST_NUM,
  };

  // s_led_handle_green = led_indicator_create(PRJDEF_INDICATOR_LED_PIN_GREEN, &indicator_config_green);
  s_led_handle_red = led_indicator_create(&indicator_config_red);
  if (NULL == s_led_handle_green) {
    ESP_LOGE(TAG, "Failed to create LED indicator red");
  }

  led_indicator_gpio_config_t led_indicator_gpio_green_config = {
    .gpio_num             = CONFIG_APP_VSCP_GPIO_STATUS_LED, /**< num of GPIO */
    .is_active_level_high = 1,
  };

  // Initialize green LED indicator
  led_indicator_config_t indicator_config_green = {
    .mode                      = LED_GPIO_MODE,
    .led_indicator_gpio_config = &led_indicator_gpio_green_config,
    .blink_lists               = vscp_led_indicator_blink_lists,
    .blink_list_num            = VSCP_BLINK_LIST_NUM,
  };
  // led_indicator_config_t indicator_config_red = {
  //   .off_level = 0, // if zero, attach led positive side to esp32 gpio pin
  //   .mode      = LED_GPIO_MODE,
  // };

  // s_led_handle_red = led_indicator_create(PRJDEF_INDICATOR_LED_PIN_RED, &indicator_config_red);
  s_led_handle_green = led_indicator_create(&indicator_config_green);
  if (NULL == s_led_handle_red) {
    ESP_LOGE(TAG, "Failed to create LED indicator green");
  }
}

///////////////////////////////////////////////////////////////////////////////
// readPersistentConfigs
//

static esp_err_t
readPersistentConfigs(void)
{
  esp_err_t rv;
  char buf[80];
  size_t length = sizeof(buf);
  uint8_t val;

  // boot counter
  rv = nvs_get_u32(s_nvsHandle, "boot_counter", &g_persistent.bootCnt);
  switch (rv) {

    case ESP_OK:
      ESP_LOGI(TAG, "Boot counter = %d", (int) g_persistent.bootCnt);
      break;

    case ESP_ERR_NVS_NOT_FOUND:
      ESP_LOGE(TAG, "The boot counter is not initialized yet!");
      break;

    default:
      ESP_LOGE(TAG, "Error (%s) reading boot counter!", esp_err_to_name(rv));
      break;
  }

  // Update and write back boot counter
  g_persistent.bootCnt++;
  rv = nvs_set_u32(s_nvsHandle, "boot_counter", g_persistent.bootCnt);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to update boot counter");
  }

  // Node name
  rv = nvs_get_str(s_nvsHandle, "node_name", buf, &length);
  switch (rv) {
    case ESP_OK:
      strncpy(g_persistent.nodeName, buf, sizeof(g_persistent.nodeName));
      ESP_LOGI(TAG, "Node Name = %s", g_persistent.nodeName);
      break;

    case ESP_ERR_NVS_NOT_FOUND:
      rv = nvs_set_str(s_nvsHandle, "node_name", g_persistent.nodeName);
      if (rv != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update node name");
      }
      break;

    default:
      ESP_LOGE(TAG, "Error (%s) reading 'node_name'!", esp_err_to_name(rv));
      break;
  }

  // Start Delay (seconds)
  rv = nvs_get_u8(s_nvsHandle, "start_delay", &g_persistent.startDelay);
  switch (rv) {

    case ESP_OK:
      ESP_LOGI(TAG, "Start delay = %d", g_persistent.startDelay);
      break;

    case ESP_ERR_NVS_NOT_FOUND:
      rv = nvs_set_u8(s_nvsHandle, "start_delay", 2);
      if (rv != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update start delay");
      }
      break;

    default:
      ESP_LOGE(TAG, "Error (%s) reading!", esp_err_to_name(rv));
      break;
  }

  // pmk (Primary key)
  length = 16;
  rv     = nvs_get_blob(s_nvsHandle, "pmk", g_persistent.pmk, &length);
  if (rv != ESP_OK) {
    const char key[] = VSCP_DEFAULT_KEY16;
    const char *pos  = key;
    for (int i = 0; i < 16; i++) {
      sscanf(pos, "%2hhx", &g_persistent.pmk[i]);
      pos += 2;
    }
    rv = nvs_set_blob(s_nvsHandle, "pmk", g_persistent.pmk, length);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to write node pmk to nvs. rv=%d", rv);
    }
  }

  // lmk (Local key)
  length = 16;
  rv     = nvs_get_blob(s_nvsHandle, "lmk", g_persistent.lmk, &length);
  if (rv != ESP_OK) {
    const char key[] = VSCP_DEFAULT_KEY32;
    const char *pos  = key + 16;
    for (int i = 0; i < 16; i++) {
      sscanf(pos, "%2hhx", &g_persistent.lmk[i]);
      pos += 2;
    }
    rv = nvs_set_blob(s_nvsHandle, "lmk", g_persistent.lmk, length);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to write node lmk to nvs. rv=%d", rv);
    }
  }

  length = 6;
  rv     = nvs_get_blob(s_nvsHandle, "keyorg", g_persistent.keyOrigin, &length);
  if (rv != ESP_OK) {
    const char key[] = { 0 };
    rv               = nvs_set_blob(s_nvsHandle, "keyorg", g_persistent.keyOrigin, length);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to write originating mac to nvs. rv=%d", rv);
    }
  }

  // espnow queueSize
  rv = nvs_get_u8(s_nvsHandle, "queue_size", &g_persistent.queueSize);
  if (ESP_OK != rv) {
    rv = nvs_set_u8(s_nvsHandle, "queue_size", g_persistent.queueSize);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update queue_size");
    }
  }

  // Long Range
  rv = nvs_get_u8(s_nvsHandle, "longr", &val);
  if (ESP_OK != rv) {
    val = (uint8_t) g_persistent.espnowLongRange;
    rv  = nvs_set_u8(s_nvsHandle, "longr", g_persistent.espnowLongRange);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update espnow long range");
    }
  }
  else {
    g_persistent.espnowLongRange = (bool) val;
  }

  // Channel
  rv = nvs_get_u8(s_nvsHandle, "channel", &g_persistent.espnowChannel);
  printf("reading channel %d\n", g_persistent.espnowChannel);
  if (ESP_OK != rv) {
    rv = nvs_set_u8(s_nvsHandle, "channel", g_persistent.espnowChannel);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update espnow channel");
    }
  }

  // Default ttl
  rv = nvs_get_u8(s_nvsHandle, "ttl", &g_persistent.espnowTtl);
  if (ESP_OK != rv) {
    rv = nvs_set_u8(s_nvsHandle, "ttl", g_persistent.espnowTtl);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update espnow ttl");
    }
  }

  // Forward
  rv = nvs_get_u8(s_nvsHandle, "fw", &val);
  if (ESP_OK != rv) {
    val = (uint8_t) g_persistent.espnowForwardEnable;
    rv  = nvs_set_u8(s_nvsHandle, "fw", g_persistent.espnowForwardEnable);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update espnow forward");
    }
  }
  else {
    g_persistent.espnowForwardEnable = (bool) val;
  }

  // Adj filter channel
  rv = nvs_get_u8(s_nvsHandle, "adjchfilt", &val);
  if (ESP_OK != rv) {
    val = (uint8_t) g_persistent.espnowFilterAdjacentChannel;
    rv  = nvs_set_u8(s_nvsHandle, "adjchfilt", g_persistent.espnowFilterAdjacentChannel);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update espnow adj channel filter");
    }
  }
  else {
    g_persistent.espnowFilterAdjacentChannel = (bool) val;
  }

  // Allow switching channel on forward
  rv = nvs_get_u8(s_nvsHandle, "swchf", &val);
  if (ESP_OK != rv) {
    val = (uint8_t) g_persistent.espnowForwardSwitchChannel;
    rv  = nvs_set_u8(s_nvsHandle, "swchf", g_persistent.espnowForwardSwitchChannel);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update espnow switch channel on forward");
    }
  }
  else {
    g_persistent.espnowFilterAdjacentChannel = (bool) val;
  }

  // RSSI limit
  rv = nvs_get_i8(s_nvsHandle, "rssi", &g_persistent.espnowFilterWeakSignal);
  if (ESP_OK != rv) {
    rv = nvs_set_u8(s_nvsHandle, "rssi", g_persistent.espnowFilterWeakSignal);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update espnow RSSI");
    }
  }

  // Log level for UART
  rv = nvs_get_u8(s_nvsHandle, "loguartlv", &val);
  if (ESP_OK != rv) {
    val = (uint8_t) g_persistent.logLevelUart;
    rv  = nvs_set_u8(s_nvsHandle, "loguartlv", g_persistent.logLevelUart);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update espnow uart log level");
    }
  }
  else {
    g_persistent.logLevelUart = val;
  }

  // Log level for espnow
  rv = nvs_get_u8(s_nvsHandle, "logenlv", &val);
  if (ESP_OK != rv) {
    val = (uint8_t) g_persistent.logLevelEspNow;
    rv  = nvs_set_u8(s_nvsHandle, "logenlv", g_persistent.logLevelEspNow);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update espnow espnow log level");
    }
  }
  else {
    g_persistent.logLevelEspNow = val;
  }

  // Log level for flash
  rv = nvs_get_u8(s_nvsHandle, "logflashlv", &val);
  if (ESP_OK != rv) {
    val = (uint8_t) g_persistent.logLevelFlash;
    rv  = nvs_set_u8(s_nvsHandle, "logflashlv", g_persistent.logLevelFlash);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update espnow espnow log level");
    }
  }
  else {
    g_persistent.logLevelFlash = val;
  }

  
  return ESP_OK;
}

//-----------------------------------------------------------------------------
//                                  OTA
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
// app_ota_responder_start
//

esp_err_t
app_ota_responder_start()
{
  espnow_ota_config_t ota_config = {
    .skip_version_check       = true,
    .progress_report_interval = 10,
  };
  return espnow_ota_responder_start(&ota_config);
}

///////////////////////////////////////////////////////////////////////////////
// app_ota_responder_stop
//

esp_err_t
app_ota_responder_stop()
{
  return espnow_ota_responder_stop();
}

//-----------------------------------------------------------------------------
//                                  SEC
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
// sec_probe_task
//

void
sec_probe_task(void *arg)
{
  int rv;

  espnow_sec_responder_start(POP);

  if (VSCP_ERROR_SUCCESS != (rv = vscp_espnow_probe())) {
    ESP_LOGE(TAG, "[%s, %d]: <Probe failed> !(%x)", __func__, __LINE__, rv);
    // goto EXIT;
  }

  // espnow_sec_responder_start(POP);

  // vTaskDelay(pdMS_TO_TICKS(100));
  // EXIT:
  vTaskDelete(NULL);
}

//-----------------------------------------------------------------------------
//                                  Wifi
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
// app_wifi_init
//

static void
app_wifi_init()
{
  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
  ESP_ERROR_CHECK(esp_wifi_start());
}

//-----------------------------------------------------------------------------
//                                Key handlers
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
// app_sec_init_press_cb
//

static void
app_sec_init_press_cb(void *arg, void *usr_data)
{
  ESP_ERROR_CHECK(!(BUTTON_SINGLE_CLICK == iot_button_get_event(arg)));

  uint8_t key_info[APP_KEY_LEN];
  if (espnow_get_key(key_info) == ESP_OK) {
    ESP_LOGW(TAG, "Security key is already set.");
    return;
  }

  blink_switch_type(s_led_handle_green, BLINK_UPDATING);

  ESP_LOGI(TAG, "Starting sec init");
  s_sec_task = (TaskHandle_t) xTaskCreate(sec_probe_task, "sec_probe", 3072, NULL, tskIDLE_PRIORITY + 1, NULL);
}

///////////////////////////////////////////////////////////////////////////////
// app_ota_start_press_cb
//

static void
app_ota_start_press_cb(void *arg, void *usr_data)
{
  esp_err_t ret;
  ESP_ERROR_CHECK(!(BUTTON_DOUBLE_CLICK == iot_button_get_event(arg)));

  if (BETA_STATE_OTA == s_stateNode) {
    ESP_LOGI(TAG, "Deactivate OTA firmware update");
    ret = app_ota_responder_stop();
    if (ESP_OK == ret) {
      blink_switch_type(s_led_handle_green, BLINK_CONNECTED);
      s_stateNode = BETA_STATE_IDLE;
    }
  }
  else if (BETA_STATE_IDLE == s_stateNode) {
    ESP_LOGI(TAG, "Initiate OTA firmware update");
    ret = app_ota_responder_start();
    if (ESP_OK == ret) {
      blink_switch_type(s_led_handle_green, BLINK_UPDATING);
      s_stateNode = BETA_STATE_OTA;
      time_ota    = getMilliSeconds();
    }
    else {
      ESP_LOGE(TAG, "Failed to start OTA");
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// app_restore_factory_defaults_press_cb
//

static void
app_restore_factory_defaults_press_cb(void *arg, void *usr_data)
{
  esp_err_t ret;
  ESP_ERROR_CHECK(!(BUTTON_LONG_PRESS_START == iot_button_get_event(arg)));

  ESP_LOGI(TAG, "Restore factory settings");
  blink_switch_type(s_led_handle_green, BLINK_FACTORY_RESET);

  // Erase all settings
  ret = nvs_erase_all(s_nvsHandle);
  if (ESP_OK != ret) {
    ESP_LOGE(TAG, "Unable to erase configuration area");
  }

  // Unbound device
  ret = espnow_erase_key();
  if (ESP_OK != ret) {
    ESP_LOGE(TAG, "Failed to erase key %X", ret);
  }

  nvs_commit(s_nvsHandle);

  // set defaults
  readPersistentConfigs();

  // Disconnect from wifi
  ret = esp_wifi_disconnect();
  if (ESP_OK != ret) {
    ESP_LOGE(TAG, "Failed to disconnect from wifi %X", ret);
  }

  // Restart system (set defaults)
  espnow_reboot(pdMS_TO_TICKS(4000));
  // esp_restart();
}

///////////////////////////////////////////////////////////////////////////////
// app_system_event_handler
//
// Event handler for catching system events
//

static void
app_system_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  esp_err_t ret;

  // if (event_base == ESP_EVENT_ESPNOW_OTA_BASE) {

  //   switch (event_id) {

  //     default:
  //       break;
  //   }
  // }
  if (event_base == ESP_EVENT_ESPNOW) {

    switch (event_id) {

      case ESP_EVENT_ESPNOW_LOG_FLASH_FULL:
        ESP_LOGW(TAG, "The flash partition that stores the log is full, size: %d", espnow_log_flash_size());
        // ret = espnow_log_flash_erase();
        // if (ESP_OK != ret) {
        //   ESP_LOGE(TAG, "Failed to clear flash.");
        // }
        char buf[80];
        size_t len = 80;
        while (ESP_OK == espnow_log_flash_read(buf, &len)) {
          printf("len=%zu buf = %s ", len, buf);
        }
        break;

      case ESP_EVENT_ESPNOW_OTA_STARTED:
        ESP_LOGI(TAG, "ESP_EVENT_ESPNOW_OTA_STARTED");
        blink_switch_type(s_led_handle_green, BLINK_UPDATING);
        break;

      case ESP_EVENT_ESPNOW_OTA_STATUS: {
        uint32_t write_percentage = *((uint32_t *) event_data);
        ESP_LOGI(TAG, "ESP_EVENT_ESPNOW_OTA_STATUS  %ld", write_percentage);
      } break;

      case ESP_EVENT_ESPNOW_OTA_FINISH:
        ESP_LOGI(TAG, "ESP_EVENT_ESPNOW_OTA_FINISH");
        blink_switch_type(s_led_handle_green, BLINK_CONNECTED);
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
        break;

      case ESP_EVENT_ESPNOW_OTA_STOPED:
        ESP_LOGI(TAG, "ESP_EVENT_ESPNOW_OTA_STOPED");
        blink_switch_type(s_led_handle_green, BLINK_CONNECTED);
        break;

      case ESP_EVENT_ESPNOW_OTA_FIRMWARE_DOWNLOAD:
        ESP_LOGI(TAG, "ESP_EVENT_ESPNOW_OTA_FIRMWARE_DOWNLOAD");
        break;

      case ESP_EVENT_ESPNOW_OTA_SEND_FINISH:
        ESP_LOGI(TAG, "ESP_EVENT_ESPNOW_OTA_SEND_FINISH");
        break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// app_main
//

void
app_main()
{
  esp_err_t ret;

  s_node_event_group = xEventGroupCreate();

  espnow_storage_init();
  app_wifi_init();

  app_led_init();
  blink_switch_type(s_led_handle_green, BLINK_CONNECTING);

  // ----------------------------------------------------------------------------
  //                        NVS - Persistent storage
  // ----------------------------------------------------------------------------

  // Init persistent storage
  ESP_LOGI(TAG, "Read persistent storage ... ");

  ret = nvs_open("config", NVS_READWRITE, &s_nvsHandle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(ret));
  }
  else {
    // Read (or set to defaults) persistent values
    readPersistentConfigs();
  }

  // Set the probed channel
  if (g_persistent.espnowChannel) {
    ESP_LOGI(TAG, "Setting channel from persistent storage to %d", g_persistent.espnowChannel);
    esp_wifi_set_channel(g_persistent.espnowChannel, WIFI_SECOND_CHAN_NONE);
  }

  espnow_config_t espnow_config = ESPNOW_INIT_CONFIG_DEFAULT();
  memcpy((uint8_t *) espnow_config.pmk, g_persistent.pmk, 16);
  espnow_config.qsize                  = g_persistent.queueSize;
  espnow_config.sec_enable             = true; // Must be enabled for all security enabled functions to work
  espnow_config.forward_enable         = true;
  espnow_config.forward_switch_channel = 0;
  espnow_config.send_retry_num         = 10;
  espnow_config.send_max_timeout       = pdMS_TO_TICKS(3000);

  espnow_init(&espnow_config);

  // ESP_ERROR_CHECK( esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N) );
  // #if CONFIG_ESPNOW_ENABLE_LONG_RANGE
  // ESP_ERROR_CHECK( esp_wifi_set_protocol(ESP_IF_WIFI_STA,
  // WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
  // #endif

  uint8_t key_info[APP_KEY_LEN];
  if (ESP_OK == espnow_get_key(key_info)) {

    // Set the key permanently
    espnow_set_key(key_info);

    // !!! Use only for key setting debug. NEVER DISCLOSE !!!
    //ESP_LOGI(TAG, "Security Key: " KEYSTR, KEY2STR(key_info));

    

    // Initializing OTA
    // espnow_ota_config_t ota_config = {
    //   .skip_version_check       = true,
    //   .progress_report_interval = 10,
    // };

    // ESP_LOGI(TAG, "Starting ota");
    // ret = espnow_ota_responder_start(&ota_config);
    // if (ESP_OK != ret) {
    //   ESP_LOGE(TAG, "Failed to start OTA responder");
    // }
  }
  else {
    ESP_LOGW(TAG, "Security key is not set");
  }

  // Initializing logging
  espnow_log_config_t log_config = {
    .log_level_uart   = g_persistent.logLevelUart,
    .log_level_espnow = g_persistent.logLevelEspNow,
    .log_level_flash  = g_persistent.logLevelFlash,
  };

  if (ESP_OK != (ret = espnow_log_init(&log_config))) {
    ESP_LOGE(TAG, "Failed to init espnow logging");
  }
  esp_log_level_set("*", ESP_LOG_INFO);

  // esp_wifi_set_channel(3, WIFI_SECOND_CHAN_NONE);

  button_config_t button_config = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = 3000,
        .gpio_button_config = {
          .gpio_num = WIFI_PROV_KEY_GPIO,
          .active_level = 0,
        },
    };

  button_handle_t button_handle = iot_button_create(&button_config);

  iot_button_register_cb(button_handle, BUTTON_SINGLE_CLICK, app_sec_init_press_cb, NULL);
  iot_button_register_cb(button_handle, BUTTON_DOUBLE_CLICK, app_ota_start_press_cb, NULL);
  iot_button_register_cb(button_handle, BUTTON_LONG_PRESS_START, app_restore_factory_defaults_press_cb, NULL);

  esp_event_handler_register(ESP_EVENT_ESPNOW, ESP_EVENT_ANY_ID, app_system_event_handler, NULL);
  esp_event_handler_register(ESP_EVENT_ESPNOW_OTA_BASE, ESP_EVENT_ANY_ID, app_system_event_handler, NULL);
  esp_event_handler_register(ESP_EVENT_ESPNOW_DEBUG_BASE, ESP_EVENT_ANY_ID, app_system_event_handler, NULL);

  // Setup VSCP esp-now

  vscp_espnow_config_t vscp_espnow_conf;
  //vscp_espnow_conf.nvsHandle = s_nvsHandle;
  

  // Set default primary key
  // uint8_t pmk[16];
  // vscp_fwhlp_hex2bin(pmk, 16, VSCP_DEFAULT_KEY16);
  // vscp_espnow_conf.pmk = pmk;

  // Initialize VSCP espnow
  if (ESP_OK != vscp_espnow_init(&vscp_espnow_conf)) {
    ESP_LOGI(TAG, "Failed to initialize VSCP espnow");
  }

  esp_wifi_get_mac(ESP_IF_WIFI_STA, ESPNOW_ADDR_SELF);
  ESP_LOGI(TAG, "mac: " MACSTR ", version: %d", MAC2STR(ESPNOW_ADDR_SELF), ESPNOW_VERSION);

  // Set timezone to GMT
  setenv("TZ", "GMT", 1);
  tzset();

  while (1) {
    if ((BETA_STATE_VIRGIN == s_stateNode) && (ESP_OK == espnow_get_key(key_info))) {
      blink_switch_type(s_led_handle_green, BLINK_CONNECTED);
      s_stateNode = BETA_STATE_IDLE;
    }

    // esp_task_wdt_reset();
    // ESP_LOGI(TAG, "heap %lu kB (%lu)",esp_get_minimum_free_heap_size()/1024,esp_get_minimum_free_heap_size());
    vTaskDelay(pdMS_TO_TICKS(1000));
    taskYIELD();

    // Check if OTA takes to long
    if ((BETA_STATE_OTA == s_stateNode) && ((getMilliSeconds() - time_ota) > 120000)) {
      ESP_LOGW(TAG, "OTA valid period over. Go back to IDLE");
      s_stateNode = BETA_STATE_IDLE;
      blink_switch_type(s_led_handle_green, BLINK_CONNECTED);
    }

    // ESP_LOG_BUFFER_HEXDUMP(TAG, g_persistent.pmk, 16, ESP_LOG_INFO);
    // ESP_LOG_BUFFER_HEXDUMP(TAG, g_persistent.lmk, 16, ESP_LOG_INFO);

    if (0) {
      time_t now;
      char strftime_buf[64];
      struct tm timeinfo;

      time(&now);

      localtime_r(&now, &timeinfo);
      strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
      ESP_LOGI(TAG, "The current date/time GMT is: %s", strftime_buf);
    }

    if (0) {
      uint32_t free_heap = esp_get_free_heap_size() / 1024;
      uint32_t min_heap  = esp_get_minimum_free_heap_size() / 1024;
      printf("Free heap: %ld kB Min heap = %ld kB\n", free_heap, min_heap);
    }
  }
}
