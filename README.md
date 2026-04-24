# kibodo-firmware

ZMK module that bridges a split keyboard to the [Kibodo](https://github.com/undergroundpost/kibodo) macOS app.

- **Central (dongle):** exposes a vendor-defined USB HID interface carrying per-peripheral battery levels, per-peripheral side labels, and the active layer (index + name).
- **Peripherals (halves):** expose a single-characteristic GATT service advertising this half's side label. No other per-peripheral traffic.

## Requirements

- A ZMK config with a central dongle, i.e. `CONFIG_ZMK_SPLIT=y` + `CONFIG_ZMK_SPLIT_ROLE_CENTRAL=y` + `CONFIG_ZMK_USB=y` on the dongle.
- Split BLE central battery fetching enabled: `CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING=y` on the dongle.

## Installation

### 1. Add the module to `west.yml`

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: undergroundpost
      url-base: https://github.com/undergroundpost
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    - name: kibodo-firmware
      remote: undergroundpost
      revision: main
  self:
    path: config
```

### 2. Enable on every device

In a config that applies to all targets (e.g. `config/<keyboard>.conf`):

```
CONFIG_KIBODO=y
```

The module auto-selects `_HID` on the central and `_PERIPHERAL` on the halves based on `CONFIG_ZMK_SPLIT_ROLE_CENTRAL`. You do not need to set these manually.

### 3. Label each half (optional but recommended)

Create a per-shield override file in your config directory and set the side label. For a Corne:

`config/corne_left.conf`:
```
CONFIG_KIBODO_SIDE_LABEL="Corne Left"
```

`config/corne_right.conf`:
```
CONFIG_KIBODO_SIDE_LABEL="Corne Right"
```

If you omit the label, the app shows "Peripheral 0" / "Peripheral 1" instead.

### 4. Build, flash, install the companion app

- Commit and push your config. GitHub Actions builds all firmware targets.
- Flash the dongle **and** both halves with the updated firmware.
- Install [Kibodo](https://github.com/undergroundpost/kibodo).

## Options

| Kconfig | Default | Description |
| ------- | ------- | ----------- |
| `CONFIG_KIBODO` | `n` | Master switch. Enable on every device. |
| `CONFIG_KIBODO_HID` | auto | Central USB HID reporting. Auto-selected on the central. |
| `CONFIG_KIBODO_PERIPHERAL` | auto | Peripheral BLE metadata service. Auto-selected on peripherals. |
| `CONFIG_KIBODO_LAYER` | `y` | Include active layer + layer names in the HID report stream. |
| `CONFIG_KIBODO_SIDE_LABEL` | `""` | Per-half name; set in each half's shield override `.conf`. |
| `CONFIG_KIBODO_HID_HEARTBEAT_SEC` | `60` | How often the central resends the HID report as a liveness signal. USB-only, does not affect BLE or peripheral sleep. |

## Report format

**Report ID 1 (battery levels):**

- Usage Page: `0xFF00`, Usage `0x01`, Report ID `0x01`
- Payload: one byte per peripheral, `0-100` = state-of-charge, `0xFF` = no data yet.

For a 2-peripheral split: `[0x01, left_pct, right_pct]` (3 bytes total).

Emitted whenever ZMK reports a peripheral battery change, and on each heartbeat.

**Report ID 2 (peripheral metadata, 32 bytes):**

- Usage Page: `0xFF00`, Usage `0x03`, Report ID `0x02`
- Payload:
  - byte 0: peripheral index
  - bytes 1-31: side label, UTF-8, null-terminated, zero-padded

Emitted once per peripheral when the label is read over BLE, and on each heartbeat.

**Report ID 3 (active layer, 1 byte):**

- Usage Page: `0xFF00`, Usage `0x04`, Report ID `0x03`
- Payload: highest active layer index (matches ZMK's on-dongle layer display).

Emitted when the resolved top layer changes, and on each heartbeat. Nested momentary layers that don't change the visible top layer are suppressed.

**Report ID 4 (layer metadata, 32 bytes):**

- Usage Page: `0xFF00`, Usage `0x05`, Report ID `0x04`
- Payload:
  - byte 0: layer index
  - bytes 1-31: layer label, UTF-8, null-terminated, zero-padded

Emitted on each heartbeat for every labeled layer. Unlabeled layers are skipped; the app falls back to "Layer N".

## Compatibility

If you enable the module only on the central and not on the halves, the app still works — you'll just see generic "Peripheral N" names. The halves remain stock ZMK.

## License

MIT.
