# zmk-battery-monitor-firmware

ZMK module that adds a vendor-defined USB HID interface to a split-keyboard dongle, reporting per-peripheral battery levels to the host. Pairs with [zmk-battery-monitor](https://github.com/undergroundpost/zmk-battery-monitor) on macOS.

## Requirements

- A ZMK config with a dongle (central) USB peripheral, i.e. `CONFIG_ZMK_SPLIT=y`, `CONFIG_ZMK_SPLIT_ROLE_CENTRAL=y`, `CONFIG_ZMK_USB=y`.
- Split BLE central battery fetching enabled: `CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING=y` on the dongle, so the dongle actually knows each peripheral's level.

## Installation

### 1. Add the module to `west.yml`

In your ZMK config repo, edit `config/west.yml`:

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

### 2. Enable the module in your dongle shield's `.conf`

In your dongle shield's configuration (e.g. `config/boards/shields/<dongle>/<dongle>.conf`):

```
CONFIG_ZMK_BATTERY_MONITOR_HID=y
CONFIG_USB_HID_DEVICE_COUNT=2
```

`CONFIG_USB_HID_DEVICE_COUNT=2` is set as a default by this module, but listing it explicitly is harmless and makes the dependency visible.

### 3. Build, flash, install the companion app

- Commit and push your config. GitHub Actions builds the firmware.
- Flash the updated dongle firmware.
- Install [zmk-battery-monitor](https://github.com/undergroundpost/zmk-battery-monitor) on macOS.

## Report format

A single vendor-defined HID input report on a secondary HID interface:

- **Report ID**: `0x01`
- **Usage Page**: `0xFF00` (vendor-defined)
- **Payload**: one byte per peripheral, state-of-charge `0-100`, or `0xFF` if the dongle has not yet received a reading for that peripheral.

For a 2-peripheral split, the full input report including ID is 3 bytes: `[0x01, left_pct, right_pct]`.

## Options

| Kconfig | Default | Description |
| ------- | ------- | ----------- |
| `CONFIG_ZMK_BATTERY_MONITOR_HID_HEARTBEAT_SEC` | `60` | How often to resend the current report as a liveness signal. Does not touch BLE, does not prevent peripheral sleep. |

## License

MIT.
