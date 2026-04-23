/*
 * Copyright (c) 2026 The zmk-battery-monitor-firmware Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Peripheral-side GATT service exposing this half's metadata:
 *   - Side label (read, static, populated from Kconfig).
 *   - Charging state (read + notify, populated from zmk/usb state).
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

#if IS_ENABLED(CONFIG_ZMK_USB)
#include <zmk/usb.h>
#include <zmk/event_manager.h>
#include <zmk/events/usb_conn_state_changed.h>
#endif

#include "protocol.h"

LOG_MODULE_REGISTER(zmk_bm_peripheral, CONFIG_ZMK_BATTERY_MONITOR_LOG_LEVEL);

static const char side_label[] = CONFIG_ZMK_BATTERY_MONITOR_SIDE_LABEL;
static uint8_t charging_state;

static ssize_t read_side_label(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             side_label, sizeof(side_label) - 1);
}

static ssize_t read_charging(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             &charging_state, sizeof(charging_state));
}

static void charging_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    /* no-op; Zephyr tracks CCC state internally. */
}

BT_GATT_SERVICE_DEFINE(zmk_bm_service,
    BT_GATT_PRIMARY_SERVICE(ZMK_BM_SERVICE_UUID),

    BT_GATT_CHARACTERISTIC(ZMK_BM_SIDE_LABEL_UUID,
                           BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ,
                           read_side_label, NULL, NULL),

    BT_GATT_CHARACTERISTIC(ZMK_BM_CHARGING_UUID,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           read_charging, NULL, NULL),
    BT_GATT_CCC(charging_ccc_changed,
                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

#if IS_ENABLED(CONFIG_ZMK_USB)

/* Attribute index in the service array for the charging characteristic value.
 * Ordering from the service definition above:
 *   [0] primary service
 *   [1] char decl (side label)     [2] char value (side label)
 *   [3] char decl (charging)       [4] char value (charging)     [5] CCC
 */
#define CHARGING_ATTR_INDEX 4

static int on_usb_conn_state_changed(const zmk_event_t *eh) {
    const struct zmk_usb_conn_state_changed *ev = as_zmk_usb_conn_state_changed(eh);
    if (ev == NULL) return ZMK_EV_EVENT_BUBBLE;

    uint8_t new_state = zmk_usb_is_powered() ? 1 : 0;
    if (new_state == charging_state) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    charging_state = new_state;

    int err = bt_gatt_notify(NULL, &zmk_bm_service.attrs[CHARGING_ATTR_INDEX],
                             &charging_state, sizeof(charging_state));
    if (err && err != -ENOTCONN) {
        LOG_DBG("charging notify err %d", err);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(zmk_bm_peripheral_usb, on_usb_conn_state_changed);
ZMK_SUBSCRIPTION(zmk_bm_peripheral_usb, zmk_usb_conn_state_changed);

static int prime_charging_state(void) {
    charging_state = zmk_usb_is_powered() ? 1 : 0;
    return 0;
}

SYS_INIT(prime_charging_state, APPLICATION, 95);

#endif /* CONFIG_ZMK_USB */
