/*
  File: tcpsrv.c

  VSCP Wireless CAN4VSCP Gateway (VSCP-WCANG)

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

#ifndef __VSCP_LINK_TCPSRV__
#define __VSCP_LINK_TCPSRV__

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "freertos/semphr.h"

#include <vscp.h>
#include <vscp-firmware-helper.h>
#include <vscp-link-protocol.h>
#include <vscp-firmware-level2.h>

// Buffer
#define TCPIP_BUF_MAX_SIZE (1024 * 3)

/**
 * VSCP TCP link protocol character buffer size
 */
#ifndef DATA_BUF_SIZE
#define DATA_BUF_SIZE 512
#endif

/**
 * Max number of events in the receive fifo
 * (Events from Droplet to VSCP link client)
 */
#define DROPLET_QUEUE_SIZE 4

/**
 * Max number of events in each of the transmit queues
 * (Events to Droplet from VSCP link client)
 */
#define CLIENT_QUEUE_SIZE 4

/*
  Socket context
  This is the context for each open socket/channel.
*/
typedef struct _vscpctx {
  int id;
  int sock;                                  // Socket
  size_t size;                               // Number of characters in buffer
  char buf[TCPIP_BUF_MAX_SIZE];              // Command Buffer
  char user[VSCP_LINK_MAX_USER_NAME_LENGTH]; // Username storage
  SemaphoreHandle_t mutexQueue;              // Protect the queue
  QueueHandle_t queueClient;                 // VSCP events to VSCP link client
  int bValidated;                            // User is validated
  uint8_t privLevel;                         // User privilege level 0-15
  int bRcvLoop;                              // Receive loop is enabled if non zero
  vscpEventFilter filter;                    // Filter for events
  VSCPStatistics statistics;                 // VSCP Statistics
  VSCPStatus status;                         // VSCP status
  uint32_t last_rcvloop_time;                // Time of last received event
} vscpctx_t;

#define MSG_MAX_CLIENTS "Max number of clients reached. Disconnecting.\r\n"

/**
 * @brief Set defaults for the Context Defaults object
 *
 * @param pctx Pointer to context
 */
void
tcpsrv_setContextDefaults(vscpctx_t *pctx);

/**
 * @fn tcpsrv_sendEventExToAllClients
 * @brief Send event ex to all active clients
 * @param pev Pointer to event to send
 * @return VSCP_EVENT_SUCCESS if all web OK. Error code otherwise.
 */
int
tcpsrv_sendEventExToAllClients(const vscpEvent *pev);

/*!
  VSCP tcp/ip link protocol task
  @param pvParameters Task parameters
*/
void
tcpsrv_task(void *pvParameters);

#endif