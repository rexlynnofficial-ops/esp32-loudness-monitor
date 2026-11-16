# V1 Validation Record

Date: 2026-08-21  
Host: Apple Silicon macOS  
Target observed by esptool: ESP32-S3 rev 0.2, 8 MB embedded PSRAM, 40 MHz crystal

| Check | Result | Evidence |
|---|---|---|
| Host protocol tests | PASS | Valid, fragmented, concatenated, corrupt and resynchronized frames. |
| Host DSP tests | PASS | Silence, tone M/S/I, TP ≥ sample peak, reset, presets, 60 s rollover and 1 kHz octave mapping. |
| Helper self-test | PASS | PCM conversion, CRC round-trip, 44.1→48 kHz cadence and 12-channel observation shape. |
| Arduino firmware compile | PASS | 484,222 bytes flash (15% of selected 3 MB app), 24,436 bytes static RAM (7%). |
| Firmware upload | PASS | esptool wrote bootloader, partitions, boot app and firmware; every hash verified. |
| USB reconnect | PASS | `/dev/cu.usbmodem14101` reappeared after hard reset. |
| Helper/ESP32 handshake | PASS | Firmware returned `CAPABILITIES` after `HELLO`. |
| Core Audio system capture | PASS | Process Tap started; permission was already available and normal output stayed active. |
| End-to-end meter | PASS | Playing `/System/Library/Sounds/Glass.aiff` changed ESP32-returned M/S/I/TP from silence to finite values. |
| Packet queue under short final test | PASS | 0 dropped blocks during the short smoke run. |
| Short Helper capture smoke test | PASS | ~10 s live capture, 0 dropped blocks, capabilities and hardware status returned. |
| Sustained Helper capture | FAIL / OPEN | Longer continuous capture accumulated dropped blocks (up to 189 observed); sustained CDC throughput/backpressure still needs resolution. |
| LCD driver initialization | PASS | Firmware hardware status bit returned `LCD OK`. |
| Touch-controller initialization | PASS | FT6X36 status returned `Touch OK`. |
| ES8311/I2S microphone | PASS | Status returned `Mic OK`; live SPL varied approximately 41–52 dB. |
| Visible LCD layout | NOT RUN | No camera or direct human visual observation was available to Codex. |
| Physical touch coordinates/actions | NOT RUN | Controller initialized; no physical press was observed. |
| Calibrated SPL accuracy | NOT RUN | Requires a 94 dB / 1 kHz acoustic calibrator. |
| Preserved-channel 7.1.4 capture | NOT RUN | No 12-channel Core Audio/WASAPI endpoint was available in this test. |
| Windows build and capture | NOT RUN | Windows source is included; no Windows toolchain/runtime was available on this Mac. |
| Hybrid observation timing | PARTIAL | Firmware now aggregates observations into 100 ms measurement windows; no dedicated 12-channel hardware source was available. |

## Real test commands

```sh
./tools/verify_all.sh
./tools/flash_firmware.sh /dev/cu.usbmodem14101
./build-host/helper/loudness-bridge \
  --device /dev/cu.usbmodem14101 --select-computer
afplay /System/Library/Sounds/Glass.aiff
./build-host/helper/loudness-bridge \
  --device /dev/cu.usbmodem14101 --select-mic
```
