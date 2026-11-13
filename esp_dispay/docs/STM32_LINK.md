# STM32 ↔ ESP32 link

Shared protocol: `src/proto/gaggia_proto.h` + `gaggia_proto.c`.
**Copy both files into the STM32 project unchanged.** Pure C, no deps.
Self-test on PC: `gcc -std=c11 -I src/proto sim/test_proto.c src/proto/gaggia_proto.c && ./a.out`

## Wiring

| ESP32 (WT32-SC01 Plus) | STM32 | |
|---|---|---|
| IO10 (RX) | USARTx TX | check the header pinout of your board |
| IO11 (TX) | USARTx RX | 3.3 V both sides, no level shifter |
| GND | GND | common ground is mandatory |

921600 8N1 (`PROTO_BAUD`). If the cable is long or noisy, drop to 460800 or
115200 on both sides: a 47-byte telemetry frame at 20 Hz needs < 1 KB/s.

## Frame

```
AA 55 | LEN | TYPE | SEQ | PAYLOAD[LEN] | CRC_L CRC_H
CRC-16/CCITT-FALSE over LEN..PAYLOAD. Little-endian, fixed-point (x10, x100).
```

## Who sends what

| Direction | Type | When | Payload |
|---|---|---|---|
| STM → ESP | `MSG_HELLO` | once after reset | fw version, proto version, reset cause |
| STM → ESP | `MSG_TELEMETRY` | every 50 ms | `msg_telemetry_t` (40 B) |
| STM → ESP | `MSG_ACK` | reply to every `CMD_*` | `{seq, result}` |
| STM → ESP | `MSG_EVENT` | shot done, fault set/cleared, watchdog reset | `msg_event_t` |
| ESP → STM | `CMD_PING` | every 1 s | — |
| ESP → STM | `CMD_SET_PARAMS` | on every settings change and after HELLO | `cmd_params_t` |
| ESP → STM | `CMD_SET_PROFILE` | with params, on profile edit/select | `cmd_profile_t` |
| ESP → STM | `CMD_BREW` | Brew / Manual / Stop | `{action, profile, manual_bar_x10}` |
| ESP → STM | `CMD_SET_PRESSURE` | manual mode, knob/± changes | `bar_x10` |
| ESP → STM | `CMD_STEAM` / `CMD_STEAM_VALVE` | steam on/off, knob opening 0..100 % | `u8` |
| ESP → STM | `CMD_PURGE` | Purge button | duration ms |
| ESP → STM | `CMD_HOT_WATER` | Dispense / Stop | `{on, ml}` |
| ESP → STM | `CMD_BACKFLUSH`, `CMD_STANDBY` | buttons | `u8` |
| ESP → STM | `CMD_RESET_COUNTER` | Settings | — |
| ESP → STM | `CMD_SET_RTC` | clock set on the screen | unix seconds |

Ownership rules:
- The STM32 owns the machine: status, phase, temperatures, pressure, flow,
  volume, actual PWM duties, faults, shot counter. The ESP only displays them.
- The ESP owns the knob: valve opening and manual pressure setpoint. The STM
  echoes them in telemetry; the ESP ignores the echo (it would lag the knob)
  and resends the valve value if the echo disagrees for 10 frames.
- Safety stays on the STM32: over-temp, dry pump, max shot time, link loss
  (no `CMD_PING` for > 2 s → finish/stop any running session, keep boilers on setpoint).

## ESP32 side (done)

`src/app/gaggia_link.c`, active with `-DGAGGIA_SIM=0` (`pio run -e release`).

| Function | Purpose |
|---|---|
| `gaggia_link_init(tx_fn)` | parser reset, first ping (called in `setup()`) |
| `gaggia_link_rx(buf, len)` | feed UART bytes (called from `loop()`) |
| `gaggia_link_tick(now)` | 1 s ping (called by `gaggia_tick`) |
| `gaggia_link_send_*()` | one per command, called by `gaggia_cmd_*()` |
| `gaggia_link_raw_dump(true)` | hex dump of TX frames into the debug log |
| `gaggia_on_telemetry(&t)` | model update, counts frames for the Telemetry screen |
| `gaggia_link_crc_error()` | counted automatically from the parser |

## STM32 side (to write)

Minimum set of functions:

```c
/* ---- link.c (STM32) ------------------------------------------------ */
#include "gaggia_proto.h"

static proto_parser_t rx_parser;
static uint8_t        rx_dma[128];      /* circular DMA buffer            */
static uint8_t        tx_buf[PROTO_MAX_FRAME];
static uint8_t        tx_seq;
static uint32_t       last_ping_ms;

void link_init(void)
{
    proto_parser_init(&rx_parser);
    HAL_UARTEx_ReceiveToIdle_DMA(&huartX, rx_dma, sizeof rx_dma);   /* or circular DMA + IDLE */
    msg_hello_t h = { PROTO_VERSION, 0, 1, 0, RCC->CSR };
    link_send(MSG_HELLO, &h, sizeof h);
    __HAL_RCC_CLEAR_RESET_FLAGS();
}

void link_send(uint8_t type, const void *p, uint8_t len)     /* non-blocking: DMA TX */
{
    size_t n = proto_encode(tx_buf, sizeof tx_buf, type, ++tx_seq, p, len);
    HAL_UART_Transmit_DMA(&huartX, tx_buf, n);   /* use a small TX queue if busy */
}

static void ack(uint8_t seq, uint8_t result)
{
    msg_ack_t a = { seq, result };
    link_send(MSG_ACK, &a, sizeof a);
}

/* RX: call from the IDLE/half/full DMA callback -> push bytes to a ring,
   then drain the ring in the main loop / RTOS task: */
void link_poll(void)
{
    uint8_t b;
    proto_frame_t f;
    while (ring_pop(&b))
        if (proto_feed(&rx_parser, b, &f))
            link_dispatch(&f);
}

void link_dispatch(const proto_frame_t *f)
{
    switch (f->type) {
    case CMD_PING:          last_ping_ms = HAL_GetTick(); ack(f->seq, ACK_OK); return;
    case CMD_SET_PARAMS:    { cmd_params_t c;  memcpy(&c, f->payload, sizeof c);
                              ack(f->seq, machine_set_params(&c)); return; }
    case CMD_SET_PROFILE:   { cmd_profile_t c; memcpy(&c, f->payload, sizeof c);
                              ack(f->seq, machine_set_profile(&c)); return; }
    case CMD_BREW:          { cmd_brew_t c;    memcpy(&c, f->payload, sizeof c);
                              ack(f->seq, machine_brew(c.action, c.manual_bar_x10)); return; }
    case CMD_SET_PRESSURE:  { cmd_u16_t c;     memcpy(&c, f->payload, sizeof c);
                              ack(f->seq, pump_set_target_bar_x10(c.value)); return; }
    case CMD_STEAM:         ack(f->seq, machine_steam(f->payload[0]));       return;
    case CMD_STEAM_VALVE:   ack(f->seq, valve_set_opening(f->payload[0]));   return;
    case CMD_PURGE:         { cmd_u16_t c; memcpy(&c, f->payload, sizeof c);
                              ack(f->seq, valve_purge(c.value)); return; }
    case CMD_HOT_WATER:     { cmd_hot_water_t c; memcpy(&c, f->payload, sizeof c);
                              ack(f->seq, machine_hot_water(c.on, c.ml)); return; }
    case CMD_BACKFLUSH:     ack(f->seq, machine_backflush(f->payload[0]));   return;
    case CMD_STANDBY:       ack(f->seq, machine_standby(f->payload[0]));     return;
    case CMD_RESET_COUNTER: shot_counter_reset(); ack(f->seq, ACK_OK);      return;
    case CMD_SET_RTC:       { msg_rtc_t c; memcpy(&c, f->payload, sizeof c);
                              rtc_set_unix(c.unix_s); ack(f->seq, ACK_OK); return; }
    default:                ack(f->seq, ACK_UNKNOWN);                        return;
    }
}

/* every 50 ms (timer flag or RTOS task) */
void link_send_telemetry(void)
{
    msg_telemetry_t t = {0};
    t.status          = machine_status();
    t.phase           = brew_phase();
    t.brew_c_x100     = (int16_t)(boiler_brew_temp() * 100);
    t.steam_c_x10     = (int16_t)(boiler_steam_temp() * 10);
    t.preheat_c_x10   = (int16_t)(boiler_preheat_temp() * 10);
    t.bar_x100        = (int16_t)(pressure_bar() * 100);
    t.target_bar_x100 = (int16_t)(pump_target_bar() * 100);
    t.flow_x100       = (int16_t)(flow_ml_s() * 100);
    t.volume_x10      = (uint16_t)(shot_volume_ml() * 10);
    t.shot_ms         = shot_elapsed_ms();
    t.pump_pct        = pump_duty_pct();
    t.valve_pct       = valve_duty_pct();
    t.steam_power     = valve_requested_pct();
    t.manual_bar_x10  = pump_manual_setpoint_x10();
    t.flags           = (brew_heater_on()    ? TLM_F_BREW_HEATER    : 0)
                      | (steam_heater_on()   ? TLM_F_STEAM_HEATER   : 0)
                      | (preheat_heater_on() ? TLM_F_PREHEAT_HEATER : 0)
                      | (brew_is_manual()    ? TLM_F_MANUAL         : 0);
    t.faults          = faults_mask();
    t.shot_count      = shot_counter();
    t.uptime_s        = HAL_GetTick() / 1000;
    link_send(MSG_TELEMETRY, &t, sizeof t);
}

/* safety: in the main loop */
void link_watchdog(void)
{
    if (HAL_GetTick() - last_ping_ms > 2000) machine_on_ui_lost();  /* stop sessions */
}
```

`machine_*`, `pump_*`, `valve_*`, `boiler_*` are your control modules; each
command handler returns an `ACK_*` code (`ACK_BAD_STATE` if e.g. brew is
requested while heating, `ACK_BAD_ARG` for out-of-range values).

Status / phase / fault codes on the wire are `WIRE_ST_*`, `WIRE_PH_*`,
`WIRE_FAULT_*` in `gaggia_proto.h`, so the STM32 project needs nothing else.
The ESP side static-asserts them against its model enums.

## Debugging the link

- Telemetry screen (long-press the status text): rx frames, fps, CRC errors,
  timeouts, every TX command.
- `gaggia_link_raw_dump(true)` → hex of every TX frame in the log.
- `pio device monitor` shows the same log on USB.
- First bring-up: loop ESP TX→RX with a wire. Every `CMD_PING` is parsed as an
  unknown frame type (0x20) in the log, which proves framing + CRC end to end.
