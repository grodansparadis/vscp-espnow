/*
  VSCP Alpha node

  This file is part of the VSCP (https://www.vscp.org)

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

#ifndef __VSCP_ESP_NOW_ALPHA_H__
#define __VSCP_ESP_NOW_ALPHA_H__

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <esp_log.h>
#include <esp_now.h>

#include <espnow_ota.h>

#include <vscp.h>
#include <vscp-espnow.h>

// #define NODETYPE VSCP_DROPLET_ALPHA

#define ESP_NOW_VER_MAJOR 2
#define ESP_NOW_VER_MINOR 1
#define ESP_NOW_VER_PATCH 1

#define CONNECTED_LED_GPIO_NUM 2
#define ACTIVE_LED_GPIO_NUM    3
#define GPIO_OUTPUT_PIN_SEL    ((1ULL << CONNECTED_LED_GPIO_NUM) | (1ULL << ACTIVE_LED_GPIO_NUM))

#define DEV_BUFFER_LENGTH 64

/*!
  Default values stored in non volatile memory
  on start up.
*/

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------

typedef enum {
  ALPHA_LOG_NONE, /*!< No log output */
  ALPHA_LOG_STD,  /*!< Standard output */
  ALPHA_LOG_UDP,  /*!< UDP */
  ALPHA_LOG_TCP,  /*!< TCP */
  ALPHA_LOG_HTTP, /*!< HTTP */
  ALPHA_LOG_MQTT, /*!< MQTT */
  ALPHA_LOG_VSCP  /*!< VSCP */
} alpha_log_output_t;

typedef struct {

  // Module
  char nodeName[32];  // Friendly name for node
  uint8_t pmk[16];    // Primary key (This key is static and set to VSCP default. Dont change!)
  uint8_t lmk[16];    // Local key (This key is static and set to VSCP default)
  uint8_t queueSize;  // espnow queue size
  uint8_t startDelay; // Delay before wifi is enabled (to charge cap)
  uint32_t bootCnt;   // Number of restarts (not editable)

  // Logging
  uint8_t logwrite2Stdout; // Enable write Logging to STDOUT
  uint8_t logLevel;        // 'ERROR' is default
  uint8_t logType;         // STDOUT / UDP / TCP / HTTP / MQTT /VSCP
  uint8_t logRetries;      // Number of log log retries
  char logUrl[32];         // For UDP/TCP/HTML
  uint16_t logPort;        // Port for UDP
  char logMqttTopic[64];   //  MQTT topic

  // VSCP Link
  bool vscplinkEnable;
  char vscplinkUrl[32];      // URL VSCP tcp/ip Link host (set to blank yto disable)
  uint16_t vscplinkPort;     // Port on VSCP tcp/ip Link host
  char vscplinkUsername[32]; // Username for VSCP tcp/ip Link host
  char vscplinkPassword[32]; // Password for VSCP tcp/ip Link host
  uint8_t vscpLinkKey[32];   // Security key (16 (EAS128)/24(AES192)/32(AES256))

  // ESP-NOW
  bool espnowEnable;
  bool espnowLongRange;             // Enable long range mode
  uint8_t espnowSizeQueue;          // Input queue size
  uint8_t espnowChannel;            // Channel to use (zero is current)
  uint8_t espnowTtl;                // Default ttl
  bool espnowForwardEnable;         // Forward when packets are received
  uint8_t espnowEncryption;         // 0=no encryption, 1=AES-128, 2=AES-192, 3=AES-256
  bool espnowFilterAdjacentChannel; // Don't receive if from other channel
  bool espnowForwardSwitchChannel;  // Allow switching channel on forward
  int8_t espnowFilterWeakSignal;    // Filter on RSSI (zero is no rssi filtering)

  // Web server
  bool webEnable;
  uint16_t webPort;     // Port web server listens on
  char webUsername[32]; // Basic Auth username
  char webPassword[32]; // Basic Auth password

  // MQTT  (mqtt[s]://[username][:password]@host.domain[:port])
  bool mqttEnable;
  char mqttUrl[32];
  uint16_t mqttPort;
  char mqttClientid[64];
  char mqttUsername[32];
  char mqttPassword[32];
  int mqttQos;
  int mqttRetain;
  char mqttSub[128];
  char mqttPub[128];
  char mqttVerification[32 * 1024]; // For server certificate
  char mqttLwTopic[128];
  char mqttLwMessage[128];
  uint8_t mqttLwQos;
  bool mqttLwRetain;
} node_persistent_config_t;

// ----------------------------------------------------------------------------

/*!
  ESP-NOW
*/
#define ESPNOW_SIZE_TX_BUF 10  /*!< Size for transmitt buffer >*/
#define ESPNOW_SIZE_RX_BUF 20  /*!< Size for receive buffer >*/
#define ESPNOW_MAXDELAY    512 // Ticks to wait for send queue access
#define ESPNOW_QUEUE_SIZE  6

#define IS_BROADCAST_ADDR(addr) (memcmp(addr, s_vscp_broadcast_mac, ESP_NOW_ETH_ALEN) == 0)

// Beta node states
// typedef enum {
//   MAIN_STATE_WORK, // Standard working state
//   MAIN_STATE_INIT, // Active state during init until wifi is connected
//   MAIN_STATE_PROV, // Active state during provisioning
//   MAIN_STATE_SET_DEFAULTS
// } beta_node_states_t;

ESP_EVENT_DECLARE_BASE(ALPHA_EVENT); // declaration of the alpha events family

/*!
  Alpha events
*/
typedef enum {
  /**
   * Start client provisioning and security transfer.
   * This state is active for 30 seconds.
   */
  ALPHA_START_CLIENT_PROVISIONING,

  /**
   * Stop client provisioning and security transfer.
   * This event happens 30 seconds after start
   */
  ALPHA_STOP_CLIENT_PROVISIONING,

  /**
   * Restart system
   */
  ALPHA_RESTART,

  /**
   * Restore factory default and erase wifi credentials
   */
  ALPHA_RESTORE_FACTORY_DEFAULTS,

  /**
   * Node is waiting to get IP address
   */
  ALPHA_GET_IP_ADDRESS_START,

  /**
   * Node have received IP address
   */
  ALPHA_GET_IP_ADDRESS_STOP,
} beta_cb_event_t;

// ----------------------------------------------------------------------------

/**
 * @brief Read processor on chip temperature
 * @return Temperature as floating point value
 */
float
read_onboard_temperature(void);

/**
 * @fn getMilliSeconds
 * @brief Get system time in Milliseconds
 *
 * @return Systemtime in milliseconds
 */
uint32_t
getMilliSeconds(void);

/**
 * @brief VSCP event over esp-now receive callback
 *
 * @param pev Pointer to received event.
 * @param userdata Pointer to user data.
 */
void
receive_cb(const vscpEvent *pev, void *userdata);

/**
 * @brief Start security initiator
 *
 * @return esp_err_t
 *
 * Key exchange is started here. If a 32 byte key+iv is not
 * already set and is configured it is generated here.
 * Scanning is done for nodes that are set into key exchange mode
 * and then handshaking is done with each node to transfer the key + iv
 */
esp_err_t
app_sec_initiator(void);

/**
 * @fn app_initiate_firmware_upload(const char *url)
 * @brief Initiate espnow firmware update
 *
 * @param url Pointer to string for url to binary that should be uploaded to remote node.
 *  If url is set to NULL the project configured file PRJDEF_FIRMWARE_UPGRADE_URL is used.
 * @return int ESP_OK is returned if all is OK. Else error code.
 */

int
app_initiate_firmware_upload(const char *url);

/**
 * @brief
 *
 * @param firmware_size
 * @param sha
 */

void
app_firmware_send(size_t firmware_size, uint8_t sha[ESPNOW_OTA_HASH_LEN]);

/**
 * @brief Download firmware form server
 *
 * @param url Url to resource
 * @return size_t Size of downloaded image
 */

size_t
app_firmware_download(const char *url);

#endif