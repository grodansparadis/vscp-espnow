/*
  VSCP Wireless CAN4VSCP Gateway (VSCP-WCANG)

  VSCP Alpha Droplet node

  MQTT SSL Client

  The MIT License (MIT)
  Copyright © 2022-2023 Ake Hedman, the VSCP project <info@vscp.org>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "vscp-compiler.h"
#include "vscp-projdefs.h"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <esp_system.h>
#include <esp_partition.h>
#include <spi_flash_mmap.h>
#include <nvs_flash.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_log.h>
#include <mqtt_client.h>
#include <esp_tls.h>
#include <esp_ota_ops.h>
#include <esp_mac.h> // esp_base_mac_addr_get
#include <sys/param.h>

#include <vscp.h>
#include <vscp-firmware-helper.h>

#include <alpha.h>
#include "mqtt.h"

// Global stuff
extern node_persistent_config_t g_persistent;        // main
//extern transport_t g_tr_tcpsrv[MAX_TCP_CONNECTIONS]; // tcpsrv

static const char *TAG = "MQTT";

esp_mqtt_client_handle_t g_mqtt_client;

// Static stuff
static bool s_mqtt_connected = false;  // true when connected

static mqtt_stats_t s_mqtt_statistics = { 0 };

// #if CONFIG_BROKER_CERTIFICATE_OVERRIDDEN == 1
// static const uint8_t mqtt_eclipseprojects_io_pem_start[] =
//   "-----BEGIN CERTIFICATE-----\n" CONFIG_BROKER_CERTIFICATE_OVERRIDE "\n-----END CERTIFICATE-----";
// #else
extern const uint8_t mqtt_eclipse_io_pem_start[] asm("_binary_mqtt_eclipse_io_pem_start");
// #endif
extern const uint8_t mqtt_eclipse_io_pem_end[] asm("_binary_mqtt_eclipse_io_pem_end");

///////////////////////////////////////////////////////////////////////////////
// send_binary
//
//
// Note: this function is for testing purposes only publishing part of the active partition
//       (to be checked against the original binary)
//

// static void
// send_binary(esp_mqtt_client_handle_t client)
// {
//   spi_flash_mmap_handle_t out_handle;
//   const void *binary_address;
//   const esp_partition_t *partition = esp_ota_get_running_partition();
//   esp_partition_mmap(partition, 0, partition->size, SPI_FLASH_MMAP_DATA, &binary_address, &out_handle);
//   // sending only the configured portion of the partition (if it's less than the partition size)
//   int binary_size = MIN(4096, partition->size);
//   int msg_id      = esp_mqtt_client_publish(client, "/topic/binary", binary_address, binary_size, 0, 0);
//   ESP_LOGI(TAG, "binary sent with msg_id=%d", msg_id);
// }

///////////////////////////////////////////////////////////////////////////////
// mqtt_send_vscp_event
//

int
mqtt_send_vscp_event(const char *topic, const vscpEvent *pev)
{
  int rv;
  const char *pTopic = topic;

  // Check event pointer
  if (NULL == pev) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  // If no topic set. Use configured topic
  if (NULL == topic) {
    pTopic = g_persistent.mqttPub;
  }

  // If not connected there is no meaning to send event
  if (!s_mqtt_connected) {
    s_mqtt_statistics.nPubFailures++;
    return VSCP_ERROR_SUCCESS;
  }

  // We publish VSCP event on JSON form
  char *pbuf = VSCP_MALLOC(2048);
  if (NULL == pbuf) {
    ESP_LOGE(TAG, "Unable to allocate JSON buffer for conversion");
    return VSCP_ERROR_MEMORY;
  }

  if (VSCP_ERROR_SUCCESS != (rv = vscp_fwhlp_create_json(pbuf, 2048, pev))) {
    VSCP_FREE(pbuf);
    ESP_LOGE(TAG, "Failed to convert event to JSON rv = %d", rv);
    return rv;
  }

  ESP_LOGV(TAG, "converted");

  /*
    {{node}}        - Node name
    {{guid}}        - Node GUID
    {{evguid}}      - Event GUID
    {{class}}       - Event class
    {{type}}        - Event type
    {{nickname}}    - Node nickname (16-bit)
    {{ecnickname}}  - Node nickname (16-bit) for node sending event
    {{sindex}}      - Sensor index (if any)
    --------------------------------------------
    {{timestamp}}   - Timestamep for event
    {{index}}       - Index (data byte 0) (if any)
    {{zone}}        - Zone (data byte 1) (if any)
    {{subzone}}     - Sub Zone (data byte 2) (if any)
    {{d[n]}}        - Data byte n (if any)
    {{year}}        - Two digit year of event (Time always in GMT).
    {{fyear}}       - Four digit year of event (Time always in GMT).
    {{month}}       - Two digit month of event (Time always in GMT).
    {{day}}         - Two digit day of event (Time always in GMT).
    {{hour}}        - Two digit hour of event (Time always in GMT).
    {{minute}}      - Two digit minute of event (Time always in GMT).
    {{second}}      - Two digit second of event (Time always in GMT).

    Typical topic
    vscp/FF:FF:FF:FF:FF:FF:FF:F5:01:00:00:00:00:00:00:02/20/9/2
    vscp/{{guid}}/{{class}}/{{type}}/{{index}}
  */

  char newTopic[128], saveTopic[128], workbuf[48];

  // Node name
  vscp_fwhlp_strsubst(newTopic, sizeof(newTopic), pTopic, "{{node}}", g_persistent.nodeName);
  strcpy(saveTopic, newTopic);

  // GUID
  uint8_t GUID[16];
  vscp_espnow_get_node_guid(GUID);
  vscp_fwhlp_writeGuidToString(workbuf, GUID);
  vscp_fwhlp_strsubst(newTopic, sizeof(newTopic), saveTopic, "{{guid}}", workbuf);
  strcpy(saveTopic, newTopic);

  // Event GUID
  vscp_fwhlp_writeGuidToString(workbuf, pev->GUID);
  vscp_fwhlp_strsubst(newTopic, sizeof(newTopic), saveTopic, "{{evguid}}", workbuf);
  strcpy(saveTopic, newTopic);

  // Class
  sprintf(workbuf, "%d", pev->vscp_class);
  vscp_fwhlp_strsubst(newTopic, sizeof(newTopic), saveTopic, "{{class}}", workbuf);
  strcpy(saveTopic, newTopic);

  // Type
  sprintf(workbuf, "%d", pev->vscp_type);
  vscp_fwhlp_strsubst(newTopic, sizeof(newTopic), saveTopic, "{{type}}", workbuf);
  strcpy(saveTopic, newTopic);

  // nickname
  sprintf(workbuf, "%d", ((GUID[14] << 8) + (GUID[15])));
  vscp_fwhlp_strsubst(newTopic, sizeof(newTopic), saveTopic, "{{nickname}}", workbuf);
  strcpy(saveTopic, newTopic);

  // event nickname
  sprintf(workbuf, "%d", ((pev->GUID[14] << 8) + (pev->GUID[15])));
  vscp_fwhlp_strsubst(newTopic, sizeof(newTopic), saveTopic, "{{evnickname}}", workbuf);
  strcpy(saveTopic, newTopic);

  // sensor index
  if (VSCP_ERROR_SUCCESS == vscp_fwhlp_isMeasurement(pev)) {
    sprintf(workbuf, "%d", vscp_fwhlp_getMeasurementSensorIndex(pev));
  }
  else {
    memset(workbuf, 0, sizeof(workbuf));
  }
  vscp_fwhlp_strsubst(newTopic, sizeof(newTopic), saveTopic, "{{sindex}}", workbuf);
  strcpy(saveTopic, newTopic);

  strcpy(newTopic,"xxx/");

  int msgid =
     esp_mqtt_client_publish(g_mqtt_client, newTopic, pbuf, strlen(pbuf), 0,0/*g_persistent.mqttQos, g_persistent.mqttRetain*/);

  //int msgid = 
  //esp_mqtt_client_enqueue(g_mqtt_client, newTopic, pbuf, strlen(pbuf), 0,0/*g_persistent.mqttQos, g_persistent.mqttRetain*/, false);
  if (-1 != msgid) {
    //ESP_LOGI(TAG, "Published MQTT message. id=%d topic=%s outbox-size = %d", msgid, newTopic, esp_mqtt_client_get_outbox_size(g_mqtt_client));
    s_mqtt_statistics.nPub++; 
  }
  else {
    s_mqtt_statistics.nPubFailures++;
    ESP_LOGE(TAG, "Failed to publish MQTT message. id=%d Topic=%s outbox-size = %d", msgid, newTopic, esp_mqtt_client_get_outbox_size(g_mqtt_client));
  }

  VSCP_FREE(pbuf);

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// mqtt_event_handler
//
/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void
mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, (int) event_id);
  esp_mqtt_event_handle_t event   = event_data;
  esp_mqtt_client_handle_t client = event->client;
  int msg_id;
  switch ((esp_mqtt_event_id_t) event_id) {

    case MQTT_EVENT_BEFORE_CONNECT:
      ESP_LOGI(TAG, "Preparing to connect");
      break;

    case MQTT_EVENT_CONNECTED:
      s_mqtt_connected = true;
      s_mqtt_statistics.nConnect++;
      ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
      msg_id =
        esp_mqtt_client_subscribe(client,
                                  /*"/topic/qos0"*/ "vscp/FF:FF:FF:FF:FF:FF:FF:FE:B8:27:EB:CF:3A:15:00:01/10/6/1/0",
                                  0);
      ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

      // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
      // ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

      // msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
      // ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
      break;

    case MQTT_EVENT_DISCONNECTED:
      s_mqtt_connected = false;
      s_mqtt_statistics.nDisconnect++;
      ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
      //esp_mqtt_client_reconnect(client);
      break;

    case MQTT_EVENT_SUBSCRIBED:
      ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
      msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
      ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
      break;

    case MQTT_EVENT_UNSUBSCRIBED:
      ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
      break;

    case MQTT_EVENT_PUBLISHED:
      s_mqtt_statistics.nPubConfirm++;
      ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
      break;

    case MQTT_EVENT_DATA:
      ESP_LOGI(TAG, "MQTT_EVENT_DATA");
      printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
      printf("DATA=%.*s\r\n", event->data_len, event->data);
      if (strncmp(event->data, "send binary please", event->data_len) == 0) {
        ESP_LOGI(TAG, "Sending the binary");
        // send_binary(client);
      }
      break;

    case MQTT_EVENT_ERROR:
      s_mqtt_statistics.nErrors++;
      ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
      if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
        ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
        ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
        ESP_LOGI(TAG,
                 "Last captured errno : %d (%s)",
                 event->error_handle->esp_transport_sock_errno,
                 strerror(event->error_handle->esp_transport_sock_errno));
      }
      else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
        ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
      }
      else {
        ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
      }
      break;

    default:
      ESP_LOGI(TAG, "Other event id:%d", event->event_id);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// mqtt_start
//

void
mqtt_start(void)
{
  ESP_LOGI(TAG, "Starting MQTT client");

  // Set client id from mac
  uint8_t mac[8];
  ESP_ERROR_CHECK(esp_base_mac_addr_get(mac));

  /*
    {{node}}        - Node name
    {{guid}}        - Node GUID

    Typical client
    {{node}}-{{guid}}
  */

  char clientid[128], save[128], workbuf[48];

  // Node name
  vscp_fwhlp_strsubst(clientid, sizeof(clientid), g_persistent.mqttClientid, "{{node}}", g_persistent.nodeName);
  strcpy(save, clientid);

  // GUID
  uint8_t GUID[16];
  vscp_espnow_get_node_guid(GUID);
  vscp_fwhlp_writeGuidToString(workbuf, GUID);
  vscp_fwhlp_strsubst(clientid, sizeof(clientid), save, "{{guid}}", workbuf);

  char uri[64];
  sprintf(uri, "mqtt://%s:%d", g_persistent.mqttUrl, g_persistent.mqttPort);

  // clang-format off
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  const esp_mqtt_client_config_t mqtt_cfg = {
    .broker = { 
                .address.uri = uri,                     // "mqtt://192.168.1.7:1883", 
                .address.port = g_persistent.mqttPort,  // 1883,
                /*.verification.certificate = (const char *) mqtt_eclipse_io_pem_start*/ 
              },    
    .session.disable_clean_session = true,
    .session.keepalive = 60,          
    .credentials.client_id               = clientid,
    .credentials.username                = g_persistent.mqttUsername,
    .credentials.authentication.password = g_persistent.mqttPassword,
    .task.priority = 5,
    //.task.stack_size = 2 * 1024,
    //.buffer.size = 1024,
    //.buffer.out_size = 2*1024,
  };
#else
  esp_mqtt_client_config_t mqtt_cfg = {
    .uri          = uri; // "mqtt://192.168.1.7:1883",
    .event_handle = mqtt_event_handler,
    .client_id    = clientid
#endif
  // clang-format on

  //ESP_LOGI(TAG, "[APP] Free memory: %lu bytes", esp_get_free_heap_size());
  g_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  // The last argument may be used to pass data to the event handler, in this example mqtt_event_handler
  if (ESP_OK != esp_mqtt_client_register_event(g_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL)) {
    ESP_LOGE(TAG,"Failed to start MQTT client");
  }

  if (ESP_OK != esp_mqtt_client_start(g_mqtt_client)) {
    ESP_LOGE(TAG,"Failed to start MQTT client");
  }

  ESP_LOGI(TAG, "Outbox-size = %d", esp_mqtt_client_get_outbox_size(g_mqtt_client));
}

///////////////////////////////////////////////////////////////////////////////
// mqtt_stop
//

void
mqtt_stop(void)
{
  esp_mqtt_client_stop(g_mqtt_client);
}
