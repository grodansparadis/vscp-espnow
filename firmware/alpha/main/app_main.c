/* Alpha node

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/param.h>
#include <sys/unistd.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <driver/ledc.h>
#include <driver/gpio.h>

#include <esp_check.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include <nvs_flash.h>
#include <esp_spiffs.h>
#include <esp_event_base.h>
#include <esp_event.h>
#include "esp_mac.h"
#include <esp_ota_ops.h>
#include <esp_https_ota.h>
#include <esp_http_server.h>

#include <wifi_provisioning/manager.h>
#include "wifi_prov.h"

#include <esp_tls_crypto.h>
#include <esp_crt_bundle.h>

#include <espnow.h>
#include <espnow_prov.h>
#include <espnow_security.h>
#include <espnow_security_handshake.h>
#include <espnow_storage.h>
#include <espnow_utils.h>
#include <espnow_ctrl.h>

#include "espnow_console.h"
#include "espnow_log.h"

#include "espnow_ota.h"

#include <iot_button.h>

// https://docs.espressif.com/projects/espressif-esp-iot-solution/en/latest/display/led_indicator.html
#include "led_indicator.h"
#include "led_indicator_blink_default.h"

#include "net_logging.h"

#include <cJSON.h>

#include <vscp.h>
#include <vscp-firmware-helper.h>
#include <vscp_class.h>
#include <vscp_type.h>

#include "websrv.h"
#include "mqtt.h"
#include "tcpsrv.h"

#include "vscp-compiler.h"
#include "vscp-projdefs.h"

#include "alpha.h"

static const char *TAG = "app";

#ifndef CONFIG_ESPNOW_VERSION
#define ESPNOW_VERSION 2
#else
#define ESPNOW_VERSION CONFIG_ESPNOW_VERSION
#endif

// Handle for nvs storage
nvs_handle_t g_nvsHandle = 0;

extern bool g_vscp_espnow_probe;

/*
  The event queue is handled in the main loop and it
  react on events from different parts of the system
*/
QueueHandle_t g_event_queue;

static espnow_addr_t ESPNOW_ADDR_SELF = { 0 };

static button_handle_t s_init_button_handle;

static led_indicator_handle_t s_led_handle_red;
static led_indicator_handle_t s_led_handle_green;

const blink_step_t alpha_blink[] = {
  { LED_BLINK_HOLD, LED_STATE_ON, 50 },   // step1: turn on LED 50 ms
  { LED_BLINK_HOLD, LED_STATE_OFF, 100 }, // step2: turn off LED 100 ms
  { LED_BLINK_HOLD, LED_STATE_ON, 150 },  // step3: turn on LED 150 ms
  { LED_BLINK_HOLD, LED_STATE_OFF, 100 }, // step4: turn off LED 100 ms
  { LED_BLINK_STOP, 0, 0 },               // step5: stop blink (off)
};

static uint32_t s_main_timer; // timing for main working loop
static int s_retry_num           = 0;
static bool s_sta_connected_flag = false;

static EventGroupHandle_t s_wifi_event_group;

// Signal Wi-Fi events on this event-group
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

QueueHandle_t g_event_queue;

typedef enum {
  APP_WIFI_PROV_INIT,
  APP_WIFI_PROV_START,
  APP_WIFI_PROV_SUCCESS,
  APP_WIFI_PROV_MAX
} app_wifi_prov_status_t;

static app_wifi_prov_status_t s_wifi_prov_status = APP_WIFI_PROV_INIT;

///////////////////////////////////////////////////////////
//                   P E R S I S T A N T
///////////////////////////////////////////////////////////

// Set default configuration

/* clang-format off */
node_persistent_config_t g_persistent = {

  // General
  .nodeName   = VSCP_PROJDEF_DEVICE_NAME,
  .pmk        = { 0 },
  .lmk        = { 0 },
  .startDelay = 2,
  .bootCnt    = 0,
  .queueSize  = CONFIG_APP_ESPNOW_QUEUE_SIZE,

  // Logging
  .logwrite2Stdout = 1,
  .logLevel        = ESP_LOG_INFO,
  .logType         = ALPHA_LOG_UDP,
  .logRetries      = 5,
  .logUrl          = "255.255.255.255",
  .logPort         = 6789,
  .logMqttTopic    = "{{guid}}/log",

  // Web server
  .webEnable   = true,
  .webPort     = 80,
  .webUsername = "vscp",
  .webPassword = "secret",

  // VSCP tcp/ip Link
  .vscplinkEnable   = true,
  .vscplinkUrl      = { 0 },
  .vscplinkPort     = VSCP_DEFAULT_TCP_PORT,
  .vscplinkUsername = "vscp",
  .vscplinkPassword = "secret",
  .vscpLinkKey      = { 0 }, // VSCP_DEFAULT_KEY32,

  // MQTT
  .mqttEnable       = true,
  .mqttUrl          = { 0 },
  .mqttPort         = 1883,
  .mqttClientid     = "{{node}}-{{guid}}",
  .mqttUsername     = "vscp",
  .mqttPassword     = "secret",
  .mqttQos          = 0,
  .mqttRetain       = 0,
  .mqttSub          = "vscp/{{guid}}/pub/#",
  .mqttPub          = "vscp/{{guid}}/{{class}}/{{type}}/{{index}}",
  .mqttPubLog       = "vscp/log/{{guid}}",
  .mqttVerification = { 0 },
  .mqttLwTopic      = { 0 },
  .mqttLwMessage    = { 0 },
  .mqttLwQos        = 0,
  .mqttLwRetain     = false,

  // espnow
  .espnowEnable                = true,
  .espnowLongRange             = false,
  .espnowChannel               = 0,                      // Use wifi channel (zero is same as STA)
  .espnowTtl                   = 32,
  .espnowSizeQueue             = 32,                     // Size fo input queue
  .espnowForwardEnable         = true,                   // Forward when packets are received
  .espnowEncryption            = VSCP_ENCRYPTION_AES128, // 0=no encryption, 1=AES-128, 2=AES-192, 3=AES-256
  .espnowFilterAdjacentChannel = true,                   // Don't receive if from other channel
  .espnowForwardSwitchChannel  = false,                  // Allow switching channel on forward
  .espnowFilterWeakSignal      = -55,                    // Filter on RSSI (zero is no rssi filtering)
};
/* clang-format on */

#if CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3
#define CONTROL_KEY_GPIO GPIO_NUM_9
#else
#define CONTROL_KEY_GPIO GPIO_NUM_0
#endif

#define WIFI_PROV_KEY_GPIO GPIO_NUM_0

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
// app_led_switch_blink_type
//

void
app_led_switch_blink_type(led_indicator_handle_t h, int type)
{
  static int last = 0;

  led_indicator_stop(h, last);
  led_indicator_start(s_led_handle_green, type);
  last = type;
}

///////////////////////////////////////////////////////////////////////////////
// get_device_guid
//

bool
get_device_guid(uint8_t *pguid)
{
  esp_err_t rv;
  size_t length = 16;

  // Check pointer
  if (NULL == pguid) {
    return false;
  }

  rv = nvs_get_blob(g_nvsHandle, "guid", pguid, &length);
  switch (rv) {

    case ESP_OK:
      break;

    case ESP_ERR_NVS_NOT_FOUND:
      printf("GUID not found in nvs\n");
      return false;

    default:
      printf("Error (%s) reading GUID from nvs!\n", esp_err_to_name(rv));
      return false;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// app_led_init
//

static void
app_led_init(void)
{
  led_indicator_gpio_config_t led_indicator_gpio_red_config = {
    .gpio_num             = PRJDEF_INDICATOR_LED_PIN_RED, /**< num of GPIO */
    .is_active_level_high = 1,
  };

  // Initialize red LED indicator
  led_indicator_config_t indicator_config_red = {
    .mode                      = LED_GPIO_MODE,
    .led_indicator_gpio_config = &led_indicator_gpio_red_config,
    .blink_lists               = default_led_indicator_blink_lists,
    .blink_list_num            = DEFAULT_BLINK_LIST_NUM,
  };

  // s_led_handle_green = led_indicator_create(PRJDEF_INDICATOR_LED_PIN_GREEN, &indicator_config_green);
  s_led_handle_green = led_indicator_create(&indicator_config_red);
  if (NULL == s_led_handle_green) {
    ESP_LOGE(TAG, "Failed to create LED indicator green");
  }

  led_indicator_gpio_config_t led_indicator_gpio_green_config = {
    .gpio_num             = PRJDEF_INDICATOR_LED_PIN_GREEN, /**< num of GPIO */
    .is_active_level_high = 1,
  };

  // Initialize green LED indicator
  led_indicator_config_t indicator_config_green = {
    .mode                      = LED_GPIO_MODE,
    .led_indicator_gpio_config = &led_indicator_gpio_green_config,
    .blink_lists               = default_led_indicator_blink_lists,
    .blink_list_num            = DEFAULT_BLINK_LIST_NUM,
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
// app_espnow_debug_recv_process
//
// Log data from beta and gama nodes
//

static esp_err_t
app_espnow_debug_recv_process(uint8_t *src_addr, void *data, size_t size, wifi_pkt_rx_ctrl_t *rx_ctrl)
{
  char *recv_data    = (char *) data;
  const char *buf    = NULL;
  size_t size_buffer = 50 + size;

  ESP_PARAM_CHECK(src_addr);
  ESP_PARAM_CHECK(data);
  ESP_PARAM_CHECK(size);
  ESP_PARAM_CHECK(rx_ctrl);

  if (g_persistent.mqttEnable) {

    buf = VSCP_CALLOC(size_buffer);
    if (NULL == buf) {
      ESP_LOGE(TAG, "Unable to allocate buffer for log message.");
      return ESP_ERR_NO_MEM;
    }

    snprintf(buf,
             size_buffer,
             "[" MACSTR "][%d][%d]: %.*s",
             MAC2STR(src_addr),
             size,
             rx_ctrl->channel,
             rx_ctrl->rssi,
             recv_data);
             
    mqtt_log(buf);
    VSCP_FREE(buf);
  }

  // printf("[" MACSTR "][%d][%d]: %.*s", MAC2STR(src_addr), rx_ctrl->channel, rx_ctrl->rssi, size, recv_data);
  // fflush(stdout);

  return ESP_OK;
}

//-----------------------------------------------------------------------------
//                                    OTA
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
// _http_event_handler
//

esp_err_t
_http_event_handler(esp_http_client_event_t *evt)
{
  switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
      ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
      break;
    case HTTP_EVENT_ON_CONNECTED:
      ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
      break;
    case HTTP_EVENT_HEADER_SENT:
      ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
      break;
    case HTTP_EVENT_ON_HEADER:
      ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
      break;
    case HTTP_EVENT_ON_DATA:
      ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
      break;
    case HTTP_EVENT_ON_FINISH:
      ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
      break;
    case HTTP_EVENT_DISCONNECTED:
      ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
      break;
    case HTTP_EVENT_REDIRECT:
      ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
      break;
  }

  return ESP_OK;
}

///////////////////////////////////////////////////////////////////////////////
// ota_task
//

void
ota_task(void *pvParameter)
{
  ESP_LOGI(TAG, "Starting OTA ");
#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF
  esp_netif_t *netif = get_example_netif_from_desc(bind_interface_name);
  if (netif == NULL) {
    ESP_LOGE(TAG, "Can't find netif from interface description");
    abort();
  }
  struct ifreq ifr;
  esp_netif_get_netif_impl_name(netif, ifr.ifr_name);
  ESP_LOGI(TAG, "Bind interface name is %s", ifr.ifr_name);
#endif
  esp_http_client_config_t config_http = {
    .url = PRJDEF_FIRMWARE_UPGRADE_URL,
    //.cert_pem          = (char *) server_cert_pem_start,
    .crt_bundle_attach = esp_crt_bundle_attach,
    .event_handler     = _http_event_handler,
    .keep_alive_enable = true,
#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF
    .if_name = &ifr,
#endif
  };

#ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
  config.skip_cert_common_name_check = true;
#endif

  esp_https_ota_config_t config = {
    .http_config           = &config_http,
    .bulk_flash_erase      = true,
    .partial_http_download = false,
    //.max_http_request_size
  };

  esp_err_t ret = esp_https_ota(&config);
  if (ret == ESP_OK) {
    esp_restart();
  }
  else {
    ESP_LOGE(TAG, "Firmware upgrade failed");
  }
  while (1) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

///////////////////////////////////////////////////////////////////////////////
// startOTA
//

void
startOTA(void)
{
  xTaskCreate(&ota_task, "ota_task", 8192, NULL, 5, NULL);
}

///////////////////////////////////////////////////////////////////////////////
// print_sha256
//

static void
print_sha256(const uint8_t *image_hash, const char *label)
{
  char hash_print[ESPNOW_OTA_HASH_LEN * 2 + 1];
  hash_print[ESPNOW_OTA_HASH_LEN * 2] = 0;
  for (int i = 0; i < ESPNOW_OTA_HASH_LEN; ++i) {
    sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
  }
  ESP_LOGI(TAG, "%s %s", label, hash_print);
}

///////////////////////////////////////////////////////////////////////////////
// get_sha256_of_partitions
//

static void
get_sha256_of_partitions(void)
{
  uint8_t sha_256[ESPNOW_OTA_HASH_LEN] = { 0 };
  esp_partition_t partition;

  // get sha256 digest for bootloader
  partition.address = ESP_BOOTLOADER_OFFSET;
  partition.size    = ESP_PARTITION_TABLE_OFFSET;
  partition.type    = ESP_PARTITION_TYPE_APP;
  esp_partition_get_sha256(&partition, sha_256);
  print_sha256(sha_256, "SHA-256 for bootloader: ");

  // get sha256 digest for running partition
  esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
  print_sha256(sha_256, "SHA-256 for current firmware: ");
}

//-----------------------------------------------------------------------------
//                                 espnow OTA
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
// firmware_download
//

size_t
app_firmware_download(const char *url)
{
#define OTA_DATA_PAYLOAD_LEN 1024

  esp_err_t ret               = ESP_OK;
  esp_ota_handle_t ota_handle = 0;
  uint8_t *data               = malloc(OTA_DATA_PAYLOAD_LEN); // TODO ESP MALLOC
  size_t total_size           = 0;
  uint32_t start_time         = xTaskGetTickCount();

  esp_http_client_config_t config = {
    .url               = url,
    .transport_type    = HTTP_TRANSPORT_UNKNOWN,
    .crt_bundle_attach = esp_crt_bundle_attach,
    .event_handler     = _http_event_handler,
    .keep_alive_enable = true,
#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF
    .if_name = &ifr,
#endif
  };

  /**
   * @brief 1. Connect to the server
   */
  esp_http_client_handle_t client = esp_http_client_init(&config);
  ESP_GOTO_ON_ERROR(!client, EXIT, TAG, "Initialise HTTP connection");

  ESP_LOGI(TAG, "Open HTTP connection: %s", url);

  /**
   * @brief First, the firmware is obtained from the http server and stored
   */
  do {
    ret = esp_http_client_open(client, 0);

    if (ret != ESP_OK) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP_LOGW(TAG, "<%s> Connection service failed", esp_err_to_name(ret));
    }
  } while (ret != ESP_OK);

  total_size = esp_http_client_fetch_headers(client);

  if (total_size <= 0) {
    ESP_LOGW(TAG, "Please check the address of the server");
    ret = esp_http_client_read(client, (char *) data, OTA_DATA_PAYLOAD_LEN);
    ESP_GOTO_ON_ERROR(ret < 0, EXIT, TAG, "<%s> Read data from http stream", esp_err_to_name(ret));

    ESP_LOGW(TAG, "Recv data: %.*s", ret, data);
    goto EXIT;
  }

  /**
   * @brief 2. Read firmware from the server and write it to the flash of the root node
   */

  const esp_partition_t *updata_partition = esp_ota_get_next_update_partition(NULL);
  /**< Commence an OTA update writing to the specified partition. */
  ret = esp_ota_begin(updata_partition, total_size, &ota_handle);
  ESP_GOTO_ON_ERROR(ret != ESP_OK, EXIT, TAG, "<%s> esp_ota_begin failed, total_size", esp_err_to_name(ret));

  for (ssize_t size = 0, recv_size = 0; recv_size < total_size; recv_size += size) {
    size = esp_http_client_read(client, (char *) data, OTA_DATA_PAYLOAD_LEN);
    ESP_GOTO_ON_ERROR(size < 0, EXIT, TAG, "<%s> Read data from http stream", esp_err_to_name(ret));

    if (size > 0) {
      /**< Write OTA update data to partition */
      ret = esp_ota_write(ota_handle, data, OTA_DATA_PAYLOAD_LEN);
      ESP_GOTO_ON_ERROR(ret != ESP_OK,
                        EXIT,
                        TAG,
                        "<%s> Write firmware to flash, size: %d, data: %.*s",
                        esp_err_to_name(ret),
                        size,
                        size,
                        data);
    }
    else {
      ESP_LOGW(TAG, "<%s> esp_http_client_read", esp_err_to_name((int) ret));
      goto EXIT;
    }
  }

  ESP_LOGI(TAG,
           "The service download firmware is complete, Spend time: %ds",
           (int) ((xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS / 1000));

  ret = esp_ota_end(ota_handle);
  ESP_GOTO_ON_ERROR(ret != ESP_OK, EXIT, TAG, "<%s> esp_ota_end", esp_err_to_name(ret));

EXIT:
  free(data);
  esp_http_client_close(client);
  esp_http_client_cleanup(client);

  return total_size;
}

///////////////////////////////////////////////////////////////////////////////
// ota_initiator_data_cb
//

esp_err_t
app_ota_initiator_data_cb(size_t src_offset, void *dst, size_t size)
{
  static const esp_partition_t *data_partition = NULL;

  if (!data_partition) {
    data_partition = esp_ota_get_next_update_partition(NULL);
  }

  return esp_partition_read(data_partition, src_offset, dst, size);
}

///////////////////////////////////////////////////////////////////////////////
// firmware_send
//

void
app_firmware_send(size_t firmware_size, uint8_t sha[ESPNOW_OTA_HASH_LEN])
{
  esp_err_t ret                         = ESP_OK;
  uint32_t start_time                   = xTaskGetTickCount();
  espnow_ota_result_t espnow_ota_result = { 0 };
  espnow_ota_responder_t *info_list     = NULL;
  espnow_addr_t *dest_addr_list         = NULL;
  size_t num                            = 0;

  espnow_ota_initiator_scan(&info_list, &num, pdMS_TO_TICKS(3000));
  ESP_LOGW(TAG, "espnow wait ota num: %u", num);

  if (!num) {
    goto EXIT;
  }

  dest_addr_list = ESP_MALLOC(num * ESPNOW_ADDR_LEN);

  for (size_t i = 0; i < num; i++) {
    memcpy(dest_addr_list[i], info_list[i].mac, ESPNOW_ADDR_LEN);
  }

  espnow_ota_initiator_scan_result_free();

  ret =
    espnow_ota_initiator_send(dest_addr_list, num, sha, firmware_size, app_ota_initiator_data_cb, &espnow_ota_result);
  ESP_ERROR_GOTO(ret != ESP_OK, EXIT, "<%s> espnow_ota_initiator_send", esp_err_to_name(ret));

  if (espnow_ota_result.successed_num == 0) {
    ESP_LOGW(TAG, "Devices upgrade failed, unfinished_num: %u", espnow_ota_result.unfinished_num);
    goto EXIT;
  }

  ESP_LOGI(TAG,
           "Firmware is sent to the device to complete, Spend time: %" PRIu32 "s",
           (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS / 1000);
  ESP_LOGI(TAG,
           "Devices upgrade completed, successed_num: %u, unfinished_num: %u",
           espnow_ota_result.successed_num,
           espnow_ota_result.unfinished_num);

EXIT:
  ESP_FREE(dest_addr_list);
  espnow_ota_initiator_result_free(&espnow_ota_result);
}

///////////////////////////////////////////////////////////////////////////////
// app_initiate_firmware_upload
//

int
app_initiate_firmware_upload(const char *url)
{
  const char *url_to_upload             = url;
  uint8_t sha_256[32]                   = { 0 };
  const esp_partition_t *data_partition = esp_ota_get_next_update_partition(NULL);

  if (NULL == url) {
    url_to_upload = PRJDEF_FIRMWARE_UPGRADE_URL;
  }

  size_t firmware_size = app_firmware_download(url_to_upload);
  esp_partition_get_sha256(data_partition, sha_256);

  // Send new firmware to clients
  app_firmware_send(firmware_size, sha_256);

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// respondToFirmwareUpload
//

int
respondToFirmwareUpload(void)
{
  espnow_ota_config_t ota_config = {
    .skip_version_check       = true,
    .progress_report_interval = 10,
  };

  // Take care of firmware update of out node
  espnow_ota_responder_start(&ota_config);

  return VSCP_ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//                               Key handlers
// ----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
// app_wifi_prov_over_espnow_start_press_cb
//

static void
app_wifi_prov_over_espnow_start_press_cb(void *arg, void *usr_data)
{
  ESP_ERROR_CHECK(!(BUTTON_SINGLE_CLICK == iot_button_get_event(arg)));

  ESP_LOGI(TAG, "espnow security provisioning");

  // app_prov_responder_init();
  // vscp_espnow_sec_initiator();

  app_led_switch_blink_type(s_led_handle_green, BLINK_PROVISIONING);

  // if (s_wifi_prov_status == APP_WIFI_PROV_SUCCESS) {

  //   bool enabled;
  //   espnow_get_config_for_data_type(ESPNOW_DATA_TYPE_PROV, &enabled);

  //   if (enabled) {
  //     ESP_LOGI(TAG, "========>>> Channel setup ESP-NOW is started");

  //     //  Start 30s prov beacon
  //     app_espnow_prov_beacon_start(60);
  //   }
  //   else {
  //     ESP_LOGI(TAG, "Start WiFi provisioning over ESP-NOW on initiator");

  //     app_prov_responder_init();

  //     app_led_switch_blink_type(s_led_handle_green, BLINK_PROVISIONING);
  //   }
  // }
  // else if (s_wifi_prov_status == APP_WIFI_PROV_START) {
  //   ESP_LOGI(TAG, "Please finish WiFi provisioning first");
  // }
  // else {
  //   ESP_LOGI(TAG, "Please start WiFi provisioning first");
  // }
  // ERROR:
}

///////////////////////////////////////////////////////////////////////////////
// app_wifi_prov_start_press_cb
//

static void
app_wifi_prov_start_press_cb(void *arg, void *usr_data)
{
  ESP_ERROR_CHECK(!(BUTTON_DOUBLE_CLICK == iot_button_get_event(arg)));

  ESP_LOGI(TAG, "Double click");

  if (s_wifi_prov_status == APP_WIFI_PROV_INIT) {

    ESP_LOGI(TAG, "Starting WiFi provisioning on initiator");
    wifi_prov();
    s_wifi_prov_status = APP_WIFI_PROV_START;
    app_led_switch_blink_type(s_led_handle_green, BLINK_PROVISIONING);
  }
  else if (s_wifi_prov_status == APP_WIFI_PROV_START) {
    ESP_LOGI(TAG, "WiFi provisioning is running");
  }
  else {
    ESP_LOGI(TAG, "sec initiator started.");
    app_led_switch_blink_type(s_led_handle_green, BLINK_PROVISIONING);
    vscp_espnow_sec_initiator();
  }
}

///////////////////////////////////////////////////////////////////////////////
// app_long_press_start_cb
//

static void
app_long_press_start_cb(void *arg, void *usr_data)
{
  ESP_ERROR_CHECK(!(BUTTON_LONG_PRESS_START == iot_button_get_event(arg)));
  s_main_timer = getMilliSeconds();
}

///////////////////////////////////////////////////////////////////////////////
// app_factory_reset_press_cb
//

static void
app_factory_reset_press_cb(void *arg, void *usr_data)
{
  esp_err_t ret;
  ESP_ERROR_CHECK(!(BUTTON_LONG_PRESS_HOLD == iot_button_get_event(arg)));

  ESP_LOGI(TAG, "Restore factorey settings");

  // Unbound device
  ret = espnow_erase_key();
  if (ESP_OK != ret) {
    ESP_LOGE(TAG, "Failed to erase key %X", ret);
  }

  // Reset provisioning
  ret = wifi_prov_mgr_reset_provisioning();
  if (ESP_OK != ret) {
    ESP_LOGE(TAG, "Failed to reset provisioning %X", ret);
  }

  // Erase all settings
  ret = nvs_erase_all(g_nvsHandle);
  if (ESP_OK != ret) {
    ESP_LOGE(TAG, "Unable to erase configuration area");
  }
  nvs_commit(g_nvsHandle);

  // Disconnect from wifi
  ret = esp_wifi_disconnect();
  if (ESP_OK != ret) {
    ESP_LOGE(TAG, "Failed to disconnect from wifi %X", ret);
  }

  // Restart system (set defaults)
  esp_restart();

  ESP_LOGI(TAG, "Reset WiFi provisioning information and restart");
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_data_cb
//

static void
vscp_espnow_data_cb(uint8_t *src_addr, void *data, size_t size, wifi_pkt_rx_ctrl_t *rx_ctrl)
{
  esp_err_t ret;
  uint8_t ch                = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  if (ESP_OK != (ret = esp_wifi_get_channel(&ch, &second))) {
    ESP_LOGE(TAG, "Failed to get wifi channel, rv = %X", ret);
  }

  ESP_LOGI(TAG,
           "esp-now data received. len=%zd ch=%d (%d) src=" MACSTR " rssi=%d",
           size,
           ch,
           second,
           MAC2STR(src_addr),
           rx_ctrl->rssi);
}

///////////////////////////////////////////////////////////////////////////////
// app_button_init
//

static void
app_button_init(void)
{
  button_config_t button_config = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = 3000,
        .gpio_button_config = {
            .gpio_num = WIFI_PROV_KEY_GPIO,
            .active_level = 0,
        },
    };

  s_init_button_handle = iot_button_create(&button_config);

  iot_button_register_cb(s_init_button_handle, BUTTON_SINGLE_CLICK, app_wifi_prov_over_espnow_start_press_cb, NULL);
  iot_button_register_cb(s_init_button_handle, BUTTON_DOUBLE_CLICK, app_wifi_prov_start_press_cb, NULL);
  iot_button_register_cb(s_init_button_handle, BUTTON_LONG_PRESS_START, app_long_press_start_cb, NULL);
  iot_button_register_cb(s_init_button_handle, BUTTON_LONG_PRESS_HOLD, app_factory_reset_press_cb, NULL);
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

  // Set default primary key
  vscp_fwhlp_hex2bin(g_persistent.vscpLinkKey, 16, VSCP_DEFAULT_KEY16);

  // boot counter
  rv = nvs_get_u32(g_nvsHandle, "boot_counter", &g_persistent.bootCnt);
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
  rv = nvs_set_u32(g_nvsHandle, "boot_counter", g_persistent.bootCnt);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to update boot counter");
  }

  // Node name
  rv = nvs_get_str(g_nvsHandle, "node_name", buf, &length);
  switch (rv) {
    case ESP_OK:
      strncpy(g_persistent.nodeName, buf, sizeof(g_persistent.nodeName));
      ESP_LOGI(TAG, "Node Name = %s", g_persistent.nodeName);
      break;

    case ESP_ERR_NVS_NOT_FOUND:
      rv = nvs_set_str(g_nvsHandle, "node_name", g_persistent.nodeName);
      if (rv != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update node name");
      }
      break;

    default:
      ESP_LOGE(TAG, "Error (%s) reading 'node_name'!", esp_err_to_name(rv));
      break;
  }

  // Start Delay (seconds)
  rv = nvs_get_u8(g_nvsHandle, "start_delay", &g_persistent.startDelay);
  switch (rv) {

    case ESP_OK:
      ESP_LOGI(TAG, "Start delay = %d", g_persistent.startDelay);
      break;

    case ESP_ERR_NVS_NOT_FOUND:
      rv = nvs_set_u8(g_nvsHandle, "start_delay", 2);
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
  rv     = nvs_get_blob(g_nvsHandle, "pmk", g_persistent.pmk, &length);
  if (rv != ESP_OK) {
    const char key[] = VSCP_DEFAULT_KEY16;
    const char *pos  = key;
    for (int i = 0; i < 16; i++) {
      sscanf(pos, "%2hhx", &g_persistent.pmk[i]);
      pos += 2;
    }
    rv = nvs_set_blob(g_nvsHandle, "pmk", g_persistent.pmk, 16);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to write node pmk to nvs. rv=%d", rv);
    }
  }

  // espnow queueSize
  rv = nvs_get_u8(g_nvsHandle, "queue_size", &g_persistent.queueSize);
  if (ESP_OK != rv) {
    rv = nvs_set_u8(g_nvsHandle, "queue_size", g_persistent.queueSize);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update queue_size");
    }
  }

  // Logging ------------------------------------------------------------------

  // logwrite2Stdout
  rv = nvs_get_u8(g_nvsHandle, "log_stdout", &g_persistent.logwrite2Stdout);
  if (ESP_OK != rv) {
    rv = nvs_set_u8(g_nvsHandle, "log_stdout", g_persistent.logwrite2Stdout);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update log-stdout");
    }
  }

  // logLevel
  esp_log_level_set("*", ESP_LOG_INFO);
  rv = nvs_get_u8(g_nvsHandle, "log_level", &g_persistent.logLevel);
  if (ESP_OK != rv) {
    rv = nvs_set_u8(g_nvsHandle, "log_level", g_persistent.logLevel);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update log-level");
    }
  }

  // logType
  rv = nvs_get_u8(g_nvsHandle, "log_type", &g_persistent.logType);
  if (ESP_OK != rv) {
    rv = nvs_set_u8(g_nvsHandle, "log_type", g_persistent.logType);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update log-type");
    }
  }

  // logRetries
  rv = nvs_get_u8(g_nvsHandle, "log_retries", &g_persistent.logRetries);
  if (ESP_OK != rv) {
    rv = nvs_set_u8(g_nvsHandle, "log:retries", g_persistent.logRetries);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update log-retries");
    }
  }

  // logUrl
  length = sizeof(g_persistent.logUrl);
  rv     = nvs_get_str(g_nvsHandle, "log_url", g_persistent.logUrl, &length);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read 'log URL' will be set to default. ret=%d", rv);
    rv = nvs_set_str(g_nvsHandle, "log_url", g_persistent.logUrl);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save log URL");
    }
  }

  // logPort
  rv = nvs_get_u16(g_nvsHandle, "log_port", &g_persistent.logPort);
  if (ESP_OK != rv) {
    rv = nvs_set_u16(g_nvsHandle, "log_port", g_persistent.logPort);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update log_port");
    }
  }

  // logMqttTopic
  length = sizeof(g_persistent.logMqttTopic);
  rv     = nvs_get_str(g_nvsHandle, "log_mqtt_topic", g_persistent.logMqttTopic, &length);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read 'log MQTT topic' will be set to default. ret=%d", rv);
    rv = nvs_set_str(g_nvsHandle, "log_mqtt_topic", g_persistent.logMqttTopic);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save log MQTT topic");
    }
  }

  // VSCP Link ----------------------------------------------------------------

  // VSCP Link enable
  rv = nvs_get_u8(g_nvsHandle, "vscp_enable", &val);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read 'VSCP link enable' will be set to default. ret=%d", rv);
    val = (uint8_t) g_persistent.vscplinkEnable;
    rv  = nvs_set_u8(g_nvsHandle, "vscp_enable", g_persistent.vscplinkEnable);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save VSCP link enable");
    }
  }
  else {
    g_persistent.vscplinkEnable = (bool) val;
  }

  // VSCP Link host
  length = sizeof(g_persistent.vscplinkUrl);
  rv     = nvs_get_str(g_nvsHandle, "vscp_url", g_persistent.vscplinkUrl, &length);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read 'VSCP link host' will be set to default. ret=%d", rv);
    rv = nvs_set_str(g_nvsHandle, "vscp_url", PRJDEF_DEFAULT_TCPIP_USER);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save VSCP link host");
    }
  }

  // VSCP link port
  rv = nvs_get_u16(g_nvsHandle, "vscp_port", &g_persistent.vscplinkPort);
  if (ESP_OK != rv) {
    rv = nvs_set_u16(g_nvsHandle, "vscp_port", g_persistent.vscplinkPort);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update VSCP link port");
    }
  }

  // VSCP Link key
  length = sizeof(g_persistent.vscpLinkKey);
  rv     = nvs_get_blob(g_nvsHandle, "vscp_key", (char *) g_persistent.vscpLinkKey, &length);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read 'VSCP link_key' will be set to default. ret=%d", rv);
    rv = nvs_set_blob(g_nvsHandle, "vscp_key", g_persistent.vscpLinkKey, sizeof(g_persistent.vscpLinkKey));
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save VSCL link key");
    }
  }

  // VSCP Link Username
  length = sizeof(g_persistent.vscplinkUsername);
  rv     = nvs_get_str(g_nvsHandle, "vscp_user", g_persistent.vscplinkUsername, &length);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read 'VSCP Username' will be set to default. ret=%d", rv);
    rv = nvs_set_str(g_nvsHandle, "vscp_user", PRJDEF_DEFAULT_TCPIP_USER);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save VSCP username");
    }
  }

  // VSCP Link password
  length = sizeof(g_persistent.vscplinkPassword);
  rv     = nvs_get_str(g_nvsHandle, "vscp_password", g_persistent.vscplinkPassword, &length);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read 'VSCP password' will be set to default. ret=%d", rv);
    nvs_set_str(g_nvsHandle, "vscp_password", PRJDEF_DEFAULT_TCPIP_PASSWORD);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save VSCP password");
    }
  }
  // ESP_LOGI(TAG, "VSCP Password: %s", buf);

  // MQTT ----------------------------------------------------------------

  // MQTT enable
  rv = nvs_get_u8(g_nvsHandle, "mqtt_enable", &val);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read 'mqtt enable' will be set to default. ret=%d", rv);
    val = (uint8_t) g_persistent.mqttEnable;
    rv  = nvs_set_u8(g_nvsHandle, "mqtt_enable", g_persistent.mqttEnable);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save MQTT enable");
    }
  }
  else {
    g_persistent.mqttEnable = (bool) val;
  }

  // MQTT host
  length = sizeof(g_persistent.mqttUrl);
  rv     = nvs_get_str(g_nvsHandle, "mqtt_url", g_persistent.mqttUrl, &length);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read 'MQTT host' will be set to default. ret=%d", rv);
    rv = nvs_set_str(g_nvsHandle, "mqtt_url", g_persistent.mqttUrl);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save MQTT host");
    }
  }

  // MQTT port
  rv = nvs_get_u16(g_nvsHandle, "mqtt_port", &g_persistent.mqttPort);
  if (ESP_OK != rv) {
    rv = nvs_set_u16(g_nvsHandle, "mqtt_port", g_persistent.mqttPort);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update MQTT port");
    }
  }

  // MQTT client
  length = sizeof(g_persistent.mqttClientid);
  rv     = nvs_get_str(g_nvsHandle, "mqtt_cid", g_persistent.mqttClientid, &length);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read 'MQTT clientid' will be set to default. ret=%d", rv);
    rv = nvs_set_str(g_nvsHandle, "mqtt_cid", g_persistent.mqttClientid);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save MQTT clientid");
    }
  }

  // MQTT Link Username
  length = sizeof(g_persistent.mqttUsername);
  rv     = nvs_get_str(g_nvsHandle, "mqtt_user", g_persistent.mqttUsername, &length);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read 'MQTT user' will be set to default. ret=%d", rv);
    rv = nvs_set_str(g_nvsHandle, "mqtt_user", PRJDEF_DEFAULT_TCPIP_USER);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save MQTT username");
    }
  }

  // MQTT password
  length = sizeof(g_persistent.mqttPassword);
  rv     = nvs_get_str(g_nvsHandle, "mqtt_password", g_persistent.mqttPassword, &length);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read 'MQTT password' will be set to default. ret=%d", rv);
    nvs_set_str(g_nvsHandle, "mqtt_password", PRJDEF_DEFAULT_TCPIP_PASSWORD);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save MQTT password");
    }
  }

  // MQTT subscribe
  length = sizeof(g_persistent.mqttSub);
  rv     = nvs_get_str(g_nvsHandle, "mqtt_sub", g_persistent.mqttSub, &length);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read 'MQTT sub' will be set to default. ret=%d", rv);
    rv = nvs_set_str(g_nvsHandle, "mqtt_sub", g_persistent.mqttSub);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save MQTT sub");
    }
  }

  // MQTT publish
  length = sizeof(g_persistent.mqttPub);
  rv     = nvs_get_str(g_nvsHandle, "mqtt_pub", g_persistent.mqttPub, &length);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read 'MQTT pub' will be set to default. ret=%d", rv);
    rv = nvs_set_str(g_nvsHandle, "mqtt_pub", g_persistent.mqttPub);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save MQTT pub");
    }
  }

  // MQTT publish log
  length = sizeof(g_persistent.mqttPubLog);
  rv     = nvs_get_str(g_nvsHandle, "mqtt_pub_log", g_persistent.mqttPubLog, &length);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read 'MQTT pub log' will be set to default. ret=%d", rv);
    rv = nvs_set_str(g_nvsHandle, "mqtt_pub_log", g_persistent.mqttPubLog);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save MQTT pub log");
    }
  }

  // WEB server ----------------------------------------------------------------

  // WEB enable
  rv = nvs_get_u8(g_nvsHandle, "web_enable", &val);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read 'web enable' will be set to default. ret=%d", rv);
    val = (uint8_t) g_persistent.webEnable;
    rv  = nvs_set_u8(g_nvsHandle, "web_enable", g_persistent.webEnable);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save web enable");
    }
  }
  else {
    g_persistent.webEnable = (bool) val;
  }

  // WEB port
  rv = nvs_get_u16(g_nvsHandle, "web_port", &g_persistent.webPort);
  if (ESP_OK != rv) {
    rv = nvs_set_u16(g_nvsHandle, "web_port", g_persistent.webPort);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update Web server port");
    }
  }

  // WEB Username
  length = sizeof(g_persistent.webUsername);
  rv     = nvs_get_str(g_nvsHandle, "web_user", g_persistent.webUsername, &length);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read 'Web server user' will be set to default. ret=%d", rv);
    rv = nvs_set_str(g_nvsHandle, "web_user", PRJDEF_DEFAULT_TCPIP_USER);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save Web Server username");
    }
  }

  // WEB password
  length = sizeof(g_persistent.webPassword);
  rv     = nvs_get_str(g_nvsHandle, "web_password", g_persistent.webPassword, &length);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read 'Web server password' will be set to default. ret=%d", rv);
    nvs_set_str(g_nvsHandle, "web_password", PRJDEF_DEFAULT_TCPIP_PASSWORD);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save Web server password");
    }
  }

  // espnow ----------------------------------------------------------------

  // VSCP Link enable
  rv = nvs_get_u8(g_nvsHandle, "drop_enable", &val);
  if (rv != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read 'espnow enable' will be set to default. ret=%d", rv);
    val = (uint8_t) g_persistent.espnowEnable;
    rv  = nvs_set_u8(g_nvsHandle, "drop_enable", g_persistent.espnowEnable);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save VSCP link enable");
    }
  }
  else {
    g_persistent.espnowEnable = (bool) val;
  }

  // Long Range
  rv = nvs_get_u8(g_nvsHandle, "drop_lr", &val);
  if (ESP_OK != rv) {
    val = (uint8_t) g_persistent.espnowLongRange;
    rv  = nvs_set_u8(g_nvsHandle, "drop_lr", g_persistent.espnowLongRange);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update espnow long range");
    }
  }
  else {
    g_persistent.espnowLongRange = (bool) val;
  }

  // Channel
  rv = nvs_get_u8(g_nvsHandle, "drop_ch", &g_persistent.espnowChannel);
  if (ESP_OK != rv) {
    rv = nvs_set_u8(g_nvsHandle, "drop_ch", g_persistent.espnowChannel);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update espnow channel");
    }
  }

  // Default queue size
  rv = nvs_get_u8(g_nvsHandle, "drop_qsize", &g_persistent.espnowSizeQueue);
  if (ESP_OK != rv) {
    rv = nvs_set_u8(g_nvsHandle, "drop_qsize", g_persistent.espnowSizeQueue);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update espnow queue size");
    }
  }

  // Default ttl
  rv = nvs_get_u8(g_nvsHandle, "drop_ttl", &g_persistent.espnowTtl);
  if (ESP_OK != rv) {
    rv = nvs_set_u8(g_nvsHandle, "drop_ttl", g_persistent.espnowTtl);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update espnow ttl");
    }
  }

  // Forward
  rv = nvs_get_u8(g_nvsHandle, "drop_fw", &val);
  if (ESP_OK != rv) {
    val = (uint8_t) g_persistent.espnowForwardEnable;
    rv  = nvs_set_u8(g_nvsHandle, "drop_fw", g_persistent.espnowForwardEnable);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update espnow forward");
    }
  }
  else {
    g_persistent.espnowForwardEnable = (bool) val;
  }

  // Encryption
  rv = nvs_get_u8(g_nvsHandle, "drop_enc", &g_persistent.espnowEncryption);
  if (ESP_OK != rv) {
    rv = nvs_set_u8(g_nvsHandle, "drop_enc", g_persistent.espnowEncryption);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update espnow encryption");
    }
  }

  // Adj filter channel
  rv = nvs_get_u8(g_nvsHandle, "drop_filt", &val);
  if (ESP_OK != rv) {
    val = (uint8_t) g_persistent.espnowFilterAdjacentChannel;
    rv  = nvs_set_u8(g_nvsHandle, "drop_filt", g_persistent.espnowFilterAdjacentChannel);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update espnow adj channel filter");
    }
  }
  else {
    g_persistent.espnowFilterAdjacentChannel = (bool) val;
  }

  // Allow switching channel on forward
  rv = nvs_get_u8(g_nvsHandle, "drop_swchf", &val);
  if (ESP_OK != rv) {
    val = (uint8_t) g_persistent.espnowForwardSwitchChannel;
    rv  = nvs_set_u8(g_nvsHandle, "drop_swchf", g_persistent.espnowForwardSwitchChannel);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update espnow shitch channel on forward");
    }
  }
  else {
    g_persistent.espnowFilterAdjacentChannel = (bool) val;
  }

  // RSSI limit
  rv = nvs_get_i8(g_nvsHandle, "drop_rssi", &g_persistent.espnowFilterWeakSignal);
  if (ESP_OK != rv) {
    rv = nvs_set_u8(g_nvsHandle, "drop_rssi", g_persistent.espnowFilterWeakSignal);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update espnow RSSI");
    }
  }

  rv = nvs_commit(g_nvsHandle);
  if (rv != ESP_OK) {
    ESP_LOGI(TAG, "Failed to commit updates to nvs\n");
  }

  return ESP_OK;
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

  if (event_base == WIFI_EVENT) {

    switch (event_id) {

      case WIFI_EVENT_WIFI_READY: {
        // Set channel
        // ESP_ERROR_CHECK(esp_wifi_set_channel(PRJDEF_espnow_CHANNEL, WIFI_SECOND_CHAN_NONE));
      } break;

      case WIFI_EVENT_STA_START: {
        ESP_LOGI(TAG, "Connecting........");
      }

      case WIFI_EVENT_STA_CONNECTED: {
        wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *) event_data;
        app_led_switch_blink_type(s_led_handle_green, BLINK_CONNECTING);
        ESP_LOGI(TAG,
                 "Connected to %s (BSSID: " MACSTR ", Channel: %d)",
                 event->ssid,
                 MAC2STR(event->bssid),
                 event->channel);
        g_persistent.espnowChannel = event->channel;
        // Save channel to persistent storage
        ret = nvs_set_u8(g_nvsHandle, "channel", g_persistent.espnowChannel);
        if (ret != ESP_OK) {
          ESP_LOGE(TAG, "Failed to update espnow channel %X", ret);
        }
        s_sta_connected_flag = true;
        break;
      }

      case WIFI_EVENT_STA_DISCONNECTED: {

        ESP_LOGI(TAG, "--------------> sta disconnect");

        if (s_retry_num < VSCP_PROJDEF_ESP_MAXIMUM_RETRY) {
          s_retry_num++;
          ESP_LOGI(TAG, "retry to connect to the AP");
          app_led_switch_blink_type(s_led_handle_green, BLINK_RECONNECTING);
          taskYIELD();
          esp_wifi_connect();
        }
        else {
          xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
          app_led_switch_blink_type(s_led_handle_green, BLINK_RECONNECTING);
        }
        break;
      }
      default:
        break;
    }
  }
  // Post 5.0 stable
  // ---------------
  else if (event_base == ESP_HTTPS_OTA_EVENT) {
    switch (event_id) {

      case ESP_HTTPS_OTA_START: {
        ESP_LOGI(TAG, "OTA https start");
      } break;

      case ESP_HTTPS_OTA_CONNECTED: {
        ESP_LOGI(TAG, "OTA https connected");
      } break;

      case ESP_HTTPS_OTA_GET_IMG_DESC: {
        ESP_LOGI(TAG, "OTA https get image description");
      } break;

      case ESP_HTTPS_OTA_VERIFY_CHIP_ID: {
        ESP_LOGI(TAG, "Verifying chip id of new image: %d", *(esp_chip_id_t *) event_data);
      } break;

      case ESP_HTTPS_OTA_DECRYPT_CB: {
        ESP_LOGI(TAG, "OTA https decrypt callback");
      } break;

      case ESP_HTTPS_OTA_WRITE_FLASH: {
        ESP_LOGI(TAG, "Writing to flash: %d written", *(int *) event_data);
      } break;

      case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION: {
        ESP_LOGI(TAG, "Boot partition updated. Next Partition: %d", *(esp_partition_subtype_t *) event_data);
      } break;

      case ESP_HTTPS_OTA_FINISH: {
        ESP_LOGI(TAG, "OTA https finish");
      } break;

      case ESP_HTTPS_OTA_ABORT: {
        ESP_LOGI(TAG, "OTA https abort");
      } break;
    }
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {

    s_retry_num = 0;

    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    ESP_LOGI(TAG, "Connected with IP Address: " IPSTR, IP2STR(&event->ip_info.ip));

    s_wifi_prov_status = APP_WIFI_PROV_SUCCESS;

    app_led_switch_blink_type(s_led_handle_green, BLINK_CONNECTED);

    // Signal main application to continue execution
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
  else if (event_base == ESP_EVENT_ESPNOW) {

    switch (event_id) {
      case ESP_EVENT_ESPNOW_LOG_FLASH_FULL: {
        ESP_LOGI(TAG, "The flash partition that stores the log is full, size: %d", espnow_log_flash_size());
        break;
      }
    }
  }
  // else if (event_base == ALPHA_EVENT) {
  //   ESP_LOGI(TAG, "Alpha event -----------------------------------------------------------> id=%ld", event_id);
  // }
}

// ----------------------------------------------------------------------------
//                                   Spiffs
// ----------------------------------------------------------------------------

void
app_init_spiffs(void)
{
  esp_err_t ret;

  // Initialize Spiffs for web pages
  ESP_LOGI(TAG, "Initializing SPIFFS");

  esp_vfs_spiffs_conf_t spiffsconf = { .base_path              = "/www",
                                       .partition_label        = "web",
                                       .max_files              = 50,
                                       .format_if_mount_failed = true };

  // Initialize and mount SPIFFS filesystem.
  ret = esp_vfs_spiffs_register(&spiffsconf);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount or format web filesystem");
    }
    else if (ret == ESP_ERR_NOT_FOUND) {
      ESP_LOGE(TAG, "Failed to find SPIFFS partition for web ");
    }
    else {
      ESP_LOGE(TAG, "Failed to initialize SPIFFS for web (%s)", esp_err_to_name(ret));
    }
    return;
  }

  ESP_LOGI(TAG, "Performing web SPIFFS_check().");
  ret = esp_spiffs_check(spiffsconf.partition_label);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SPIFFS_web check() failed (%s)", esp_err_to_name(ret));
    return;
  }
  else {
    ESP_LOGI(TAG, "SPIFFS_web check() successful");
  }

  ESP_LOGI(TAG, "SPIFFS for web initialized");

  size_t total = 0, used = 0;
  ret = esp_spiffs_info(spiffsconf.partition_label, &total, &used);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
    esp_spiffs_format(spiffsconf.partition_label);
    return;
  }
  else {
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
  }

  DIR *dir = opendir("/www");
  if (dir == NULL) {
    return;
  }

  while (true) {

    struct dirent *de = readdir(dir);
    if (!de) {
      break;
    }

    printf("Found file: %s\n", de->d_name);
  }

  closedir(dir);
}

///////////////////////////////////////////////////////////////////////////////
// app_main
//

void
app_main()
{
  esp_err_t ret;

  espnow_storage_init();

  wifi_prov_init();

  // Set default parameters for espnow
  //     Set here due to persistent writing
  espnow_config_t espnow_config = ESPNOW_INIT_CONFIG_DEFAULT();

  app_led_init();
  app_led_switch_blink_type(s_led_handle_green, BLINK_CONNECTING);

  // Initialize button behaviour
  app_button_init();

  // ----------------------------------------------------------------------------
  //                        NVS - Persistent storage
  // ----------------------------------------------------------------------------

  // Init persistent storage
  ESP_LOGI(TAG, "Read persistent storage ... ");

  ret = nvs_open("config", NVS_READWRITE, &g_nvsHandle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(ret));
  }
  else {
    // Read (or set to defaults) persistent values
    readPersistentConfigs();
  }

  s_wifi_event_group = xEventGroupCreate();

  memcpy((uint8_t *) espnow_config.pmk, g_persistent.pmk, 16);
  espnow_config.qsize                  = g_persistent.queueSize; // CONFIG_APP_ESPNOW_QUEUE_SIZE;
  espnow_config.sec_enable             = true; // Must be enabled for all security enabled functions to work
  espnow_config.forward_enable         = true;
  espnow_config.forward_switch_channel = 0;
  espnow_config.send_retry_num         = 10;
  espnow_config.send_max_timeout       = pdMS_TO_TICKS(3000);

  espnow_init(&espnow_config);

  // Init web file system
  app_init_spiffs();

  // Register our event handler for Wi-Fi, IP and Provisioning related events
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &app_system_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &app_system_event_handler, NULL));
  // ESP_ERROR_CHECK(esp_event_handler_register(ALPHA_EVENT, ESP_EVENT_ANY_ID, &system_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &app_system_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(ESP_EVENT_ESPNOW, ESP_EVENT_ANY_ID, &app_system_event_handler, NULL));

  // --------------------------------------------------------------------------

  // app_espnow_alpha();

  // --------------------------------------------------------------------------
  //                     Wait for Wi-Fi connection | WIFI_FAIL_BIT
  // --------------------------------------------------------------------------

  ESP_LOGI(TAG, "Waiting for wifi connection...");
  {
    EventBits_t bits;
    while (
      !(bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, false, false, 1000 / portTICK_PERIOD_MS))) {
      if (bits & WIFI_CONNECTED_BIT) {
        break;
      }
    }
  }

  // ----------------------------------------------------------------------------
  //                                  Logging
  // ----------------------------------------------------------------------------

  switch (g_persistent.logType) {

    case ALPHA_LOG_NONE:
      break;

    case ALPHA_LOG_UDP:
      ESP_ERROR_CHECK(udp_logging_init(g_persistent.logUrl, g_persistent.logPort, g_persistent.logwrite2Stdout));
      break;

    case ALPHA_LOG_TCP:
      ESP_ERROR_CHECK(tcp_logging_init(g_persistent.logUrl, g_persistent.logPort, g_persistent.logwrite2Stdout));
      break;

    case ALPHA_LOG_HTTP:
      ESP_ERROR_CHECK(http_logging_init(g_persistent.logUrl, g_persistent.logwrite2Stdout));
      break;

    case ALPHA_LOG_MQTT:
      ESP_ERROR_CHECK(mqtt_logging_init(g_persistent.logUrl, g_persistent.logMqttTopic, g_persistent.logwrite2Stdout));
      break;

    case ALPHA_LOG_VSCP:
      // ESP_ERROR_CHECK(mqtt_logging_init( CONFIG_LOG_MQTT_SERVER_URL, CONFIG_LOG_MQTT_PUB_TOPIC,
      // g_persistent.logwrite2Stdout ));
      break;

    case ALPHA_LOG_STD:
    default:
      break;
  }

  uint8_t key_info[APP_KEY_LEN];

  if (espnow_get_key(key_info) != ESP_OK) {
    ESP_LOGW(TAG, "Generate new security key");
    esp_fill_random(key_info, APP_KEY_LEN);
  }

  // !!! Use only for key setting debug. NEVER DISCLOSE !!!
  // ESP_LOGI(TAG, "Security Key: " KEYSTR, KEY2STR(key_info));
  espnow_set_key(key_info);

  // Simple test to set a common key
  // uint8_t key[32];
  // vscp_fwhlp_hex2bin(key, 32, "a3ea2c7811f37e29ed6a677da9d9bdedef64f3541dd0c27dd17ea6c127027efe");
  // espnow_set_key(key);

  // -------------------------------------------------------------------------------

  ESP_LOGI(TAG, "Starting time sync");

  // Initialize time synchronization
  ret = espnow_timesync_start();
  if (ESP_OK != ret) {
    ESP_LOGI(TAG, "Failed to start timesync %X", ret);
  }

  // ESP_LOGI(TAG, "Starting log");

  // espnow_console_config_t console_config = {
  //   .monitor_command.uart   = true,
  //   //.monitor_command.espnow = true,
  //   .store_history        = {
  //   .base_path = "/spiffs",
  //   .partition_label = "console" },
  //  };

  //  ret = espnow_console_init(&console_config);
  //  if (ESP_OK != ret) {
  //   ESP_LOGI(TAG,"Failed to init console %X", ret);
  //}

  // espnow_log_config_t log_config = {
  //   .log_level_uart   = ESP_LOG_INFO,
  //   .log_level_espnow = ESP_LOG_INFO,
  //   .log_level_flash  = ESP_LOG_INFO,
  // };

  // espnow_console_commands_register();

  // espnow_log_init(&log_config);
  //  esp_log_level_set("*", ESP_LOG_INFO);

  // -------------------------------------------------------------------------------

  // Setup VSCP esp-now

  // espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, vscp_espnow_data_cb);
  // if (ESP_OK != ret) {
  //   ESP_LOGE(TAG, "Failed to set VSCP event callback");
  // }

  // Start heartbeat task vscp_heartbeat_task
  // xTaskCreate(&vscp_espnow_heartbeat_task, "vscp_hb", 1024 * 3, NULL, 1, NULL);

  vscp_espnow_config_t vscp_espnow_conf;

  // Initialize VSCP espnow
  if (ESP_OK != vscp_espnow_init(&vscp_espnow_conf)) {
    ESP_LOGI(TAG, "Failed to initialize VSCP espnow");
  }

  // Start web server
  httpd_handle_t h_webserver;
  if (g_persistent.webEnable) {
    h_webserver = start_webserver();
  }

  // Start MQTT client
  if (g_persistent.mqttEnable) {
    mqtt_start();
  }

  // Start the VSCP Link Protocol Server
  if (g_persistent.vscplinkEnable) {
#ifdef PRJDEF_IPV6
    xTaskCreate(&tcpsrv_task, "vscp_tcpsrv_task", 4096, (void *) AF_INET6, 5, NULL);
#else
    xTaskCreate(&tcpsrv_task, "vscp_tcpsrv_task", 4096, (void *) AF_INET, 5, NULL);
#endif
  }

  ret = espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DEBUG_LOG, true, app_espnow_debug_recv_process);
  if (ESP_OK != ret) {
    ESP_LOGE(TAG, "Failed to set log callback");
  }

  ESP_LOGI(TAG, "Going to work now");

  // [BLINK_FACTORY_RESET]
  // [BLINK_UPDATING]
  // [BLINK_CONNECTED]
  // [BLINK_PROVISIONED]
  // [BLINK_RECONNECTING]
  // [BLINK_CONNECTING]
  // [BLINK_PROVISIONING]
  // led_indicator_start(s_led_handle_green, BLINK_FACTORY_RESET);
  // vTaskDelay(pdMS_TO_TICKS(3000));
  // led_indicator_stop(s_led_handle_green, BLINK_FACTORY_RESET);

  // led_indicator_start(s_led_handle_green, BLINK_UPDATING);
  // vTaskDelay(pdMS_TO_TICKS(3000));
  // led_indicator_stop(s_led_handle_green, BLINK_UPDATING);

  // led_indicator_start(s_led_handle_green, BLINK_CONNECTED);
  // vTaskDelay(pdMS_TO_TICKS(3000));
  // led_indicator_stop(s_led_handle_green, BLINK_CONNECTED);

  // led_indicator_start(s_led_handle_green, BLINK_PROVISIONED);
  // vTaskDelay(pdMS_TO_TICKS(3000));
  // led_indicator_stop(s_led_handle_green, BLINK_PROVISIONED);

  // led_indicator_start(s_led_handle_green, BLINK_RECONNECTING);
  // vTaskDelay(pdMS_TO_TICKS(3000));
  // led_indicator_stop(s_led_handle_green, BLINK_RECONNECTING);

  // led_indicator_start(s_led_handle_green, BLINK_CONNECTING);
  // vTaskDelay(pdMS_TO_TICKS(3000));
  // led_indicator_stop(s_led_handle_green, BLINK_CONNECTING);

  // led_indicator_start(s_led_handle_green, BLINK_PROVISIONING);
  // vTaskDelay(pdMS_TO_TICKS(3000));
  // led_indicator_stop(s_led_handle_green, BLINK_PROVISIONING);

  // led_indicator_start(s_led_handle_green, BLINK_FACTORY_RESET);

  // app_led_switch_blink_type(s_led_handle_green, BLINK_FACTORY_RESET);
  // vTaskDelay(pdMS_TO_TICKS(3000));

  // app_led_switch_blink_type(s_led_handle_green, BLINK_UPDATING);
  // vTaskDelay(pdMS_TO_TICKS(3000));

  // app_led_switch_blink_type(s_led_handle_green, BLINK_CONNECTED);
  // vTaskDelay(pdMS_TO_TICKS(3000));

  // app_led_switch_blink_type(s_led_handle_green, BLINK_PROVISIONED);
  // vTaskDelay(pdMS_TO_TICKS(3000));

  // app_led_switch_blink_type(s_led_handle_green, BLINK_RECONNECTING);
  // vTaskDelay(pdMS_TO_TICKS(3000));

  // app_led_switch_blink_type(s_led_handle_green, BLINK_CONNECTING);
  // vTaskDelay(pdMS_TO_TICKS(3000));

  // app_led_switch_blink_type(s_led_handle_green, BLINK_PROVISIONING);
  // vTaskDelay(pdMS_TO_TICKS(3000));

  // app_led_switch_blink_type(s_led_handle_green, BLINK_FACTORY_RESET);

  esp_wifi_get_mac(ESP_IF_WIFI_STA, ESPNOW_ADDR_SELF);
  ESP_LOGI(TAG, "mac: " MACSTR ", version: %d", MAC2STR(ESPNOW_ADDR_SELF), ESPNOW_VERSION);

  // Set timezone to GMT
  setenv("TZ", "GMT", 1);
  tzset();

  // uint8_t addr[] = { 0xcc, 0x50, 0xe3, 0x80, 0x10, 0xbc };
  uint8_t addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

  // esp_log_level_set("espnow_sec_init", ESP_LOG_DEBUG);

  uint8_t ttt = 0;
  while (1) {
    // esp_task_wdt_reset();
    // vTaskDelay(pdMS_TO_TICKS(1000));
    // taskYIELD();

    // ESP_LOGI(TAG, "heap %lu kB (%lu)",esp_get_minimum_free_heap_size()/1024,esp_get_minimum_free_heap_size());
    vTaskDelay(pdMS_TO_TICKS(1000));
    taskYIELD();

    if (g_vscp_espnow_probe) {
      vscp_espnow_sec_initiator();
      g_vscp_espnow_probe = false;
    }

    ttt++;
    if (ttt > 10) {
      ttt            = 0;
      vscpEvent *pev = vscp_fwhlp_newEvent();
      if (NULL == pev) {
        ESP_LOGE(TAG, "Unable to allocate VSCP event");
        continue;
      }

      pev->pdata = VSCP_CALLOC(8);
      if (NULL == pev->pdata) {
        ESP_LOGE(TAG, "Unable to allocate event data");
        continue;
      }

#ifdef __USE_TIME_BITS64
      ESP_LOGE(TAG, "64 bit timer is not supported.");
#endif

      // We send timesync only if we have fetched time from NTP server
      if (espnow_timesync_check()) {
        time_t now;
        char strftime_buf[64];
        struct tm timeinfo;

        time(&now);

        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);
        int64_t time_us = (int64_t) tv_now.tv_sec * 1000000L + (int64_t) tv_now.tv_usec;

        // ESP_LOGI(TAG, ">>> The current date/time GMT is: %lld %s", tv_now.tv_sec, strftime_buf);

        pev->vscp_class = VSCP_CLASS1_INFORMATION;
        pev->vscp_type  = VSCP_TYPE_INFORMATION_TIME;
        pev->sizeData   = 8;
        pev->pdata[0]   = 0x00;                           // Index
        pev->pdata[1]   = 0xff;                           // Zone
        pev->pdata[2]   = 0xff;                           // Sub-Zone
        pev->pdata[3]   = timeinfo.tm_hour;               // Hour
        pev->pdata[4]   = timeinfo.tm_min;                // Minutes
        pev->pdata[5]   = timeinfo.tm_sec;                // Seconds
        pev->pdata[6]   = ((time_us / 1000) >> 8) & 0xff; // Milliseconds (MSB)
        pev->pdata[7]   = (time_us / 1000) & 0xff;        // Milliseconds (LSB)
        pev->timestamp  = esp_timer_get_time();

        vscp_espnow_sendEvent(ESPNOW_ADDR_BROADCAST, pev, true, pdMS_TO_TICKS(1000));

        if (NULL != pev) {
          vscp_fwhlp_deleteEvent(&pev);
        }
      }
    }
  }

  // We should not reach here but works the same as cosmetics

  stop_webserver(h_webserver);

  // Unmount web spiffs partition and disable SPIFFS
  // esp_vfs_spiffs_unregister(spiffsconf.partition_label);
  // ESP_LOGI(TAG, "web SPIFFS unmounted");

  // Clean up
  iot_button_delete(s_init_button_handle);

  // Close
  nvs_close(g_nvsHandle);
}
