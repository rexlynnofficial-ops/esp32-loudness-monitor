# Loudness Monitor Wire Protocol 1.0

All multi-byte integers and IEEE-754 floats are little-endian. Each message is:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | ASCII `LMV1` |
| 4 | 1 | major version (`1`) |
| 5 | 1 | message type |
| 6 | 2 | flags |
| 8 | 2 | payload bytes (maximum 4096) |
| 10 | 2 | reserved, zero |
| 12 | 4 | sequence number |
| 16 | 8 | monotonic timestamp in microseconds |
| 24 | N | payload |
| 24+N | 4 | CRC-32/ISO-HDLC over header and payload |

Message types:

- `HELLO` (1): sender capabilities and requested engine.
- `CAPABILITIES` (2): device limits and selected transport.
- `PCM_S16` (3): interleaved signed 16-bit PCM. Payload starts with sample rate `u32`, channel count `u8`, channel map `u8`, frames `u16`, then PCM.
- `OBSERVATION` (4): 100 ms hybrid block containing weighted mean-square energy, sample peak, true peak and peak-channel index.
- `CONTROL` (5): reset, pause, resume, target, preset and mode operations.
- `STATUS` (6): firmware status, counters and current meter snapshot.
- `PING` / `PONG` (7/8): liveness and latency measurement.

Unknown message types are ignored after CRC validation. Version-major mismatches are rejected. Receivers resynchronize by scanning for `LMV1`, enforce the payload limit before allocation and count corrupt frames.

