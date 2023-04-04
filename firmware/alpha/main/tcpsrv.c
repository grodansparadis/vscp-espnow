/*
  File: tcpsrv.c

  VSCP tcp/ip link server

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

  Config
  ------
  Enable (yes)
  ip-address for server
  port (9598)
  Valid client ip's (all)
  Username (admin)
  Password (secret)
*/

#include <freertos/FreeRTOS.h>
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include <esp_timer.h>

#include <nvs_flash.h>

#include <lwip/err.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>

#include <string.h>
#include <sys/param.h>

#include "alpha.h"
#include <vscp.h>

#include "tcpsrv.h"

#define KEEPALIVE_IDLE                                                                                                 \
  5                          // Keep-alive idle time.
                             // In idle time without receiving any data from peer, will send keep-alive probe packet
#define KEEPALIVE_INTERVAL 5 // Keep-alive probe packet interval time.
#define KEEPALIVE_COUNT    3 // Keep-alive probe packet retry count.

static const char *TAG    = "tcpsrv";
static uint8_t cntClients = 0; // Holds current number of clients

/**
  Received events are written to this queue
  from all channels and events is consumed by the
  VSCP droplet handler. The queue is shared with
  other event receivers like MQTT.
*/
// QueueHandle_t g_queueDroplet = NULL;

// Mutex that protect the droplet queue
// SemaphoreHandle_t g_mutexQueueDroplet;

// Buffers
// static transport_t s_tr_tcpsrv[MAX_TCP_CONNECTIONS];

/*!
  This is the socket context for open channels. It holds all
  contect data including the socket it's state and the output
  queue to the VSCP tcp/ip link client
*/
static vscpctx_t g_ctx[MAX_TCP_CONNECTIONS]; // Socket context

///////////////////////////////////////////////////////////////////////////////
// tcpsrv_setContextDefaults
//

void
tcpsrv_setContextDefaults(vscpctx_t *pctx)
{
  pctx->sock              = 0;
  pctx->bValidated        = 0;
  pctx->privLevel         = 0;
  pctx->bRcvLoop          = 0;
  pctx->size              = 0;
  pctx->last_rcvloop_time = esp_timer_get_time();
  memset(pctx->buf, 0, TCPIP_BUF_MAX_SIZE);
  memset(pctx->user, 0, VSCP_LINK_MAX_USER_NAME_LENGTH);
  // Filter: All events received
  memset(&pctx->filter, 0, sizeof(vscpEventFilter));
  memset(&pctx->statistics, 0, sizeof(VSCPStatistics));
  memset(&pctx->status, 0, sizeof(VSCPStatus));
}

///////////////////////////////////////////////////////////////////////////////
// tcpsrv_sendEventExToAllClients
//

int
tcpsrv_sendEventExToAllClients(const vscpEvent *pev)
{
  for (int i = 0; i < MAX_TCP_CONNECTIONS; i++) {
    if (g_ctx[i].sock && (NULL != g_ctx[i].queueClient)) {
      vscpEvent *pnew = vscp_fwhlp_mkEventCopy(pev);
      if (NULL == pnew) {
        vscp_fwhlp_deleteEvent(&pnew);
        ESP_LOGE(TAG, "Unable to allocate memory for event for client %d", i);
        return VSCP_ERROR_MEMORY;
      }
      else {
        if (pdTRUE == xSemaphoreTake(g_ctx[i].mutexQueue, 10 / portTICK_PERIOD_MS)) {
          if (pdTRUE != xQueueSend(g_ctx[i].queueClient, &(pev), 0)) {
            xSemaphoreGive(g_ctx[i].mutexQueue);
            vscp_fwhlp_deleteEvent(&pnew);
            g_ctx[i].statistics.cntOverruns++;
            ESP_LOGI(TAG, "Queue is full for client %d", i);
            return VSCP_ERROR_TRM_FULL; // yes, receive queue, but transmit for sender
          }
          xSemaphoreGive(g_ctx[i].mutexQueue);
        }
        else {
          ESP_LOGI(TAG, "Mutex timeout for client %d", i);
          return VSCP_ERROR_TIMEOUT;
        }
      }
    }
  }

  return VSCP_ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// do_retransmit
//

// static void
//  do_retransmit(const int sock)
//  {
//    int len;
//    char rx_buffer[128];

//   do {
//     len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
//     if (len < 0) {
//       ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
//     }
//     else if (len == 0) {
//       ESP_LOGW(TAG, "Connection closed");
//     }
//     else {
//       rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
//       ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);

//       // send() can return less bytes than supplied length.
//       // Walk-around for robust implementation.
//       int to_write = len;
//       while (to_write > 0) {
//         int written = send(sock, rx_buffer + (len - to_write), to_write, 0);
//         if (written < 0) {
//           ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
//         }
//         to_write -= written;
//       }
//     }
//   } while (len > 0);
// }

///////////////////////////////////////////////////////////////////////////////
// client_task
//

static void
client_task(void *pvParameters)
{
  int rv;
  size_t len;
  fd_set readset;  // Socket read set
  fd_set writeset; // Socket write set
  fd_set errset;   // Socket error set
  struct timeval tv;
  vscpctx_t *pctx = (vscpctx_t *) pvParameters;

  // Mark transport channel as open
  // g_tr_tcpsrv[pctx->id].open = true;

  ESP_LOGI(TAG, "Client worker socket=%d id=%d", pctx->sock, pctx->id);

  // Greet client
  // send(pctx->sock, TCPSRV_WELCOME_MSG, sizeof(TCPSRV_WELCOME_MSG), 0);
  vscp_link_callback_welcome(pctx);

  // Another client
  cntClients++;

  do {
    FD_ZERO(&readset);
    FD_SET(pctx->sock, &readset);
    FD_ZERO(&writeset);
    FD_SET(pctx->sock, &writeset);
    FD_ZERO(&errset);
    FD_SET(pctx->sock, &errset);
    tv.tv_sec  = 1;
    tv.tv_usec = 0;
    int ret    = select(pctx->sock + 1, &readset, &writeset, &errset, &tv);
    if (FD_ISSET(pctx->sock, &writeset)) {

      len = (rv = recv(pctx->sock, pctx->buf + pctx->size, (sizeof(pctx->buf) - pctx->size) - 1, 0 /*MSG_DONTWAIT*/));
      if ((rv < 0)) {
        // If nothing to read do idle work
        if (errno == EAGAIN) {
          // Handle rcvloop etc
          vscp_link_idle_worker(pctx);
          // vTaskDelay(1 / portTICK_PERIOD_MS);
          continue;
        }
        ESP_LOGE(TAG, "Error occurred during receiving: rv=%d, errno=%d", rv, errno);
        memset(pctx->buf, 0, sizeof(pctx->buf));
        pctx->size = 0;
        close(pctx->sock);
        pctx->sock = 0;
        break;
      }
      else if (rv == 0) {
        close(pctx->sock);
        pctx->sock = 0;
        ESP_LOGW(TAG, "Connection closed");
        break;
      }
      else {

        pctx->size += len;
        pctx->buf[pctx->size + 1] = 0;

        // Parse VSCP command
        char *pnext = NULL;
        if (VSCP_ERROR_SUCCESS == vscp_link_parser(pctx, pctx->buf, &pnext)) {

          if ((NULL != pnext) && *pnext) {
            // printf("Copy [%s]\n", pnext);
            strncpy(pctx->buf, pnext, sizeof(pctx->buf));
            pctx->size = strlen(pctx->buf);
          }
          else {
            memset(pctx->buf, 0, sizeof(pctx->buf));
            pctx->size = 0;
          }
        }
        else if (1 <= (sizeof(pctx->buf) - pctx->size)) {
          // printf("Full buffer without crlf\n");
          *pctx->buf = 0;
          pctx->size = 0;
        }

        // printf("post len %d\n", pctx->size);

        // If socket gets closed ("quit" command)
        // pctx->sock is zero
        if (!pctx->sock) {
          break;
        }

        // Get event from out fifo to feed to
        // protocol handler etc
        // vscpEvent *pev = NULL;
        // if (pdTRUE == xSemaphoreTake(pctx->mutexQueue, (TickType_t) 10/ portTICK_PERIOD_MS)) {
        //   if (pdTRUE != xQueueReceive(pctx->queueClient, &pev, 0)) {
        //     pev = NULL;
        //   }
        //   xSemaphoreGive(pctx->mutexQueue);
        // }
        // else {
        //   ESP_LOGW(TAG, "Unable to get mutex for client queue for client %d", pctx->id);
        // }

        // pev is NULL if no event is available here
        // The worker is still called.
        // if pev != NULL the worker is responsible for
        // freeing the event

        // Do protocol work here
        // if (NULL != pev) {
        //  vscp2_do_work(pev);
        //}

        // Handle rcvloop etc
        vscp_link_idle_worker(pctx);
        // vTaskDelay(1 / portTICK_PERIOD_MS);
      }
    }
  } while (pctx->sock);

  // Mark transport channel as closed
  // g_tr_tcpsrv[pctx->id].open = false;

  vscpEvent *pev;
  if (pdTRUE == xSemaphoreTake(pctx->mutexQueue, 5000 / portTICK_PERIOD_MS)) {

    while (pdTRUE == xQueueReceive(pctx->queueClient, &(pev), 0)) {
      vscp_fwhlp_deleteEvent(&pev);
    }
    xSemaphoreGive(pctx->mutexQueue);
  }
  else {
    ESP_LOGE(TAG, "Could not empty input queue on close (mutex)");
  }

  // Empty the queue
  // xQueueReset(g_tr_tcpsrv[pctx->id].msg_queue);

  ESP_LOGI(TAG, "Closing down tcp/ip client");

  // If not closed do it here
  if (pctx->sock) {
    shutdown(pctx->sock, 0);
    close(pctx->sock);
  }

  tcpsrv_setContextDefaults(pctx);

  cntClients--;
  ESP_LOGI(TAG, "Number of clients %d.", cntClients);
  vTaskDelete(NULL);
}

///////////////////////////////////////////////////////////////////////////////
// tcpsrv_task
//

void
tcpsrv_task(void *pvParameters)
{
  char addr_str[128];
  int addr_family  = (int) pvParameters;
  int ip_protocol  = 0;
  int keepAlive    = 1;
  int keepIdle     = KEEPALIVE_IDLE;
  int keepInterval = KEEPALIVE_INTERVAL;
  int keepCount    = KEEPALIVE_COUNT;
  struct sockaddr_storage dest_addr;
  vscpEvent *pev;

  ESP_LOGI(TAG, "VSCP tcp/ip Link server started.");

  for (int i = 0; i < MAX_TCP_CONNECTIONS; i++) {
    // g_tr_tcpsrv[i].msg_queue = xQueueCreate(10, VSCP_ESPNOW_MAX_FRAME); // tcp/ip link channel i
    g_ctx[i].id         = i;
    g_ctx[i].sock       = 0;
    g_ctx[i].mutexQueue = xSemaphoreCreateMutex();
    if (NULL == g_ctx[i].mutexQueue) {
      ESP_LOGE(TAG, "Failed to create mutex for client queue for client %d", i);
    }
    g_ctx[i].queueClient = xQueueCreate(CLIENT_QUEUE_SIZE, sizeof(pev));
    if (NULL == g_ctx[i].queueClient) {
      ESP_LOGE(TAG, "Failed to create client queue for client %d", i);
    }
    tcpsrv_setContextDefaults(&g_ctx[i]);
  }

  if (addr_family == AF_INET) {
    struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *) &dest_addr;
    dest_addr_ip4->sin_addr.s_addr    = htonl(INADDR_ANY);
    dest_addr_ip4->sin_family         = AF_INET;
    dest_addr_ip4->sin_port           = htons(VSCP_DEFAULT_TCP_PORT);
    ip_protocol                       = IPPROTO_IP;
  }
  else if (addr_family == AF_INET6) {
    struct sockaddr_in6 *dest_addr_ip6 = (struct sockaddr_in6 *) &dest_addr;
    bzero(&dest_addr_ip6->sin6_addr.un, sizeof(dest_addr_ip6->sin6_addr.un));
    dest_addr_ip6->sin6_family = AF_INET6;
    dest_addr_ip6->sin6_port   = htons(VSCP_DEFAULT_TCP_PORT);
    ip_protocol                = IPPROTO_IPV6;
  }

  int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
  if (listen_sock < 0) {
    ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
    vTaskDelete(NULL);
    return;
  }
  int opt = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#if defined(CONFIG_EXAMPLE_IPV4) && defined(CONFIG_EXAMPLE_IPV6)
  // Note that by default IPV6 binds to both protocols, it must be disabled
  // if both protocols used at the same time (used in CI)
  setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
#endif

  int err = bind(listen_sock, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
  if (err != 0) {
    ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
    ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
    goto CLEAN_UP;
  }

  err = listen(listen_sock, 1);
  if (err != 0) {
    ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
    goto CLEAN_UP;
  }

  int sock;
  while (1) {

    struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
    socklen_t addr_len = sizeof(source_addr);
    sock               = accept(listen_sock, (struct sockaddr *) &source_addr, &addr_len);
    if (sock < 0) {
      ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
      break;
    }

    ESP_LOGI(TAG, "Accepted");

    // Set tcp keepalive option
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

    // Convert ip address to string
    if (source_addr.ss_family == PF_INET) {
      inet_ntoa_r(((struct sockaddr_in *) &source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
    }
    else if (source_addr.ss_family == PF_INET6) {
      inet6_ntoa_r(((struct sockaddr_in6 *) &source_addr)->sin6_addr, addr_str, sizeof(addr_str) - 1);
    }

    ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

    if (cntClients >= MAX_TCP_CONNECTIONS) {
      ESP_LOGW(TAG, "Max number of clients %d. Closing connection", cntClients);
      send(sock, MSG_MAX_CLIENTS, sizeof(MSG_MAX_CLIENTS), 0);
      close(sock);
      continue;
    }

    // Start task
    for (int i = 0; i < MAX_TCP_CONNECTIONS; i++) {
      if (!g_ctx[i].sock) {
        g_ctx[i].sock = sock;
        // Create VSCP Link tcp/ip client task
        xTaskCreate(client_task, "link-client", 8 * 1024, (void *) &g_ctx[i], 5, NULL);
        break;
      }
    }
  }

CLEAN_UP:
  close(listen_sock);
  vTaskDelete(NULL);
}