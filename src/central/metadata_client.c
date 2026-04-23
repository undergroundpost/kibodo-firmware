/*
 * Copyright (c) 2026 The zmk-battery-monitor-firmware Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Central-side: reads each connected peripheral's side label via GATT and
 * caches it per slot. Calls into battery_hid.c to push a Report ID 2 once
 * the label is known.
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

#include "protocol.h"
#include "metadata.h"

LOG_MODULE_REGISTER(zmk_bm_meta_client, CONFIG_ZMK_BATTERY_MONITOR_LOG_LEVEL);

#define PERIPHERAL_COUNT CONFIG_ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS

/* Declared in ZMK's app/src/split/bluetooth/central.c. Non-static external
 * linkage; not in a public header. */
extern int peripheral_slot_index_for_conn(struct bt_conn *conn);

static struct peripheral_metadata metadata[PERIPHERAL_COUNT];

static struct bt_gatt_read_params label_read_params[PERIPHERAL_COUNT];
static struct bt_conn *pending_conn[PERIPHERAL_COUNT];
static struct k_work_delayable read_work[PERIPHERAL_COUNT];

const struct peripheral_metadata *zmk_bm_get_peripheral_metadata(int slot) {
    if (slot < 0 || slot >= PERIPHERAL_COUNT) {
        return NULL;
    }
    return &metadata[slot];
}

static uint8_t label_read_cb(struct bt_conn *conn, uint8_t err,
                             struct bt_gatt_read_params *params,
                             const void *data, uint16_t length) {
    int slot = peripheral_slot_index_for_conn(conn);
    if (slot < 0 || slot >= PERIPHERAL_COUNT) {
        return BT_GATT_ITER_STOP;
    }

    if (err == 0 && data != NULL && length > 0) {
        size_t copy = MIN(length, ZMK_BM_METADATA_LABEL_MAX - 1);
        memset(metadata[slot].label, 0, sizeof(metadata[slot].label));
        memcpy(metadata[slot].label, data, copy);
        metadata[slot].has_label = true;
        LOG_INF("slot %d label: %s", slot, metadata[slot].label);
        zmk_bm_metadata_changed(slot);
    } else {
        LOG_DBG("slot %d label read err %d (peripheral may not have the module)", slot, err);
    }

    return BT_GATT_ITER_STOP;
}

static void read_work_handler(struct k_work *work) {
    struct k_work_delayable *dw = k_work_delayable_from_work(work);
    int slot = dw - read_work;
    if (slot < 0 || slot >= PERIPHERAL_COUNT) return;

    struct bt_conn *conn = pending_conn[slot];
    if (!conn) return;

    int confirmed = peripheral_slot_index_for_conn(conn);
    if (confirmed != slot) {
        LOG_DBG("slot %d: conn no longer matches (now %d), skipping", slot, confirmed);
        bt_conn_unref(conn);
        pending_conn[slot] = NULL;
        return;
    }

    label_read_params[slot].func = label_read_cb;
    label_read_params[slot].handle_count = 0;
    label_read_params[slot].by_uuid.uuid = ZMK_BM_SIDE_LABEL_UUID;
    label_read_params[slot].by_uuid.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    label_read_params[slot].by_uuid.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;

    int err = bt_gatt_read(conn, &label_read_params[slot]);
    if (err) {
        LOG_DBG("slot %d: bt_gatt_read err %d", slot, err);
    }

    bt_conn_unref(conn);
    pending_conn[slot] = NULL;
}

static void on_connected(struct bt_conn *conn, uint8_t err) {
    if (err) return;

    int slot = peripheral_slot_index_for_conn(conn);
    if (slot < 0 || slot >= PERIPHERAL_COUNT) {
        return;
    }

    if (pending_conn[slot]) {
        bt_conn_unref(pending_conn[slot]);
    }
    pending_conn[slot] = bt_conn_ref(conn);
    k_work_reschedule(&read_work[slot], K_MSEC(1500));
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason) {
    int slot = peripheral_slot_index_for_conn(conn);
    if (slot < 0 || slot >= PERIPHERAL_COUNT) return;

    if (pending_conn[slot]) {
        bt_conn_unref(pending_conn[slot]);
        pending_conn[slot] = NULL;
    }
}

static struct bt_conn_cb conn_callbacks = {
    .connected = on_connected,
    .disconnected = on_disconnected,
};

static int zmk_bm_meta_client_init(void) {
    for (int i = 0; i < PERIPHERAL_COUNT; i++) {
        k_work_init_delayable(&read_work[i], read_work_handler);
        memset(&metadata[i], 0, sizeof(metadata[i]));
    }
    bt_conn_cb_register(&conn_callbacks);
    return 0;
}

SYS_INIT(zmk_bm_meta_client_init, APPLICATION, 95);
