/*
 * Copyright (c) 2026 The zmk-battery-monitor-firmware Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Peripheral-side GATT service exposing this half's side label.
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

#include "protocol.h"

LOG_MODULE_REGISTER(zmk_bm_peripheral, CONFIG_ZMK_BATTERY_MONITOR_LOG_LEVEL);

static const char side_label[] = CONFIG_ZMK_BATTERY_MONITOR_SIDE_LABEL;

static ssize_t read_side_label(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             side_label, sizeof(side_label) - 1);
}

BT_GATT_SERVICE_DEFINE(zmk_bm_service,
    BT_GATT_PRIMARY_SERVICE(ZMK_BM_SERVICE_UUID),
    BT_GATT_CHARACTERISTIC(ZMK_BM_SIDE_LABEL_UUID,
                           BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ,
                           read_side_label, NULL, NULL),
);
