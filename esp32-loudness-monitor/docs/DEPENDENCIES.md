# Dependencies and System Changes

## Detected, not installed by this project

- Arduino IDE 2 with bundled Arduino CLI 1.5.1.
- ESP32 Arduino Core 3.2.0 (ESP-IDF 5.4-based libraries).
- Arduino_GFX 1.5.5, SensorLib 0.3.1, TCA9554 0.1.2, LVGL 8.4.0 and XPowersLib 0.2.9 in the user's Arduino library directory.
- Apple Clang 15 and CMake from Homebrew.

## Downloaded reference

- The official Waveshare `ESP32-S3-Touch-LCD-3.5` repository was cloned with depth 1 into the parent task's `work/waveshare-reference` directory. It is a read-only development reference and is excluded from this repository.

## System changes

- No packages were installed.
- No Arduino core or library was modified.
- No login item, daemon, virtual audio device, system permission or audio route was changed.

## Primary sources

- Waveshare Arduino setup and exact library versions: https://docs.waveshare.com/ESP32-S3-Touch-LCD-3.5/Arduino
- Waveshare hardware resources and schematic: https://docs.waveshare.com/ESP32-S3-Touch-LCD-3.5/Resources-And-Documents
- ESP32 Arduino I2S API: https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2s.html
- ESP32 Arduino USB CDC/DFU: https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/cdc_dfu_flash.html
- Espressif USB Audio device driver (direct-UAC reference): https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_device/usb_device_uac.html
- Apple Core Audio Process Tap sample: https://developer.apple.com/documentation/coreaudio/capturing-system-audio-with-core-audio-taps
- Microsoft WASAPI Loopback: https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording

