/*
 * Copyright (c) 2026 The zmk-battery-monitor-firmware Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/bluetooth/uuid.h>

/*
 * Custom GATT service exposed by peripheral halves. The central reads the
 * side-label characteristic to populate per-peripheral metadata in the
 * HID Report ID 2.
 */
#define ZMK_BM_SERVICE_UUID_VAL                                                                    \
    BT_UUID_128_ENCODE(0x6d0e0001, 0x8b4e, 0x4e8a, 0x8e7d, 0x9a5e9b8c7d6e)

#define ZMK_BM_SIDE_LABEL_UUID_VAL                                                                 \
    BT_UUID_128_ENCODE(0x6d0e0002, 0x8b4e, 0x4e8a, 0x8e7d, 0x9a5e9b8c7d6e)

#define ZMK_BM_SERVICE_UUID    BT_UUID_DECLARE_128(ZMK_BM_SERVICE_UUID_VAL)
#define ZMK_BM_SIDE_LABEL_UUID BT_UUID_DECLARE_128(ZMK_BM_SIDE_LABEL_UUID_VAL)

/*
 * USB HID report layout.
 *
 * Report ID 1 (battery levels):
 *   1 byte per peripheral, state-of-charge 0-100 or 0xFF = unknown.
 *
 * Report ID 2 (peripheral metadata, 32 bytes):
 *   byte 0       peripheral index (0-based)
 *   bytes 1..31  side label, UTF-8, null-terminated, zero-padded
 *
 * Report ID 3 (active layer, 1 byte):
 *   byte 0       highest active layer index (matches ZMK's on-dongle display)
 *
 * Report ID 4 (layer metadata, 32 bytes):
 *   byte 0       layer index
 *   bytes 1..31  layer label, UTF-8, null-terminated, zero-padded
 */
#define ZMK_BM_BATTERY_REPORT_ID       0x01
#define ZMK_BM_METADATA_REPORT_ID      0x02
#define ZMK_BM_LAYER_REPORT_ID         0x03
#define ZMK_BM_LAYER_NAME_REPORT_ID    0x04

#define ZMK_BM_METADATA_REPORT_SIZE    32
#define ZMK_BM_METADATA_LABEL_OFFSET   1
#define ZMK_BM_METADATA_LABEL_MAX      31 /* includes null terminator */

#define ZMK_BM_LAYER_NAME_REPORT_SIZE  32
#define ZMK_BM_LAYER_NAME_OFFSET       1
#define ZMK_BM_LAYER_NAME_MAX          31 /* includes null terminator */
