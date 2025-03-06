/*
  sntp.c

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

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "sntp.h"
#include "esp_netif_sntp.h"
#include "lwip/ip_addr.h"
#include "esp_sntp.h"

static const char *TAG = "example";

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 48
#endif

///////////////////////////////////////////////////////////////////////////////
// print_servers
//


static void
print_servers(void)
{
  ESP_LOGI(TAG, "List of configured NTP servers:");

  for (uint8_t i = 0; i < SNTP_MAX_SERVERS; ++i) {
    if (esp_sntp_getservername(i)) {
      ESP_LOGI(TAG, "server %d: %s", i, esp_sntp_getservername(i));
    }
    else {
      // we have either IPv4 or IPv6 address, let's print it
      char buff[INET6_ADDRSTRLEN];
      ip_addr_t const *ip = esp_sntp_getserver(i);
      if (ipaddr_ntoa_r(ip, buff, INET6_ADDRSTRLEN) != NULL)
        ESP_LOGI(TAG, "server %d: %s", i, buff);
    }
  }
}


///////////////////////////////////////////////////////////////////////////////
// time_sync_notification_cb
//

void
time_sync_notification_cb(struct timeval *tv)
{
  time_t nowtime;
  struct tm *nowtm;
  char tmbuf[64], buf[80];

  nowtime = tv->tv_sec;
  nowtm   = localtime(&nowtime);
  strftime(tmbuf, sizeof(tmbuf), "%Y-%m-%d %H:%M:%S", nowtm);
  snprintf(buf, sizeof(buf), "%s.%06ld", tmbuf, tv->tv_usec);

  ESP_LOGD(TAG, "Notification of a time synchronization event %lu %lu %s", (uint32_t) tv->tv_sec, (uint32_t) tv->tv_usec, buf);
}

///////////////////////////////////////////////////////////////////////////////
// get_time
//

void
get_ntp_time(void)
{
  // ESP_ERROR_CHECK( nvs_flash_init() );
  // ESP_ERROR_CHECK(esp_netif_init());
  // ESP_ERROR_CHECK( esp_event_loop_create_default() );

#if LWIP_DHCP_GET_NTP_SRV
  /**
   * NTP server address could be acquired via DHCP,
   * see following menuconfig options:
   * 'LWIP_DHCP_GET_NTP_SRV' - enable STNP over DHCP
   * 'LWIP_SNTP_DEBUG' - enable debugging messages
   *
   * NOTE: This call should be made BEFORE esp acquires IP address from DHCP,
   * otherwise NTP option would be rejected by default.
   */
  ESP_LOGI(TAG, "Initializing SNTP");
  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SNTP_TIME_SERVER);
  config.start             = false; // start SNTP service explicitly (after connecting)
  config.server_from_dhcp  = true;  // accept NTP offers from DHCP server, if any (need to enable *before* connecting)
  config.renew_servers_after_new_IP = true; // let esp-netif update configured SNTP server(s) after receiving DHCP lease
  config.index_of_first_server      = 1;    // updates from server num 1, leaving server 0 (from DHCP) intact
                                            // configure the event on which we renew servers
#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
  config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
#else
  config.ip_event_to_renew = IP_EVENT_ETH_GOT_IP;
#endif
  config.sync_cb = time_sync_notification_cb; // only if we need the notification function
  esp_netif_sntp_init(&config);

#endif /* LWIP_DHCP_GET_NTP_SRV */

#if LWIP_DHCP_GET_NTP_SRV
  ESP_LOGI(TAG, "Starting SNTP");
  esp_netif_sntp_start();
#if LWIP_IPV6 && SNTP_MAX_SERVERS > 2
  /* This demonstrates using IPv6 address as an additional SNTP server
   * (statically assigned IPv6 address is also possible)
   */
  ip_addr_t ip6;
  if (ipaddr_aton("2a01:3f7::1", &ip6)) { // ipv6 ntp source "ntp.netnod.se"
    esp_sntp_setserver(2, &ip6);
  }
#endif /* LWIP_IPV6 */

#else
  ESP_LOGI(TAG, "Initializing and starting SNTP");
#if CONFIG_LWIP_SNTP_MAX_SERVERS > 1
  /* This demonstrates configuring more than one server
   */
  esp_sntp_config_t config =
    ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2, ESP_SNTP_SERVER_LIST(CONFIG_SNTP_TIME_SERVER, "pool.ntp.org"));
#else
  /*
   * This is the basic default config with one server and starting the service
   */
  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SNTP_TIME_SERVER);
#endif
  config.sync_cb = time_sync_notification_cb; // Note: This is only needed if we want
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
  config.smooth_sync = true;
#endif

  esp_netif_sntp_init(&config);
#endif

  print_servers();

  // wait for time to be set
  time_t now            = 0;
  struct tm timeinfo    = { 0 };
  int retry             = 0;
  const int retry_count = 15;
  while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count) {
    ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
  }
  time(&now);
  localtime_r(&now, &timeinfo);

  // ESP_ERROR_CHECK( example_disconnect() );
  esp_netif_sntp_deinit();
}