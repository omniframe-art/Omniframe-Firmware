# Omniframe Firmware

Firmware for the Omni e-paper frame built on ESP-IDF (target `esp32s3`) using a modified `epdiy` driver stack. The device wakes periodically or on button press, syncs with a backend, fetches images, shows them on a 1600x1200 panel, and returns to deep sleep to save power.

## Highlights
- ESP-IDF 5.x project (`idf.py` workflow) with OTA partitions (`ota_0`, `ota_1`) and a dedicated `images` data partition for downloaded content.
- E-paper rendering via `components/epdiy` with custom waveform loading from the server.
- Wi‑Fi STA with credentials cached in NVS (defaults set in `main/network/wifi.h`).
- Backend sync (`main/network/api.c`): posts device status, fetches image queue, waveforms, and firmware update metadata.
- Image pipeline (`main/display` + `main/network/download.c`): queue kept in NVS, downloads to flash image slots or SD card, displays first item.
- Power features: battery/solar voltage sampling, SD card power control, deep-sleep wake on button or timer.

## Repository Layout
- `main/` – application sources
  - `main.c` – boot flow, sleep scheduling, setup of Wi‑Fi, NVS, display, queue sync.
  - `display/` – image queue management, flash/SD image access, text rendering.
  - `network/` – Wi‑Fi station, REST API client, OTA update, image download.
  - `drivers/` – TPS power IC, buttons, SD card, ADC measurements.
  - `config.*` – board pinout (ESP32-S3), display config, epdiy renderer init.
- `components/epdiy/` – bundled epdiy driver.
- `partitions.csv` – NVS, factory, OTA slots, FAT `storage`, and `images` partition.
- `scripts/fonts/` – bitmap fonts and `font_processor.py` to regenerate `display/text` font data.

## Prerequisites
- ESP-IDF 5.2.x (or newer per `main/idf_component.yml`).
- ESP32-S3 toolchain available on PATH (`idf.py --version` should work).
- Python for ESP-IDF and optional font script (`scripts/fonts/font_processor.py`).

## Build and Flash
```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor   # adjust port
```
- `partitions.csv` is enforced via `CMakeLists.txt`.
- Use `idf.py monitor` (or the combined `flash monitor`) for logs.

## Configuration
- Wi‑Fi: edit defaults in `main/network/wifi.h` or save credentials to NVS at runtime (they persist).
- Backend: API base URL is `BASE_URL` in `main/network/api.h`.
- Device identity: `SERIAL_NUMBER` and `VCOM` are read from NVS (see `config.c`); set them beforehand with your provisioning flow.
- Image queue: size 4, filenames stored in NVS (`main/display/image_queue.*`). First queue item is displayed after sync.
- Storage: images saved either to the `images` partition or `/sdcard/image_files/` when SD is mounted/enabled.

## Runtime Flow
1) Boot/wake → init NVS, buttons, power rails, ADC, SD, Wi‑Fi, and epdiy renderer.
2) Sync with server → send device info and voltages, fetch queue + waveform.
3) Download any missing images → store to flash/SD, display first queue entry.
4) Enter deep sleep → wake via button or timer (10-minute timer configured in `main.c`).

## Font Assets
- Character bitmaps live in `scripts/fonts/font_bitmaps/`.
- Regenerate the packed C header used by `display/text.c`:
  ```bash
  cd scripts/fonts
  python font_processor.py
  ```

## Notes
- Default Wi‑Fi SSID/password are placeholders; change before shipping.
- OTA uses the dual-slot layout from `partitions.csv`; ensure backend serves compatible firmware.
