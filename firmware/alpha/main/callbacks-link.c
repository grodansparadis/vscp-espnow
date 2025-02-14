// FILE: callbacks-link.c

// This file holds callbacks for the VSCP tcp/ip link protocol

/* ******************************************************************************
 * 	VSCP (Very Simple Control Protocol)
 * 	https://www.vscp.org
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2000-2025 Ake Hedman, Grodans Paradis AB <info@grodansparadis.com>
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
 *	This file is part of VSCP - Very Simple Control Protocol
 *	https://www.vscp.org
 *
 * ******************************************************************************
 */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lwip/err.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>

#include <esp_timer.h>

#include "vscp-compiler.h"
#include "vscp-projdefs.h"

#include "vscp-espnow.h"
#include "tcpsrv.h"
#include "alpha.h"

#define TAG "linkcb"

extern node_persistent_config_t g_persistent;
extern QueueHandle_t g_queueDroplet; // Received events from VSCP link clients
// extern SemaphoreHandle_t g_droplet_send_lock; // From droplet
//  extern vscpctx_t g_ctx[CONFIG_APP_MAX_TCP_CONNECTIONS];

// Constants from droplet
extern const uint8_t DROPLET_ADDR_NONE[6];
extern const uint8_t DROPLET_ADDR_BROADCAST[6];

// ****************************************************************************
//                       VSCP Link protocol callbacks
// ****************************************************************************

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_write_client
//

#define TCPSRV_WELCOME_MSG                                                                                             \
  "Welcome to the %s node\r\n"                                                                                         \
  "Copyright (C) 2000-2025 Åke Hedman, Grodans Paradis AB\r\n"                                                        \
  "https://www.grodansparadis.com\r\n"                                                                                 \
  "+OK\r\n"

int
vscp_link_callback_welcome(const void *pdata)
{
  char *pbuf = ESP_MALLOC(strlen(TCPSRV_WELCOME_MSG) + strlen(g_persistent.nodeName) + 1);
  if (NULL == pbuf) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  if (NULL == pdata) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx = (vscpctx_t *) pdata;

  sprintf(pbuf, TCPSRV_WELCOME_MSG, g_persistent.nodeName);
  send(pctx->sock, pbuf, strlen(pbuf), 0);

  ESP_FREE(pbuf);
  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_write_client
//

int
vscp_link_callback_write_client(const void *pdata, const char *msg)
{
  if ((NULL == pdata) && (NULL == msg)) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx = (vscpctx_t *) pdata;
  send(pctx->sock, (uint8_t *) msg, strlen(msg), 0);
  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_quit
//

int
vscp_link_callback_quit(const void *pdata)
{
  // Check pointer
  if (NULL == pdata) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx = (vscpctx_t *) pdata;

  // Confirm quit
  send(pctx->sock, VSCP_LINK_MSG_GOODBY, strlen(VSCP_LINK_MSG_GOODBY), 0);

  // Disconnect from client
  close(pctx->sock);

  // Set context defaults (and socket to zero to terminate working thread)
  tcpsrv_setContextDefaults(pctx);

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_help
//

int
vscp_link_callback_help(const void *pdata, const char *arg)
{
  if ((NULL == pdata) && (NULL == arg)) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx = (vscpctx_t *) pdata;
  send(pctx->sock, VSCP_LINK_MSG_OK, strlen(VSCP_LINK_MSG_OK), 0);
  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_get_interface_count
//

uint16_t
vscp_link_callback_get_interface_count(const void *pdata)
{
  /* Return number of interfaces we support */
  return 1;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_get_interface
//

int
vscp_link_callback_get_interface(const void *pdata, uint16_t index, struct vscp_interface_info *pif)
{
  if ((NULL == pdata) && (NULL == pif)) {
    return VSCP_ERROR_UNKNOWN_ITEM;
  }

  if (index != 0) {
    return VSCP_ERROR_UNKNOWN_ITEM;
  }

  // interface-id-n, type, interface-GUID-n, interface_real-name-n
  // interface types in vscp.h

  
  pif->idx  = index;
  pif->type = VSCP_INTERFACE_TYPE_INTERNAL;
  vscp_espnow_get_node_guid(pif->guid);
  vscp_espnow_get_node_guid(pif->guid);
  strncpy(pif->description, "Interface for the device itself", sizeof(pif->description));

  // We have no interfaces
  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_check_user
//

int
vscp_link_callback_check_user(const void *pdata, const char *arg)
{
  if ((NULL == pdata) && (NULL == arg)) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  // trim
  const unsigned char *p = (const unsigned char *) arg;
  while (*p && isspace(*p)) {
    p++;
  }

  vscpctx_t *pctx = (vscpctx_t *) pdata;
  strncpy(pctx->user, (char *) p, VSCP_LINK_MAX_USER_NAME_LENGTH);
  send(pctx->sock, VSCP_LINK_MSG_USENAME_OK, strlen(VSCP_LINK_MSG_USENAME_OK), 0);
  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_check_password
//

int
vscp_link_callback_check_password(const void *pdata, const char *arg)
{
  if ((NULL == pdata) && (NULL == arg)) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx = (vscpctx_t *) pdata;

  // Must have a username before a password
  if (*(pctx->user) == '\0') {
    send(pctx->sock, VSCP_LINK_MSG_NEED_USERNAME, strlen(VSCP_LINK_MSG_NEED_USERNAME), 0);
    return VSCP_ERROR_SUCCESS;
  }

  const unsigned char *p = (const unsigned char *) arg;
  while (*p && isspace(*p)) {
    p++;
  }

  // if (!pctx->bValidated) {

  // }
  if (0 == strcmp(pctx->user, "admin") && 0 == strcmp((const char *) p, "secret")) {
    pctx->bValidated = true;
    pctx->privLevel  = 15;

    // Send out early to identify ourself
    // no need to send earlier as bValidate must be true
    // for events to get delivered

    // vscp2_send_heartbeat();
    // vscp2_send_caps();
  }
  else {
    pctx->user[0]    = '\0';
    pctx->bValidated = false;
    pctx->privLevel  = 0;
    send(pctx->sock, VSCP_LINK_MSG_PASSWORD_ERROR, strlen(VSCP_LINK_MSG_PASSWORD_ERROR), 0);
    return VSCP_ERROR_SUCCESS;
  }

  send(pctx->sock, VSCP_LINK_MSG_PASSWORD_OK, strlen(VSCP_LINK_MSG_PASSWORD_OK), 0);
  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_challenge
//

int
vscp_link_callback_challenge(const void *pdata, const char *arg)
{
  uint8_t buf[80];
  uint8_t random_data[32];
  if ((NULL == pdata) && (NULL == arg)) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx = (vscpctx_t *) pdata;

  const unsigned char *p = (const unsigned char *) arg;
  while (*p && isspace(*p)) {
    p++;
  }

  strcpy((char *) buf, "+OK - ");
  p = (const unsigned char *) buf + strlen((const char *) buf);

  for (int i = 0; i < 32; i++) {
    random_data[i] = rand() >> 16;
    if (i < sizeof(p)) {
      random_data[i] += (uint8_t) p[i];
    }
    vscp_fwhlp_dec2hex(random_data[i], (char *) p, 2);
    p++;
  }

  strcat((char *) buf, "\r\n");
  send(pctx->sock, buf, strlen((const char *) buf), 0);
  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_check_authenticated
//

int
vscp_link_callback_check_authenticated(const void *pdata)
{
  if (NULL == pdata) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx = (vscpctx_t *) pdata;

  if (pctx->bValidated) {
    return VSCP_ERROR_SUCCESS;
  }

  return VSCP_ERROR_INVALID_PERMISSION;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_check_privilege
//

int
vscp_link_callback_check_privilege(const void *pdata, uint8_t priv)
{
  if (NULL == pdata) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx = (vscpctx_t *) pdata;

  if (pctx->privLevel >= priv) {
    return VSCP_ERROR_SUCCESS;
  }

  return VSCP_ERROR_INVALID_PERMISSION;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_challenge
//

int
vscp_link_callback_test(const void *pdata, const char *arg)
{
  if ((NULL == pdata) && (NULL == arg)) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx = (vscpctx_t *) pdata;

  send(pctx->sock, VSCP_LINK_MSG_OK, strlen(VSCP_LINK_MSG_OK), 0);
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_send
//

int
vscp_link_callback_send(const void *pdata, vscpEvent *pev)
{
  if ((NULL == pdata) && (NULL == pev)) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx = (vscpctx_t *) pdata;

  // Filter
  if (!vscp_fwhlp_doLevel2Filter(pev, &pctx->filter)) {
    return VSCP_ERROR_SUCCESS; // Filter out == OK
  }

  // Update send statistics
  pctx->statistics.cntTransmitFrames++;
  pctx->statistics.cntTransmitData += pev->sizeData;

  // if obid is not set set to channel id
  if (!pev->obid) {
    pev->obid = pctx->id;
  }

  // if (ESP_OK != (ret = droplet_sendEvent(DROPLET_ADDR_BROADCAST, pev, NULL, 100))) {
  //   ESP_LOGE(TAG, "Failed to send event. rv = %d", ret);
  //   return VSCP_ERROR_ERROR;
  // }

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_chkData
//

int
vscp_link_callback_chkData(const void *pdata, uint16_t *pcount)
{
  // Check pointer
  if (NULL == pdata) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx = (vscpctx_t *) pdata;
  *pcount         = uxQueueMessagesWaiting(pctx->queueClient);

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_retr
//

int
vscp_link_callback_retr(const void *pdata, vscpEvent **pev)
{
  if ((NULL == pdata) && (NULL == pev)) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx = (vscpctx_t *) pdata;

  if (pdTRUE == xSemaphoreTake(pctx->mutexQueue, 10 / portTICK_PERIOD_MS)) {
    if (pdTRUE != xQueueReceive(pctx->queueClient, pev, 0)) {
      xSemaphoreGive(pctx->mutexQueue);
      return VSCP_ERROR_RCV_EMPTY; // Yes receive
    }
    xSemaphoreGive(pctx->mutexQueue);
  }

  // Update receive statistics
  pctx->statistics.cntReceiveFrames++;
  pctx->statistics.cntReceiveData += (*pev)->sizeData;

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_enable_rcvloop
//

int
vscp_link_callback_enable_rcvloop(const void *pdata, int bEnable)
{
  // Check pointer
  if (NULL == pdata) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx = (vscpctx_t *) pdata;

  pctx->bRcvLoop          = bEnable;
  pctx->last_rcvloop_time = esp_timer_get_time();

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_get_rcvloop_status
//

int
vscp_link_callback_get_rcvloop_status(const void *pdata, int *pRcvLoop)
{
  // Check pointer
  if (NULL == pdata) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx = (vscpctx_t *) pdata;
  *pRcvLoop       = pctx->bRcvLoop;

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_clrAll
//

int
vscp_link_callback_clrAll(const void *pdata)
{
  // Check pointer
  if (NULL == pdata) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx = (vscpctx_t *) pdata;

  vscpEvent *pev;
  if (pdTRUE == xSemaphoreTake(pctx->mutexQueue, 10 / portTICK_PERIOD_MS)) {

    while (pdTRUE == xQueueReceive(pctx->queueClient, &(pev), 0)) {
      vscp_fwhlp_deleteEvent(&pev);
    }
    xSemaphoreGive(pctx->mutexQueue);
  }
  else {
    return VSCP_ERROR_ERROR;
  }

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_get_channel_id
//

int
vscp_link_callback_get_channel_id(const void *pdata, uint16_t *pchid)
{
  // Check pointer
  if ((NULL == pdata) && (NULL == pchid)) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx = (vscpctx_t *) pdata;
  *pchid          = pctx->sock;

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_get_guid
//

int
vscp_link_callback_get_guid(const void *pdata, uint8_t *pguid)
{
  // Check pointers
  if ((NULL == pdata) || (NULL == pguid)) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscp_espnow_get_node_guid(pguid);
  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_set_guid
//

int
vscp_link_callback_set_guid(const void *pdata, uint8_t *pguid)
{
  // Check pointers
  if ((NULL == pdata) || (NULL == pguid)) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscp_espnow_get_node_guid(pguid);
  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_get_version
//

int
vscp_link_callback_get_version(const void *pdata, uint8_t *pversion)
{
  // Check pointers
  if ((NULL == pdata) || (NULL == pversion)) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  pversion[0] = 0; // THIS_FIRMWARE_MAJOR_VERSION;
  pversion[1] = 0; // THIS_FIRMWARE_MINOR_VERSION;
  pversion[2] = 0; // THIS_FIRMWARE_RELEASE_VERSION;
  pversion[3] = 0; // THIS_FIRMWARE_BUILD_VERSION;

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_setFilter
//

int
vscp_link_callback_setFilter(const void *pdata, vscpEventFilter *pfilter)
{
  // Check pointer
  if ((NULL == pdata) || (NULL == pfilter)) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx              = (vscpctx_t *) pdata;
  pctx->filter.filter_class    = pfilter->filter_class;
  pctx->filter.filter_type     = pfilter->filter_type;
  pctx->filter.filter_priority = pfilter->filter_priority;
  memcpy(pctx->filter.filter_GUID, pfilter->filter_GUID, 16);

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_setMask
//

int
vscp_link_callback_setMask(const void *pdata, vscpEventFilter *pfilter)
{
  // Check pointer
  if ((NULL == pdata) || (NULL == pfilter)) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx            = (vscpctx_t *) pdata;
  pctx->filter.mask_class    = pfilter->mask_class;
  pctx->filter.mask_type     = pfilter->mask_type;
  pctx->filter.mask_priority = pfilter->mask_priority;
  memcpy(pctx->filter.mask_GUID, pfilter->mask_GUID, 16);

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_statistics
//

int
vscp_link_callback_statistics(const void *pdata, VSCPStatistics *pStatistics)
{
  // Check pointer
  if ((NULL == pdata) || (NULL == pStatistics)) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx = (vscpctx_t *) pdata;
  memcpy(pStatistics, &pctx->statistics, sizeof(VSCPStatistics));

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_info
//

int
vscp_link_callback_info(const void *pdata, VSCPStatus *pstatus)
{
  // Check pointer
  if ((NULL == pdata) || (NULL == pstatus)) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx = (vscpctx_t *) pdata;
  memcpy(pstatus, &pctx->status, sizeof(VSCPStatus));

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_rcvloop
//

int
vscp_link_callback_rcvloop(const void *pdata, vscpEvent **pev)
{
  // Check pointer
  if (NULL == pdata) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  vscpctx_t *pctx = (vscpctx_t *) pdata;

  // Every second output '+OK\r\n' in rcvloop mode
  if ((esp_timer_get_time() - pctx->last_rcvloop_time) > 1000000l) {
    pctx->last_rcvloop_time = esp_timer_get_time();
    return VSCP_ERROR_TIMEOUT;
  }

  if (pdTRUE == xSemaphoreTake(pctx->mutexQueue, 0)) {
    if (pdTRUE != xQueueReceive(pctx->queueClient, pev, 0)) {
      xSemaphoreGive(pctx->mutexQueue);
      return VSCP_ERROR_RCV_EMPTY;
    }
    xSemaphoreGive(pctx->mutexQueue);
  }
  else {
    return VSCP_ERROR_TIMEOUT;
  }

  xSemaphoreGive(pctx->mutexQueue);

  // Update receive statistics
  pctx->statistics.cntReceiveFrames++;
  pctx->statistics.cntReceiveData += (*pev)->sizeData;

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_wcyd
//

int
vscp_link_callback_wcyd(const void *pdata, uint64_t *pwcyd)
{
  // Check pointers
  if ((NULL == pdata) || (NULL == pwcyd)) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  // vscpctx_t *pctx = (vscpctx_t *) pdata;

  *pwcyd = VSCP_SERVER_CAPABILITY_TCPIP | VSCP_SERVER_CAPABILITY_DECISION_MATRIX | VSCP_SERVER_CAPABILITY_IP4 |
           /*VSCP_SERVER_CAPABILITY_SSL |*/
           VSCP_SERVER_CAPABILITY_TWO_CONNECTIONS;

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_shutdown
//

int
vscp_link_callback_shutdown(const void *pdata)
{
  // Check pointer
  if (NULL == pdata) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  // vscpctx_t *pctx = (vscpctx_t *) pdata;
  esp_restart();

  // At this point
  // Shutdown the system
  // Set everything in a safe and inactive state

  // Stay here until someone presses the reset button
  // or power cycles the board
  while (1) {
    // watchdog_update();
  }

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_restart
//

int
vscp_link_callback_restart(const void *pdata)
{
  // Check pointer
  if (NULL == pdata) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  // vscpctx_t *pctx = (vscpctx_t *) pdata;

  esp_restart(); // Restart

  return VSCP_ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//                                 Binary
// ----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_bretr
//

int
vscp_link_callback_bretr(const void *pdata)
{
  // Check pointer
  if (NULL == pdata) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  // vscpctx_t *pctx = (vscpctx_t *) pdata;

  esp_restart(); // Restart

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_bsend
//

int
vscp_link_callback_bsend(const void *pdata)
{
  // Check pointer
  if (NULL == pdata) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  // vscpctx_t *pctx = (vscpctx_t *) pdata;

  esp_restart(); // Restart

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_brcvloop
//

int
vscp_link_callback_brcvloop(const void *pdata)
{
  // Check pointer
  if (NULL == pdata) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  // vscpctx_t *pctx = (vscpctx_t *) pdata;

  esp_restart(); // Restart

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// vscp_link_callback_sec
//

int
vscp_link_callback_sec(const void *pdata)
{
  // Check pointer
  if (NULL == pdata) {
    return VSCP_ERROR_INVALID_POINTER;
  }

  // vscpctx_t *pctx = (vscpctx_t *) pdata;

  esp_restart(); // Restart

  return VSCP_ERROR_SUCCESS;
}