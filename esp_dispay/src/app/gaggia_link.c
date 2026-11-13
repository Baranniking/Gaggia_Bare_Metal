/**
 * gaggia_link.c — ESP32 side of the STM32 link.
 */
#include "gaggia_link.h"
#include "gaggia_model.h"
#include "gaggia_log.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

/* wire codes must match the model enums 1:1 (int casts: two different enums) */
#define SAME(a, b) ((int)(a) == (int)(b))
_Static_assert(SAME(GS_BOOT, WIRE_ST_BOOT) && SAME(GS_READY, WIRE_ST_READY) &&
               SAME(GS_BREWING, WIRE_ST_BREWING) && SAME(GS_STEAM, WIRE_ST_STEAM) &&
               SAME(GS_FAULT, WIRE_ST_FAULT), "status codes");
_Static_assert(SAME(PH_IDLE, WIRE_PH_IDLE) && SAME(PH_EXTRACTION, WIRE_PH_EXTRACTION) &&
               SAME(PH_DONE, WIRE_PH_DONE), "phase codes");
_Static_assert(SAME(FAULT_WATER_EMPTY, WIRE_FAULT_WATER_EMPTY) &&
               SAME(FAULT_OVERTEMP, WIRE_FAULT_OVERTEMP), "fault bits");
#undef SAME

static gaggia_link_tx_fn s_tx;
static proto_parser_t    s_parser;
static uint8_t           s_seq;
static uint32_t          s_last_ping;
static bool              s_dump;

/* ------------------------------------------------------------------ */

static int16_t  q(float v, float k)   { return (int16_t)lroundf(v * k); }
static uint16_t uq(float v, float k)  { return v <= 0 ? 0 : (uint16_t)lroundf(v * k); }

static void dump(const char *dir, const uint8_t *f, size_t n)
{
    char hex[3 * 24 + 4];
    size_t k = 0;
    for (size_t i = 0; i < n && i < 24; i++) k += (size_t)snprintf(hex + k, sizeof(hex) - k, "%02X ", f[i]);
    if (n > 24) snprintf(hex + k, sizeof(hex) - k, "..");
    glog(LOG_I, "%s %s", dir, hex);
}

static uint8_t send(uint8_t type, const void *payload, uint8_t len)
{
    uint8_t frame[PROTO_MAX_FRAME];
    uint8_t seq = ++s_seq;
    size_t n = proto_encode(frame, sizeof(frame), type, seq, payload, len);
    if (!n || !s_tx) return 0;
    if (s_dump) dump("tx", frame, n);
    s_tx(frame, n);
    return seq;
}

/* ------------------------------------------------------------------ */
/* RX                                                                  */
/* ------------------------------------------------------------------ */

static void on_telemetry(const msg_telemetry_t *w)
{
    const gaggia_telemetry_t *cur = gaggia_tm();
    gaggia_telemetry_t t;
    memset(&t, 0, sizeof(t));

    t.status       = (gaggia_status_t)w->status;
    t.phase        = (brew_phase_t)w->phase;
    t.brew_temp    = w->brew_c_x100 / 100.0f;
    t.steam_temp   = w->steam_c_x10 / 10.0f;
    t.preheat_temp = w->preheat_c_x10 / 10.0f;
    t.pressure     = w->bar_x100 / 100.0f;
    t.target_bar   = w->target_bar_x100 / 100.0f;
    t.flow         = w->flow_x100 / 100.0f;
    t.volume       = w->volume_x10 / 10.0f;
    t.shot_ms      = w->shot_ms;
    t.pump_pct     = w->pump_pct;
    t.steam_valve_pct = w->valve_pct;
    t.clean_cycle  = w->clean_cycle;
    t.clean_total  = w->clean_total;
    t.brew_heater_on    = (w->flags & TLM_F_BREW_HEATER) != 0;
    t.steam_heater_on   = (w->flags & TLM_F_STEAM_HEATER) != 0;
    t.preheat_heater_on = (w->flags & TLM_F_PREHEAT_HEATER) != 0;
    t.manual       = (w->flags & TLM_F_MANUAL) != 0;
    t.faults       = w->faults;
    t.shot_count   = w->shot_count;
    t.uptime_s     = w->uptime_s;
    t.link_ok      = true;

    /* The ESP owns these two (knob / touch). The STM echo lags by a frame
       or two, so taking it back would make the arcs jitter. */
    t.steam_power  = cur->steam_power;
    t.manual_bar   = cur->manual_bar;
    if (w->steam_power != cur->steam_power && !t.manual && t.status == GS_STEAM) {
        /* persistent mismatch = lost command; resend */
        static uint8_t miss;
        if (++miss >= 10) { miss = 0; gaggia_link_send_valve(cur->steam_power); }
    }

    gaggia_on_telemetry(&t);
}

static void on_frame(const proto_frame_t *f)
{
    switch (f->type) {
    case MSG_TELEMETRY:
        if (f->len == sizeof(msg_telemetry_t)) {
            msg_telemetry_t w;
            memcpy(&w, f->payload, sizeof(w));   /* payload may be unaligned */
            on_telemetry(&w);
        } else {
            glog(LOG_E, "telemetry len %u != %u", f->len, (unsigned)sizeof(msg_telemetry_t));
        }
        break;

    case MSG_HELLO: {
        msg_hello_t h;
        memset(&h, 0, sizeof(h));
        memcpy(&h, f->payload, f->len < sizeof(h) ? f->len : sizeof(h));
        glog(h.proto_ver == PROTO_VERSION ? LOG_I : LOG_E,
             "stm32 hello fw %u.%u.%u proto %u reset 0x%08lX", h.fw_major, h.fw_minor,
             h.fw_patch, h.proto_ver, (unsigned long)h.reset_cause);
        /* STM32 (re)booted: push our state */
        gaggia_link_send_params();
        gaggia_link_send_profile(gaggia_settings()->active_profile);
        gaggia_link_send_valve(gaggia_tm()->steam_power);
        break;
    }

    case MSG_ACK: {
        msg_ack_t a;
        memcpy(&a, f->payload, sizeof(a) <= f->len ? sizeof(a) : f->len);
        if (a.result != ACK_OK) glog(LOG_W, "stm32 nack seq %u result %u", a.seq, a.result);
        break;
    }

    case MSG_EVENT: {
        msg_event_t ev;
        memset(&ev, 0, sizeof(ev));
        memcpy(&ev, f->payload, f->len < sizeof(ev) ? f->len : sizeof(ev));
        glog(ev.code == EVT_WATCHDOG_RESET ? LOG_E : LOG_I, "stm32 event %u arg 0x%04X val %lu",
             ev.code, ev.arg, (unsigned long)ev.value);
        break;
    }

    case MSG_RTC:
        /* TODO: settimeofday() via ui_hal when the STM32 RTC is set up */
        break;

    default:
        glog(LOG_W, "unknown frame type 0x%02X", f->type);
        break;
    }
}

void gaggia_link_rx(const uint8_t *data, size_t len)
{
    proto_frame_t f;
    uint32_t crc_before = s_parser.crc_errors;
    for (size_t i = 0; i < len; i++) {
        if (proto_feed(&s_parser, data[i], &f)) {
            if (s_dump && f.type != MSG_TELEMETRY) glog(LOG_I, "rx type 0x%02X seq %u len %u", f.type, f.seq, f.len);
            on_frame(&f);
        }
    }
    while (crc_before++ < s_parser.crc_errors) gaggia_link_crc_error();
}

/* ------------------------------------------------------------------ */

void gaggia_link_init(gaggia_link_tx_fn tx)
{
    s_tx = tx;
    proto_parser_init(&s_parser);
    glog(LOG_I, "link up: proto v%u, %lu baud", PROTO_VERSION, (unsigned long)PROTO_BAUD);
    send(CMD_PING, NULL, 0);
}

void gaggia_link_tick(uint32_t now)
{
    if (now - s_last_ping >= 1000) { s_last_ping = now; send(CMD_PING, NULL, 0); }
}

bool gaggia_link_raw_dump(bool on) { bool was = s_dump; s_dump = on; return was; }

/* ------------------------------------------------------------------ */
/* Senders                                                             */
/* ------------------------------------------------------------------ */

void gaggia_link_send_params(void)
{
    const gaggia_settings_t *st = gaggia_settings();
    cmd_params_t c;
    memset(&c, 0, sizeof(c));
    c.brew_set_x10    = q(gaggia_brew_setpoint(), 10);
    c.steam_set_x10   = q(st->steam_set, 10);
    c.preheat_set_x10 = q(st->preheat_set, 10);
    c.preheat_on      = st->preheat_on;
    c.eco             = st->eco;
    c.kp_x100         = uq(st->kp, 100);
    c.ki_x1000        = uq(st->ki, 1000);
    c.kd_x10          = uq(st->kd, 10);
    c.active_profile  = st->active_profile;
    send(CMD_SET_PARAMS, &c, sizeof(c));
}

void gaggia_link_send_profile(uint8_t index)
{
    const brew_profile_t *p = gaggia_profile(index);
    if (!p) return;
    cmd_profile_t c;
    memset(&c, 0, sizeof(c));
    c.index          = index;
    c.preinf_bar_x10 = (uint8_t)uq(p->preinf_bar, 10);
    c.brew_bar_x10   = (uint8_t)uq(p->brew_bar, 10);
    c.end_bar_x10    = (uint8_t)uq(p->end_bar, 10);
    c.preinf_ms      = uq(p->preinf_s, 1000);
    c.soak_ms        = uq(p->soak_s, 1000);
    c.temp_x10       = q(p->temp_c, 10);
    c.target_ml      = p->target_ml;
    c.max_s          = p->max_s;
    send(CMD_SET_PROFILE, &c, sizeof(c));
}

void gaggia_link_send_brew(uint8_t action, float manual_bar)
{
    cmd_brew_t c = { action, gaggia_settings()->active_profile, uq(manual_bar, 10) };
    send(CMD_BREW, &c, sizeof(c));
}

void gaggia_link_send_pressure(float bar)  { cmd_u16_t c = { uq(bar, 10) }; send(CMD_SET_PRESSURE, &c, sizeof(c)); }
void gaggia_link_send_steam(bool on)       { cmd_u8_t c = { on };          send(CMD_STEAM, &c, sizeof(c)); }
void gaggia_link_send_valve(uint8_t pct)   { cmd_u8_t c = { pct };         send(CMD_STEAM_VALVE, &c, sizeof(c)); }
void gaggia_link_send_purge(uint16_t ms)   { cmd_u16_t c = { ms };         send(CMD_PURGE, &c, sizeof(c)); }
void gaggia_link_send_backflush(bool on)   { cmd_u8_t c = { on };          send(CMD_BACKFLUSH, &c, sizeof(c)); }
void gaggia_link_send_standby(bool on)     { cmd_u8_t c = { on };          send(CMD_STANDBY, &c, sizeof(c)); }
void gaggia_link_send_reset_counter(void)  { send(CMD_RESET_COUNTER, NULL, 0); }
void gaggia_link_send_rtc(uint32_t unix_s) { msg_rtc_t c = { unix_s };     send(CMD_SET_RTC, &c, sizeof(c)); }

void gaggia_link_send_hot_water(bool on, uint16_t ml)
{
    cmd_hot_water_t c = { on, 0, ml };
    send(CMD_HOT_WATER, &c, sizeof(c));
}
