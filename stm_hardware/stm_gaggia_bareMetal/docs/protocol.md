# Gaggia link protocol (STM32 ↔ ESP32)

Binary framing over UART, shared by both MCUs. Implementation: [`common/gaggia_proto.c`](../../../common/gaggia_proto.c),
[`common/gaggia_proto.h`](../../../common/gaggia_proto.h) (a single shared file for ESP32 and STM32). On the STM32 side
the snapshot is converted to a telemetry message by [`tlm_adapter.c`](../Core/Src/tlm_adapter.c) and sent by
[`esp_link.c`](../Core/Src/esp_link.c) (DMA TX). Source data: [`system_snapshot.h`](../Core/Inc/system_snapshot.h).

- Byte order: **little-endian**, all payload structs are `packed`.
- Protocol version: `PROTO_VERSION = 1` (sent in `MSG_HELLO`, not in every frame).
- UART speed: `PROTO_BAUD = 921600`.
- Telemetry period: `PROTO_TELEMETRY_PERIOD_MS = 50`. Link timeout: `PROTO_LINK_TIMEOUT_MS = 500`.
- CRC-16/CCITT-FALSE (poly `0x1021`, init `0xFFFF`, no reflection, no final XOR), computed over
  `LEN, TYPE, SEQ, PAYLOAD`. The start bytes are not covered. Check value: `"123456789"` → `0x29B1`.

## Frame

| Offset | Size | Field | Meaning |
|---:|---:|---|---|
| 0 | 1 | SOF0 | `0xAA` |
| 1 | 1 | SOF1 | `0x55` |
| 2 | 1 | LEN | payload length, `0..64` (`PROTO_MAX_PAYLOAD`) |
| 3 | 1 | TYPE | message type, see below |
| 4 | 1 | SEQ | frame counter, wraps at 255 |
| 5 | LEN | PAYLOAD | message body |
| 5+LEN | 1 | CRC_L | CRC low byte |
| 6+LEN | 1 | CRC_H | CRC high byte |

Overhead is 7 bytes (`PROTO_OVERHEAD`), maximum frame is 71 bytes (`PROTO_MAX_FRAME`).
The frame length is variable: `LEN + 7`.

`SEQ` is a per-sender counter of sent frames. In `MSG_ACK` the payload field `seq` echoes the `SEQ` of the command
being acknowledged.

## Parser

`proto_feed()` is a byte-wise state machine (O(1), no allocation, ISR-safe):
`SOF0 → SOF1 → LEN → TYPE → SEQ → PAYLOAD → CRC_L → CRC_H`.

- Garbage before the start bytes is skipped. A repeated `0xAA` while waiting for `0x55` keeps the parser in the
  `SOF1` state.
- `LEN > 64` → `len_errors++`, back to searching for `SOF0`.
- CRC mismatch → `crc_errors++`, the frame is dropped, back to searching for `SOF0` (no resynchronisation inside the
  dropped frame).
- Valid frame → `frames_ok++`, `proto_feed()` returns `true` and fills `proto_frame_t` (`type`, `seq`, `len`,
  `payload`). The payload pointer is valid until the next `proto_feed()` call.

API: `proto_crc16()`, `proto_encode()` (returns frame length, `0` if it does not fit), `proto_parser_init()`,
`proto_feed()`.

## Message types

### STM32 → ESP32

| TYPE | Name | Payload | Description |
|---:|---|---|---|
| `0x01` | `MSG_HELLO` | `msg_hello_t` (8) | after reset |
| `0x02` | `MSG_TELEMETRY` | `msg_telemetry_t` (40) | every 50 ms |
| `0x03` | `MSG_ACK` | `msg_ack_t` (2) | reply to every `CMD_*` |
| `0x04` | `MSG_EVENT` | `msg_event_t` (8) | shot done, faults, … |
| `0x05` | `MSG_RTC` | `msg_rtc_t` (4) | STM32 RTC time (VBAT) |

### ESP32 → STM32

| TYPE | Name | Payload | Description |
|---:|---|---|---|
| `0x20` | `CMD_PING` | – | |
| `0x21` | `CMD_SET_PARAMS` | `cmd_params_t` (16) | setpoints, PID |
| `0x22` | `CMD_SET_PROFILE` | `cmd_profile_t` (16) | active brew profile |
| `0x23` | `CMD_BREW` | `cmd_brew_t` (4) | start / stop |
| `0x24` | `CMD_SET_PRESSURE` | `cmd_u16_t` (2) | manual pressure, bar × 10 |
| `0x25` | `CMD_STEAM` | `cmd_u8_t` (1) | `1` start / `0` stop |
| `0x26` | `CMD_STEAM_VALVE` | `cmd_u8_t` (1) | opening 0…100 % |
| `0x27` | `CMD_PURGE` | `cmd_u16_t` (2) | duration, ms |
| `0x28` | `CMD_HOT_WATER` | `cmd_hot_water_t` (4) | |
| `0x29` | `CMD_BACKFLUSH` | `cmd_u8_t` (1) | `1` start / `0` stop |
| `0x2A` | `CMD_STANDBY` | `cmd_u8_t` (1) | `1` enter / `0` exit |
| `0x2B` | `CMD_RESET_COUNTER` | – | |
| `0x2C` | `CMD_SET_RTC` | `msg_rtc_t` (4) | |

## Payloads

### `msg_telemetry_t` (40 bytes)

Full telemetry frame = 47 bytes.

| Offset | Size | Field | Type | Unit / meaning |
|---:|---:|---|---|---|
| 0 | 1 | status | u8 | `WIRE_ST_*` |
| 1 | 1 | phase | u8 | `WIRE_PH_*` |
| 2 | 1 | pump_pct | u8 | actual pump PWM duty, % |
| 3 | 1 | valve_pct | u8 | actual steam valve PWM duty, % |
| 4 | 2 | brew_c_x100 | i16 | brew boiler, °C × 100 |
| 6 | 2 | steam_c_x10 | i16 | steam boiler, °C × 10 |
| 8 | 2 | preheat_c_x10 | i16 | preheat boiler, °C × 10 |
| 10 | 2 | bar_x100 | i16 | pressure, bar × 100 |
| 12 | 2 | target_bar_x100 | i16 | target pressure, bar × 100 |
| 14 | 2 | flow_x100 | i16 | flow, ml/s × 100 |
| 16 | 2 | volume_x10 | u16 | volume, ml × 10 |
| 18 | 2 | manual_bar_x10 | u16 | echo of the manual setpoint in use, bar × 10 |
| 20 | 4 | shot_ms | u32 | shot time, ms |
| 24 | 2 | faults | u16 | `WIRE_FAULT_*` bit mask |
| 26 | 1 | flags | u8 | `TLM_F_*` bit mask |
| 27 | 1 | steam_power | u8 | echo of the requested valve opening |
| 28 | 1 | clean_cycle | u8 | |
| 29 | 1 | clean_total | u8 | |
| 30 | 2 | reserved | u16 | `0` |
| 32 | 4 | shot_count | u32 | |
| 36 | 4 | uptime_s | u32 | seconds since boot |

#### Temperatures

Signed integers. Brew is sent in hundredths of °C (`93.00` → `9300`), steam and preheat in tenths (`135.0` → `1350`).
The value `-32768` (`0x8000`) means "invalid" and is sent when the NTC is faulty (its bit in `state_ntc_boiler` of the
snapshot is 0), the reading is NaN/infinite, or it is outside the range −5 … 200 °C (both bounds excluded).

#### `status` (`WIRE_ST_*`)

`0` BOOT, `1` HEATING, `2` READY, `3` BREWING, `4` STEAM, `5` HOT_WATER, `6` CLEANING, `7` STANDBY, `8` FAULT.

#### `phase` (`WIRE_PH_*`)

`0` IDLE, `1` PREINFUSION, `2` SOAK, `3` EXTRACTION, `4` DONE.

#### `faults` (`WIRE_FAULT_*`)

| Bit | Meaning |
|---:|---|
| 0 | water empty |
| 1 | brew sensor |
| 2 | steam sensor |
| 3 | over-temperature |
| 4 | link lost – set by the ESP itself, never sent |

#### `flags` (`TLM_F_*`)

| Bit | Meaning |
|---:|---|
| 0 | brew heater on |
| 1 | steam heater on |
| 2 | preheat heater on |
| 3 | manual mode |

### Other STM32 → ESP32 messages

| Message | Field | Type | Meaning |
|---|---|---|---|
| `msg_hello_t` (8) | proto_ver | u8 | `PROTO_VERSION` |
| | fw_major, fw_minor, fw_patch | u8 × 3 | firmware version |
| | reset_cause | u32 | copy of `RCC->CSR` |
| `msg_ack_t` (2) | seq | u8 | `SEQ` of the acknowledged command |
| | result | u8 | `ACK_OK` 0, `ACK_BAD_STATE` 1, `ACK_BAD_ARG` 2, `ACK_UNKNOWN` 3, `ACK_BAD_LEN` 4 |
| `msg_event_t` (8) | code | u8 | `EVT_*` (below) |
| | pad | u8 | |
| | arg | u16 | fault bits, … |
| | value | u32 | shot ms, … |
| `msg_rtc_t` (4) | unix_s | u32 | Unix time, s |

`EVT_*`: `1` SHOT_DONE, `2` FAULT_SET, `3` FAULT_CLEARED, `4` BACKFLUSH_DONE, `5` WATER_DONE, `6` WATCHDOG_RESET.

### Commands (ESP32 → STM32)

| Payload | Field | Type | Meaning |
|---|---|---|---|
| `cmd_params_t` (16) | brew_set_x10 | i16 | brew setpoint, °C × 10 (already includes group offset) |
| | steam_set_x10 | i16 | °C × 10 |
| | preheat_set_x10 | i16 | °C × 10 |
| | preheat_on | u8 | |
| | eco | u8 | |
| | kp_x100 | u16 | |
| | ki_x1000 | u16 | |
| | kd_x10 | u16 | |
| | active_profile | u8 | |
| | pad | u8 | |
| `cmd_profile_t` (16) | index | u8 | |
| | preinf_bar_x10, brew_bar_x10, end_bar_x10 | u8 × 3 | bar × 10 |
| | preinf_ms, soak_ms | u16 × 2 | ms |
| | temp_x10 | i16 | °C × 10 |
| | target_ml | u16 | |
| | max_s | u16 | |
| | pad | u16 | |
| `cmd_brew_t` (4) | action | u8 | `BREW_STOP` 0, `BREW_PROFILE` 1, `BREW_MANUAL` 2 |
| | profile | u8 | |
| | manual_bar_x10 | u16 | start setpoint for `BREW_MANUAL` |
| `cmd_u8_t` (1) | value | u8 | |
| `cmd_u16_t` (2) | value | u16 | |
| `cmd_hot_water_t` (4) | on | u8 | |
| | pad | u8 | |
| | ml | u16 | |

Struct sizes are fixed by `_Static_assert` in the header (hello 8, telemetry 40, event 8, params 16, profile 16).

## STM32 side: current state

[`tlm_from_snapshot()`](../Core/Src/tlm_adapter.c) fills the telemetry from `system_snapshot_t`. Currently filled:
`uptime_s`, `pump_pct`, `valve_pct`, `brew_c_x100`, `steam_c_x10`, `preheat_c_x10`. All other fields (`status`, `phase`,
pressure, flow, volume, faults, flags, shot data, …) are sent as `0` for now.

[`esp_link_send_telemetry()`](../Core/Src/esp_link.c) encodes `MSG_TELEMETRY` with an incrementing `SEQ` and sends it with
`HAL_UART_Transmit_DMA`. If the previous transfer is still running it returns `HAL_BUSY` and the frame is skipped
(`SEQ` is not advanced).

## Bench status

- The ESP32 display is running and receives the STM32 telemetry over the link. Temperatures and fault values are
  shown on the screen (on the photo: brew boiler 24.3 °C, target 94.5, steam 24 °, pressure 0.0 bar, profile
  "Classic 9 bar").
- Hardware changes in the machine:
  - a hole for a USB Type-C cable was made in the case, for flashing and debugging;
  - a 24 V switching power supply was installed.

## Reference frame

`MSG_ACK`, `SEQ = 5`, payload `{seq = 1, result = ACK_OK}`:

```
AA 55 02 03 05 01 00 92 16
```

Manual check: [`tests/test_proto_gaggia.c`](../tests/test_proto_gaggia.c) (encodes this frame, feeds it to the parser
behind some garbage bytes and prints the result).
