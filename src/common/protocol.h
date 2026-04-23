/*
 * Copyright (c) 2026 The zmk-battery-monitor-firmware Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/bluetooth/uuid.h>

/*
 * Custom GATT service exposed by peripheral halves. The central reads these
 * characteristics to populate per-peripheral metadata in the HID Report ID 2.
 */
#define ZMK_BM_SERVICE_UUID_VAL                                                                    \
    BT_UUID_128_ENCODE(0x6d0e0001, 0x8b4e, 0x4e8a, 0x8e7d, 0x9a5e9b8c7d6e)

#define ZMK_BM_SIDE_LABEL_UUID_VAL                                                                 \
    BT_UUID_128_ENCODE(0x6d0e0002, 0x8b4e, 0x4e8a, 0x8e7d, 0x9a5e9b8c7d6e)

#define ZMK_BM_CHARGING_UUID_VAL                                                                   \
    BT_UUID_128_ENCODE(0x6d0e0003, 0x8b4e, 0x4e8a, 0x8e7d, 0x9a5e9b8c7d6e)

#define ZMK_BM_SERVICE_UUID    BT_UUID_DECLARE_128(ZMK_BM_SERVICE_UUID_VAL)
#define ZMK_BM_SIDE_LABEL_UUID BT_UUID_DECLARE_128(ZMK_BM_SIDE_LABEL_UUID_VAL)
#define ZMK_BM_CHARGING_UUID   BT_UUID_DECLARE_128(ZMK_BM_CHARGING_UUID_VAL)

/*
 * HID Report ID 2: metadata per peripheral. 32 bytes, always that size so the
 * host parses it uniformly. Sent once per peripheral when new metadata becomes
 * available (e.g. label read from BLE completes).
 *
 *   byte 0       peripheral index (0-based)
 *   byte 1       flags
 *                  bit 0: charging
 *                  bit 1: voltage valid
 *                  bits 2..7: reserved, must be 0
 *   bytes 2..3   voltage in millivolts (LE uint16; 0 when unavailable)
 *   bytes 4..31  side label, UTF-8, null-terminated, zero-padded
 */
#define ZMK_BM_METADATA_REPORT_ID        0x02
#define ZMK_BM_METADATA_REPORT_SIZE      32
#define ZMK_BM_METADATA_LABEL_OFFSET     4
#define ZMK_BM_METADATA_LABEL_MAX        28 /* includes null terminator */

#define ZMK_BM_METADATA_FLAG_CHARGING    (1 << 0)
#define ZMK_BM_METADATA_FLAG_VOLTAGE_VALID (1 << 1)

#define ZMK_BM_BATTERY_REPORT_ID         0x01
