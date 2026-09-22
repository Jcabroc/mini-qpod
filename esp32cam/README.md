# ESP32-CAM USB camera diagnostic

Hardware: ESP32-D0WD-V3 rev 3.1, 4 MB flash, ESP32-CAM-MB CH340,
OV2640 camera using the AI Thinker pin mapping.

Windows serial port: COM25. Working CH340 driver: 3.7.2022.1.

Build with `./build-test.ps1`. ESP-IDF 5.4.4 is installed at
`C:/Espressif/v5.4.4/esp-idf`. Sources are copied to an ASCII-only path
because the installed Kconfig tooling mishandles the accented user path.
Camera driver commit: ee087821fb6da4dc9f294747e61c936a7169c9a7.

The test initializes PSRAM and OV2640, warms up exposure, then sends a VGA
JPEG as hexadecimal text every five seconds over the 115200-baud UART.
It does not configure Wi-Fi. Run `capture.py` using the Espressif Python
environment to save one frame under `results/`.

Original firmware backup: `backups/original-4MB.bin` (only usable after the
read completes successfully). Keep this file private; firmware can contain
saved configuration. Test binaries are under `firmware/`.

Flash test (after a successful backup), with `python -m esptool`:

```
--port COM25 --baud 115200 write_flash --flash_mode dio --flash_freq 40m --flash_size 4MB 0x1000 firmware/bootloader.bin 0x8000 firmware/partition-table.bin 0x10000 firmware/cam_usb_test.bin
```

Restore original with `python -m esptool --port COM25 --baud 115200 write_flash 0 backups/original-4MB.bin`.

Validated on 2026-09-22: full original backup verified by device MD5; test flash verified; PSRAM 8388608 bytes; OV2640 PID 0x26 initialized successfully; captured and visually inspected a 640x480 JPEG in results/camera.jpg.
