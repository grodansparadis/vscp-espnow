/*
  projdefs.h

  This file contains project definitions for the VSCP TCP/IP link protocol code.
*/

#ifndef _VSCP_PROJDEFS_H_
#define _VSCP_PROJDEFS_H_


/**
  ----------------------------------------------------------------------------
                                    VSCP
  ----------------------------------------------------------------------------
  Defines for firmware level II
*/

/**
 * Device name for level II device
 * capabilities event
 */
#define THIS_FIRMWARE_DEVICE_NAME   "Beta node"

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
#define THIS_FIRMWARE_MDF_URL "eurosource.se/beta0.mdf"

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

// ----------------------------------------------------------------------------
//                        VSCP helper lib defines
// ----------------------------------------------------------------------------

#define VSCP_FWHLP_CRYPTO_SUPPORT // AES crypto support
#define VSCP_FWHLP_JSON_SUPPORT   // Enable JSON support (Need cJSON lib)


#endif // _VSCP_PROJDEFS_H_