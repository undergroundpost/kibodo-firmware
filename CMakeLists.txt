zephyr_library()

zephyr_library_sources_ifdef(CONFIG_ZMK_BATTERY_MONITOR_HID src/battery_hid.c)
zephyr_library_include_directories(${APPLICATION_SOURCE_DIR}/include)
