# Changelog

## [0.1.0] - 2026-08-21

### Added

- Arduino firmware for Waveshare ESP32-S3 Touch LCD 3.5 with loudness, Mic SPL, octave, settings and device-info pages.
- Portable BS.1770-style loudness core, gating, LRA, sample peak and 4x true-peak architecture.
- CRC-protected versioned Hybrid protocol and USB CDC runtime.
- macOS Core Audio Process Tap Helper and Windows WASAPI/COM-port prototype.
- Deterministic host tests, Arduino build/flash scripts and hardware validation record.

### Fixed

- Stop clearing the complete LCD every 100 ms; steady-state refreshes now repaint only dynamic regions, eliminating full-screen flicker.
- Replace the 1/3-octave bar visualization with a chronological 240-point waveform trace.
- Add persistent waveform height control and a dedicated USB / Helper status page with touch mode selection.
- Increase numeric meter refresh from 10 Hz to approximately 30 Hz while throttling graph redraws to 10 Hz.
- Update the loudness core at 50 Hz (20 ms blocks) and add a dedicated Mic Peak reset control.
- Label waveform amplitude explicitly as linear FS with a 0 dBFS reference.
- Add persistent Computer Input Trim for aligning a system Tap reference with a DAW meter reference.
- Add a double-clickable macOS `LoudnessBridge.command` launcher with automatic port discovery and first-run build.
