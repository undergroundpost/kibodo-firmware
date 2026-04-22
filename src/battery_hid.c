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

LOG_MODULE_REGISTER(zmk_battery_monitor_hid, CONFIG_ZMK_BATTERY_MONITOR_HID_LOG_LEVEL);

#define PERIPHERAL_COUNT CONFIG_ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS
#define REPORT_ID        0x01
#define PERCENT_UNKNOWN  0xFF
#define REPORT_LEN       (1 + PERIPHERAL_COUNT)

static const uint8_t hid_report_desc[] = {
    0x06, 0x00, 0xFF,       // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01,             // Usage (Vendor 0x01)
    0xA1, 0x01,             // Collection (Application)
    0x85, REPORT_ID,        //   Report ID (1)
    0x09, 0x02,             //   Usage (Vendor 0x02)
    0x15, 0x00,             //   Logical Minimum (0)
    0x26, 0xFF, 0x00,       //   Logical Maximum (255)
    0x75, 0x08,             //   Report Size (8 bits)
    0x95, PERIPHERAL_COUNT, //   Report Count (per-peripheral bytes)
    0x81, 0x02,             //   Input (Data, Variable, Absolute)
    0xC0,                   // End Collection
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

static int push_report(void) {
    if (hid_dev == NULL) {
        return -ENODEV;
    }

    uint8_t report[REPORT_LEN];
    report[0] = REPORT_ID;
    for (int i = 0; i < PERIPHERAL_COUNT; i++) {
        report[1 + i] = peripheral_percent[i];
    }

    switch (zmk_usb_get_status()) {
    case USB_DC_SUSPEND:
        return usb_wakeup_request();
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
        int err = hid_int_ep_write(hid_dev, report, sizeof(report), NULL);
        if (err) {
            k_sem_give(&hid_sem);
            LOG_DBG("hid_int_ep_write failed: %d", err);
        }
        return err;
    }
    }
}

static int battery_listener_cb(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev =
        as_zmk_peripheral_battery_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (ev->source < PERIPHERAL_COUNT) {
        peripheral_percent[ev->source] = ev->state_of_charge;
        push_report();
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(zmk_battery_monitor_hid, battery_listener_cb);
ZMK_SUBSCRIPTION(zmk_battery_monitor_hid, zmk_peripheral_battery_state_changed);

static void heartbeat_handler(struct k_work *work) {
    push_report();
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
