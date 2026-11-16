# ADR-0002: Keep meter state on ESP32 in Hybrid mode

## Status

Accepted

## Date

2026-08-21

## Context

ESP32-S3 Full-Speed USB and CPU budget cannot carry and process 12 channels of 48 kHz/24-bit PCM together with the display workload. The product must nevertheless preserve the 7.1.4 architecture rather than silently collapse it to stereo.

## Decision

- Mono/stereo: Helper sends PCM; ESP32 performs all measurement.
- More than two channels: Helper performs per-channel unpacking, 48 kHz K-weighting, LFE exclusion and block peak observation. It sends a 100 ms `OBSERVATION`; ESP32 owns M/S/I, relative gating, LRA, reset, target, history and UI.
- The protocol carries channel count and peak-channel index up to 12 channels.

## Alternatives Considered

### Send 12-channel PCM over USB CDC

Rejected because bandwidth and scheduling margin are not robust on Full-Speed USB while rendering the UI.

### Send final LUFS values from Helper

Rejected because session semantics would move to the desktop and the device would no longer be the meter engine.

## Consequences

- A preserved-channel Core Audio device stream or multichannel Windows render endpoint is required for real 7.1.4 input.
- V1 hybrid true-peak transport currently reports sample peak in the true-peak field; the protocol and firmware UI are ready for the Helper's certified oversampler upgrade.

