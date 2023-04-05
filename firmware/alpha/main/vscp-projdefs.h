/*
  projdefs.h

  This file contains project definitions for the VSCP TCP/IP link protocol code.
*/

#ifndef _VSCP_PROJDEFS_H_
#define _VSCP_PROJDEFS_H_

#include <vscp.h>

// Define one of
// #define VSCP_PROJDEF_LED_STRIP
#define VSCP_PROJDEF_LED_SIMPLE

#define VSCP_PROJDEF_ESPNOW_SESSION_POP "VSCPDEVICE"

/*!
  Name of device for level II capabilities announcement event.
*/

#define VSCP_PROJDEF_DEVICE_NAME "Frankfurt wifi alpha"

/*!
  Number of buttons
*/
#define VSCP_PROJDEF_BUTTON_CNT 1

#define VSCP_PROJDEF_ESP_MAXIMUM_RETRY 5

/**
 * Max 16 byte product id used for provisioning
 */
#define VSCP_PROJDEF_PROVISIONING_PRODUCT_ID "VSCP ESPNOW"

// ----------------------------------------------------------------------------
//                        VSCP helper lib defines
// ----------------------------------------------------------------------------

#define VSCP_FWHLP_CRYPTO_SUPPORT // AES crypto support
#define VSCP_FWHLP_JSON_SUPPORT   // Enable JSON support (Need cJSON lib)

// ----------------------------------------------------------------------------

// Node type for this node
#define PRJDEF_NODE_TYPE VSCP_DROPLET_ALPHA

// 16-bit nickname for node
#define PRJDEF_NODE_NICKNAME 0

// GPIO number for init. button
#define PRJDEF_INIT_BUTTON_PIN 0

// GPIO number for indicator LED's
#define PRJDEF_INDICATOR_LED_PIN_GREEN 2
#define PRJDEF_INDICATOR_LED_PIN_RED   3

// OTA mode
// ESPNOW_OTA_INITATOR or ESPNOW_OTA_RESPONDEDER
#define ESPNOW_OTA_MODE ESPNOW_OTA_INITATOR
// URL of server which hosts the firmware image.
#define PRJDEF_FIRMWARE_UPGRADE_URL "https://eurosource.se:443/download/alpha/vscp_espnow_alpha.bin"

// This allows you to skip the validation of OTA server certificate CN field.
#define PRJDEF_SKIP_COMMON_NAME_CHECK false

// This allows you to bind specified interface in OTA example.
#define PRJDEF_FIRMWARE_UPGRADE_BIND_IF false

// Select which interface type of OTA data go through.
// FIRMWARE_UPGRADE_BIND_IF_STA or EXAMPLE_FIRMWARE_UPGRADE_BIND_IF_ETH
#define PRJDEF_FIRMWARE_UPGRADE_BIND_IF_TYPE FIRMWARE_UPGRADE_BIND_IF_STA

// Wi-Fi provisioning component offers both, SoftAP and BLE transports. Choose one.
// PROV_TRANSPORT_BLE or PROV_TRANSPORT_SOFTAP
// #define PROV_TRANSPORT_BLE      PROV_TRANSPORT_BLE
// #define PROV_TRANSPORT_SOFTAP   PROV_TRANSPORT_SOFTAP

//
// ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_1 or ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_2
#define PRJDEF_ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_1
// #define ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_2

// This enables the production mode for security version 2.
// PROV_SEC2_PROD_MODE or PROV_SEC2_DEV_MODE
#define PRJDEF_PROV_MODE PROV_SEC2_PROD_MODE

//
// 1 = PROV_TRANSPORT_BLE, 2 = PROV_TRANSPORT_SOFTAP
#define PRJDEF_PROV_TRANSPORT 1

// Enable reseting provisioned credentials and state machine after session failure.
// This will restart the provisioning service after retries are exhausted.
#define PRJDEF_RESET_PROV_MGR_ON_FAILURE true

// Set the Maximum retry to avoid reconnecting to an inexistent AP or if credentials
// are misconfigured. Provisioned credentials are erased and internal state machine
// is reset after this threshold is reached.
#define PRJDEF_PROV_MGR_MAX_RETRY_CNT 5

// Show the QR code for provisioning.
#define PRJDEF_PROV_SHOW_QR true

// This enables BLE 4.2 features for Bluedroid.
// On IDF_TARGET_ESP32C3 || IDF_TARGET_ESP32S3
#define PRJDEF_PROV_USING_BLUEDROID true

// Channel should always be zero for alpha and gamma nodes
#define PRJDEF_VSCP_ESPNOW_CHANNEL 0

// Wifi mode
// ESPNOW_WIFI_MODE_STATION or ESPNOW_WIFI_MODE_STATION_SOFTAP
#define PRJDEF_VSCP_ESPNOW_WIFI_MODE WIFI_MODE_APSTA

// ESP_IF_WIFI_AP or WIFI_MODE_STA
#define PRJDEF_VSCP_ESPNOW_WIFI_IF ESP_IF_WIFI_AP

// Proof of Possession (PoP) string used to authorize session and derive shared key.
// #define VSCP_ESPNOW_SESSION_POP    "ESPNOW VSCP node ver 1"

// ESPNOW primary master for the example to use. The length of ESPNOW primary master must be 16 bytes.
#define PRJDEF_VSCP_ESPNOW_PMK "pmk1234567890123"

// ESPNOW local master for the example to use. The length of ESPNOW local master must be 16 bytes.
#define PRJDEF_VSCP_ESPNOW_LMK "lmk1234567890123"

// Select the ESP-NOW Sec Mode.
// ESPNOW_SEC_INITATOR or ESPNOW_SEC_RESPONDER
#define PRJDEF_VSCP_ESPNOW_SEC_MODE ESPNOW_SEC_INITATOR

// Select the ESP-NOW Prov Mode.
// ESPNOW_PROV_INITATOR or ESPNOW_PROV_RESPONDER
#define PRJDEF_VSCP_ESPNOW_PROV_MODE ESPNOW_PROV_INITATOR

// The channel on which sending and receiving ESPNOW data.
// #define ESPNOW_CHANNEL        0

// When enable long range, the PHY rate of ESP32 will be 512Kbps or 256Kbps
#define PRJDEF_ESPNOW_ENABLE_LONG_RANGE false

// Default login credentials
#define PRJDEF_DEFAULT_TCPIP_USER     "vscp"
#define PRJDEF_DEFAULT_TCPIP_PASSWORD "secret"

/**
  ----------------------------------------------------------------------------
                              Access Point
  ----------------------------------------------------------------------------
*/

// Channel for access point
#define PRJDEF_AP_CHANNEL 8

// Min 8 characters
#define PRJDEF_AP_PASSWORD ("0123456789")

// Maximum number of connetions to AP
#define PRJDEF_AP_MAX_CONNECTIONS 1

// Interval between beacon frames
#define PRJDEF_AP_BEACON_INTERVAL 100

/**
  ----------------------------------------------------------------------------
                              VSCP TCP/IP Link
  ----------------------------------------------------------------------------
  Defines for firmware level II
*/

/*!
  Max buffer for level II events. The buffer size is needed to
  convert an event to string. To handle all level II events
  512*5 + 110 = 2670 bytes is needed. In reality this is
  seldom needed so the value can be set to a lower value. In this
  case one should check the max data size for events that are of
  interest and set the max size accordingly
*/
#define PRJDEF_VSCP_LINK_MAX_BUF (2680)

/*!
  Define to show custom help. The callback is called so you can respond
  with your custom help text.  This can be used to save memory if you work
  on a constraint environment.

  If zero standard help is shown.
*/
// #define VSCP_LINK_CUSTOM_HELP_TEXT

/**
 * Undefine to send incoming events to all clients (default).
 */
#define PRJDEF_VSCP_LINK_SEND_TO_ALL

/*!
  Size for inout buffer and outputbuffer.
  Must be at least one for each fifo
*/
#define PRJDEF_VSCP_LINK_MAX_IN_FIFO_SIZE  (10)
#define PRJDEF_VSCP_LINK_MAX_OUT_FIFO_SIZE (10)

/**
 * Enable command also when rcvloop is active
 * Only 'quit' and 'quitloop' will work if
 * set to zero.
 */
#define PRJDEF_VSCP_LINK_ENABLE_RCVLOOP_CMD (1)

/**
 * If defined an UDP heartbeat is broadcasted every minute.
 */
#define THIS_FIRMWARE_USE_UDP_ANNOUNCE

/**
 * If defined a multicast heartbeat is broadcasted every minute.
 */
#define THIS_FIRMWARE_USE_MULTICAST_ANNOUNCE

/**
 * Firmware version
 */

#define THIS_FIRMWARE_MAJOR_VERSION   (0)
#define THIS_FIRMWARE_MINOR_VERSION   (0)
#define THIS_FIRMWARE_RELEASE_VERSION (1)
#define THIS_FIRMWARE_BUILD_VERSION   (0)

/**
 * User id (this is only defaults)
 */
#define THIS_FIRMWARE_USER_ID0 (0)
#define THIS_FIRMWARE_USER_ID1 (0)
#define THIS_FIRMWARE_USER_ID2 (0)
#define THIS_FIRMWARE_USER_ID3 (0)
#define THIS_FIRMWARE_USER_ID4 (0)

/**
 * Manufacturer id
 */
#define THIS_FIRMWARE_MANUFACTURER_ID0 (0)
#define THIS_FIRMWARE_MANUFACTURER_ID1 (0)
#define THIS_FIRMWARE_MANUFACTURER_ID2 (0)
#define THIS_FIRMWARE_MANUFACTURER_ID3 (0)

/**
 * Manufacturer subid
 */
#define THIS_FIRMWARE_MANUFACTURER_SUBID0 (0)
#define THIS_FIRMWARE_MANUFACTURER_SUBID1 (0)
#define THIS_FIRMWARE_MANUFACTURER_SUBID2 (0)
#define THIS_FIRMWARE_MANUFACTURER_SUBID3 (0)

/**
 * Set bootloader algorithm
 */
#define THIS_FIRMWARE_BOOTLOADER_ALGORITHM (0)

/**
 * Device family code 32-bit
 */
#define THIS_FIRMWARE_DEVICE_FAMILY_CODE (0ul)

/**
 * Device type code 32-bit
 */
#define THIS_FIRMWARE_DEVICE_TYPE_CODE (0ul)

/**
  Interval for heartbeats in seconds
*/
#define THIS_FIRMWARE_INTERVAL_HEARTBEATS (60)

/**
 * Interval for capabilities report in seconds
 */
#define THIS_FIRMWARE_INTERVAL_CAPS (60)

/**
 * Buffer size
 */
#define THIS_FIRMWARE_BUFFER_SIZE VSCP_MAX(vscp.h)

/**
 * Enable logging
 */
#define THIS_FIRMWARE_ENABLE_LOGGING

/**
 * Enable error reporting
 */
#define THIS_FIRMWARE_ENABLE_ERROR_REPORTING

/**
 * @brief Uncomment to enable writing to write protected areas
 *
 * Writing manufacturer data and GUID
 */
#define THIS_FIRMWARE_ENABLE_WRITE_2PROTECTED_LOCATIONS

/**
 * @brief Send server probe
 *
 */
#define THIS_FIRMWARE_VSCP_DISCOVER_SERVER

/**
 * GUID for this node (no spaces)
 */
#define THIS_FIRMWARE_GUID                                                                                             \
  {                                                                                                                    \
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x08, 0xdc, 0x12, 0x34, 0x56, 0x00, 0x01                     \
  }

/**
 * URL to MDF file
 */
#define THIS_FIRMWARE_MDF_URL "eurosource.se/wcang0.mdf"

/**
 * 16-bit firmware code for this device
 */
#define THIS_FIRMWARE_CODE (0)

/**
 * 16-bit firmware code for this device
 */
#define THIS_FIRMWARE_FAMILY_CODE (0)

/**
 * 16-bit firmware code for this device
 */
#define THIS_FIRMWARE_FAMILY_TYPE (0)

/**
 * @brief Maximum number of simultanonus TCP/IP connections
 * This is the maximum simultaneous number
 * of connections to the server
 */
#define MAX_TCP_CONNECTIONS 2

#endif // _VSCP_PROJDEFS_H_