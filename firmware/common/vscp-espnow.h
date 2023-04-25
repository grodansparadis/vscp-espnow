/**
 * @brief           VSCP over esp-now code
 * @file            vscp_espnow.h
 * @author          Ake Hedman, The VSCP Project, www.vscp.org
 *
 *********************************************************************/

/* ******************************************************************************
 * VSCP (Very Simple Control Protocol)
 * http://www.vscp.org
 *
 * The MIT License (MIT)
 *
 * Copyright © 2000-2023 Ake Hedman, the VSCP project <info@vscp.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *  This file is part of VSCP - Very Simple Control Protocol
 *  http://www.vscp.org
 *
 * ******************************************************************************
 */

#ifndef VSCP_ESPNOW_H
#define VSCP_ESPNOW_H

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <esp_wifi_types.h>
#include <espnow.h>

#include <vscp.h>

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------------------

#define VSCP_ESPNOW_WAIT_MS_DEFAULT   1000      // One second

// ----------------------------------------------------------------------------

#define VSCP_ESPNOW_VERSION 0

// Frame id

#define VSCP_ESPNOW_ID_MSB 0x55
#define VSCP_ESPNOW_ID_LSB 0xAA

/**
 * @brief Frame positions for data in the VSCP esp-now frame
 */

// Identify as esp-now frame (0x55/0xAA)
#define VSCP_ESPNOW_POS_ID 0

// 0xab where b = esp-now protocol version and a = type (alpha/beta...)
// bit 7,6 - Alfa/beta/gamma
// bit 5,4 - protocol version (0)
// bit 3,2,1,0 - Encryption (0=none/1=AES128(/2=AES192/3=AES256))
#define VSCP_ESPNOW_POS_TYPE_VER 2

// Sequence counter byte can be used to protect from replay attacks.
// It is increase by on for each event sent
#define VSCP_ESPNOW_POS_SEQ 3

// Time stamp is the time_t from the time() call. Not that
// time_t can be 65 bits on some systems (__USE_TIME_BITS64)

// NOTE! This timestamp is not the same as the event timestamp and
// is only relevant to vscp-espnow
#define VSCP_ESPNOW_POS_TIME_STAMP 4

// VSCP content
#define VSCP_ESPNOW_POS_HEAD       8  // VSCP head bytes (2)
#define VSCP_ESPNOW_POS_NICKNAME   10 // Node nickname (2)
#define VSCP_ESPNOW_POS_VSCP_CLASS 12 // VSCP class (2)
#define VSCP_ESPNOW_POS_VSCP_TYPE  14 // VSCP Type (2)
#define VSCP_ESPNOW_POS_SIZE       16 // Data size (needed because of encryption padding) (1)
#define VSCP_ESPNOW_POS_DATA       17 // VSCP data (max 128 bytes)

#define VSCP_ESPNOW_MIN_FRAME VSCP_ESPNOW_POS_DATA // Number of bytes in minimum frame
#define VSCP_ESPNOW_MAX_DATA                                                                                           \
  (ESPNOW_SEC_PACKET_MAX_SIZE - VSCP_ESPNOW_MIN_FRAME) // Max VSCP data (of possible 512 bytes) that a frame can hold
#define VSCP_ESPNOW_MAX_FRAME (ESPNOW_SEC_PACKET_MAX_SIZE - VSCP_ESPNOW_MIN_FRAME - 16) // 16 byte IV if VSCP encryption

/*
  Note on max data size
  ---------------------
  An esp-now frame can hold a payload of max 250 bytes
  IV is 16 bytes
  VSCP frame data is 16 -bytes
  So left for Droplet data is 250-16-15 = 219 bytes
*/

#define VSCP_ESPNOW_IV_LEN 16

/*
  The idel state is the normal state a node is in. This is where it does all it's
  work if it has been initialized.

  Alpha nodes can only be in the idle or one of the SRV states.
  Beta nodes can be both in one of the SRV states and in one of the CLIENT states and in idle.
  Gamma nodes can only be in CLIENT state and idle.
*/
typedef enum {
  VSCP_ESPNOW_STATE_VIRGIN, // A node that is uninitialized
  VSCP_ESPNOW_STATE_IDLE,   // Normal state for all nodes. Initialized.
  VSCP_ESPNOW_STATE_PROBE,  // Probe in progress (alpha/beta).
  VSCP_ESPNOW_STATE_OTA,    // OTA in progress.
} vscp_espnow_state_t;

// The event queue

typedef enum {
  VSCP_ESPNOW_ALPHA_PROBE,
  VSCP_ESPNOW_RECV_CB,
} vscp_espnow_event_id_t;

typedef struct {
  uint8_t node_type; // VSCP_DROPLET_ALPHA / VSCP_DROPLET_BETA / VSCP_DROPLET_GAMMA
  uint8_t freq;      // Heart beat frequency
} vscp_espnow_heart_beat_t;

/*
  **VSCP espnow** persistent storage
*/
typedef struct {
  // VSCP
  uint16_t nickname;      // 16-bit nickname of node (two lsb of GUID)

  // User id
  uint8_t userid0;
  uint8_t userid1;
  uint8_t userid2;
  uint8_t userid3;
  uint8_t userid4;
} vscp_espnow_persistent_t;

/**
 * @brief Item in table for replay attack preventions
 *
 * An event is accepted from a node only if:
 *  Timestamp is not null.
 *  The timestamp is within 200 ms of the sync time.
 *
 * Timestamp is set to current timestamp when first heartbeat is received
 *    from node with this mac adress.
 */

// Maximum number of seq nodes in replay prevention table
#define MAX_SEQ_NODES 100

typedef struct {
  uint8_t seq;                  // Last seq counter
  uint8_t mac[ESPNOW_ADDR_LEN]; // MAC address for node
} vscp_espnow_last_event_t;

/* When ESPNOW sending or receiving callback function is called, post event to ESPNOW task. */
typedef struct {
  vscp_espnow_event_id_t id;
  // vscp_espnow_event_info_t info;
} vscp_espnow_event_t;

/**
 * @brief Initialize the configuration of esp-now
 */
typedef struct {
  //nvs_handle_t nvsHandle;   // Handle to persistent storage
} vscp_espnow_config_t;



/**
 * @brief Send and receive statistics
 *
 */
typedef struct {
  uint32_t nSend;            // # sent frames
  uint32_t nSendFailures;    // Number of send failures
  uint32_t nSendLock;        // Number of send lock give ups
  uint32_t nSendAck;         // # of failed send confirms
  uint32_t nRecv;            // # received frames
  uint32_t nRecvOverruns;    // Number of receive overruns
  uint32_t nRecvFrameFault;  // Frame to big or to small
  uint32_t nRecvAdjChFilter; // Adjacent channel filter
  uint32_t nRecvŔssiFilter;  // RSSI filter stats
  uint32_t nForw;            // # Number of forwarded frames
  uint32_t nTimeDiffLarge;   // Frames skipped with time diff to large
} vscp_espnow_stats_t;

/**
 * @brief Provision data
 * This stucture is sent to node when the provisioning button
 * is activated. It sets channel for the cluster among other espnow
 * parameters
 */
typedef struct {
  uint8_t espnowLongRange;             // Enable long range mode
  uint8_t espnowSizeQueue;             // Input queue size
  uint8_t espnowChannel;               // Channel to use (zero is current)
  uint8_t espnowTtl;                   // Default ttl
  uint8_t espnowForwardEnable;         // Forward when packets are received
  uint8_t espnowFilterAdjacentChannel; // Don't receive if from other channel
  uint8_t espnowForwardSwitchChannel;  // Allow switching channel on forward
  int8_t espnowFilterWeakSignal;       // Filter on RSSI (zero is no rssi filtering)
} vscp_espnow_prov_data_t;

#define KEYSTR                                                                                                         \
  "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x"   \
  ":%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x"
#define KEY2STR(a)                                                                                                     \
  (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5], (a)[6], (a)[7], (a)[8], (a)[9], (a)[10], (a)[11], (a)[12], (a)[13],  \
    (a)[14], (a)[15], (a)[16], (a)[17], (a)[18], (a)[19], (a)[20], (a)[21], (a)[22], (a)[23], (a)[24], (a)[25],        \
    (a)[26], (a)[27], (a)[28], (a)[29], (a)[30], (a)[31]

#define VSCP_ESPNOW_MSG_CACHE_SIZE      32    // Size for magic cache
#define VSCP_ESPNOW_HEART_BEAT_INTERVAL 30000 // Milliseconds between heartbeat events (30 seconds)

ESP_EVENT_DECLARE_BASE(VSCP_ESPNOW_EVENT); // declaration of the vscp espnow events family

// Callback functions

// Callback for esp-now received events
typedef void (*vscp_event_handler_cb_t)(const vscpEvent *pev, void *userdata);

// Callback for client node attach to network
typedef void (*vscp_espnow_attach_network_handler_cb_t)(wifi_pkt_rx_ctrl_t *prxdata, void *userdata);

// ----------------------------------------------------------------------------

/**
 * @brief Read VSCP register(s)
 * 
 * @param reg Register to start read at (<0xffff0000)
 * @param cnt Number of bytes to read (max 508 bytes)
 * @return int Return VSCP_ERROR_SUCCESS if OK, error code if not
 */
int
vscp_espnow_read_reg(uint32_t reg, uint16_t cnt);

/**
 * @brief Read VSCP standard register(s)
 * 
 * @param reg Register to start read at (>=0xffff0000)
 * @param cnt Number of bytes to read
 * @return int Return VSCP_ERROR_SUCCESS if OK, error code if not
 */
int
vscp_espnow_read_std_reg(uint32_t reg, uint16_t cnt);

/**
 * @brief Write VSCP standard register(s)
 * 
 * @param reg Register to write (>=0xffff0000)
 * @param cnt Number of bytes to write
 * @param pdata Pointer to data to write
 * @return int Return VSCP_ERROR_SUCCESS if OK, error code if not
 */
int
vscp_espnow_write_reg(uint32_t reg, uint16_t cnt, uint16_t *pdata);

/**
 * @brief Write VSCP standard register(s)
 * 
 * @param reg Register to write (>=0xffff0000)
 * @param cnt Number of bytes to write
 * @param pdata Pointer to data to write
 * @return int Return VSCP_ERROR_SUCCESS if OK, error code if not
 */
int
vscp_espnow_write_std_reg(uint32_t reg, uint16_t cnt, uint16_t *pdata);

/**
 * @brief Get VSCP timestamp
 *
 * @return timestamp in microsecond. The returned timestamp is based on the set system time.
 */

uint64_t
vscp_espnow_timestamp(void);

/**
 * @brief Start security initiation
 *
 * @return esp_err_t
 */

esp_err_t
vscp_espnow_sec_initiator(void);

// ----------------------------------------------------------------------------

/**
 * @fn vscp_espnow_heartbeat_task
 * @brief Task that send VSCP heartbeat events every minute
 *
 * @param pvParameter Pinter to data paremeter for task (not used)
 */
void
vscp_espnow_heartbeat_task(void *pvParameter);

/**
 * @brief Initialize VSCP esp-now
 *
 * @param sizeQueue Size for input queue
 * @return esp_err_t
 */
esp_err_t
vscp_espnow_init(const vscp_espnow_config_t *pconfig);

/**
 * @brief Send alpha probe
 *
 * @return int VSCP_ERROR_SUCCESS if all is OK
 *
 * A Beta/Gamma node send a VSCP probe on all channels until it get a
 * response from an alpha node. If it does it starts security key
 * exchange with that node. The node use the channel it received the probe on.
 */

int
vscp_espnow_probe(void);

/**
 * @brief  Check if GUID ios to me
 * 
 * @param pguid Pointer to GUID to check
 * @return Return true if GUID is same as ours
 */
bool
vscp_espnow_to_me(const uint8_t *pguid);

/**
 * @brief Build full GUID from mac address
 *
 * @param pguid Pointer to GUID that will get data
 * @return int VSCP_ERROR_SUCCESS is returned if all goes well. Otherwise VSCP error code is returned.
 */
int
vscp_espnow_get_node_guid(uint8_t *pguid);

/**
 * @fn vscp_espnow_sendEvent
 * @brief  Send event on vscp_espnow network
 *
 * @param destAddr Destination address. Can be NULL in which case the event
 *  is sent to all hosts in table.
 * @param pev Event to send
 * @param bSec Set to true to send encrypted.
 * @param wait_ms Time in milliseconds to wait for send
 * @return int Error code. VSCP_ERROR_SUCCESS if all is OK.
 */

int
vscp_espnow_sendEvent(const uint8_t *destAddr, const vscpEvent *pev, bool bSec, uint32_t wait_ms);

/**
 * @fn vscp_espnow_sendEventEx
 * @brief Send event ex on vscp_espnow network
 *
 * @param destAddr Destination address. Can be NULL in which case the event
 *  is sent to all hosts in table.
 * @param pex Pointer to event ex to send.
 * @param bSec Set to true to send encrypted.
 * @param wait_ms Time in milliseconds to wait for send.
 * @return int Error code. VSCP_ERROR_SUCCESS if all is OK.
 */
int
vscp_espnow_sendEventEx(const uint8_t *destAddr, const vscpEventEx *pex, bool bSec, uint32_t wait_ms);

/**
 * @fn vscp_espnow_getMinBufSizeEv
 * @brief Get minimum buffer size for a VSCP event
 *
 * @param pev Pointer to event
 * @param pkey Pointer to 32 bit key used for encryption.
 * @return size_t Needed buffer size or zero for error (invalid event pointer).
 */
size_t
vscp_espnow_getMinBufSizeEv(const vscpEvent *pev);

/**
 * @fn vscp_espnow_getMinBufSizeEx
 * @brief Get minimum buffer size for a VSCP ex event
 *
 * @param pex Pointer to event ex
 * @return size_t Needed buffer size or zero for error (invalid event pointer).
 */
size_t
vscp_espnow_getMinBufSizeEx(const vscpEventEx *pex);

/**
 * @brief Construct VSCP ESP-NOW frame form event structure
 *
 * @param buf Pointer to buffer that will get the frame data
 * @param len Size of buffer. The buffer should have room for the frame plus VSCP data so it
 * should have a length that exceeds VSCP_ESPNOW_PACKET_MIN_SIZE + VSCP event data length.
 * @param pev Pointer to VSCP event which will have its content written to the buffer.
 * @return int VSCP_ERROR_SUCCES is returned if all goes well. Otherwise VSCP error code is returned.
 */

int
vscp_espnow_evToFrame(uint8_t *buf, uint8_t len, const vscpEvent *pev);

/**
 * @brief Construct VSCP ESP-NOW frame form event ex structure
 *
 * @param buf Pointer to buffer that will get the frame data
 * @param len Size of buffer. The buffer should have room for the frame plus VSCP data so it
 * should have a length that exceeds VSCP_ESPNOW_PACKET_MIN_SIZE + VSCP event data length.
 * @param pex Pointer to VSCP event ex which will have its content written to the buffer.
 * @return int VSCP_ERROR_SUCCES is returned if all goes well. Otherwise VSCP error code is returned.
 */

int
vscp_espnow_exToFrame(uint8_t *buf, uint8_t len, const vscpEventEx *pex);

/**
 * @brief Fill in Data of VSCP ex event from esp-now frame
 *
 * @param pev Pointer to VSCP event
 * @param buf  Buffer holding esp-now frame data
 * @param len  Len of buffer
 * @param timestamp The event timestamp normally comes from wifi_pkt_rx_ctrl_t in the wifi frame. If
 * set to zero  it will be set from tickcount
 * @return int VSCP_ERROR_SUCCES is returned if all goes well. Otherwise VSCP error code is returned.
 */
int
vscp_espnow_frameToEv(vscpEvent *pev, const uint8_t *buf, uint8_t len, uint32_t timestamp);

/**
 * @brief Fill in Data of VSCP ex event from esp-now frame
 *
 * @param pex Pointer to VSCP ex event
 * @param buf  Buffer holding esp-now frame data
 * @param len  Len of buffer
 * @param timestamp The event timestamp normally comes from wifi_pkt_rx_ctrl_t in the wifi frame. If
 * set to zero  it will be set from tickcount
 * @return int VSCP_ERROR_SUCCES is returned if all goes well. Otherwise VSCP error code is returned.
 */
int
vscp_espnow_frameToEx(vscpEventEx *pex, const uint8_t *buf, uint8_t len, uint32_t timestamp);

/**
 * @fn vscp_espnow_set_vscp_user_handler_cb
 * @brief Set the VSCP event receive handler callback
 *
 * @param cb Callback that can do work when when a VSCP event is received.
 *
 * Set the VSCP event receive handler callback
 *
 */
void
vscp_espnow_set_vscp_user_handler_cb(vscp_event_handler_cb_t cb);

/**
 * @fn vscp_espnow_clear_vscp_handler_cb
 * @brief Clear VSCP event receive handler callback
 *
 */
void
vscp_espnow_clear_vscp_handler_cb(void);

/**
 * @fn vscp_espnow_parse_vscp_json
 * @brief Convert JSON string to VSCP event
 *
 * @param jsonVscpEventObj1
 * @param pev
 * @return int
 */
int
vscp_espnow_parse_vscp_json(vscpEvent *pev, const char *jsonVscpEventObj);

/**
 * @fn vscp_espnow_create_vscp_json
 * @brief Convert pointer to VSCP event to VSCP JSON string
 *
 * @param strObj String buffer that will get result
 * @param len Size of string buffer
 * @param pev Pointer to event
 * @return int Returns VSCP_ERROR_SUCCESS on OK, error code else.
 */
int
vscp_espnow_create_vscp_json(char *strObj, size_t len, vscpEvent *pev);

#ifdef __cplusplus
}
#endif /**< _cplusplus */

#endif