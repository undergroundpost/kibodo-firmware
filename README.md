# zmk-battery-monitor-firmware

ZMK module that bridges a split keyboard to the [zmk-battery-monitor](https://github.com/undergroundpost/zmk-battery-monitor) macOS app.

- **Central (dongle):** exposes a vendor-defined USB HID interface carrying per-peripheral battery levels and metadata.
- **Peripherals (halves):** expose a custom GATT service with per-half side label and (coming soon) charging state.

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
    - name: zmk-battery-monitor-firmware
      remote: undergroundpost
      revision: main
  self:
    path: config
```

### 2. Enable on every device

In a config that applies to all targets (e.g. `config/<keyboard>.conf`):

```
CONFIG_ZMK_BATTERY_MONITOR=y
```

The module auto-selects `_HID` on the central and `_PERIPHERAL` on the halves based on `CONFIG_ZMK_SPLIT_ROLE_CENTRAL`. You do not need to set these manually.

### 3. Label each half (optional but recommended)

Create a per-shield override file in your config directory and set the side label. For a Corne:

`config/corne_left.conf`:
```
CONFIG_ZMK_BATTERY_MONITOR_SIDE_LABEL="Corne Left"
```

`config/corne_right.conf`:
```
CONFIG_ZMK_BATTERY_MONITOR_SIDE_LABEL="Corne Right"
```

If you omit the label, the app shows "Peripheral 0" / "Peripheral 1" instead.

### 4. Build, flash, install the companion app

- Commit and push your config. GitHub Actions builds all firmware targets.
- Flash the dongle **and** both halves with the updated firmware.
- Install [zmk-battery-monitor](https://github.com/undergroundpost/zmk-battery-monitor).

## Options

| Kconfig | Default | Description |
| ------- | ------- | ----------- |
| `CONFIG_ZMK_BATTERY_MONITOR` | `n` | Master switch. Enable on every device. |
| `CONFIG_ZMK_BATTERY_MONITOR_HID` | auto | Central USB HID reporting. Auto-selected on the central. |
| `CONFIG_ZMK_BATTERY_MONITOR_PERIPHERAL` | auto | Peripheral BLE metadata service. Auto-selected on peripherals. |
| `CONFIG_ZMK_BATTERY_MONITOR_SIDE_LABEL` | `""` | Per-half name; set in each half's shield override `.conf`. |
| `CONFIG_ZMK_BATTERY_MONITOR_HID_HEARTBEAT_SEC` | `60` | How often the central resends the HID report as a liveness signal. USB-only, does not affect BLE or peripheral sleep. |

## Report format

**Report ID 1 (battery levels, frequent):**

- Usage Page: `0xFF00`, Usage `0x01`, Report ID `0x01`
- Payload: one byte per peripheral, `0-100` = state-of-charge, `0xFF` = no data yet.

For a 2-peripheral split: `[0x01, left_pct, right_pct]` (3 bytes total).

**Report ID 2 (metadata, 32 bytes per peripheral):**

- Usage Page: `0xFF00`, Usage `0x03`, Report ID `0x02`
- Payload:
  - byte 0: peripheral index
  - byte 1: flags (bit 0: charging, bit 1: voltage_valid, bits 2-7: reserved)
  - bytes 2-3: voltage mV (LE uint16, 0 = unavailable) — reserved for future
  - bytes 4-31: side label, UTF-8, null-terminated, zero-padded

Emitted once per peripheral on label discovery + on each heartbeat + on charging state change.

## Charging detection (peripheral)

Charging state is reported when a half's USB power pin goes high. This requires `CONFIG_ZMK_USB=y` on the peripherals (ZMK's default for peripherals is `n` to save startup time). If USB is not enabled on a half, the charging bit stays `0` regardless of actual state.

To enable, add to your per-half shield override:
```
CONFIG_ZMK_USB=y
```

## Compatibility

If you enable the module only on the central and not on the halves, the app still works — you'll just see generic "Peripheral N" names and no charging/voltage data. The halves remain stock ZMK.

## License

MIT.
