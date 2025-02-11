/*
  VSCP Wireless CAN4VSCP Gateway (VSCP-WCANG)

  VSCP Alpha Droplet node

  MQTT SSL Client

  The MIT License (MIT)
  Copyright © 2022-2025 Ake Hedman, the VSCP project <info@vscp.org>

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

#ifndef __DROPLET_MQTT__
#define __DROPLET_MQTT__

#include <vscp.h>

#define DROPLET_MQTT_STATISTIC_PUBLISH_INTERVAL 60000

// Topics for send and receive statistics
#define DROPLET_MQTT_TOPIC_STATS_RECV_CNT "droplet/alpha/statistics/rcvcnt"
#define DROPLET_MQTT_TOPIC_STATS_TX_CNT   "droplet/alpha/statistics/txcnt"

/**
 * @brief Send and receive statistics
 *
 */
typedef struct {
  uint32_t nConnect;        // # connects
  uint32_t nDisconnect;     // # disconnects
  uint32_t nErrors;         // # MQTT Error count
  uint32_t nPub;            // # published frames
  uint32_t nPubFailures;    // Number of publish failures
  uint32_t nPubConfirm;     // # of OK publish
  uint32_t nPubLog;         // # published logframes
  uint32_t nPubLogFailures; // Number of publish log failures
  uint32_t nPubLogConfirm;  // # of OK log publish
  uint32_t nSub;            // # received events
} mqtt_stats_t;

/**
 * @fn mqtt_start
 * @brief Start MQTT client
 *
 */

void
mqtt_start(void);

/**
 * @fn mqtt_stop
 * @brief Stop MQTT client
 *
 */

void
mqtt_stop(void);

/**
 * @fn mqtt_send_vscp_event
 * @brief Send VSCP event on configured topic
 * @param topic Topic to publish event on.
 *        If set to NULL configured topic will be used.
 * @param pev Pointer to event to publish
 * @return int VSCP_ERROR_SUCCESS if OK, else error code.
 */

int
mqtt_send_vscp_event(const char *topic, const vscpEvent *pev);

/**
 * @fn mqtt_log
 * @brief Log message to MQTT
 *
 * @param buf Null terminated log message
 * @return VSCP_ERROR_SUCCESS is returned if all is OK. Error code else.
 *
 * A log message is sent to the MQTT broker if the broker is started. Log format
 * is [mac-address][channel]rssi]: Log message where the mac-address is the mac-address
 * for the device that sent the log message.
 */

int
mqtt_log(const char *msg);

#endif