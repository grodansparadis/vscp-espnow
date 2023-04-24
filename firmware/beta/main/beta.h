/*
  VSCP Beta node

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

#ifndef __VSCP_ESP_NOW_BETA_H__
#define __VSCP_ESP_NOW_BETA_H__

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <esp_log.h>
#include "esp_now.h"

#include <vscp.h>
#include <vscp-espnow.h>

#define NODETYPE VSCP_DROPLET_BETA

#define CONNECTED_LED_GPIO_NUM 2
#define ACTIVE_LED_GPIO_NUM    3
#define GPIO_OUTPUT_PIN_SEL    ((1ULL << CONNECTED_LED_GPIO_NUM) | (1ULL << ACTIVE_LED_GPIO_NUM))

#define DEV_BUFFER_LENGTH 64

/*!
  Default values stored in non volatile memory
  on start up.
*/

#define DEFAULT_GUID "" // Empty constructs from MAC, "-" all nills, "xx:yy:..." set GUID

// ----------------------------------------------------------------------------

typedef struct {

  // Module
  char nodeName[32];    // User name for node
  uint8_t pmk[16];      // Primary key (This key is static and set to VSCP default. Dont change!)
  uint8_t lmk[16];      // Local key (This key is static and set to VSCP default)
  uint8_t keyOrigin[6]; // MAC address for node that sent common system key
  uint8_t queueSize;    // espnow queue size
  uint8_t startDelay;   // Delay before wifi is enabled (to charge cap.)
  uint32_t bootCnt;     // Number of restarts (not editable)

  // espnow
  bool espnowLongRange;             // Enable long range mode (hidden)
  uint8_t espnowSizeQueue;          // Input queue size
  uint8_t espnowChannel;            // Channel to use (zero is current) (hidden)
  uint8_t espnowTtl;                // Default ttl
  bool espnowForwardEnable;         // Forward when packets are received
  bool espnowFilterAdjacentChannel; // Don't receive if from other channel
  bool espnowForwardSwitchChannel;  // Allow switching channel on forward
  int8_t espnowFilterWeakSignal;    // Filter on RSSI (zero is no rssi filtering)

  // beta common
  uint8_t logLevelUart;   // Log level to uart  
  uint8_t logLevelEspNow; // Log level to espnow
  uint8_t logLevelFlash;  // Log level to flash
  
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
typedef enum {
  BETA_STATE_IDLE,         // Standard working state
  BETA_STATE_VIRGIN,       // Node is uninitialized
  BETA_STATE_KEY_EXCHANGE, // Key exchange active
  BETA_STATE_OTA,          // OTA responser active
  BETA_STATE_MAX
} beta_node_states_t;

ESP_EVENT_DECLARE_BASE(ALPHA_EVENT); // declaration of the alpha events family

/*!
  Beta events
*/
typedef enum {
  /**
   * Start client provisioning and security transfer.
   * This state is active for 30 seconds.
   */
  BETA_START_CLIENT_PROVISIONING,

  /**
   * Stop client provisioning and security transfer.
   * This event happens 30 seconds after start
   */
  BETA_STOP_CLIENT_PROVISIONING,

  /**
   * Restart system
   */
  BETA_RESTART,

  /**
   * Restore factory default and erase wifi credentials
   */
  BETA_RESTORE_FACTORY_DEFAULTS,

  /**
   * Node is waiting to get IP address
   */
  BETA_GET_IP_ADDRESS_START,

  /**
   * Node have received IP address
   */
  BETA_GET_IP_ADDRESS_STOP,
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
 * @brief receive callback
 *
 * @param pev Pointer to received event.
 * @param userdata Pointer to user data.
 */
void
receive_cb(const vscpEvent *pev, void *userdata);

#endif