/*
 * Copyright (c) 2026 The zmk-battery-monitor-firmware Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>

#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>

#include <zephyr/logging/log.h>

#include <zmk/usb.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>

#include "protocol.h"
#include "metadata.h"

LOG_MODULE_REGISTER(zmk_battery_monitor_hid, CONFIG_ZMK_BATTERY_MONITOR_LOG_LEVEL);

#define PERIPHERAL_COUNT CONFIG_ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS
#define PERCENT_UNKNOWN  0xFF

#define BATT_REPORT_LEN  (1 + PERIPHERAL_COUNT)
#define META_REPORT_LEN  (1 + ZMK_BM_METADATA_REPORT_SIZE)

/*
 * HID descriptor: one Application Collection carrying two distinct reports.
 *   Report ID 1: battery levels (1 byte per peripheral)
 *   Report ID 2: metadata (32 bytes fixed, first byte is peripheral index)
 */
static const uint8_t hid_report_desc[] = {
    0x06, 0x00, 0xFF,       /* Usage Page (Vendor 0xFF00) */
    0x09, 0x01,             /* Usage (0x01) */
    0xA1, 0x01,             /* Collection (Application) */

    /* Report 1: battery levels */
    0x85, ZMK_BM_BATTERY_REPORT_ID,
    0x09, 0x02,             /*   Usage (0x02) */
    0x15, 0x00,             /*   Logical Min 0 */
    0x26, 0xFF, 0x00,       /*   Logical Max 255 */
    0x75, 0x08,             /*   Report Size 8 */
    0x95, PERIPHERAL_COUNT, /*   Report Count (N peripherals) */
    0x81, 0x02,             /*   Input (Data, Var, Abs) */

    /* Report 2: metadata */
    0x85, ZMK_BM_METADATA_REPORT_ID,
    0x09, 0x03,             /*   Usage (0x03) */
    0x75, 0x08,             /*   Report Size 8 */
    0x95, ZMK_BM_METADATA_REPORT_SIZE,
    0x81, 0x02,             /*   Input (Data, Var, Abs) */

    0xC0,                   /* End Collection */
};

static uint8_t peripheral_percent[PERIPHERAL_COUNT];
static const struct device *hid_dev;
static K_SEM_DEFINE(hid_sem, 1, 1);
static struct k_work_delayable heartbeat_work;

static void in_ready_cb(const struct device *dev) {
    k_sem_give(&hid_sem);
}

static const struct hid_ops ops = {
    .int_in_ready = in_ready_cb,
};

static int write_report(const uint8_t *buf, size_t len) {
    if (hid_dev == NULL) {
        return -ENODEV;
    }

    switch (zmk_usb_get_status()) {
    case USB_DC_SUSPEND:
        /* Host asleep. Do not transmit — we do not want to wake the host. */
        return 0;
    case USB_DC_ERROR:
    case USB_DC_RESET:
    case USB_DC_DISCONNECTED:
    case USB_DC_UNKNOWN:
        return -ENODEV;
    default: {
        int ret = k_sem_take(&hid_sem, K_MSEC(30));
        if (ret) {
            LOG_DBG("sem take failed: %d", ret);
            return ret;
        }
        int err = hid_int_ep_write(hid_dev, buf, len, NULL);
        if (err) {
            k_sem_give(&hid_sem);
            LOG_DBG("hid_int_ep_write failed: %d", err);
        }
        return err;
    }
    }
}

static int push_battery_report(void) {
    uint8_t report[BATT_REPORT_LEN];
    report[0] = ZMK_BM_BATTERY_REPORT_ID;
    for (int i = 0; i < PERIPHERAL_COUNT; i++) {
        report[1 + i] = peripheral_percent[i];
    }
    return write_report(report, sizeof(report));
}

static int push_metadata_report(int slot) {
    if (slot < 0 || slot >= PERIPHERAL_COUNT) {
        return -EINVAL;
    }
    const struct peripheral_metadata *meta = zmk_bm_get_peripheral_metadata(slot);
    if (!meta || !meta->has_label) {
        return -ENODEV;
    }

    uint8_t report[META_REPORT_LEN];
    memset(report, 0, sizeof(report));
    report[0] = ZMK_BM_METADATA_REPORT_ID;
    report[1] = (uint8_t)slot;

    size_t copy = strnlen(meta->label, ZMK_BM_METADATA_LABEL_MAX - 1);
    memcpy(&report[1 + ZMK_BM_METADATA_LABEL_OFFSET], meta->label, copy);
    /* Remainder already zeroed. */

    return write_report(report, sizeof(report));
}

void zmk_bm_metadata_changed(int slot) {
    push_metadata_report(slot);
}

static int battery_listener_cb(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev =
        as_zmk_peripheral_battery_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (ev->source < PERIPHERAL_COUNT) {
        peripheral_percent[ev->source] = ev->state_of_charge;
        push_battery_report();
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(zmk_battery_monitor_hid, battery_listener_cb);
ZMK_SUBSCRIPTION(zmk_battery_monitor_hid, zmk_peripheral_battery_state_changed);

static void heartbeat_handler(struct k_work *work) {
    push_battery_report();
    /* Opportunistically resend metadata we know about. Safe because
     * Report 2 is idempotent and labels rarely change. */
    for (int i = 0; i < PERIPHERAL_COUNT; i++) {
        const struct peripheral_metadata *meta = zmk_bm_get_peripheral_metadata(i);
        if (meta && meta->has_label) {
            push_metadata_report(i);
        }
    }
    k_work_reschedule(&heartbeat_work,
                      K_SECONDS(CONFIG_ZMK_BATTERY_MONITOR_HID_HEARTBEAT_SEC));
}

static int zmk_battery_monitor_hid_init(void) {
    for (int i = 0; i < PERIPHERAL_COUNT; i++) {
        peripheral_percent[i] = PERCENT_UNKNOWN;
    }

    hid_dev = device_get_binding("HID_1");
    if (hid_dev == NULL) {
        LOG_ERR("Unable to locate HID_1 device");
        return -EINVAL;
    }

    usb_hid_register_device(hid_dev, hid_report_desc, sizeof(hid_report_desc), &ops);

    int err = usb_hid_init(hid_dev);
    if (err) {
        LOG_ERR("Failed to init HID_1: %d", err);
        return err;
    }

    k_work_init_delayable(&heartbeat_work, heartbeat_handler);
    k_work_reschedule(&heartbeat_work,
                      K_SECONDS(CONFIG_ZMK_BATTERY_MONITOR_HID_HEARTBEAT_SEC));

    LOG_INF("Battery monitor HID initialized (%d peripherals)", PERIPHERAL_COUNT);
    return 0;
}

SYS_INIT(zmk_battery_monitor_hid_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
