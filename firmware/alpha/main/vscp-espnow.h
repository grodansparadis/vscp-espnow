/*
  VSCP espnow

  This file is part of the VSCP (https://www.vscp.org)

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

#pragma once

#ifndef __VSCP_ESP_NOW_ALPHA_H__
#define __VSCP_ESP_NOW_ALPHA_H__

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <esp_now.h>

#include <vscp.h>
#include <vscp-espnow.h>

#define OTA_HASH_LEN 16 // SHA256 hash length in bytes
#define APP_KEY_LEN  16

#define VSCP_ESPNOW_DEFAULT_CHANNEL 6   // Default wifi channel app uses
#define VSCP_ESPNOW_DEFAULT_TTL     10  // Default hop count
#define VSCP_ESPNOW_DEFAULT_RSSI    -95 // Default minimum RSSI

#define VSCP_ESPNOW_EVENT_BASE 0x1000

// ----------------------------------------------------------------------------

typedef enum {
  VSCP_ESPNOW_MODE_FULL,  // Full mode (Wifi, Websrv, MQTT, VSCP Link etc)
  VSCP_ESPNOW_MODE_LIGHT, // Just esp-now
} vscp_esp_now_mode_t;

// ----------------------------------------------------------------------------

typedef struct {

  // Module
  vscp_esp_now_mode_t mode; // VSCP_ESPNOPW_MODE_HEAVY=provision with web i/f etc,
                            // VSCP_ESPNOW_MODE_LIGHT=esp-now only
  char nodeName[32];        // Friendly name for node
  uint8_t key[16];          // Security key (16 bytes)
  uint16_t nickname;        // Node nickname
  uint8_t encryption;       // 0=none, 1=AES128, 2=AES192, 3=AES256
  uint8_t channel;          // Channel to use (zero is current)
  uint8_t ttl;              // Time to live for esp-now frames
  int8_t rssi;              // Minimum RSSI for received frames

  uint32_t bootCnt; // Number of restarts (not editable)

  // VSCP Link
  bool vscplinkEnable;
  char vscplinkUrl[32];      // URL VSCP tcp/ip Link host (set to blank yto disable)
  uint16_t vscplinkPort;     // Port on VSCP tcp/ip Link host
  char vscplinkUsername[32]; // Username for VSCP tcp/ip Link host
  char vscplinkPassword[32]; // Password for VSCP tcp/ip Link host
  uint8_t vscpLinkKey[32];   // Security key (16 (EAS128)/24(AES192)/32(AES256))

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
  char mqttPubLog[128];
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
#define ESPNOW_SIZE_TX_BUF 10  // Size for transmitt buffer
#define ESPNOW_SIZE_RX_BUF 20  // Size for receive buffer
#define ESPNOW_MAXDELAY    512 // Ticks to wait for send queue access
#define ESPNOW_QUEUE_SIZE  10  // Size of the send/receive queue

// Node states
typedef enum {
  VSCP_ESPNOW_STATE_IDLE,   // Standard working state
  VSCP_ESPNOW_STATE_VIRGIN, // Node is uninitialized (needs provisioning)
  VSCP_ESPNOW_STATE_INIT,   // Active state during init until wifi is connected
  VSCP_ESPNOW_STATE_SET_DEFAULTS,
  VSCP_ESPNOW_STATE_KEY_EXCHANGE, // key exchange initiated
  VSCP_ESPNOW_STATE_PROVISIONING, // Active state during provisioning
  VSCP_ESPNOW_STATE_OTA,          // OTA update in progress
  VSCP_ESPNOW_STATE_PROBE,        // Probe for new nodes
  VSCP_ESPNOW_STATE_MAX
} vscp_espnow_state_t;

ESP_EVENT_DECLARE_BASE(VSCP_ESPNOW_EVENT); // declaration of the alpha events family

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
} vscp_espnow_cb_event_t;

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------

#define VSCP_ESPNOW_WAIT_MS_DEFAULT 1000 // One second

// ----------------------------------------------------------------------------

/*
  This is the version of VSCP we implement functionality for
*/
#define VSCP_STD_VERSION_MAJOR     1
#define VSCP_STD_VERSION_MINOR     14
#define VSCP_STD_VERSION_SUB_MINOR 10

/*
  The frame vesion (see VSCP over UDP in VSCP specification.)
  https://grodansparadis.github.io/vscp-doc-spec/#/./vscp_over_udp
*/
#define VSCP_ESPNOW_VERSION (1)

// Frame id

#define VSCP_ESPNOW_ID_MSB (0x55)
#define VSCP_ESPNOW_ID_LSB (0xAA)

/**
 * @brief Frame positions for data in the VSCP esp-now frame
 */

// Identify as vscp esp-now frame (0x55/0xAA)
#define VSCP_ESPNOW_POS_ID (0)

#define VSCP_ESPNOW_POS_TTL (2) // Number of hops frame can travel

// Sequence counter byte can be used to protect from replay attacks.
// It is increase by on for each event sent
#define VSCP_ESPNOW_POS_SEQ (3)

// 0xab where b = esp-now protocol version and a = type (alpha/beta...)
// bit 7,6 5,4 - protocol version (1)
// bit 3,2,1,0 - Encryption (0=none/1=AES128(/2=AES192/3=AES256))
#define VSCP_ESPNOW_POS_TYPE_VER (4)

#define VSCP_ESPNOW_POS_HEAD (5) // VSCP head bytes (2)

// Time stamp is the time_t from the time() call. Not that
// time_t can be 65 bits on some systems (__USE_TIME_BITS64)

/*

  The VSCP espnow frame

  | 0xAA | 0x55 | ttl | seq (1 byte) | Packet type & encryption settings (1 byte) || AES-128 CBC encrypted data (event)
  || IV (16-byte) |

  0 	    5					VSCP Level II Head MSB
  1 	    6					VSCP Level II Head LSB
  2 	    7					Timestamp microseconds MSB
  3 	    8					Timestamp microseconds
  4 	    9					Timestamp microseconds
  5 	    10				Timestamp microseconds LSB
  6 	    11				CLASS MSB
  7 	    12				CLASS LSB
  8 	    13				TYPE MSB
  9  	    14				TYPE LSB
  10      15        len data
  11-n 	  16				data ... limited to max 217 bytes
  len-2 	16 + len	CRC MSB (Calculated on HEAD + CLASS + TYPE + ADDRESS + SIZE + DATA…)
  len-1 	17 + len	CRC LSB
*/

// NOTE! This timestamp is not the same as the event timestamp and
// is only relevant to vscp-espnow
#define VSCP_ESPNOW_POS_TIME_STAMP  (7)  // 4 bytes
#define VSCP_ESPNOW_POS_VSCP_CLASS  (11) // VSCP class (2)
#define VSCP_ESPNOW_POS_VSCP_TYPE   (13) // VSCP Type (2)
#define VSCP_ESPNOW_POS_VSCP_LENGTH (15) // Data size (1)
#define VSCP_ESPNOW_POS_DATA        (16) // VSCP data (max 217 bytes)
// MSB of CRC is at 16 + (len of data)
// LSB of CRC is at 17 + (len of data)

/*!
  Size of VSCP part of frame (data should be added to this)

  head (2)
  timestamp (4)
  vscp class/type (4)
  data size (1)
  crc (2)
*/
#define VSCP_ESPNOW_VSCP_MIN_FRAME (13)

/*
  Minimum frame size
  5 bytes (0xAA 0x55 ttl seq pkt-type) + VSCP_ESPNOW_VSCP_MIN_FRAME (13) + 16 (IV)
  5 + 13 + 16 = 34

  Final fram has data size and padding
*/
#define VSCP_ESPNOW_MIN_FRAME (5 + VSCP_ESPNOW_VSCP_MIN_FRAME + 16)

/*
  Max VSCP data (of possible 512 bytes) that a frame can hold
  VSCP_ESPNOW_MIN_FRAME + 211 = 13 + 211 = 224
  224 % 16 = 0  (Max frame size)
*/
#define VSCP_ESPNOW_MAX_DATA (211)

/*
  Encryption length
  This is the part of the frame that should be encrypted.
*/
#define VSCP_ESPNOW_ENCRYPTION_LENGTH (VSCP_ESPNOW_MIN_FRAME - 5 - 16)

/*
  Note on max data size
  ---------------------
  An esp-now frame can hold a payload of max 250 bytes
  IV is 16 bytes
  VSCP frame data is 18 -bytes
  So left for Droplet data is 250-16-18 = 216 bytes
*/

#define VSCP_ESPNOW_IV_LEN (16)

// The event queue
typedef enum { VSCP_ESPNOW_SEND, VSCP_ESPNOW_RECV } vscp_espnow_event_id_t;

typedef struct {
  uint8_t dest_addr[ESP_NOW_ETH_ALEN];
  esp_now_send_status_t status;
} vscp_espnow_event_send_info_t;

typedef struct {
  uint8_t channel; // Channel message was received on
  uint8_t rssi;    // RSSI for received message
  uint8_t src_addr[ESP_NOW_ETH_ALEN];
  uint8_t *data;
  int size;
} vscp_espnow_event_rcv_info_t;

typedef union {
  vscp_espnow_event_send_info_t send;
  vscp_espnow_event_rcv_info_t rcv;
} vscp_espnow_event_info_t;

/*
  When ESPNOW sending or receiving callback function is called,
  post event to ESPNOW task.
 */
typedef struct {
  vscp_espnow_event_id_t id;
  vscp_espnow_event_info_t info;
} vscp_espnow_event_t;

/* Parameters of sending ESPNOW data. */
typedef struct {
  bool unicast;    // Send unicast ESPNOW data.
  bool broadcast;  // Send broadcast ESPNOW data.
  uint8_t state;   // Indicate that if has received broadcast ESPNOW data or not.
  uint32_t magic;  // Magic number which is used to determine which device to send unicast ESPNOW data.
  uint16_t count;  // Total count of unicast ESPNOW data to be sent.
  uint16_t delay;  // Delay between sending two ESPNOW data, unit: ms.
  int len;         // Length of ESPNOW data to be sent, unit: byte.
  uint8_t *buffer; // Buffer pointing to ESPNOW data.
  uint8_t dest_mac[ESP_NOW_ETH_ALEN]; // MAC address of destination device.
} vscp_espnow_send_param_t;

typedef struct {
  uint8_t node_type; // VSCP_DROPLET_ALPHA / VSCP_DROPLET_BETA / VSCP_DROPLET_GAMMA
  uint8_t freq;      // Heart beat frequency in seconds
} vscp_espnow_heart_beat_t;

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
  uint8_t seq;                   // Last seq counter
  uint8_t mac[ESP_NOW_ETH_ALEN]; // MAC address for node
} vscp_espnow_last_event_t;

/**
 * @brief Send and receive statistics
 *
 */
typedef struct {
  uint32_t nSend;             // # sent frames
  uint32_t nSendFailures;     // Number of send failures
  uint32_t nSendLock;         // Number of send lock give ups
  uint32_t nRcv;              // # received frames
  uint32_t nRcvVSCP;          // # received VSCP frames
  uint32_t nRcvFrameFailures; // Receive frame failures
  uint32_t nRcvOverruns;      // Number of receive overruns
  uint32_t nTimeDiffLarge;    // Frames skipped with time diff to large
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
  "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x" \
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

uint16_t
vscp_espnow_calculate_msg_checksum(const uint8_t *msg, uint8_t len);

/**
 * @brief Read processor on chip temperature
 * @return Temperature as floating point value
 */
float
app_read_onboard_temperature(void);

/**
 * @fn getMilliSeconds
 * @brief Get system time in Milliseconds
 *
 * @return Systemtime in milliseconds
 */
uint32_t
app_getMilliSeconds(void);

/**
 * @brief Get GUID for device
 *
 * @param pguid  Pointer to 16-byte buffer that will get resulting GUID
 * @return true on success.
 * @return false on failure.
 */

bool
app_get_device_guid(uint8_t *pguid);

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

// void
// app_firmware_send(size_t firmware_size, uint8_t sha[ESPNOW_OTA_HASH_LEN]);

/**
 * @brief Download firmware form server
 *
 * @param url Url to resource
 * @return size_t Size of downloaded image
 */

size_t
app_firmware_download(const char *url);

// ----------------------------------------------------------------------------

/*
  @breif Init the VSCP espnow subsystem

  @return VSCP_ERROR_SUCCESS on success, errorcode if error.
*/

int
vscp_espnow_init(void);

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

// ----------------------------------------------------------------------------

/**
 * @brief Send error event
 *
 * @param err Error code to send.
 * @return int VSCP_ERROR_SUCCESS is returne if OK.
 *
 * VSCP_CLASS1_ERROR, VSCP_TYPE_ERROR_ERROR is sent
 * with erro code as argument.
 */
int
vscp_espnow_send_error(uint8_t err);

/**
 * @fn vscp_espnow_heartbeat_task
 * @brief Task that send VSCP heartbeat events every minute
 *
 * @param pvParameter Pinter to data paremeter for task (not used)
 */
void
vscp_espnow_heartbeat_task(void *pvParameter);

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
vscp_espnow_is_to_me(const uint8_t *pguid);

/**
 * @brief Build full GUID from mac address
 *
 * @param pguid Pointer to GUID that will get data
 * @return int VSCP_ERROR_SUCCESS is returned if all goes well. Otherwise VSCP error code is returned.
 */
int
vscp_espnow_get_node_guid(uint8_t *pguid);

/*!
  Handler for received espnow frame
  @param prcv Received frame
  @return VSCP_ERROR_SUCCESS if all is OK. Error code on failure.
*/
int
vscp_espnow_receive_message(const vscp_espnow_event_rcv_info_t *prcv);

/**
 * @fn vscp_espnow_sendEvent
 * @brief  Send event on vscp_espnow network
 *
 * @param destAddr Destination address. Can be NULL in which case the event
 *  is sent to all hosts in table.
 * @param pev Event to send
 * @param wait_ms Time in milliseconds to wait for send
 * @return VSCP_ERROR_SUCCESS if all is OK. Error code on failure.
 */

int
vscp_espnow_sendEvent(const uint8_t *destAddr, const vscpEvent *pev, uint32_t wait_ms);

/**
 * @fn vscp_espnow_sendEventEx
 * @brief Send event ex on vscp_espnow network
 *
 * @param destAddr Destination address. Can be NULL in which case the event
 *  is sent to all hosts in table.
 * @param pex Pointer to event ex to send.
 * @param wait_ms Time in milliseconds to wait for send.
 * @return int Error code. VSCP_ERROR_SUCCESS if all is OK.
 */
int
vscp_espnow_sendEventEx(const uint8_t *destAddr, const vscpEventEx *pex, uint32_t wait_ms);

/**
 * @fn vscp_espnow_getMinBufSizeEv
 * @brief Get minimum buffer size for a VSCP event
 *
 * @param pev Pointer to event
 * @param pkey Pointer to 32 bit key used for encryption.
 * @return size_t Needed buffer size or zero for error (invalid event pointer).
 */
size_t
vscp_espnow_getFrameBufSizeEv(const vscpEvent *pev);

/**
 * @fn vscp_espnow_getMinBufSizeEx
 * @brief Get minimum buffer size for a VSCP ex event
 *
 * @param pex Pointer to event ex
 * @return size_t Needed buffer size or zero for error (invalid event pointer).
 *
 * Gets the minimum buffer size for a VSCP ex event placed in a frame. This includes
 * padding to go to a 16 byte boundary for the encrypted part of the frame (vscp-head -. crc).
 * The total size is
 *
 * frame start bytes + vscp-even + vscp-event data + padding to 16 byte boundary + iv (16)
 */
size_t
vscp_espnow_getFrameBufSizeEx(const vscpEventEx *pex);

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
 * @param mac Source mac address
 * @return int VSCP_ERROR_SUCCES is returned if all goes well. Otherwise VSCP error code is returned.
 */
int
vscp_espnow_frameToEv(vscpEvent *pev, const uint8_t *buf, uint8_t len, const uint8_t *mac);

/**
 * @brief Fill in Data of VSCP ex event from esp-now frame
 *
 * @param pex Pointer to VSCP ex event
 * @param buf  Buffer holding esp-now frame data
 * @param len  Len of buffer
 * @param mac Source mac address
 * @return int VSCP_ERROR_SUCCES is returned if all goes well. Otherwise VSCP error code is returned.
 */
int
vscp_espnow_frameToEx(vscpEventEx *pex, const uint8_t *buf, uint8_t len, const uint8_t *mac);

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

// ----------------------------------------------------------------------------
//          Callbacks that need to be implemented by module creator
// ----------------------------------------------------------------------------

/*!
  @fn vscp2_get_ms_cb
  @brief Get the time in milliseconds.

  @param Time in milliseconds.
*/

uint32_t
vscp2_get_ms_cb(void);

/**
 * @fn vscp2_get_stdreg_alarm_cb
 * @brief Get standard register alarm status.
 *
 * @param alarm Pointer to alarm register content.
 * @return VSCP_ERROR_SUCCESS on success.
 *
 * Eight bits are available to flag alarm status. Set bit
 * to indicate alarm. If you want more bits for alarm use user registers
 * and only use the bits here to indicate bits are set in user alarm
 * registers.
 *
 * Alarms should be cleared when read.
 *
 * If your device does not have alarm functionality
 * just return zero here.
 */

uint8_t
vscp2_get_stdreg_alarm_cb(void);

/**
  @fn vscp2_get_fw_ver_cb
  @brief Get firmware version

  @param major Pointer to integer that will get major version.
  @param minor Pointer to integer that will get minor version.
  @param patch Pointer to integer that will get patch version.

  you find the version in esp_app_desc_t which you can get
  a pointer to by calling esp_app_get_description() The .version
  string in this stucture holds the version set in the top cmake
  file.

  Here we use a the form  major.minor.patch  If you use another form
  you ned to adopt the parsing to your storage format
*/

int
vscp2_get_fw_ver_cb(int *major, int *minor, int *patch);

#endif