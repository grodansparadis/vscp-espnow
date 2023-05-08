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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/time.h>

#include <freertos/FreeRTOS.h>
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <esp_check.h>
#include <esp_crc.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_now.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>

#include <protocomm.h>
#include <protocomm_security1.h>

#include "espnow.h"
#include <espnow_utils.h>
#include "espnow_security_handshake.h"

#include <cJSON.h>

#include <vscp-firmware-helper.h>
#include <vscp.h>

#include "vscp-espnow.h"

#include "vscp-compiler.h"
#include "vscp-projdefs.h"

static const char *TAG = "vscpnow";

/*
  This is the lowest time we will see in the system
  taken from espnow_timesync.c
*/
#define VSCP_ESPNOW_REF_TIME 1577808000 // 2020-01-01 00:00:00

// Globals
bool g_vscp_espnow_probe = false;

// Our  node type
#ifdef CONFIG_APP_VSCP_NODE_TYPE_ALPHA
uint8_t s_my_node_type = VSCP_DROPLET_ALPHA;
#endif
#ifdef CONFIG_APP_VSCP_NODE_TYPE_BETA
uint8_t s_my_node_type = VSCP_DROPLET_BETA;
#endif
#ifdef CONFIG_APP_VSCP_NODE_TYPE_GAMMA
uint8_t s_my_node_type = VSCP_DROPLET_GAMMA;
#endif

// Static
nvs_handle_t s_nvsHandle; // From app main

/*
  State machine state for the vscp_espnow stack

  All nodes come up as virgins. This means it is uninitialized and need to be
  set up in some way. Later in the boot process the node may be set as initialized
  and will then go to idle state.

  Nodes are initialized if:
  -------------------------
  alpha - If wifi provision has been done with a positive result (connection).
  beta  - If init sequency has been done with a positive result (ack on probe and key received).
  gamma - T.B.
*/
static vscp_espnow_state_t s_stateVscpEspNow = VSCP_ESPNOW_STATE_VIRGIN;

// static vscp_espnow_config_t s_vscp_espnow_config = { 0 };

// **VSCP espnow** persistent data
static vscp_espnow_persistent_t s_vscp_persistent = {
  .nickname = 0xffff,

  // Registers
  .userid = { 0 },
};

#define VSCP_ESPNOW_MAX_BUFFERED_NUM                                                                                   \
  (CONFIG_ESP32_WIFI_DYNAMIC_TX_BUFFER_NUM / 2) // Not more than CONFIG_ESP32_WIFI_DYNAMIC_TX_BUFFER_NUM

// Free running counter that is updated for every sent frame
uint8_t g_vscp_espnow_sendSequence = 0;

static EventGroupHandle_t s_vscp_espnow_event_group;
#define VSCP_ESPNOW_WAIT_PROBE_RESPONSE_BIT BIT0 // Wait for probe response

static const uint8_t scan_channel_sequence[] = { 1, 6, 11, 1, 6, 11, 2, 3, 4, 5, 7, 8, 9, 10, 12, 13 };
#define RESEND_SCAN_COUNT_MAX (sizeof(scan_channel_sequence) * 2)

static uint8_t VSCP_ESPNOW_ADDR_SELF[6] = { 0 };

const uint8_t VSCP_ESPNOW_ADDR_NONE[6]      = { 0 };
const uint8_t VSCP_ESPNOW_ADDR_BROADCAST[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0Xff };

/*
  This is the mac address for the beta/gamma node that is probed. If
  initialization is started and this address is all zero any node will
  get a response form the initialization process.
*/
#if (s_my_node_type == VSCP_DROPLET_ALPHA)
uint8_t VSCP_ESPNOW_ADDR_PROBE_NODE[6] = { 0 };
#endif

/*!
  GUID for unassigned node.
*/
static uint8_t s_VSCP_ESPNOW_GUID_NONE[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/*!
  GUID used for a node that is uninitialized.
*/
static uint8_t s_VSCP_ESPNOW_GUID_UNINIT[16] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE,
                                                 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00 };

// User handler for received vscp_espnow frames/events
static vscp_event_handler_cb_t s_vscp_event_handler_cb = NULL;

// User handler for client when attaching to network
static vscp_espnow_attach_network_handler_cb_t s_vscp_espnow_attach_network_handler_cb = NULL;

/*
  Structure that holds data for node that should
  be provisioned.
*/
// typedef struct {
//   uint8_t mac[6];       // MAC address for node to provision
//   uint8_t keyLocal[16]; // Local key for node
// } vscp_espnow_provisioning_t;

/*
  Info about node that is under provisioning
*/
// static vscp_espnow_provisioning_t s_provisionNodeInfo = { 0 };

// Statistics
static uint8_t s_vscp_espnow_seq = 0; // Sequency counter for sent events

// static uint8_t s_cntSeqNodes = 0;
// vscp_espnow_last_event_t **s_pSeqNodes;

static vscp_espnow_stats_t s_vscpEspNowStats;

// Forward declarations

// ----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_read_reg
//

int
vscp_espnow_read_reg(uint32_t address, uint16_t cnt)
{
  // vscp espnow can't handle 512 byte frames and cnt
  // can be 508 so we may need to do more than one
  // read/write reply
  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_read_standard_reg
//
// Address is in range 0xffff0000 - 0xffffffff
// Current standard register defines are 0x80 - 0xff
// Read as 0xffffff80 - 0xffffffff

int
vscp_espnow_read_standard_reg(uint32_t address, uint16_t cnt)
{
  uint32_t raddr = address & 0xff;
  uint16_t rcnt  = 0;

  // Check that we are reading a valid register
  if (address < 0xffffff80) {
    ESP_LOGE(TAG, "[%s, %d]: Invalid standard register address addr=%lX", __func__, __LINE__, address);
    return VSCP_ERROR_MEMORY;
  }

  vscpEvent *pev = vscp_fwhlp_newEvent();
  if (NULL == pev) {
    ESP_LOGE(TAG, "[%s, %d]: Could not allocate memory for event", __func__, __LINE__);
    return VSCP_ERROR_MEMORY;
  }

  pev->pdata = VSCP_CALLOC(cnt);
  if (NULL == pev->pdata) {
    ESP_LOGE(TAG, "[%s, %d]: Could not allocate memory for event data", __func__, __LINE__);
    vscp_fwhlp_deleteEvent(&pev);
    return VSCP_ERROR_MEMORY;
  }

  pev->sizeData = cnt;

  // Work thrue all requested registers
  while (rcnt < cnt) {

    //  Return alarm status
    if (VSCP_STD_REGISTER_ALARM_STATUS == raddr) {
      pev->pdata[rcnt] = vscp2_get_stdreg_alarm_cb;
    }

    else if (VSCP_STD_REGISTER_MAJOR_VERSION == raddr) {
      pev->pdata[rcnt] = VSCP_STD_VERSION_MAJOR;
    }

    else if (VSCP_STD_REGISTER_MINOR_VERSION == raddr) {
      pev->pdata[rcnt] = VSCP_STD_VERSION_MINOR;
    }

    else if (VSCP_STD_REGISTER_SUB_VERSION == raddr) {
      pev->pdata[rcnt] = VSCP_STD_VERSION_SUB_MINOR;
    }

    //  User id
    else if ((VSCP_STD_REGISTER_USER_ID >= raddr) && ((VSCP_STD_REGISTER_USER_ID + 4) <= raddr)) {
      pev->pdata[rcnt] = s_vscp_persistent.userid[raddr - VSCP_STD_REGISTER_USER_ID];
    }

    /*
      Manufacturer id space
    */
    else if ((VSCP_STD_REGISTER_USER_MANDEV_ID >= raddr) && ((VSCP_STD_REGISTER_USER_MANDEV_ID + 3) <= raddr)) {

      switch (raddr - VSCP_STD_REGISTER_USER_MANDEV_ID) {
        case 0:
          pev->pdata[rcnt] = THIS_FIRMWARE_MANUFACTURER_ID0;
          break;

        case 1:
          pev->pdata[rcnt] = THIS_FIRMWARE_MANUFACTURER_ID1;
          break;

        case 2:
          pev->pdata[rcnt] = THIS_FIRMWARE_MANUFACTURER_ID2;
          break;

        case 3:
          pev->pdata[rcnt] = THIS_FIRMWARE_MANUFACTURER_ID3;
          break;
      }
    }

    /*
      Manufacturer id space
    */
    else if ((VSCP_STD_REGISTER_USER_MANSUBDEV_ID >= raddr) && ((VSCP_STD_REGISTER_USER_MANSUBDEV_ID + 3) <= raddr)) {
      switch (raddr - VSCP_STD_REGISTER_USER_MANSUBDEV_ID) {
        case 0:
          pev->pdata[rcnt] = THIS_FIRMWARE_MANUFACTURER_SUBID0;
          break;

        case 1:
          pev->pdata[rcnt] = THIS_FIRMWARE_MANUFACTURER_SUBID1;
          break;

        case 2:
          pev->pdata[rcnt] = THIS_FIRMWARE_MANUFACTURER_SUBID2;
          break;

        case 3:
          pev->pdata[rcnt] = THIS_FIRMWARE_MANUFACTURER_SUBID3;
          break;
      }
    }

    // Nickname LSB
    else if (VSCP_STD_REGISTER_NICKNAME_ID_LSB == raddr) {
      pev->pdata = s_vscp_persistent.nickname & 0xff;
    }

    // Nickname MSB
    else if (VSCP_STD_REGISTER_PAGE_SELECT_MSB == raddr) {
      pev->pdata = (s_vscp_persistent.nickname >> 8) & 0xff;
    }

    // Firmware version
    else if ((VSCP_STD_REGISTER_FIRMWARE_MAJOR >= raddr) && (VSCP_STD_REGISTER_FIRMWARE_SUBMINOR <= raddr)) {
      int rv;
      int major, minor, patch;
      if (VSCP_ERROR_SUCCESS == (rv = vscp2_get_fw_ver_cb(&major, &minor, &patch))) {
        switch (raddr) {

          case VSCP_STD_REGISTER_FIRMWARE_MAJOR:
            pev->pdata[rcnt] = major;
            break;

          case VSCP_STD_REGISTER_FIRMWARE_MINOR:
            pev->pdata[rcnt] = minor;
            break;

          case VSCP_STD_REGISTER_FIRMWARE_SUBMINOR:
            pev->pdata[rcnt] = patch;
            break;
        }
      }
      else {
        ESP_LOGE(TAG, "[%s, %d]: Failed to get firmware version", __func__, __LINE__);
        vscp_espnow_send_error(rv);
      }
      break;
    }

    else if (VSCP_STD_REGISTER_BOOT_LOADER == raddr) {
      // Espressif ESP32 standard algorithm
      pev->pdata[rcnt] = VSCP_BOOTLOADER_ESP;
    }

    else if (VSCP_STD_REGISTER_BUFFER_SIZE == raddr) {
      // Not used
      pev->pdata[cnt] = 0;
    }

    else if (VSCP_STD_REGISTER_PAGES_COUNT == raddr) {
      // Not used
      pev->pdata[cnt] = 0;
    }

    /* Unsigned 32-bit integer for family code */
    else if ((VSCP_STD_REGISTER_FAMILY_CODE <= raddr) && ((VSCP_STD_REGISTER_FAMILY_CODE + 3) >= raddr)) {
      // We do not use
      pev->pdata[cnt] = 0;
      break;
    }

    /* Unsigned 32-bit integer for device type */
    else if ((VSCP_STD_REGISTER_DEVICE_TYPE <= raddr) && ((VSCP_STD_REGISTER_DEVICE_TYPE + 3) >= raddr)) {
      // We do not use
      pev->pdata[cnt] = 0;
      break;
    }

    /* Firmware code for device (MSB). */
    else if (VSCP_STD_REGISTER_FIRMWARE_CODE_MSB == raddr) {
      pev->pdata[cnt] = THIS_FIRMWARE_CODE >> 8;
    }

    /* Firmware code for device (LSB). */
    else if (VSCP_STD_REGISTER_FIRMWARE_CODE_LSB == raddr) {
      pev->pdata[cnt] = THIS_FIRMWARE_CODE & 0xff;
    }

    /* 0xd0 - 0xdf  - GUID  */
    else if (VSCP_STD_REGISTER_GUID == raddr) {
      uint8_t GUID[16];
      vscp_espnow_get_node_guid(GUID);
    }

    /* 0xe0 - 0xff  - MDF  */
    else if (VSCP_STD_REGISTER_DEVICE_URL == raddr) {
      uint8_t pos = raddr - VSCP_STD_REGISTER_DEVICE_URL;
      char mdf[32];
      if (pos < 32) {
        memset(mdf, 0, 32);
        strcpy(mdf, THIS_FIRMWARE_MDF_URL);
        pev->pdata[cnt] = mdf[pos];
      }
    }

    else {
      pev->pdata[rcnt] = 0;
    }

    rcnt++;
    raddr++;

    // If read count is to large - break
    // allocate new data
    if (!raddr) {
      pev->pdata    = realloc(pev->pdata, rcnt);
      pev->sizeData = rcnt;
      break;
    }

  } // while

  VSCP_FREE(pev);
  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_write_reg
//

int
vscp_espnow_write_reg(uint32_t reg, uint16_t cnt, uint16_t *pdata)
{
  vscpEvent *pev = vscp_fwhlp_newEvent();
  if (NULL == pev) {
    return VSCP_ERROR_MEMORY;
  }

  // Reset device
  // else if (VSCP_STD_REGISTER_NODE_RESET == raddr) {
  //   if (
  //   if (vscp_nore_rest_time
  // }

  VSCP_FREE(pev);
  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_write_std_reg
//

int
vscp_espnow_write_std_reg(uint32_t reg, uint16_t cnt, uint16_t *pdata)
{
  vscpEvent *pev = vscp_fwhlp_newEvent();
  if (NULL == pev) {
    return VSCP_ERROR_MEMORY;
  }

  VSCP_FREE(pev);
  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_send_errror
//

int
vscp_espnow_send_error(uint8_t err)
{
  vscpEventEx ex;

  memset(&ex, 0, sizeof(vscpEventEx));
  ex.vscp_class = VSCP_CLASS1_ERROR;
  ex.vscp_type  = VSCP_TYPE_ERROR_ERROR;
  ex.sizeData   = 5;
  ex.data[3]    = err;
  return vscp_espnow_sendEventEx(ESPNOW_ADDR_BROADCAST, &ex, true, 1000);
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_set_nickname
//

static int
vscp_espnow_set_nickname(uint16_t nickname)
{
  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_process
//
// VSCP event protocol processing takes here. Different callbacks are called
// to handle
//

static int
vscp_espnow_event_process(const vscpEvent *pev)
{
  uint16_t vscp_class;
  uint8_t offset = 0;

  // Check pointer
  if (NULL == pev) {
    ESP_LOGE(TAG, "NULL Event");
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscp_class = pev->vscp_class;
  if ((vscp_class >= 512) && (vscp_class < 1024)) {
    offset = 16;       // Data is after GUID
    vscp_class -= 512; // We pretend to be level I event
  }

  if (VSCP_CLASS1_PROTOCOL == vscp_class) {
    switch (pev->vscp_type) {

      case VSCP_TYPE_PROTOCOL_PROBE_ACK: {
        vscpEventEx ex;
        memset(&ex, 0, sizeof(vscpEventEx));
        ex.vscp_class = VSCP_CLASS1_PROTOCOL;
        ex.vscp_type  = VSCP_TYPE_PROTOCOL_PROBE_ACK;
        ex.sizeData   = 0;
        vscp_espnow_sendEventEx(ESPNOW_ADDR_BROADCAST, &ex, true, 1000);
      } break;

      // 8/16 bit versions accepted
      case VSCP_TYPE_PROTOCOL_SET_NICKNAME: {
        uint16_t nickname_new;
        // uint16_t nickname;

        if (4 == pev->sizeData) {
          // nickname     = ((pev->pdata[0]) << 8) + pev->pdata[2];
          nickname_new = ((pev->pdata[1]) << 8) + pev->pdata[3];
          vscp_espnow_set_nickname(nickname_new);
        }
        else if (2 == pev->sizeData) {
          // nickname     = pev->pdata[0];
          nickname_new = pev->pdata[1];
          vscp_espnow_set_nickname(nickname_new);
        }
        else {
          return (VSCP_ERROR_SUCCESS == vscp_espnow_send_error(VSCP_ERROR_INVALID_SYNTAX));
        }
      } break;

      case VSCP_TYPE_PROTOCOL_DROP_NICKNAME:
        vscp_espnow_set_nickname(0xffff);
        break;

      case VSCP_TYPE_PROTOCOL_READ_REGISTER:
        // We do nothing, this is a level II node
        break;

      case VSCP_TYPE_PROTOCOL_WRITE_REGISTER:
        // We do nothing this is a level II node
        break;

      case VSCP_TYPE_PROTOCOL_RESET_DEVICE:
        // TODO
        break;

      case VSCP_TYPE_PROTOCOL_PAGE_READ:
        // We do nothing this is a level II node
        break;

      case VSCP_TYPE_PROTOCOL_PAGE_WRITE:
        // We do nothing this is a level II node
        break;

      case VSCP_TYPE_PROTOCOL_INCREMENT_REGISTER:
        // We do nothing this is a level II node
        break;

      case VSCP_TYPE_PROTOCOL_DECREMENT_REGISTER:
        // We do nothing this is a level II node
        break;

      case VSCP_TYPE_PROTOCOL_WHO_IS_THERE:
        // TODO
        break;

      case VSCP_TYPE_PROTOCOL_GET_MATRIX_INFO:
        // TODO
        break;

      case VSCP_TYPE_PROTOCOL_GET_EMBEDDED_MDF:
        // TODO
        break;

      case VSCP_TYPE_PROTOCOL_EXTENDED_PAGE_READ:
        // We do nothing this is a level II node
        break;

      case VSCP_TYPE_PROTOCOL_EXTENDED_PAGE_WRITE:
        // We do nothing this is a level II node
        break;

      case VSCP_TYPE_PROTOCOL_GET_EVENT_INTEREST:
        // TODO
        break;
    }
  }
  else if (VSCP_CLASS2_PROTOCOL == pev->vscp_class) {

    switch (pev->vscp_type) {

      case VSCP2_TYPE_PROTOCOL_READ_REGISTER: {

        uint32_t reg;
        uint8_t val;
        uint16_t cnt;

        if (pev->sizeData >= 22) {

          if (vscp_espnow_to_me(pev->pdata)) {

            // Get register
            reg = ((uint32_t) pev->pdata[16] << 24) + ((uint32_t) pev->pdata[17] << 16) +
                  ((uint32_t) pev->pdata[18] << 8) + pev->pdata[19];

            // Get # registers to read
            cnt = ((uint16_t) pev->pdata[20] << 8) + pev->pdata[21];

            if (cnt > 508) {
              return (VSCP_ERROR_SUCCESS == vscp_espnow_send_error(VSCP_ERROR_INVALID_SYNTAX));
            }

            if (reg > 0xffff0000) {
              return vscp_espnow_read_standard_reg(reg, cnt);
            }
            else {
              return vscp_espnow_read_reg(reg, cnt);
            }
          }
        }
        else {
          return (VSCP_ERROR_SUCCESS == vscp_espnow_send_error(VSCP_ERROR_INVALID_SYNTAX));
        }
      } break;

      case VSCP2_TYPE_PROTOCOL_WRITE_REGISTER:
        break;
    }
  }
  else {
  }

  return VSCP_ERROR_SUCCESS;
}

/*
static bool
addSeqNode(const uint8_t *pmac, uint8_t seq)
{
  // Is this the first
  if (!s_cntSeqNodes) {
    s_pSeqNodes = malloc(sizeof(vscp_espnow_last_event_t[1]));
    if (NULL == s_pSeqNodes) {
      ESP_LOGE(TAG, "Failed to allocate seq node structure");
      return false;
    }
    s_cntSeqNodes++;
    s_pSeqNodes[0]->seq = seq;
    memcpy(s_pSeqNodes[0]->mac, pmac, ESPNOW_ADDR_LEN);
  }
  else {
    // Check count
    if (s_cntSeqNodes >= MAX_SEQ_NODES) {
      ESP_LOGE(TAG, "Max number of seq nodes has been reached.");
      return false;
    }

    s_pSeqNodes = realloc(s_pSeqNodes, sizeof(vscp_espnow_last_event_t[s_cntSeqNodes + 1]));
    if (NULL == s_pSeqNodes) {
      ESP_LOGE(TAG, "Failed to allocate seq node structure");
      return false;
    }
    s_cntSeqNodes++;
    s_pSeqNodes[s_cntSeqNodes]->seq = seq;
    memcpy(s_pSeqNodes[s_cntSeqNodes]->mac, pmac, ESPNOW_ADDR_LEN);
  }

  return true;
}

static bool
validateSeqNode(const uint8_t *pmac, uint8_t seq)
{
  // Find node in table and if seq is greater than previous seq update seq
  // and return true
  for (int i = 0; i < s_cntSeqNodes; i++) {
    if ((0 == memcmp(s_pSeqNodes[s_cntSeqNodes]->mac, pmac, ESPNOW_ADDR_LEN)) &&
        (seq > s_pSeqNodes[s_cntSeqNodes]->seq)) {
      s_pSeqNodes[s_cntSeqNodes]->seq = seq;
      return true;
    }
  }

  // If nodes first event add it to the table.
  // There should be no risk for a replay on a first
  // event from a node. You can't replay something that
  // has not been sent yet.
  // If it can be added true is returned.
  return addSeqNode(pmac, seq);
}
*/

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_timestamp
//

uint64_t
vscp_espnow_timestamp(void)
{
  struct timeval tv_now;
  if (-1 == gettimeofday(&tv_now, NULL)) {
    return ESP_ERR_ESPNOW_INTERNAL;
  }

  return (int64_t) tv_now.tv_sec * 1000000L + (int64_t) tv_now.tv_usec;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_sec_initiator
//
// Alpha node start security key transfer to beta nodes with this method
//

esp_err_t
vscp_espnow_sec_initiator(void)
{
  esp_err_t ret;
  uint8_t key_info[APP_KEY_LEN];

  ESP_LOGI(TAG, "----> sec initiator started");

  if (espnow_get_key(key_info) != ESP_OK) {
    ESP_LOGI(TAG, "----> New security key is created");
    esp_fill_random(key_info, APP_KEY_LEN);
  }
  espnow_set_key(key_info);

  ESP_LOGI(TAG, "----> Starting security node scan");

  uint32_t start_time1                  = xTaskGetTickCount();
  espnow_sec_result_t espnow_sec_result = { 0 };
  espnow_sec_responder_t *info_list     = NULL;
  size_t num                            = 0;
  ret                                   = espnow_sec_initiator_scan(&info_list, &num, pdMS_TO_TICKS(3000));
  ESP_LOGI(TAG, "----> Nodes waiting for security params: %u", num);

  if (num == 0) {
    ESP_FREE(info_list);
    ESP_LOGW(TAG, "No sec nodes found");
    return ESP_ERR_INVALID_SIZE;
  }

  espnow_addr_t *dest_addr_list = ESP_MALLOC(num * ESPNOW_ADDR_LEN);

  for (size_t i = 0; i < num; i++) {
    ESP_LOGI(TAG, "sec node %d - " MACSTR " ", i, MAC2STR(info_list[i].mac));
    memcpy(dest_addr_list[i], info_list[i].mac, ESPNOW_ADDR_LEN);
  }

  espnow_sec_initiator_scan_result_free();

  uint32_t start_time2 = xTaskGetTickCount();
  ret = espnow_sec_initiator_start(key_info, CONFIG_APP_ESPNOW_SESSION_POP, dest_addr_list, num, &espnow_sec_result);
  ESP_ERROR_GOTO(ret != ESP_OK, EXIT, "<%s> espnow_sec_initiator_start", esp_err_to_name(ret));

  ESP_LOGI(TAG,
           "App key is sent to the device to complete, Spend time: %" PRId32 "ms, Scan time: %" PRId32 "ms",
           (xTaskGetTickCount() - start_time1) * portTICK_PERIOD_MS,
           (start_time2 - start_time1) * portTICK_PERIOD_MS);
  ESP_LOGI(TAG,
           "Devices security completed, successes_num: %u, unfinished_num: %u",
           espnow_sec_result.successed_num,
           espnow_sec_result.unfinished_num);

EXIT:
  ESP_FREE(dest_addr_list);
  return espnow_sec_initiator_result_free(&espnow_sec_result);
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_is_to_me
//

bool
vscp_espnow_to_me(const uint8_t *pguid)
{
  uint8_t GUID[16];
  vscp_espnow_get_node_guid(GUID);
  return (0 == memcmp(GUID, pguid, 16)) ? true : false;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_get_node_guid
//

int
vscp_espnow_get_node_guid(uint8_t *pguid)
{
  esp_err_t ret;

  // Ethernet based GUID
  uint8_t prebytes[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe };

  // Check GUID pointer
  if (NULL == pguid) {
    ESP_LOGE(TAG, "Pointer to GUID is NULL");
    return VSCP_ERROR_INVALID_POINTER;
  }

  ret = esp_read_mac(pguid + 8, ESP_MAC_WIFI_STA);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_efuse_mac_get_default failed to get GUID. rv=%d", ret);
  }

  pguid[14] = (s_vscp_persistent.nickname << 8) & 0xff;
  pguid[15] = s_vscp_persistent.nickname & 0xff;

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_getMinBufSizeEv
//

size_t
vscp_espnow_getMinBufSizeEv(const vscpEvent *pev)
{
  // Need event pointer
  if (NULL == pev) {
    ESP_LOGE(TAG, "Pointer to event is NULL");
    return 0;
  }

  return (VSCP_ESPNOW_MIN_FRAME + pev->sizeData);
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_getMinBufSizeEx
//

size_t
vscp_espnow_getMinBufSizeEx(const vscpEventEx *pex)
{
  // Need event ex pointer
  if (NULL == pex) {
    ESP_LOGE(TAG, "Pointer to event ex is NULL");
    return 0;
  }

  return (VSCP_ESPNOW_MIN_FRAME + pex->sizeData);
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_evToFrame
//

int
vscp_espnow_evToFrame(uint8_t *buf, uint8_t len, const vscpEvent *pev)
{
  // Need a buffer
  if (NULL == buf) {
    ESP_LOGE(TAG, "Pointer to buffer is NULL");
    return VSCP_ERROR_INVALID_POINTER;
  }

  // Need event
  if (NULL == pev) {
    ESP_LOGE(TAG, "Pointer to event is NULL");
    return VSCP_ERROR_INVALID_POINTER;
  }

  // Must have room for frame
  if (len < (VSCP_ESPNOW_MIN_FRAME + pev->sizeData)) {
    ESP_LOGE(TAG, "Size of buffer is to small to fit event, len:%d", len);
    return VSCP_ERROR_PARAMETER;
  }

  memset(buf, 0, len);

  buf[VSCP_ESPNOW_POS_ID]     = VSCP_ESPNOW_ID_MSB;
  buf[VSCP_ESPNOW_POS_ID + 1] = VSCP_ESPNOW_ID_LSB;

// Set encryption
#ifdef CONFIG_APP_VSCP_NODE_TYPE_ALPHA
  buf[VSCP_ESPNOW_POS_TYPE_VER] = (VSCP_DROPLET_ALPHA << 6) + (VSCP_ESPNOW_VERSION << 4) + (0 & 0x0f);
#endif

#ifdef CONFIG_APP_VSCP_NODE_TYPE_BETA
  buf[VSCP_ESPNOW_POS_TYPE_VER] = (VSCP_DROPLET_BETA << 6) + (VSCP_ESPNOW_VERSION << 4) + (0 & 0x0f);
#endif

#ifdef CONFIG_APP_VSCP_NODE_TYPE_GAMMA
  buf[VSCP_ESPNOW_POS_TYPE_VER] = (VSCP_DROPLET_GAMMA << 6) + (VSCP_ESPNOW_VERSION << 4) + (0 & 0x0f);
#endif

  // Set seq count
  buf[VSCP_ESPNOW_POS_SEQ] = s_vscp_espnow_seq++;

  // Set timestamp (in seconds)
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  buf[VSCP_ESPNOW_POS_TIME_STAMP]     = (tv_now.tv_sec >> 24) & 0xff;
  buf[VSCP_ESPNOW_POS_TIME_STAMP + 1] = (tv_now.tv_sec >> 16) & 0xff;
  buf[VSCP_ESPNOW_POS_TIME_STAMP + 2] = (tv_now.tv_sec >> 8) & 0xff;
  buf[VSCP_ESPNOW_POS_TIME_STAMP + 3] = tv_now.tv_sec & 0xff;

  long long node_time =
    (long long int) (((uint32_t) buf[VSCP_ESPNOW_POS_TIME_STAMP] << 24) +
                     ((uint32_t) buf[VSCP_ESPNOW_POS_TIME_STAMP + 1] << 16) +
                     ((uint32_t) buf[VSCP_ESPNOW_POS_TIME_STAMP + 2] << 8) + buf[VSCP_ESPNOW_POS_TIME_STAMP + 3]);

  // head
  buf[VSCP_ESPNOW_POS_HEAD]     = (pev->head >> 8) & 0xff;
  buf[VSCP_ESPNOW_POS_HEAD + 1] = pev->head & 0xff;

  // nickname
  buf[VSCP_ESPNOW_POS_NICKNAME]     = pev->GUID[14];
  buf[VSCP_ESPNOW_POS_NICKNAME + 1] = pev->GUID[15];

  // vscp-class
  buf[VSCP_ESPNOW_POS_VSCP_CLASS]     = (pev->vscp_class >> 8) & 0xff;
  buf[VSCP_ESPNOW_POS_VSCP_CLASS + 1] = pev->vscp_class & 0xff;

  // vscp-type
  buf[VSCP_ESPNOW_POS_VSCP_TYPE]     = (pev->vscp_type >> 8) & 0xff;
  buf[VSCP_ESPNOW_POS_VSCP_TYPE + 1] = pev->vscp_type & 0xff;

  buf[VSCP_ESPNOW_POS_SIZE] = pev->sizeData;

  // data
  if (pev->sizeData) {
    memcpy((buf + VSCP_ESPNOW_POS_DATA), pev->pdata, pev->sizeData);
  }

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_exToFrame
//

int
vscp_espnow_exToFrame(uint8_t *buf, uint8_t len, const vscpEventEx *pex)
{
  // Need a buffer
  if (NULL == buf) {
    ESP_LOGE(TAG, "Pointer to buffer is NULL");
    return VSCP_ERROR_INVALID_POINTER;
  }

  // Need event
  if (NULL == pex) {
    ESP_LOGE(TAG, "Pointer to event is NULL");
    return VSCP_ERROR_INVALID_POINTER;
  }

  // Must have room for frame
  if (len < (VSCP_ESPNOW_MIN_FRAME + pex->sizeData)) {
    ESP_LOGE(TAG, "Size of buffer is to small to fit event, len:%d", len);
    return VSCP_ERROR_PARAMETER;
  }

  memset(buf, 0, len);

  buf[VSCP_ESPNOW_POS_ID]     = VSCP_ESPNOW_ID_MSB;
  buf[VSCP_ESPNOW_POS_ID + 1] = VSCP_ESPNOW_ID_LSB;

// Set encryption
#ifdef CONFIG_APP_VSCP_NODE_TYPE_ALPHA
  buf[VSCP_ESPNOW_POS_TYPE_VER] = (VSCP_DROPLET_ALPHA << 6) + (VSCP_ESPNOW_VERSION << 4) + (0 & 0x0f);
#endif

#ifdef CONFIG_APP_VSCP_NODE_TYPE_BETA
  buf[VSCP_ESPNOW_POS_TYPE_VER] = (VSCP_DROPLET_BETA << 6) + (VSCP_ESPNOW_VERSION << 4) + (0 & 0x0f);
#endif

#ifdef CONFIG_APP_VSCP_NODE_TYPE_GAMMA
  buf[VSCP_ESPNOW_POS_TYPE_VER] = (VSCP_DROPLET_GAMMA << 6) + (VSCP_ESPNOW_VERSION << 4) + (0 & 0x0f);
#endif

  // Set seq count
  buf[VSCP_ESPNOW_POS_SEQ] = s_vscp_espnow_seq++;

  // Set timestamp (in seconds)
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  buf[VSCP_ESPNOW_POS_TIME_STAMP]     = (tv_now.tv_sec >> 24) & 0xff;
  buf[VSCP_ESPNOW_POS_TIME_STAMP + 1] = (tv_now.tv_sec >> 16) & 0xff;
  buf[VSCP_ESPNOW_POS_TIME_STAMP + 2] = (tv_now.tv_sec >> 8) & 0xff;
  buf[VSCP_ESPNOW_POS_TIME_STAMP + 3] = tv_now.tv_sec & 0xff;

  // head
  buf[VSCP_ESPNOW_POS_HEAD]     = (pex->head >> 8) & 0xff;
  buf[VSCP_ESPNOW_POS_HEAD + 1] = pex->head & 0xff;

  // nickname
  buf[VSCP_ESPNOW_POS_NICKNAME]     = pex->GUID[14];
  buf[VSCP_ESPNOW_POS_NICKNAME + 1] = pex->GUID[15];

  // vscp-class
  buf[VSCP_ESPNOW_POS_VSCP_CLASS]     = (pex->vscp_class >> 8) & 0xff;
  buf[VSCP_ESPNOW_POS_VSCP_CLASS + 1] = pex->vscp_class & 0xff;

  // vscp-type
  buf[VSCP_ESPNOW_POS_VSCP_TYPE]     = (pex->vscp_type >> 8) & 0xff;
  buf[VSCP_ESPNOW_POS_VSCP_TYPE + 1] = pex->vscp_type & 0xff;

  buf[VSCP_ESPNOW_POS_SIZE] = pex->sizeData;

  // data
  if (pex->sizeData) {
    memcpy((buf + VSCP_ESPNOW_POS_DATA), pex->data, pex->sizeData);
  }

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_frameToEv
//

int
vscp_espnow_frameToEv(vscpEvent *pev, const uint8_t *buf, uint8_t len, uint32_t timestamp)
{
  // Need event
  if (NULL == pev) {
    ESP_LOGE(TAG, "Pointer to event is NULL");
    return VSCP_ERROR_INVALID_POINTER;
  }

  // Must be at least have min size
  if (len < VSCP_ESPNOW_MIN_FRAME) {
    ESP_LOGE(TAG, "esp-now data is too short, len:%d", len);
    return VSCP_ERROR_MTU;
  }

  // Must have valid paket type byte
  if ((buf[VSCP_ESPNOW_POS_ID] != 0x55) || (buf[VSCP_ESPNOW_POS_ID + 1] != 0xAA)) {
    ESP_LOGE(TAG, "esp-now data is an invalid frame");
    return VSCP_ERROR_INVALID_FRAME;
  }

  // To be sure
  memset(pev, 0, sizeof(vscpEvent));

  // Free any allocated event data
  if (NULL != pev->pdata) {
    VSCP_FREE(pev->pdata);
    pev->pdata = NULL;
  }

  // Set VSCP size
  pev->sizeData = MIN(buf[VSCP_ESPNOW_POS_SIZE], len - VSCP_ESPNOW_MIN_FRAME);
  if (pev->sizeData) {
    pev->pdata = VSCP_MALLOC(pev->sizeData);
    if (NULL == pev->pdata) {
      return VSCP_ERROR_MEMORY;
    }
  }

  // Copy in VSCP data
  memcpy(pev->pdata, buf + VSCP_ESPNOW_MIN_FRAME, pev->sizeData);

  // Set timestamp if not set
  if (!timestamp) {
    pev->timestamp = vscp_espnow_timestamp(); // esp_timer_get_time();
  }
  else {
    pev->timestamp = timestamp;
  }

  // Head
  pev->head = (buf[VSCP_ESPNOW_POS_HEAD] << 8) + buf[VSCP_ESPNOW_POS_HEAD + 1];

  // Nickname
  pev->GUID[14] = buf[VSCP_ESPNOW_POS_NICKNAME];
  pev->GUID[15] = buf[VSCP_ESPNOW_POS_NICKNAME + 1];

  // VSCP class
  pev->vscp_class = (buf[VSCP_ESPNOW_POS_VSCP_CLASS] << 8) + buf[VSCP_ESPNOW_POS_VSCP_CLASS + 1];

  // VSCP type
  pev->vscp_type = (buf[VSCP_ESPNOW_POS_VSCP_TYPE] << 8) + buf[VSCP_ESPNOW_POS_VSCP_TYPE + 1];

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_frameToEx
//

int
vscp_espnow_frameToEx(vscpEventEx *pex, const uint8_t *buf, uint8_t len, uint32_t timestamp)
{
  // Need event
  if (NULL == pex) {
    ESP_LOGE(TAG, "Pointer to event is NULL");
    return VSCP_ERROR_INVALID_POINTER;
  }

  // Must at least have min size
  if (len < VSCP_ESPNOW_MIN_FRAME) {
    ESP_LOGE(TAG, "esp-now data is too short, len:%d", len);
    return VSCP_ERROR_MTU;
  }

  // Must have valid first byte
  if ((buf[0] & 0xff) > VSCP_ENCRYPTION_AES256) {
    ESP_LOGE(TAG, "esp-now data is an invalid frame");
    return VSCP_ERROR_MTU;
  }

  memset(pex, 0, sizeof(vscpEventEx));

  // Set VSCP size
  pex->sizeData = buf[VSCP_ESPNOW_POS_SIZE];

  // Copy in VSCP data
  memcpy(pex->data, buf + VSCP_ESPNOW_MIN_FRAME, pex->sizeData);

  // Set timestamp if not set
  if (!timestamp) {
    pex->timestamp = vscp_espnow_timestamp(); // esp_timer_get_time();
  }
  else {
    pex->timestamp = timestamp;
  }

  // Head
  pex->head = (buf[VSCP_ESPNOW_POS_HEAD] << 8) + buf[VSCP_ESPNOW_POS_HEAD + 1];

  // Nickname
  pex->GUID[14] = buf[VSCP_ESPNOW_POS_NICKNAME];
  pex->GUID[15] = buf[VSCP_ESPNOW_POS_NICKNAME + 1];

  // VSCP class
  pex->vscp_class = (buf[VSCP_ESPNOW_POS_VSCP_CLASS] << 8) + buf[VSCP_ESPNOW_POS_VSCP_CLASS + 1];

  // VSCP type
  pex->vscp_type = (buf[VSCP_ESPNOW_POS_VSCP_TYPE] << 8) + buf[VSCP_ESPNOW_POS_VSCP_TYPE + 1];

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_sendEvent
//

int
vscp_espnow_sendEvent(const uint8_t *destAddr, const vscpEvent *pev, bool bSec, uint32_t wait_ms)
{
  int rv;
  uint8_t GUID[16];
  uint8_t *pbuf = NULL;
  size_t len    = vscp_espnow_getMinBufSizeEv(pev);

  // Need dest address
  if (NULL == destAddr) {
    ESP_LOGE(TAG, "Pointer to destAddr is NULL");
    return VSCP_ERROR_INVALID_POINTER;
  }

  // Need event
  if (NULL == pev) {
    ESP_LOGE(TAG, "Pointer to event is NULL");
    return VSCP_ERROR_INVALID_POINTER;
  }

  pbuf = VSCP_CALLOC(len);
  if (NULL == pbuf) {
    ESP_LOGE(TAG, "Failed to allocate memory for send buffer-");
    return VSCP_ERROR_MEMORY;
  }

  // If the GUID is zero we set it to the nodes GUID
  if (0 == memcmp(pev->GUID, s_VSCP_ESPNOW_GUID_NONE, 16)) {
    if (VSCP_ERROR_SUCCESS != (rv = vscp_espnow_get_node_guid(GUID))) {
      ESP_LOGE(TAG, "Failed to get GUID");
      return rv;
    }
  }

  if (VSCP_ERROR_SUCCESS != (rv = vscp_espnow_evToFrame(pbuf, len, pev))) {
    VSCP_FREE(pbuf);
    ESP_LOGE(TAG, "Failed to convert event to frame. rv=%d", rv);
    return rv;
  }

  // ESP_LOG_BUFFER_HEXDUMP(TAG, pbuf, len, ESP_LOG_DEBUG);

  ESP_LOGD(TAG, "Send mac: " MACSTR ", version: %d", MAC2STR(destAddr), VSCP_ESPNOW_VERSION);

  espnow_frame_head_t espnowhead     = ESPNOW_FRAME_CONFIG_DEFAULT();
  espnowhead.security                = bSec;
  espnowhead.channel                 = ESPNOW_CHANNEL_CURRENT;
  espnowhead.filter_adjacent_channel = true;

  espnowhead.broadcast          = true;
  espnowhead.ack                = true;
  espnowhead.magic              = esp_random();
  espnowhead.retransmit_count   = 1;
  espnowhead.forward_ttl        = 10;
  espnowhead.forward_rssi       = -65;
  espnowhead.filter_weak_signal = true;

  esp_err_t ret = espnow_send(ESPNOW_DATA_TYPE_DATA, destAddr, pbuf, len, &espnowhead, pdMS_TO_TICKS(wait_ms));
  if (ESP_OK != ret) {

    if (ESP_ERR_INVALID_ARG == ret) {
      ESP_LOGE(TAG, "Invalid parameter");
      rv = VSCP_ERROR_PARAMETER;
      goto ERROR;
    }
    else if (ESP_ERR_TIMEOUT == ret) {
      ESP_LOGE(TAG, "Timeout");
      rv = VSCP_ERROR_TIMEOUT;
      goto ERROR;
    }
    else if (ESP_ERR_WIFI_TIMEOUT == ret) {
      ESP_LOGE(TAG, "Wifi timeout");
      rv = VSCP_ERROR_TIMEOUT;
      goto ERROR;
    }
    else {
      ESP_LOGE(TAG, "Unknow error %X", ret);
      rv = VSCP_ERROR_ERROR;
      goto ERROR;
    }
  }

  rv = VSCP_ERROR_SUCCESS;

ERROR:
  VSCP_FREE(pbuf);
  return rv;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_sendEventEx
//

esp_err_t
vscp_espnow_sendEventEx(const uint8_t *destAddr, const vscpEventEx *pex, bool bSec, uint32_t wait_ms)
{
  esp_err_t rv;
  uint8_t *pbuf;
  size_t len = vscp_espnow_getMinBufSizeEx(pex);

  ESP_LOGI(TAG, "Send Event");

  // Need event
  if (NULL == pex) {
    ESP_LOGE(TAG, "Pointer to event ex is NULL");
    return ESP_ERR_INVALID_ARG;
  }

  // Need dest address
  if (NULL == destAddr) {
    ESP_LOGE(TAG, "Pointer to destAddr is NULL");
    return VSCP_ERROR_INVALID_POINTER;
  }

  pbuf = VSCP_MALLOC(len);
  if (NULL == pbuf) {
    return ESP_ERR_NO_MEM;
  }

  if (VSCP_ERROR_SUCCESS != (rv = vscp_espnow_exToFrame(pbuf, len, pex))) {
    VSCP_FREE(pbuf);
    ESP_LOGE(TAG, "Failed to convert event to frame. rv=%d", rv);
    return ESP_ERR_INVALID_ARG;
  }
// Set encryption
#ifdef CONFIG_APP_VSCP_NODE_TYPE_ALPHA
  pbuf[VSCP_ESPNOW_POS_TYPE_VER] = (VSCP_DROPLET_ALPHA << 6) + (VSCP_ESPNOW_VERSION << 4) + (0 & 0x0f);
#endif

#ifdef CONFIG_APP_VSCP_NODE_TYPE_BETA
  pbuf[VSCP_ESPNOW_POS_TYPE_VER] = (VSCP_DROPLET_BETA << 6) + (VSCP_ESPNOW_VERSION << 4) + (0 & 0x0f);
#endif

#ifdef CONFIG_APP_VSCP_NODE_TYPE_GAMMA
  pbuf[VSCP_ESPNOW_POS_TYPE_VER] = (VSCP_DROPLET_GAMMA << 6) + (VSCP_ESPNOW_VERSION << 4) + (0 & 0x0f);
#endif

  // ESP_LOG_BUFFER_HEXDUMP(TAG, pbuf, len, ESP_LOG_DEBUG);

  ESP_LOGD(TAG, "Send mac: " MACSTR ", version: %d", MAC2STR(destAddr), VSCP_ESPNOW_VERSION);

  espnow_frame_head_t espnowhead     = ESPNOW_FRAME_CONFIG_DEFAULT();
  espnowhead.security                = bSec;
  espnowhead.channel                 = ESPNOW_CHANNEL_CURRENT;
  espnowhead.filter_adjacent_channel = true;

  espnowhead.broadcast          = true;
  espnowhead.ack                = true;
  espnowhead.magic              = esp_random();
  espnowhead.retransmit_count   = 10;
  espnowhead.forward_ttl        = 10;
  espnowhead.forward_rssi       = -65;
  espnowhead.filter_weak_signal = true;

  esp_err_t ret = espnow_send(ESPNOW_DATA_TYPE_DATA, destAddr, pbuf, len, &espnowhead, pdMS_TO_TICKS(wait_ms));
  if (ESP_OK != ret) {

    if (ESP_ERR_INVALID_ARG == ret) {
      ESP_LOGE(TAG, "Invalid parameter");
      rv = VSCP_ERROR_PARAMETER;
      goto ERROR;
    }
    else if (ESP_ERR_TIMEOUT == ret) {
      ESP_LOGE(TAG, "Timeout");
      rv = VSCP_ERROR_TIMEOUT;
      goto ERROR;
    }
    else if (ESP_ERR_WIFI_TIMEOUT == ret) {
      ESP_LOGE(TAG, "Wifi timeout");
      rv = VSCP_ERROR_TIMEOUT;
      goto ERROR;
    }
    else {
      ESP_LOGE(TAG, "Unknow error %X", ret);
      rv = VSCP_ERROR_ERROR;
      goto ERROR;
    }
  }

  rv = VSCP_ERROR_SUCCESS;

ERROR:
  VSCP_FREE(pbuf);
  return rv;
}

// void
// vscp_espnow_data_cb(uint8_t *src_addr, void *data, size_t size, wifi_pkt_rx_ctrl_t *rx_ctrl)
// {
// }

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_set_vscp_user_handler_cb
//

void
vscp_espnow_set_vscp_user_handler_cb(vscp_event_handler_cb_t cb)
{
  s_vscp_event_handler_cb = cb;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_clear_vscp_handler_cb
//

void
vscp_espnow_clear_vscp_handler_cb(void)
{
  s_vscp_event_handler_cb = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_send_probe_event
//

int
vscp_espnow_send_probe_event(const uint8_t *dest_addr, uint8_t channel, TickType_t wait_ticks)
{
  int rv        = VSCP_ERROR_SUCCESS;
  esp_err_t ret = ESP_OK;

  vscpEvent *pev = vscp_fwhlp_newEvent();
  if (NULL == pev) {
    ESP_LOGE(TAG, "Failed to allocate memory for event");
    return VSCP_ERROR_MEMORY;
  }

  // GUID is data
  pev->pdata = VSCP_CALLOC(16);
  if (NULL == pev->pdata) {
    VSCP_FREE(pev);
    ESP_LOGE(TAG, "Failed to allocate memory for event data");
    return VSCP_ERROR_MEMORY;
  }

  pev->head       = 0;
  pev->timestamp  = vscp_espnow_timestamp(); // esp_timer_get_time();
  pev->vscp_class = VSCP_CLASS1_PROTOCOL;
  pev->vscp_type  = VSCP_TYPE_PROTOCOL_NEW_NODE_ONLINE;

  // Set uninitialized
  pev->sizeData = VSCP_SIZE_GUID;
  memcpy(pev->pdata, s_VSCP_ESPNOW_GUID_UNINIT, VSCP_SIZE_GUID);

  size_t len   = vscp_espnow_getMinBufSizeEv(pev);
  uint8_t *buf = VSCP_MALLOC(len);
  if (NULL == buf) {
    ESP_LOGE(TAG, "Failed to allocate memory for event buffer");
    vscp_fwhlp_deleteEvent(&pev);
    return VSCP_ERROR_MEMORY;
  }

  if (VSCP_ERROR_SUCCESS != (rv = vscp_espnow_evToFrame(buf, len, pev))) {
    ESP_LOGE(TAG, "Failed to convert event to frame");
    return rv;
  }

  // Set timestamp (in seconds)
  // struct timeval tv_now;
  // gettimeofday(&tv_now, NULL);
  // buf[VSCP_ESPNOW_POS_TIME_STAMP]     = (tv_now.tv_sec >> 24) & 0xff;
  // buf[VSCP_ESPNOW_POS_TIME_STAMP + 1] = (tv_now.tv_sec >> 16) & 0xff;
  // buf[VSCP_ESPNOW_POS_TIME_STAMP + 2] = (tv_now.tv_sec >> 8) & 0xff;
  // buf[VSCP_ESPNOW_POS_TIME_STAMP + 3] = tv_now.tv_sec & 0xff;

  // We set timestamp to "high value". This let the prove event
  // be accepted as an event with an initiated time by receiving nodes
  buf[VSCP_ESPNOW_POS_TIME_STAMP]     = 0xff;
  buf[VSCP_ESPNOW_POS_TIME_STAMP + 1] = 0xff;
  buf[VSCP_ESPNOW_POS_TIME_STAMP + 2] = 0xff;
  buf[VSCP_ESPNOW_POS_TIME_STAMP + 3] = 0xff;

  espnow_frame_head_t espnowhead = {
    .security                = false,
    .broadcast               = true,
    .retransmit_count        = 10,
    .magic                   = esp_random(),
    .ack                     = true,
    .filter_adjacent_channel = true,
    .channel                 = channel,
    .forward_ttl             = 10,
    .forward_rssi            = -65,
    .filter_weak_signal      = false,
  };

  for (int count = 0; count < 3; ++count) {
    ret = espnow_send(ESPNOW_DATA_TYPE_DATA, dest_addr, buf, len, &espnowhead, pdMS_TO_TICKS(wait_ticks));
    if (ESP_OK != ret) {

      if (ESP_ERR_INVALID_ARG == ret) {
        ESP_LOGE(TAG, "Invalid parameter sending probe event");
        rv = VSCP_ERROR_PARAMETER;
        goto EXIT;
      }
      else if (ESP_ERR_TIMEOUT == ret) {
        ESP_LOGE(TAG, "Timeout sending probe event");
        rv = VSCP_ERROR_TIMEOUT;
        goto EXIT;
      }
      else if (ESP_ERR_WIFI_TIMEOUT == ret) {
        ESP_LOGE(TAG, "Wifi timeout sending probe event");
        rv = VSCP_ERROR_TIMEOUT;
        goto EXIT;
      }
      else {
        ESP_LOGE(TAG, "Unknow error %X  sending probe event", ret);
        rv = VSCP_ERROR_ERROR;
        goto EXIT;
      }
    }
  }

EXIT:
  vscp_fwhlp_deleteEvent(&pev);
  VSCP_FREE(buf);
  return rv;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_probe
//

int
vscp_espnow_probe(void)
{
  int rv = VSCP_ERROR_SUCCESS;
  int ret;
  bool bProbeAck = false;
  // uint8_t primary           = 0;
  // wifi_second_chan_t second = 0;

  ESP_LOGI(TAG, "Probe starting");

  s_stateVscpEspNow = VSCP_ESPNOW_STATE_PROBE;

  // Clear the probe response bit
  xEventGroupClearBits(s_vscp_espnow_event_group, VSCP_ESPNOW_WAIT_PROBE_RESPONSE_BIT);

  for (int i = 0; i < RESEND_SCAN_COUNT_MAX; i++) {

    ret = esp_wifi_set_channel(scan_channel_sequence[i % sizeof(scan_channel_sequence)], WIFI_SECOND_CHAN_NONE);
    if (ESP_OK != rv) {
      ESP_LOGE(TAG, "[%s, %d]: Failed to set channel !(%x)", __func__, __LINE__, ret);
    }

    rv = vscp_espnow_send_probe_event(ESPNOW_ADDR_BROADCAST,
                                      scan_channel_sequence[i % sizeof(scan_channel_sequence)],
                                      100);
    if (VSCP_ERROR_SUCCESS != rv) {
      ESP_LOGE(TAG, "[%s, %d]: Probe failed !(%x)", __func__, __LINE__, rv);
      return rv;
    }

    // Wait for response
    EventBits_t bits = xEventGroupWaitBits(s_vscp_espnow_event_group,
                                           VSCP_ESPNOW_WAIT_PROBE_RESPONSE_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(100));
    if (bits & VSCP_ESPNOW_WAIT_PROBE_RESPONSE_BIT) {
      printf("Probe ack\n");
      bProbeAck = true;
      break;
    }
    // taskYIELD();
  } // for

  // EventBits_t bits = xEventGroupWaitBits(s_vscp_espnow_event_group,
  //                                        VSCP_ESPNOW_WAIT_PROBE_RESPONSE_BIT,
  //                                        pdFALSE,
  //                                        pdFALSE,
  //                                        pdMS_TO_TICKS(1000));
  // if (!(bits & VSCP_ESPNOW_WAIT_PROBE_RESPONSE_BIT)) {
  //   ESP_LOGW(TAG, "Timeout waiting for response");
  //   rv = VSCP_ERROR_TIMEOUT;
  // }

  ESP_LOGI(TAG, "Probe ending %d", rv);

  if (!bProbeAck) {
    ESP_LOGW(TAG, "Timeout waiting for response");
    rv = VSCP_ERROR_TIMEOUT;
  }

#if (s_my_node_type == VSCP_DROPLET_ALPHA)
  s_stateVscpEspNow = VSCP_ESPNOW_STATE_IDLE;
#else
  s_stateVscpEspNow = VSCP_ESPNOW_STATE_VIRGIN;
#endif

  return rv;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_data_cb
//
// Network cluster data is received here. If the data is valid (as far as we can tell)
// it is sent to the application event receive callback,
//

static void
vscp_espnow_data_cb(uint8_t *src_addr, uint8_t *data, size_t size, wifi_pkt_rx_ctrl_t *rx_ctrl)
{
  long long node_time; // Event node time (not VSCP timestamp)
  long diff;           // Time diff between node and our time from frame timestamp

  if ((src_addr == NULL) || (data == NULL) || (rx_ctrl == NULL) || (size <= 0)) {
    ESP_LOGE(TAG, "Receive cb arg error");
    return;
  }

  ESP_LOGI(TAG,
           "<<< 1.) Receive event from: " MACSTR " , RSSI %d Channel %d, espnow size %zd",
           MAC2STR((src_addr)),
           rx_ctrl->rssi,
           rx_ctrl->channel,
           size);

  // Check that frame length is within limits
  if ((size < VSCP_ESPNOW_MIN_FRAME) || (size > VSCP_ESPNOW_MAX_FRAME) ||
      ((data[VSCP_ESPNOW_POS_TYPE_VER] & 0x0f) > VSCP_ENCRYPTION_AES256)) {

    ESP_LOGE(TAG,
             "[%s, %d]: Frame length/type is invalid len=%d (%d) type=%d",
             __func__,
             __LINE__,
             size,
             VSCP_ESPNOW_MAX_FRAME,
             data[VSCP_ESPNOW_POS_TYPE_VER]);

    ESP_LOG_BUFFER_HEXDUMP(TAG, data, size, ESP_LOG_DEBUG);

    s_vscpEspNowStats.nRecvFrameFault++; // Increase receive frame faults
    return;
  }

  // Check frame id and vscp espnow protocol version
  if ((data[VSCP_ESPNOW_POS_ID] != 0x55) || (data[VSCP_ESPNOW_POS_ID + 1] != 0xAA)) {
    ESP_LOGW(TAG,
             "Frame is invalid. id=%X,  protocol version=%d",
             (data[VSCP_ESPNOW_POS_ID] << 8) + data[VSCP_ESPNOW_POS_ID + 1],
             data[VSCP_ESPNOW_POS_ID + 1] & 0xf);
    return;
  }

  uint8_t node_type  = (data[VSCP_ESPNOW_POS_TYPE_VER] >> 6) & 0x03;
  uint8_t proto_ver  = (data[VSCP_ESPNOW_POS_TYPE_VER] >> 4) & 0x03;
  uint8_t encryption = data[VSCP_ESPNOW_POS_TYPE_VER] & 0x07;

  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);

  node_time =
    (long long) (((uint32_t) data[VSCP_ESPNOW_POS_TIME_STAMP] << 24) +
                 ((uint32_t) data[VSCP_ESPNOW_POS_TIME_STAMP + 1] << 16) +
                 ((uint32_t) data[VSCP_ESPNOW_POS_TIME_STAMP + 2] << 8) + data[VSCP_ESPNOW_POS_TIME_STAMP + 3]);

  /*
    We skip frames with node_time < VSCP_ESPNOW_REF_TIME here. For a node that
    scan for a channel, which is not initiated time wise will have a "high value"
    here that makes the fram being accepted.
  */
  if (node_time < VSCP_ESPNOW_REF_TIME) {
    ESP_LOGW(TAG, "Node time stamp is lower then reference time");
    return;
  }

  // diff = abs(node_time - (int) tv_now.tv_sec);
  diff = node_time - tv_now.tv_sec;

  ESP_LOGI(TAG, "node_time  node_time: %lld, tv_now: %lld ---------> diff: %ld\n", node_time, tv_now.tv_sec, diff);

  vscpEvent *pev = vscp_fwhlp_newEvent();
  if (NULL == pev) {
    ESP_LOGE(TAG, "[%s, %d]: Could not ", __func__, __LINE__);
    return;
  }

  if (VSCP_ERROR_SUCCESS != vscp_espnow_frameToEv(pev, data, size, rx_ctrl->timestamp)) {
    vscp_fwhlp_deleteEvent(&pev);
    return;
  }

  // ----------------------------------------------------------------------------
  //                             Channel Probing
  // ----------------------------------------------------------------------------

  if (s_my_node_type == VSCP_DROPLET_ALPHA) {
    /*
      1.) When Beta and Gamma nodes are in virgin state they don't know the secret key or the
          channel to communicate on. Therefore a pairing has to talke place. This is done by pressing
          a button on a alpha-node anda beta/gamma node that should be paired. Now the beta/gamma node
          send CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_NEW_NODE_ONLINE on all chanels until it get a response
          and it then set the channel, save the address of the alpha node and initiate the key exhange
          process. When this is done they are part of the segment.
      2.) Gamma nodes also send  CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_NEW_NODE_ONLINE when they wake up.
          This is just to get a response so they can synd the timestamp for further communication. Note
          that the gamma node needs
    */
    if (/*(VSCP_ESPNOW_STATE_PROBE == s_stateVscpEspNow) &&*/ (node_type != VSCP_DROPLET_ALPHA) &&
        (VSCP_CLASS1_PROTOCOL == pev->vscp_class) && (VSCP_TYPE_PROTOCOL_NEW_NODE_ONLINE == pev->vscp_type) &&
        (16 == pev->sizeData)) {

      ESP_LOGI(TAG, "Probe Alpha: New node on-line");

      // Send probe response if probe node is all zero or same as probing
      if (!memcmp(VSCP_ESPNOW_ADDR_PROBE_NODE, VSCP_ESPNOW_ADDR_NONE, 6)) {
        ESP_LOGI(TAG, "Sending probe event on channel %d", rx_ctrl->channel);
        int rv = vscp_espnow_send_probe_event(ESPNOW_ADDR_BROADCAST, rx_ctrl->channel, 1000);
        xEventGroupSetBits(s_vscp_espnow_event_group, VSCP_ESPNOW_WAIT_PROBE_RESPONSE_BIT);
        s_stateVscpEspNow   = VSCP_ESPNOW_STATE_IDLE;
        g_vscp_espnow_probe = true;
      }
      else if (!memcmp(VSCP_ESPNOW_ADDR_PROBE_NODE, src_addr, 6)) {
        ESP_LOGI(TAG, "Sending addressed probe event on channel %d", rx_ctrl->channel);
        int rv = vscp_espnow_send_probe_event(src_addr, rx_ctrl->channel, 1000);
        xEventGroupSetBits(s_vscp_espnow_event_group, VSCP_ESPNOW_WAIT_PROBE_RESPONSE_BIT);
        s_stateVscpEspNow   = VSCP_ESPNOW_STATE_IDLE;
        g_vscp_espnow_probe = true;
      }
      else {
        ESP_LOGE(TAG, "Strange probe address: " MACSTR, MAC2STR(VSCP_ESPNOW_ADDR_PROBE_NODE));
      }
    }
    /*
      1. Beta/Gamma nodes set channel and save src address of alpha node when they get the probe respons.
      2. Gamma nodes also send a probe when they wakeup. To be able to communicate further they
         store the time also at this state.
    */
  }
  else if ((s_my_node_type == VSCP_DROPLET_BETA) || (s_my_node_type == VSCP_DROPLET_GAMMA)) {

    int ret;
    printf("------------------------------------------------------------------\n");
    if ((node_type == VSCP_DROPLET_ALPHA) && (VSCP_ESPNOW_STATE_PROBE == s_stateVscpEspNow) &&
        (VSCP_CLASS1_PROTOCOL == pev->vscp_class) && (VSCP_TYPE_PROTOCOL_NEW_NODE_ONLINE == pev->vscp_type) &&
        (16 == pev->sizeData)) {

      ESP_LOGI(TAG, "Probe Beta: New node on-line");

      xEventGroupSetBits(s_vscp_espnow_event_group, VSCP_ESPNOW_WAIT_PROBE_RESPONSE_BIT);
      if (ESP_OK != (ret = esp_wifi_set_channel(rx_ctrl->channel, WIFI_SECOND_CHAN_NONE))) {
        ESP_LOGE(TAG, "[%s, %d]: Failed to set espnow channel %X", __func__, __LINE__, ret);
      }

      // Save channel to persistent storage
      if (ESP_OK != (ret = nvs_set_u8(s_nvsHandle, "channel", rx_ctrl->channel))) {
        ESP_LOGE(TAG, "[%s, %d]: Failed to update espnow nvs channel %X", __func__, __LINE__, ret);
      }

      // Save sender MAC address of alpha node to persistent storage
      size_t length = 6;
      int rv        = nvs_set_blob(s_nvsHandle, "keyorg", src_addr, length);
      if (rv != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write originating max to nvs. rv=%d", rv);
      }

      // Sync time with alpha node
      struct timeval tm;

      tm.tv_sec  = (long long int) (((uint32_t) pev->pdata[1] << 24) + ((uint32_t) pev->pdata[2] << 16) +
                                   ((uint32_t) pev->pdata[3] << 8) + pev->pdata[4]);
      tm.tv_usec = 0;

      ESP_LOGI(TAG, "Gamma: Setting/updating system time.");
      if (-1 == settimeofday(&tm, NULL)) {
        ESP_LOGE(TAG, "Gamma: Failed to set time.");
      }

      diff = 0;

      // #if (s_my_node_type == VSCP_DROPLET_GAMMA)
      //     struct timeval tm;

      //     tm.tv_sec  = (long long int) (((uint32_t) pev->pdata[1] << 24) + ((uint32_t) pev->pdata[2] << 16) +
      //                                  ((uint32_t) pev->pdata[3] << 8) + pev->pdata[4]);
      //     tm.tv_usec = 0;

      //     ESP_LOGI(TAG, "Gamma: Setting/updating system time.");
      //     if (-1 == settimeofday(&tm, NULL)) {
      //       ESP_LOGE(TAG, "Gamma: Failed to set time.");
      //     }

      //     diff = 0;
      //     //diff = abs(node_time - tv_now.tv_sec);
      //     //node_time - tv_now.tv_sec;
      // #endif
    }

    // Check if we got a heartbeat from an alpha node. If we do we set/update the time
    // for this node
    if ((node_type == VSCP_DROPLET_ALPHA) && (VSCP_CLASS1_PROTOCOL == pev->vscp_class) &&
        (VSCP_TYPE_PROTOCOL_SEGCTRL_HEARTBEAT == pev->vscp_type) && (pev->sizeData >= 5)) {

      struct timeval tm;

      tm.tv_sec  = (long long int) (((uint32_t) pev->pdata[1] << 24) + ((uint32_t) pev->pdata[2] << 16) +
                                   ((uint32_t) pev->pdata[3] << 8) + pev->pdata[4]);
      tm.tv_usec = 0;

      ESP_LOGI(TAG, "Setting/updating system time.");
      if (-1 == settimeofday(&tm, NULL)) {
        ESP_LOGE(TAG, "Failed to set time.");
      }

      diff = 0;
      // diff = abs(node_time - tv_now.tv_sec);
      // node_time - tv_now.tv_sec;
    }
  }

  // ----------------------------------------------------------------
  //                   Replay protection point
  // ----------------------------------------------------------------

  /*
    Time can never be back in time. We don't accept events in that case,

    If our node have a non initiated time and the originating node is initiated the diff will be a very
    high positive value here and the frame should not be accepted because replay frames could sneak in.
    This is typically also the case for a time sync.

    If the originating node is not initiated but we are the diff will be a high negative value
    and the frame will be dropped here until time is synced.

    If both originator and we are unsynced we will have a either a negative or a positive number
    with a low value.

    If both originating node and we are initiated we will have a low value (0,1) here.
  */

  if (diff > 1) {
    ESP_LOGE(TAG, "Event have timestamp out of range. diff = %lu", diff);
    goto EXIT;
  }

  ESP_LOGI(TAG,
           "<<< 2.) esp-now data received: len=%zd ch=%d src=" MACSTR
           " rssi=%d class=%d, type=%d size-data=%d timestamp=%lX",
           size,
           rx_ctrl->channel,
           MAC2STR(src_addr),
           rx_ctrl->rssi,
           pev->vscp_class,
           pev->vscp_type,
           pev->sizeData,
           pev->timestamp);

  // Handle incomming events
  vscp_espnow_event_process(pev);

EXIT:
  vscp_fwhlp_deleteEvent(&pev);
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_heartbeat_task
//
// Sent periodically as a broadcast to all zones/subzones
//

void
vscp_espnow_heartbeat_task(void *pvParameter)
{
  // time_t now;

  // vscp_espnow_heart_beat_t *pconfig = (vscp_espnow_heart_beat_t *) pvParameter;
  // if (NULL == pconfig) {
  //   ESP_LOGE(TAG, "Invalid (NULL) parameter given");
  //   vTaskDelete(NULL);
  // }

  vscpEvent *pev = vscp_fwhlp_newEvent();
  if (NULL == pev) {
    ESP_LOGE(TAG, "Unable to allocate heartbeat event");
    goto ERROR;
  }

#if (s_my_node_type == VSCP_DROPLET_ALPHA)
  pev->pdata = VSCP_CALLOC(5);
#elif (s_my_node_type == VSCP_DROPLET_BETA)
  pev->pdata        = VSCP_CALLOC(3);
#else
  pev->pdata = VSCP_CALLOC(3);
#endif

  if (NULL == pev->pdata) {
    ESP_LOGE(TAG, "Unable to allocate heartbeat event data");
    goto ERROR;
  }

  while (true) {

    if (1 /*VSCP_ESPNOW_STATE_IDLE == s_stateVscpEspNow*/) {

      esp_err_t ret;
      uint8_t ch                = 0;
      wifi_second_chan_t second = 0;

      if (ESP_OK != (ret = esp_wifi_get_channel(&ch, &second))) {
        ESP_LOGE(TAG, "Failed to get wifi channel, rv = %X", ret);
      }

      ESP_LOGI(TAG, "Sending heartbeat ch=%d (%d).", ch, second);

      // time(&now); // Get current time
      struct timeval tv_now;
      gettimeofday(&tv_now, NULL);

#if (s_my_node_type == VSCP_DROPLET_ALPHA)
      // Alpha node send protocol heartbeat
      pev->vscp_class = VSCP_CLASS1_PROTOCOL;
      pev->vscp_type  = VSCP_TYPE_PROTOCOL_SEGCTRL_HEARTBEAT;
      pev->sizeData   = 5;
      pev->pdata[0]   = 0x00; // CRC for GUID
      pev->pdata[1]   = (tv_now.tv_sec >> 24) & 0xff;
      pev->pdata[2]   = (tv_now.tv_sec >> 16) & 0xff;
      pev->pdata[3]   = (tv_now.tv_sec >> 8) & 0xff;
      pev->pdata[4]   = tv_now.tv_sec & 0xff;
      // printf("now=%lld  %lld\n",
      //        tv_now.tv_sec,
      //        (long long int) (((uint32_t) pev->pdata[1] << 24) + ((uint32_t) pev->pdata[2] << 16) +
      //                         ((uint32_t) pev->pdata[3] << 8) + pev->pdata[4]));
#elif (s_my_node_type == VSCP_DROPLET_BETA)
      // Beta nodes send information heartbeat
      pev->vscp_class = VSCP_CLASS1_INFORMATION;
      pev->vscp_type  = VSCP_TYPE_INFORMATION_NODE_HEARTBEAT;
      pev->sizeData   = 3;
      pev->pdata[0]   = 0xff; // index
      pev->pdata[1]   = 0xff; // zone
      pev->pdata[2]   = 0xff; // subzone
#else // Gamma nodes
      // Beta nodes send information heartbeat
      pev->vscp_class = VSCP_CLASS1_INFORMATION;
      pev->vscp_type  = VSCP_TYPE_INFORMATION_NODE_HEARTBEAT;
      pev->sizeData   = 3;
      pev->pdata[0]   = 0xff; // index
      pev->pdata[1]   = 0xff; // zone
      pev->pdata[2]   = 0xff; // subzone
#endif

      if (espnow_timesync_check()) {
        pev->timestamp = vscp_espnow_timestamp(); // esp_timer_get_time();
        vscp_espnow_sendEvent(ESPNOW_ADDR_BROADCAST, pev, false, pdMS_TO_TICKS(1000));
      }
    }

    vTaskDelay(pdMS_TO_TICKS(VSCP_ESPNOW_HEART_BEAT_INTERVAL));
  }

ERROR:
  if (NULL != pev) {
    vscp_fwhlp_deleteEvent(&pev);
  }
  ESP_LOGW(TAG, "Heartbeat task exit");
  vTaskDelete(NULL);
}

///////////////////////////////////////////////////////////////////////////////
// readPersistentConfigs
//

static int
readPersistentConfigs(void)
{
  esp_err_t rv;

  // Start Delay (seconds)
  rv = nvs_get_u16(s_nvsHandle, "nickname", &s_vscp_persistent.nickname);
  switch (rv) {

    case ESP_OK:
      ESP_LOGI(TAG, "Nickname = %d", s_vscp_persistent.nickname);
      break;

    case ESP_ERR_NVS_NOT_FOUND:
      rv = nvs_set_u16(s_nvsHandle, "nickname", 0xff);
      if (rv != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update nickname");
      }
      break;

    default:
      ESP_LOGE(TAG, "Error (%s) reading!", esp_err_to_name(rv));
      break;
  }

  // User id
  rv = nvs_get_blob(s_nvsHandle, "usrid", &s_vscp_persistent.userid, 5);
  if (ESP_OK != rv) {
    rv = nvs_set_blob(s_nvsHandle, "usrid", s_vscp_persistent.userid, 5);
    if (rv != ESP_OK) {
      ESP_LOGE(TAG, "Failed to update usrid");
    }
  }

  rv = nvs_commit(s_nvsHandle);
  if (rv != ESP_OK) {
    ESP_LOGI(TAG, "Failed to commit updates to nvs\n");
  }

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_espnow_init
//

esp_err_t
vscp_espnow_init(const vscp_espnow_config_t *pconfig)
{
  esp_err_t ret;
  s_stateVscpEspNow = VSCP_ESPNOW_STATE_IDLE;

  // ----------------------------------------------------------------------------
  //                        NVS - Persistent storage
  // ----------------------------------------------------------------------------

  // Init persistent storage
  ret = nvs_open("config", NVS_READWRITE, &s_nvsHandle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(ret));
  }
  else {
    // Read (or set to defaults) persistent values
    readPersistentConfigs();
  }

  // Create signaling bits
  s_vscp_espnow_event_group = xEventGroupCreate();

  ret = espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, vscp_espnow_data_cb);
  if (ESP_OK != ret) {
    ESP_LOGE(TAG, "Failed to set VSCP event callback");
  }

  esp_wifi_get_mac(ESP_IF_WIFI_STA, VSCP_ESPNOW_ADDR_SELF);
  ESP_LOGD(TAG, "mac: " MACSTR ", version: %d", MAC2STR(VSCP_ESPNOW_ADDR_SELF), VSCP_ESPNOW_VERSION);

  // Start heartbeat task vscp_heartbeat_task
  xTaskCreate(&vscp_espnow_heartbeat_task, "vscp_hb", 1024 * 3, NULL, tskIDLE_PRIORITY + 1, NULL);

  return ESP_OK;
}
