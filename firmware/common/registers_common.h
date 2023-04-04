/*
  Register definitions for Alpha/Beta/Gamma nodes
*/

#define REG_VSCP_ESPNOW_RESET 0x00000000 // Write 0x55 followed by 0xAA withing 500 ms to reset unit. (Write only)
// Standard #define REG_VSCP_ESPNOW_GUID       0x00000010 // GUID 16 byte          (Read Only)
#define REG_VSCP_ESPNOW_CHANNEL            0x00000020 // Channel to work on    (Read/Write)
#define REG_VSCP_ESPNOW_LOG_CFG            0x00000021 // Control register for logging (Read/Write)
#define REG_VSCP_ESPNOW_REBOOT_COUNT       0x00000022 // Boot count UINT32 (Read Only)
#define REG_VSCP_ESPNOW_OTA_URL            0x00000026 // URL for device image (64 bytes)
#define REG_VSCP_ESPNOW_CHIP_TYPE          0x00000102 // (Read Only)
#define REG_VSCP_ESPNOW_SILICON_REV        0x00000103 // (Read Only)
#define REG_VSCP_ESPNOW_FLASH_SIZE         0x00000104 // (Read Only)
#define REG_VSCP_ESPNOW_IDF_VER            0x00000105 // 3 bytes (Read Only)
#define REG_VSCP_ESPNOW_FREE_HEAP          0x00000108 // (Read Only)
#define REG_VSCP_ESPNOW_MIN_FREE_HEAP      0x00000109 // (Read Only)
#define REG_VSCP_ESPNOW_RESET_REASON       0x00000110 // (Read Only)
#define REG_VSCP_ESPNOW_OTA_PARTITION_SIZE 0x00000111 // 2 bytes kB (Read Only)
#define REG_VSCP_ESPNOW_LOG_LEVEL          0x00000113 //  (Read/Write)

// ESP-NOW

// espnow configuration byte 1 (Read/Write)
// bit 0 - Long range
// bit 1 - Forward enable
// bit 2 - Filter Adjacent channel
// bit 3 - Forward Switch channel
// bit 4 -
// bit 5 -
// bit 6 -
// bit 7 -
#define REG_VSCP_ESPNOW_CFG1 0x00002000 // espnow configuration flags 1 (Read/Write)

// espnow configuration byte 2 (Read/Write)
// bit 0 - Enable logging
// bit 1 -
// bit 2 -
// bit 3 -
// bit 4 -
// bit 5 -
// bit 6 -
// bit 7 -
#define REG_VSCP_ESPNOW_CFG2 0x00002001 // espnow configuration flags 2 (Read/Write)

// rssi value for weak signals (Read/Write)
#define REG_VSCP_ESPNOW_FILTER_WEAK_SIGNAL 0x00000204 // Filter for weak signal (int8_t)

// Size for receive queue (Read/Write)
#define REG_VSCP_ESPNOW_QUEUE_SIZE 0x00002005 // Size of receive queue

// Channel (Read Only)
#define REG_VSCP_ESPNOW_CHANNEL 0x00002006 // Wifi/espnow channel

// ttl (Read/Write)
#define REG_VSCP_ESPNOW_TTL 0x00002007 // Time to live for sent frame
