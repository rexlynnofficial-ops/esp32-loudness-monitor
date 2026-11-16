# Spec: Loudness Monitor V1

## Objective

Build a usable desktop loudness monitor for the Waveshare ESP32-S3 Touch LCD 3.5. The device owns measurement state and UI. A lightweight desktop Helper captures an unchanged system mix and transports stereo PCM to the device; when channel count or sample rate exceeds the local budget, the Helper sends weighted block-energy and true-peak observations while the ESP32 retains M/S/I, LRA, target and session state.

## Tech Stack

- Firmware: Arduino ESP32 Core 3.2.0, C++17, LVGL 8.4.0, Arduino_GFX 1.5.5, SensorLib 0.3.1, TCA9554 0.1.2 and the Waveshare ES8311 driver.
- Hardware: Waveshare ESP32-S3 Touch LCD 3.5, ESP32-S3 N8R8, ST7796S 320x480 rotated to 480x320, FT6336U/FT6X36 touch and ES8311 codec.
- Helper: C++17 with CMake; Core Audio Process Tap on macOS 14.2+ and WASAPI Loopback on Windows 10+.
- Tests: dependency-free host C++ tests using CTest, plus Arduino CLI compile/upload and scripted protocol loopback.

## Commands

```sh
cmake -S . -B build -G "Unix Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
./tools/build_firmware.sh
./tools/build_helper.sh
./tools/flash_firmware.sh /dev/cu.usbmodem14101
```

## Project Structure

```text
firmware/LoudnessMonitor/  Arduino sketch and recursively compiled source
helper/                    Native macOS/Windows capture application
protocol/                  Stable binary protocol specification
tests/                     Host-side DSP and protocol tests
tools/                     Reproducible build, flash and diagnostics scripts
docs/                      Architecture, setup, dependency and test records
```

## Code Style

```cpp
struct MeterSnapshot {
  float momentary_lufs = -INFINITY;
  float short_term_lufs = -INFINITY;
  float integrated_lufs = -INFINITY;
};

bool decode_frame(std::span<const uint8_t> bytes, Frame& output);
```

Use `snake_case` for functions and local variables, `PascalCase` for types, explicit-width integers on the wire, RAII on desktop, no allocation in audio callbacks, and no Arduino dependency in the DSP/protocol core.

## Testing Strategy

- Unit tests validate protocol framing/CRC, K-weight filter response, gate/session behavior, target presets, history rollover, true-peak interpolation and octave-band mapping.
- Integration tests feed deterministic PCM through the same pipeline used by firmware and validate Helper-to-device frames.
- Arduino CLI compile is a required firmware gate.
- Hardware checks record flash success, boot log, display, touch, ES8311 mic samples and USB transport. A missing physical observation is recorded as `NOT RUN`, never inferred from compilation.
- macOS Helper is run against a real Core Audio capture source; Windows code is compiled on Windows CI or recorded as not verified locally.

## Boundaries

- Always: validate every external frame, keep audio callbacks allocation-free, run tests before commits, preserve source citations and record exact tool versions.
- Ask first: install or upgrade Arduino libraries/cores, enable macOS audio-capture permission, configure login items, or change system audio routing.
- Never: silently install a service, bypass OS privacy controls, overwrite user Arduino libraries, modify system files, claim hardware validation without evidence, or send 7.1.4 raw PCM over ESP32-S3 Full-Speed USB.

## Success Criteria

- Arduino sketch builds for `esp32:esp32:esp32s3` with 16 MB flash and OPI PSRAM.
- Device UI contains the main LUFS screen, 60 s momentary history, target adjustment, reset, USB/Mic choice, Mic SPL, 1/3-octave framework, device info and calibration storage.
- Local pipeline reports LUFS-M/S/I, LRA, sample peak and a documented 4x true-peak estimate from mono/stereo PCM.
- Helper protocol negotiates capabilities and supports stereo PCM plus multichannel hybrid observations without changing the protocol version.
- macOS Helper captures outgoing audio with Core Audio Process Tap without muting the normal output, reconnects to the ESP32 transport, and reports actionable errors.
- Windows Helper contains a buildable WASAPI shared-mode loopback and COM-port prototype; runtime verification is a Windows release gate.
- README documents build, flash, run, permissions, limitations and the exact validation state.

## Open Questions Resolved by Safe Defaults

- Display orientation defaults to landscape (480x320).
- Meter sample rate defaults to 48 kHz; 44.1 kHz is accepted by Helper and resampled/packetized to 48 kHz before local DSP.
- Stereo PCM uses signed little-endian 16-bit samples to fit a robust USB CDC transport budget.
- Silence session reset defaults to 30 s and remains user-configurable.
