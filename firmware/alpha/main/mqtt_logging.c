/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.

   https://github.com/nopnop2002/esp-idf-net-logging
*/

#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/message_buffer.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_wifi.h> // esp_wifi_get_channel
#include <esp_mac.h>  // esp_base_mac_addr_get
#include <mqtt_client.h>

#include "vscp-compiler.h"
#include "vscp-projdefs.h"

#include "alpha.h"
#include "mqtt.h"
#include "net_logging.h"

// External globals
extern node_persistent_config_t g_persistent;
extern esp_mqtt_client_handle_t g_mqtt_client;

static const char *TAG = "mqtt_log";

EventGroupHandle_t mqtt_status_event_group;
#define MQTT_CONNECTED_BIT BIT2

extern MessageBufferHandle_t xMessageBufferTrans;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static void
mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
#else
static esp_err_t
mqtt_event_handler(esp_mqtt_event_handle_t event)
#endif
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  esp_mqtt_event_handle_t event = event_data;
#endif
  switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
      // ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
      xEventGroupSetBits(mqtt_status_event_group, MQTT_CONNECTED_BIT);
      break;
    case MQTT_EVENT_DISCONNECTED:
      // ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
      xEventGroupClearBits(mqtt_status_event_group, MQTT_CONNECTED_BIT);
      break;
    case MQTT_EVENT_SUBSCRIBED:
      // ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
      break;
    case MQTT_EVENT_UNSUBSCRIBED:
      // ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
      break;
    case MQTT_EVENT_PUBLISHED:
      // ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
      break;
    case MQTT_EVENT_DATA:
      // ESP_LOGI(TAG, "MQTT_EVENT_DATA");
      break;
    case MQTT_EVENT_ERROR:
      // ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
      break;
    default:
      // ESP_LOGI(TAG, "Other event id:%d", event->event_id);
      break;
  }
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
  return ESP_OK;
#endif
}

void
mqtt_pub(void *pvParameters)
{
  PARAMETER_t *task_parameter = pvParameters;
  PARAMETER_t param;
  memcpy((char *) &param, task_parameter, sizeof(PARAMETER_t));
  // printf("Start:param.url=[%s] param.topic=[%s]\n", param.url, param.topic);

  // Create Event Group
  mqtt_status_event_group = xEventGroupCreate();
  configASSERT(mqtt_status_event_group);

  /*
  // Set client id from mac
  uint8_t mac[8];
  ESP_ERROR_CHECK(esp_base_mac_addr_get(mac));
  char client_id[64];
  sprintf(client_id, "pub-%02x%02x%02x%02x%02x%02x", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  //printf("client_id=[%s]\n", client_id);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = param.url,
    .credentials.client_id = client_id
  };
#else
  esp_mqtt_client_config_t mqtt_cfg = {
    .uri = param.url,
    .event_handle = mqtt_event_handler,
    .client_id = client_id
  };
#endif


  // Connect broker
  esp_mqtt_client_handle_t mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
#endif

  esp_mqtt_client_start(mqtt_client);
  xEventGroupClearBits(mqtt_status_event_group, MQTT_CONNECTED_BIT);

  // Wait for connection
  xEventGroupWaitBits(mqtt_status_event_group, MQTT_CONNECTED_BIT, false, true, portMAX_DELAY);
  //printf("Connected to MQTT Broker\n");
*/

  // Send ready to receive notify
  char buffer[LOG_MSG_ITEM_SIZE];
  xTaskNotifyGive(param.taskHandle);

  while (1) {
    size_t size = xMessageBufferReceive(xMessageBufferTrans, buffer, sizeof(buffer), portMAX_DELAY);
    // printf("xMessageBufferReceive received=%d\n", received);
    if (size > 0) {
      // printf("xMessageBufferReceive buffer=[%.*s]\n",received, buffer);
      EventBits_t EventBits = xEventGroupGetBits(mqtt_status_event_group);
      // printf("EventBits=%x\n", EventBits);
      if (EventBits & MQTT_CONNECTED_BIT) {

        if (g_persistent.mqttEnable) {

          const char *buf    = NULL;
          size_t size_buffer = 50 + size;

          buf = ESP_CALLOC(1,size_buffer);
          if (NULL == buf) {
            ESP_LOGE(TAG, "Unable to allocate buffer for log message.");
            return; // ESP_ERR_NO_MEM
          }

          uint8_t src_addr[6];
          esp_read_mac(src_addr, ESP_MAC_WIFI_STA);

          uint8_t primary;
          wifi_second_chan_t secondary;
          esp_wifi_get_channel(&primary, &secondary);

          snprintf(buf, size_buffer, "[" MACSTR "][%d][%d]: %.*s", MAC2STR(src_addr), size, primary, 0, buffer);

          mqtt_log(buf);
          ESP_FREE(buf);
        }
        // esp_mqtt_client_publish(g_mqtt_client, param.topic, buffer, received, 1, 0);
        // printf("sent publish successful\n");
      }
      else {
        ESP_LOGW(TAG, "Disconnect to MQTT Broker. Skip to send");
      }
    }
    else {
      ESP_LOGE(TAG, "xMessageBufferReceive fail");
      break;
    }
  } // end while

  // Stop connection
  // esp_mqtt_client_stop(mqtt_client);
  vTaskDelete(NULL);
}
