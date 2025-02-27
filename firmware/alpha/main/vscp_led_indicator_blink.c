/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <led_indicator.h>
#include "vscp_led_indicator_blink.h"

/*********************************** Config Blink List in Different Conditions ***********************************/
/**
 * @brief connecting to AP (or Cloud)
 *
 */
static const blink_step_t vscp_connecting[] = {
  { LED_BLINK_HOLD, LED_STATE_ON, 200 },
  { LED_BLINK_HOLD, LED_STATE_OFF, 800 },
  { LED_BLINK_LOOP, 0, 0 },
};

/**
 * @brief connected to AP (or Cloud) succeed
 *
 */
static const blink_step_t vscp_connected[] = {
  { LED_BLINK_HOLD, LED_STATE_ON, 1000 },
  { LED_BLINK_LOOP, 0, 0 },
};

/**
 * @brief reconnecting to AP (or Cloud), if lose connection
 *
 */
static const blink_step_t vscp_reconnecting[] = {
  { LED_BLINK_HOLD, LED_STATE_ON, 100 },
  { LED_BLINK_HOLD, LED_STATE_OFF, 400 },
  { LED_BLINK_LOOP, 0, 0 },
}; // offline

/**
 * @brief updating software
 * Two . . -
 */
static const blink_step_t vscp_updating[] = {
  { LED_BLINK_HOLD, LED_STATE_ON, 50 },
  { LED_BLINK_HOLD, LED_STATE_OFF, 200 },
  { LED_BLINK_HOLD, LED_STATE_ON, 50 },
  { LED_BLINK_HOLD, LED_STATE_OFF, 1200 },
  { LED_BLINK_LOOP, 0, 0 },
};

/**
 * @brief restoring factory settings
 * Three . . . -
 */
static const blink_step_t vscp_factory_reset[] = {
  { LED_BLINK_HOLD, LED_STATE_ON, 50 },
  { LED_BLINK_HOLD, LED_STATE_OFF, 100 },
  { LED_BLINK_LOOP, 0, 0 },
};

/**
 * @brief Indicate error
 * Three . . . -
 */
static const blink_step_t vscp_error[] = {
  { LED_BLINK_HOLD, LED_STATE_ON, 50 },
  { LED_BLINK_HOLD, LED_STATE_OFF, 50 },
  { LED_BLINK_LOOP, 0, 0 },
};

/**
 * @brief provision done
 * Steady off
 */
static const blink_step_t vscp_provisioned[] = {
  { LED_BLINK_HOLD, LED_STATE_OFF, 1000 },
  { LED_BLINK_STOP, 0, 0 },
};

/**
 * @brief provisioning
 * Slow blink
 */
static const blink_step_t vscp_provisioning[] = {
  { LED_BLINK_HOLD, LED_STATE_ON, 50 },
  { LED_BLINK_HOLD, LED_STATE_OFF, 200 },
  { LED_BLINK_HOLD, LED_STATE_ON, 50 },
  { LED_BLINK_HOLD, LED_STATE_OFF, 200 },
  { LED_BLINK_HOLD, LED_STATE_ON, 50 },
  { LED_BLINK_HOLD, LED_STATE_OFF, 1200 },
  { LED_BLINK_LOOP, 0, 0 },
};

/**
 * @brief LED indicator blink lists, the index like BLINK_FACTORY_RESET defined the priority of the blink
 *
 */

/* clang-format off */
blink_step_t const *vscp_led_indicator_blink_lists[] = {
  [BLINK_FACTORY_RESET] = vscp_factory_reset, 
  [BLINK_UPDATING]      = vscp_updating,
  [BLINK_CONNECTED]     = vscp_connected,         
  [BLINK_PROVISIONED]   = vscp_provisioned,
  [BLINK_RECONNECTING]  = vscp_reconnecting,   
  [BLINK_CONNECTING]    = vscp_connecting,
  [BLINK_PROVISIONING]  = vscp_provisioning,   
  [BLINK_ERROR]         = vscp_error, 
  [BLINK_MAX] = NULL,
};
/* clang-format off */

/* LED blink_steps handling machine implementation */
const int VSCP_BLINK_LIST_NUM = (sizeof(vscp_led_indicator_blink_lists) / sizeof(vscp_led_indicator_blink_lists[0]));

// ----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
// blink_switch_type
//

void
blink_switch_type(led_indicator_handle_t h, int type)
{
  static int last = 0;

  led_indicator_stop(h, last);
  led_indicator_start(h, type);
  last = type;
}