# Telemetry protocol

STM32 → ESP32 telemetry frame. Implemented in [`protocol.c`](../Core/Src/protocol.c), constants in
[`protocol.h`](../Core/Inc/protocol.h), source data in [`system_snapshot.h`](../Core/Inc/system_snapshot.h).

- Fixed length: **47 bytes** (5 header + 40 data + 2 CRC).
- Byte order: **little-endian**.
- Format version: **1**, frame type: `0x01` (telemetry).
- CRC-16/CCITT-FALSE (poly `0x1021`, init `0xFFFF`, no reflection, no final XOR), computed over bytes `2..44`
  (version through the last data byte). The start bytes are not covered. Check value: `"123456789"` → `0x29B1`.

## Frame layout

| Offset | Size | Field | Type | Unit / meaning |
|---:|---:|---|---|---|
| 0 | 1 | start 1 | u8 | `0xAA` |
| 1 | 1 | start 2 | u8 | `0x55` |
| 2 | 1 | version | u8 | `1` |
| 3 | 1 | type | u8 | `0x01` telemetry |
| 4 | 1 | data length | u8 | `40` |
| 5 | 4 | up_time_ms | u32 | ms since boot |
| 9 | 4 | seq | u32 | sample counter |
| 13 | 4 | flowmeter | u32 | pulses |
| 17 | 2 | faults | u16 | bit mask, see below |
| 19 | 2 | temp_brew | i16 | °C × 10 |
| 21 | 2 | temp_steam | i16 | °C × 10 |
| 23 | 2 | temp_preheat | i16 | °C × 10 |
| 25 | 2 | rpm_pump | u16 | rpm |
| 27 | 2 | pressure_pump | u16 | bar × 100 |
| 29 | 2 | amplitude_steam_probe | u16 | ADC value |
| 31 | 2 | amplitude_min_level | u16 | ADC value |
| 33 | 2 | amplitude_max_level | u16 | ADC value |
| 35 | 1 | machine_mode | u8 | machine mode |
| 36 | 1 | state_ntc_boiler | u8 | bit mask, see below |
| 37 | 1 | state_ssr_boiler | u8 | bit mask, see below |
| 38 | 1 | state_pump | u8 | `0` fault, `1` stop, `2` run |
| 39 | 1 | percent_pump | u8 | % |
| 40 | 1 | state_pwm_valve | u8 | `0` fault, `1` stop, `2` run |
| 41 | 1 | percent_pwm_valve | u8 | % |
| 42 | 1 | valves | u8 | bit mask, see below |
| 43 | 1 | state_steam_level_probe | u8 | `0` unknown, `1` dry, `2` wet |
| 44 | 1 | state_tank_level_probe | u8 | `0` unknown, `1` error, `2` full, `3` low, `4` empty |
| 45 | 2 | crc16 | u16 | over bytes 2..44 |

## Temperatures

Sent as signed integers in tenths of °C (`25.19` → `252`, `93.0` → `930`). The value `-32768` (`0x8000`) means
"invalid" and is sent when the NTC is faulty (its bit in `state_ntc_boiler` is 0), the reading is NaN/infinite, or it is
outside the range −5 … 200 °C (both bounds excluded).

## Bit masks

| Field | Bit | Meaning |
|---|---:|---|
| faults | 0 | pump |
| | 1 | steam valve |
| | 2 | NTC preheat |
| | 3 | NTC steam |
| | 4 | NTC brew |
| | 5 | steam probe |
| | 6 | tank sensor |
| | 7 | tank empty |
| state_ntc_boiler | 0 / 1 / 2 | preheat / steam / brew: `1` OK, `0` fault |
| state_ssr_boiler | 0 / 1 / 2 | preheat / steam / brew: `1` on, `0` off |
| valves | 0 | group valve |
| | 1 | steam fill valve |
| | 2 | hot water valve |
| | 3 | air venting valve |

## Reference frame

Snapshot: `up_time_ms=100000`, `seq=5`, `faults=0x0060`, brew 93.0 °C, steam 135.0 °C, preheat 80.0 °C,
`state_ntc_boiler=7`, `rpm_pump=3750`, `pressure_pump=900`, probe amplitudes 2753 / 500 / 2700, pump running at 50 %,
PWM valve stopped, `valves=0x05`, steam probe dry.

```
AA 55 01 01 28 A0 86 01 00 05 00 00 00 00 00 00
00 60 00 A2 03 46 05 20 03 A6 0E 84 03 C1 0A F4
01 8C 0A 00 07 00 02 32 01 00 05 01 00 BC E8
```

It is checked by [`tests/test_protocol.c`](../tests/test_protocol.c) (`sh tests/run.sh`).
