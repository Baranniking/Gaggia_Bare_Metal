/**
 * gaggia_proto.h — UART protocol between the STM32 (machine control) and the
 * ESP32-S3 (UI). SHARED FILE: copy gaggia_proto.h/.c into the STM32 project
 * unchanged. Pure C99/C11, no dependencies besides libc headers.
 *
 * Frame (little-endian, both MCUs are LE):
 *
 *   +------+------+-----+------+-----+-------------+-------+-------+
 *   | 0xAA | 0x55 | version | TYPE | data length | up_time_ms| seq | data[] | crc16
 *   +------+------+-----+------+-----+-------------+-------+-------+
 *   CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over LEN, TYPE, SEQ, PAYLOAD.
 *
 * Values are fixed-point integers (x10 / x100) — no floats on the wire.
 * Every ESP->STM command is answered by MSG_ACK{seq, result}.
 * STM sends MSG_TELEMETRY every 50 ms; silence > 500 ms = link lost.
 * STM sends MSG_HELLO after reset; ESP answers by pushing params + profile.
 */
#ifndef GAGGIA_PROTO_H
#define GAGGIA_PROTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTO_VERSION      1
#define PROTO_BAUD         921600u
#define PROTO_SOF0         0xAAu
#define PROTO_SOF1         0x55u
#define PROTO_MAX_PAYLOAD  64u
#define PROTO_OVERHEAD     7u          /* SOF0 SOF1 LEN TYPE SEQ CRC_L CRC_H */
#define PROTO_MAX_FRAME    (PROTO_MAX_PAYLOAD + PROTO_OVERHEAD)
#define PROTO_TELEMETRY_PERIOD_MS  50u
#define PROTO_LINK_TIMEOUT_MS      500u

#if defined(__GNUC__)
#define PROTO_PACKED __attribute__((packed))
#else
#define PROTO_PACKED
#endif

/* ------------------------------------------------------------------ */
/* Message types                                                       */
/* ------------------------------------------------------------------ */
typedef enum {
    /* STM32 -> ESP32 */
    MSG_HELLO          = 0x01,   /* msg_hello_t      after reset           */
    MSG_TELEMETRY      = 0x02,   /* msg_telemetry_t  every 50 ms           */
    MSG_ACK            = 0x03,   /* msg_ack_t        reply to every CMD_*  */
    MSG_EVENT          = 0x04,   /* msg_event_t      shot done, faults...  */
    MSG_RTC            = 0x05,   /* msg_rtc_t        STM32 RTC time (VBAT) */

    /* ESP32 -> STM32 */
    CMD_PING           = 0x20,   /* no payload                             */
    CMD_SET_PARAMS     = 0x21,   /* cmd_params_t     setpoints, PID        */
    CMD_SET_PROFILE    = 0x22,   /* cmd_profile_t    active brew profile   */
    CMD_BREW           = 0x23,   /* cmd_brew_t       start/stop            */
    CMD_SET_PRESSURE   = 0x24,   /* cmd_u16_t        manual bar x10        */
    CMD_STEAM          = 0x25,   /* cmd_u8_t         1 start / 0 stop      */
    CMD_STEAM_VALVE    = 0x26,   /* cmd_u8_t         opening 0..100 %      */
    CMD_PURGE          = 0x27,   /* cmd_u16_t        duration ms           */
    CMD_HOT_WATER      = 0x28,   /* cmd_hot_water_t                        */
    CMD_BACKFLUSH      = 0x29,   /* cmd_u8_t         1 start / 0 stop      */
    CMD_STANDBY        = 0x2A,   /* cmd_u8_t         1 enter / 0 exit      */
    CMD_RESET_COUNTER  = 0x2B,   /* no payload                             */
    CMD_SET_RTC        = 0x2C,   /* msg_rtc_t                              */
} proto_type_t;

/* ACK results */
enum { ACK_OK = 0, ACK_BAD_STATE = 1, ACK_BAD_ARG = 2, ACK_UNKNOWN = 3, ACK_BAD_LEN = 4 };

/* MSG_EVENT codes */
enum { EVT_SHOT_DONE = 1, EVT_FAULT_SET = 2, EVT_FAULT_CLEARED = 3, EVT_BACKFLUSH_DONE = 4,
       EVT_WATER_DONE = 5, EVT_WATCHDOG_RESET = 6 };

/* status / phase / fault codes in msg_telemetry_t (STM32 is the source).
   The ESP side static-asserts these against gaggia_model.h. */
enum { WIRE_ST_BOOT = 0, WIRE_ST_HEATING, WIRE_ST_READY, WIRE_ST_BREWING, WIRE_ST_STEAM,
       WIRE_ST_HOT_WATER, WIRE_ST_CLEANING, WIRE_ST_STANDBY, WIRE_ST_FAULT };
enum { WIRE_PH_IDLE = 0, WIRE_PH_PREINFUSION, WIRE_PH_SOAK, WIRE_PH_EXTRACTION, WIRE_PH_DONE };
#define WIRE_FAULT_WATER_EMPTY   (1u << 0)
#define WIRE_FAULT_BREW_SENSOR   (1u << 1)
#define WIRE_FAULT_STEAM_SENSOR  (1u << 2)
#define WIRE_FAULT_OVERTEMP      (1u << 3)
/* bit 4 (link lost) is set by the ESP itself, never sent */

/* telemetry flags */
#define TLM_F_BREW_HEATER     (1u << 0)
#define TLM_F_STEAM_HEATER    (1u << 1)
#define TLM_F_PREHEAT_HEATER  (1u << 2)
#define TLM_F_MANUAL          (1u << 3)

/* CMD_BREW actions */
enum { BREW_STOP = 0, BREW_PROFILE = 1, BREW_MANUAL = 2 };

/* ------------------------------------------------------------------ */
/* Payloads                                                            */
/* ------------------------------------------------------------------ */
typedef struct PROTO_PACKED {
    uint8_t  proto_ver;
    uint8_t  fw_major, fw_minor, fw_patch;
    uint32_t reset_cause;        /* RCC->CSR copy, for the log */
} msg_hello_t;

typedef struct PROTO_PACKED {
    uint8_t  status;             /* gaggia_status_t */
    uint8_t  phase;              /* brew_phase_t    */
    uint8_t  pump_pct;           /* actual PWM duty */
    uint8_t  valve_pct;          /* actual steam valve PWM duty */
    int16_t  brew_c_x100;
    int16_t  steam_c_x10;
    int16_t  preheat_c_x10;
    int16_t  bar_x100;
    int16_t  target_bar_x100;
    int16_t  flow_x100;          /* ml/s */
    uint16_t volume_x10;         /* ml   */
    uint16_t manual_bar_x10;     /* echo of the manual setpoint in use */
    uint32_t shot_ms;
    uint16_t faults;             /* gaggia_fault_t bits */
    uint8_t  flags;              /* TLM_F_* */
    uint8_t  steam_power;        /* echo of the requested valve opening */
    uint8_t  clean_cycle, clean_total;
    uint16_t reserved;
    uint32_t shot_count;
    uint32_t uptime_s;
} msg_telemetry_t;

typedef struct PROTO_PACKED { uint8_t seq; uint8_t result; } msg_ack_t;

typedef struct PROTO_PACKED {
    uint8_t  code;               /* EVT_* */
    uint8_t  pad;
    uint16_t arg;                /* fault bits, ... */
    uint32_t value;              /* shot ms, ...    */
} msg_event_t;

typedef struct PROTO_PACKED { uint32_t unix_s; } msg_rtc_t;

typedef struct PROTO_PACKED {
    int16_t  brew_set_x10;       /* already includes group offset */
    int16_t  steam_set_x10;
    int16_t  preheat_set_x10;
    uint8_t  preheat_on;
    uint8_t  eco;
    uint16_t kp_x100;
    uint16_t ki_x1000;
    uint16_t kd_x10;
    uint8_t  active_profile;
    uint8_t  pad;
} cmd_params_t;

typedef struct PROTO_PACKED {
    uint8_t  index;
    uint8_t  preinf_bar_x10;
    uint8_t  brew_bar_x10;
    uint8_t  end_bar_x10;
    uint16_t preinf_ms;
    uint16_t soak_ms;
    int16_t  temp_x10;
    uint16_t target_ml;
    uint16_t max_s;
    uint16_t pad;
} cmd_profile_t;

typedef struct PROTO_PACKED {
    uint8_t  action;             /* BREW_* */
    uint8_t  profile;
    uint16_t manual_bar_x10;     /* BREW_MANUAL start setpoint */
} cmd_brew_t;

typedef struct PROTO_PACKED { uint8_t  value; } cmd_u8_t;
typedef struct PROTO_PACKED { uint16_t value; } cmd_u16_t;
typedef struct PROTO_PACKED { uint8_t on; uint8_t pad; uint16_t ml; } cmd_hot_water_t;

/* Layout is part of the contract: fail the build if a struct changes size. */
_Static_assert(sizeof(msg_hello_t)     == 8,  "msg_hello_t");
_Static_assert(sizeof(msg_telemetry_t) == 40, "msg_telemetry_t");
_Static_assert(sizeof(msg_event_t)     == 8,  "msg_event_t");
_Static_assert(sizeof(cmd_params_t)    == 16, "cmd_params_t");
_Static_assert(sizeof(cmd_profile_t)   == 16, "cmd_profile_t");
_Static_assert(sizeof(msg_telemetry_t) <= PROTO_MAX_PAYLOAD, "payload too big");

/* ------------------------------------------------------------------ */
/* API                                                                 */
/* ------------------------------------------------------------------ */
uint16_t proto_crc16(const uint8_t *data, size_t len, uint16_t crc);

/* Build a frame into out[]. Returns frame length, 0 if it doesn't fit. */
size_t proto_encode(uint8_t *out, size_t cap, uint8_t type, uint8_t seq,
                    const void *payload, uint8_t len);

typedef struct {
    uint8_t  type;
    uint8_t  seq;
    uint8_t  len;
    const uint8_t *payload;      /* valid until the next proto_feed() */
} proto_frame_t;

typedef struct {
    uint8_t  state;
    uint8_t  len, type, seq, idx;
    uint8_t  crc_lo;
    uint8_t  buf[PROTO_MAX_PAYLOAD];
    uint32_t frames_ok;
    uint32_t crc_errors;
    uint32_t len_errors;
} proto_parser_t;

void proto_parser_init(proto_parser_t *p);

/* Feed one received byte (ISR-safe: no allocation, O(1)).
 * Returns true when a complete, CRC-valid frame is in *out. */
bool proto_feed(proto_parser_t *p, uint8_t byte, proto_frame_t *out);

#ifdef __cplusplus
}
#endif
#endif /* GAGGIA_PROTO_H */
