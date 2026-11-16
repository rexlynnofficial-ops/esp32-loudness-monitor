# Capability Map: Loudness Monitor V1

| Module id | Responsibility | Depends on |
|---|---|---|
| `wire-protocol` | Versioned, CRC-protected ESP32/Helper transport contract | — |
| `loudness-core` | BS.1770 K-weighting, M/S/I, LRA, peak and true-peak state | `wire-protocol` |
| `waveshare-bsp` | LCD, touch, ES8311 microphone, NVS and USB serial transport | `wire-protocol` |
| `meter-ui` | Main meter, 60 s graph, mic/RTA/settings pages and touch flows | `loudness-core`, `waveshare-bsp` |
| `desktop-helper` | macOS Core Audio Tap and Windows WASAPI capture, device negotiation and transport | `wire-protocol` |
| `system-validation` | Host tests, Arduino build/flash checks and hardware test records | all modules |

Build order: `wire-protocol` → `loudness-core` → `waveshare-bsp` → `meter-ui` → `desktop-helper` → `system-validation`.

