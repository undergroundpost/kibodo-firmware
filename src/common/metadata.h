/*
 * Copyright (c) 2026 The zmk-battery-monitor-firmware Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Shared interface between central-side metadata_client.c and battery_hid.c.
 */

#pragma once

#include <stdbool.h>
#include "protocol.h"

struct peripheral_metadata {
    char label[ZMK_BM_METADATA_LABEL_MAX];
    bool has_label;
};

/* Provided by metadata_client.c. NULL if slot is out of range. */
const struct peripheral_metadata *zmk_bm_get_peripheral_metadata(int slot);

/* Provided by battery_hid.c. Called whenever a peripheral's metadata changes
 * (e.g. new label). Triggers a Report ID 2 push to the host. */
void zmk_bm_metadata_changed(int slot);
