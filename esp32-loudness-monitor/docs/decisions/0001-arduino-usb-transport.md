# ADR-0001: Use Arduino Core with CDC transport for V1

## Status

Accepted

## Date

2026-08-21

## Context

The target computer already has Arduino IDE, ESP32 Arduino Core 3.2.0 and the Waveshare libraries. The product also needs a single USB cable, desktop system-mix capture without changing the user's listening output, and ESP32-owned stereo DSP.

ESP32-S3 can implement UAC using Espressif's `esp_device_uac`, but the Arduino Core 3.2.0 prebuilt TinyUSB library has `CFG_TUD_AUDIO=0`; the audio class implementation and endpoints are not linked. Enabling it from a sketch define cannot rebuild that static library. A direct-UAC Arduino sketch would therefore require a custom Core build or an ESP-IDF project with Arduino as a component.

## Decision

The normal V1 Arduino build uses the ESP32-S3 hardware USB CDC/JTAG serial function. The Helper captures the system mix and transports signed 16-bit stereo PCM. The ESP32 performs K-weighting, gating, LUFS and true-peak processing. Multichannel capture switches to a compact observation packet.

The firmware input boundary remains independent of CDC so a future custom-Core UAC input can feed the same DSP without replacing UI or session logic.

## Alternatives Considered

### Claim UAC support from TinyUSB headers

Rejected because headers are present but the audio class is compiled out of the shipped Arduino static library.

### Switch back to pure ESP-IDF

Rejected for this deliverable because the user explicitly chose the installed Arduino environment. Espressif's UAC component remains the recommended direct-UAC route for a later variant.

### Compute all values on the desktop

Rejected because it would turn the ESP32 into a display and violate the local-meter product boundary.

## Consequences

- The Helper is required to capture an unchanged system mix, even for stereo; its stereo role is capture and transport, not loudness computation.
- Direct routing to the ESP32 as a USB sound card is not provided by this Arduino build.
- One cable still provides power, flashing, control and audio-data transport.

