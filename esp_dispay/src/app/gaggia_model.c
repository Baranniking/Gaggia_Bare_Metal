/**
 * gaggia_model.c — state store + command layer + (optional) simulator.
 */
#include "gaggia_model.h"
#include "knob_input.h"
#include "gaggia_log.h"
#if !GAGGIA_SIM
#include "gaggia_link.h"
#endif

#include <string.h>
#include <math.h>
#include <stdlib.h>

/* Replace with a FreeRTOS mutex once telemetry arrives from another task. */
#define GAGGIA_LOCK()   do {} while (0)
#define GAGGIA_UNLOCK() do {} while (0)

static gaggia_telemetry_t s_tm;
static gaggia_store_t     s_store;
static bool               s_dirty;
static uint32_t           s_tick_ms;
static gaggia_link_stats_t s_link;
static bool               s_link_seen;   /* at least one frame received */

/* change tracking for the event log */
static gaggia_status_t s_prev_status = GS_BOOT;
static brew_phase_t    s_prev_phase  = PH_IDLE;
static uint16_t        s_prev_faults;
static bool            s_prev_link;
static float           s_shot_peak_bar;

const gaggia_link_stats_t *gaggia_link_stats(void) { return &s_link; }
void gaggia_link_crc_error(void) { s_link.crc_errors++; }

static gaggia_knob_mode_t s_knob_mode = KNOB_STEAM;
static uint8_t knob_pct_of_current_target(void);

/* every command that would go over UART passes here */
#define TX(...) do { s_link.tx_cmds++; glog(LOG_I, __VA_ARGS__); } while (0)

/* ------------------------------------------------------------------ */
/* Defaults                                                            */
/* ------------------------------------------------------------------ */

static const brew_profile_t k_default_profiles[] = {
    /* name            pre_s pre_bar soak brew  end   temp   ml  max */
    { "Classic 9 bar",  4.0f, 2.0f, 0.0f, 9.0f, 9.0f, 93.0f, 36, 40 },
    { "Blooming",       6.0f, 3.0f, 8.0f, 8.5f, 6.0f, 92.0f, 40, 55 },
    { "Lever",          8.0f, 2.5f, 0.0f, 9.0f, 4.5f, 93.5f, 38, 45 },
    { "Turbo",          2.0f, 3.0f, 0.0f, 6.0f, 6.0f, 94.0f, 45, 22 },
    { "Light roast",   10.0f, 3.0f, 5.0f, 9.5f, 7.0f, 95.0f, 45, 55 },
};

static void load_defaults(void)
{
    memset(&s_store, 0, sizeof(s_store));
    s_store.magic   = GAGGIA_STORE_MAGIC;
    s_store.version = GAGGIA_STORE_VERSION;

    size_t n = sizeof(k_default_profiles) / sizeof(k_default_profiles[0]);
    memcpy(s_store.profiles, k_default_profiles, sizeof(k_default_profiles));
    s_store.profile_count = (uint8_t)n;

    gaggia_settings_t *st = &s_store.settings;
    st->steam_set      = 135.0f;
    st->preheat_set    = 80.0f;
    st->preheat_on     = true;
    st->temp_offset    = 0.0f;
    st->kp = 8.0f; st->ki = 0.15f; st->kd = 40.0f;
    st->hot_water_ml   = 150;
    st->brightness     = 90;
    st->dim_after_s    = 120;
    st->auto_off_min   = 45;
    st->sound          = true;
    st->eco            = false;
    st->knob_auto_steam = true;
    st->touch_arcs      = GAGGIA_SIM;   /* demo: no knob yet, allow touch */
    st->active_profile = 0;
}

/* ------------------------------------------------------------------ */
/* Profile math (shared by sim, brew chart preview, profile editor)    */
/* ------------------------------------------------------------------ */

#define RAMP_S 1.5f

float gaggia_profile_duration(const brew_profile_t *p)
{
    return p ? (float)p->max_s : 0.0f;
}

float gaggia_profile_target_bar(const brew_profile_t *p, float t)
{
    if (!p || t < 0.0f) return 0.0f;

    float t1 = p->preinf_s;
    float t2 = t1 + p->soak_s;
    float tmax = (float)p->max_s;

    if (t < t1) {                                   /* pre-infusion */
        float k = t / 1.0f; if (k > 1.0f) k = 1.0f;
        return p->preinf_bar * k;
    }
    if (t < t2) {                                   /* soak: decay  */
        return p->preinf_bar * expf(-(t - t1) / 2.0f);
    }
    /* extraction: ramp up, then linear decline brew_bar -> end_bar */
    float start = (p->soak_s > 0.0f) ? p->preinf_bar * expf(-p->soak_s / 2.0f)
                                     : p->preinf_bar;
    float te = t - t2;
    if (te < RAMP_S) return start + (p->brew_bar - start) * (te / RAMP_S);

    float span = tmax - t2 - RAMP_S;
    if (span <= 0.1f) return p->brew_bar;
    float k = (te - RAMP_S) / span; if (k > 1.0f) k = 1.0f;
    return p->brew_bar + (p->end_bar - p->brew_bar) * k;
}

#if GAGGIA_SIM   /* only the simulator derives the phase; STM32 reports it */
static brew_phase_t phase_at(const brew_profile_t *p, float t)
{
    if (t < p->preinf_s) return PH_PREINFUSION;
    if (t < p->preinf_s + p->soak_s) return PH_SOAK;
    return PH_EXTRACTION;
}
#endif

/* ------------------------------------------------------------------ */
/* Public: state / settings                                            */
/* ------------------------------------------------------------------ */

const gaggia_telemetry_t *gaggia_tm(void) { return &s_tm; }

void gaggia_on_telemetry(const gaggia_telemetry_t *t)
{
    if (!t) return;
    GAGGIA_LOCK();
    s_tm = *t;
    s_link.rx_frames++;
    s_link.last_rx_ms = s_tick_ms;
    s_link_seen = true;
    GAGGIA_UNLOCK();
}

gaggia_settings_t *gaggia_settings(void)      { return &s_store.settings; }
uint8_t gaggia_profile_count(void)            { return s_store.profile_count; }

brew_profile_t *gaggia_profile(uint8_t idx)
{
    return (idx < s_store.profile_count) ? &s_store.profiles[idx] : NULL;
}

brew_profile_t *gaggia_active_profile(void)
{
    return gaggia_profile(s_store.settings.active_profile);
}

void gaggia_select_profile(uint8_t idx)
{
    if (idx >= s_store.profile_count) return;
    s_store.settings.active_profile = idx;
    glog(LOG_I, "profile -> %s", s_store.profiles[idx].name);
    gaggia_commit();
}

float gaggia_brew_setpoint(void)
{
    brew_profile_t *p = gaggia_active_profile();
    return (p ? p->temp_c : 93.0f) + s_store.settings.temp_offset;
}

void gaggia_commit(void)
{
    gaggia_settings_t *st = &s_store.settings;
    if (st->preheat_set < GAGGIA_PREHEAT_MIN) st->preheat_set = GAGGIA_PREHEAT_MIN;
    if (st->preheat_set > GAGGIA_PREHEAT_MAX) st->preheat_set = GAGGIA_PREHEAT_MAX;
    s_dirty = true;
    TX("tx params: brew %.1f steam %.0f pre %s%.0f pid %.1f/%.2f/%.0f",
       (double)gaggia_brew_setpoint(), (double)st->steam_set,
       st->preheat_on ? "" : "off/", (double)st->preheat_set,
       (double)st->kp, (double)st->ki, (double)st->kd);
#if !GAGGIA_SIM
    gaggia_link_send_params();
    gaggia_link_send_profile(st->active_profile);
#endif
}

const gaggia_store_t *gaggia_store(void) { return &s_store; }
bool gaggia_store_dirty(void)            { return s_dirty; }
void gaggia_store_clear_dirty(void)      { s_dirty = false; }

const char *gaggia_status_str(gaggia_status_t s)
{
    switch (s) {
    case GS_BOOT:      return "Connecting";
    case GS_HEATING:   return "Heating";
    case GS_READY:     return "Ready";
    case GS_BREWING:   return "Brewing";
    case GS_STEAM:     return "Steam";
    case GS_HOT_WATER: return "Hot water";
    case GS_CLEANING:  return "Cleaning";
    case GS_STANDBY:   return "Standby";
    case GS_FAULT:     return "Fault";
    }
    return "?";
}

const char *gaggia_phase_str(brew_phase_t p)
{
    switch (p) {
    case PH_IDLE:        return "Idle";
    case PH_PREINFUSION: return "Pre-infusion";
    case PH_SOAK:        return "Blooming";
    case PH_EXTRACTION:  return "Extraction";
    case PH_DONE:        return "Finished";
    }
    return "?";
}

/* ================================================================== */
#if GAGGIA_SIM
/* ================================================================== */
/*
 * Crude but believable physics so the UI can be developed without the
 * STM32 board. Nothing here is meant to be accurate.
 */
typedef struct {
    uint32_t last_ms;
    uint32_t t0_ms;          /* session start             */
    bool     water_ok;
    bool     purge;
    uint32_t purge_until;
    float    absorbed_ml;    /* puck + headspace retention */
    float    pump_f, valve_f;   /* actuator duty, float state (no rounding stall) */
} sim_t;

static sim_t s_sim = { .water_ok = true };

static float frand(void) { return (float)rand() / (float)RAND_MAX - 0.5f; }

static float approach(float v, float target, float rate_per_s, float dt)
{
    float d = target - v, step = rate_per_s * dt;
    if (fabsf(d) <= step) return target;
    return v + (d > 0 ? step : -step);
}

void gaggia_sim_set_water(bool ok) { s_sim.water_ok = ok; }

static void sim_init(void)
{
    memset(&s_tm, 0, sizeof(s_tm));
    s_tm.status     = GS_BOOT;
    s_tm.brew_temp  = 24.0f;
    s_tm.steam_temp = 24.0f;
    s_tm.preheat_temp = 24.0f;
    s_tm.shot_count = 1284;
    s_tm.steam_power = 0;      /* knob starts closed */
    s_tm.manual_bar  = 9.0f;
}

static void sim_end_session(void)
{
    s_tm.status = GS_HEATING;   /* re-evaluated below */
    s_tm.pressure = 0;
    s_tm.flow = 0;
    s_tm.target_bar = 0;
}

static void sim_step(uint32_t now)
{
    if (s_sim.last_ms == 0) { s_sim.last_ms = now; s_sim.t0_ms = now; }
    float dt = (now - s_sim.last_ms) / 1000.0f;
    if (dt <= 0.0f) return;
    if (dt > 0.2f) dt = 0.2f;
    s_sim.last_ms = now;

    s_tm.uptime_s = now / 1000;
    s_tm.link_ok  = true;

    if (s_tm.status == GS_BOOT && now > 1500) s_tm.status = GS_HEATING;

    float brew_set  = gaggia_brew_setpoint();
    float steam_set = s_store.settings.steam_set;
    bool  standby   = (s_tm.status == GS_STANDBY);

    /* --- boilers ------------------------------------------------- */
    float bt = standby ? 24.0f : brew_set;
    float st = (standby || (s_store.settings.eco && s_tm.status != GS_STEAM))
               ? 24.0f : steam_set;

    s_tm.brew_heater_on  = s_tm.brew_temp  < bt - 0.2f;
    s_tm.steam_heater_on = s_tm.steam_temp < st - 0.5f;

    /* preheat boiler: slow-ish, dips while the pump draws water through it */
    float pt = (standby || !s_store.settings.preheat_on) ? 24.0f : s_store.settings.preheat_set;
    s_tm.preheat_heater_on = s_tm.preheat_temp < pt - 0.5f;
    s_tm.preheat_temp = approach(s_tm.preheat_temp, pt, 1.5f, dt);
    if (s_tm.status == GS_BREWING || s_tm.status == GS_HOT_WATER)
        s_tm.preheat_temp -= 0.25f * s_tm.flow * dt;

    float brew_rate = (fabsf(bt - s_tm.brew_temp) > 3.0f) ? 2.5f : 0.4f;
    s_tm.brew_temp  = approach(s_tm.brew_temp, bt, brew_rate, dt);
    s_tm.steam_temp = approach(s_tm.steam_temp, st, 2.2f, dt);
    s_tm.brew_temp += frand() * 0.04f;

    /* --- active sessions ----------------------------------------- */
    float t = (now - s_sim.t0_ms) / 1000.0f;

    switch (s_tm.status) {
    case GS_BREWING: {
        brew_profile_t *p = gaggia_active_profile();
        s_tm.shot_ms    = now - s_sim.t0_ms;
        if (s_tm.manual) {
            s_tm.phase      = PH_EXTRACTION;
            s_tm.target_bar = approach(s_tm.target_bar, s_tm.manual_bar, 4.0f, dt); /* slew */
        } else {
            s_tm.phase      = phase_at(p, t);
            s_tm.target_bar = gaggia_profile_target_bar(p, t);
        }

        float tau = (t < 3.0f) ? 1.2f : 0.35f;        /* puck saturating */
        s_tm.pressure += (s_tm.target_bar - s_tm.pressure) * (dt / tau);
        s_tm.pressure += frand() * 0.06f;
        if (s_tm.pressure < 0) s_tm.pressure = 0;

        float g = 0.32f + 0.012f * t;                 /* puck erodes */
        float fill = 2.8f * expf(-t / 1.8f);          /* headspace   */
        float flow = (s_tm.phase == PH_SOAK) ? 0.05f : fill + g * sqrtf(s_tm.pressure);
        s_tm.flow += (flow - s_tm.flow) * (dt / 0.4f);

        float in = s_tm.flow * dt;
        if (s_sim.absorbed_ml < 7.0f) {               /* first drops */
            s_sim.absorbed_ml += in * 0.85f;
            in *= 0.15f;
        }
        s_tm.volume   += in;
        s_tm.brew_temp -= 0.03f * dt * s_tm.flow;     /* small dip   */

        bool done = s_tm.manual ? (t >= GAGGIA_MANUAL_MAX_S)
                                : (s_tm.volume >= p->target_ml || t >= p->max_s);
        if (done) {
            s_tm.phase = PH_DONE;
            s_tm.shot_count++;
            sim_end_session();
        }
        break;
    }
    case GS_STEAM: {
        s_tm.shot_ms = now - s_sim.t0_ms;
        float pw = s_tm.steam_power / 100.0f;
        if (s_sim.purge && now < s_sim.purge_until) pw = 1.0f;
        else s_sim.purge = false;
        s_tm.steam_temp -= pw * 1.6f * dt;
        s_tm.pressure = approach(s_tm.pressure, 1.3f * pw + 0.2f, 1.5f, dt);
        break;
    }
    case GS_HOT_WATER: {
        s_tm.shot_ms = now - s_sim.t0_ms;
        s_tm.flow    = approach(s_tm.flow, 5.0f, 4.0f, dt);
        s_tm.volume += s_tm.flow * dt;
        s_tm.steam_temp -= 0.8f * dt;
        if (s_tm.volume >= s_store.settings.hot_water_ml) sim_end_session();
        break;
    }
    case GS_CLEANING: {
        /* 5 x (10 s pump on @9 bar, 5 s off) */
        s_tm.shot_ms = now - s_sim.t0_ms;
        int cyc = (int)(t / 15.0f);
        float in_cyc = fmodf(t, 15.0f);
        s_tm.clean_cycle = (uint8_t)(cyc + 1);
        float target = (in_cyc < 10.0f) ? 9.0f : 0.0f;
        s_tm.pressure += (target - s_tm.pressure) * (dt / 0.4f);
        if (cyc >= s_tm.clean_total) { s_tm.clean_cycle = s_tm.clean_total; sim_end_session(); }
        break;
    }
    default:
        s_tm.pressure = approach(s_tm.pressure, 0.0f, 3.0f, dt);
        s_tm.flow     = approach(s_tm.flow, 0.0f, 3.0f, dt);
        break;
    }

    /* --- actuator duty (on the real machine: reported by STM32) ---- */
    {
        float pump = 0.0f;
        if (s_tm.status == GS_BREWING || s_tm.status == GS_CLEANING) {
            if (s_tm.phase != PH_SOAK || s_tm.status == GS_CLEANING)
                pump = 6.0f + s_tm.pressure * 6.5f + s_tm.flow * 5.0f
                     + (s_tm.target_bar - s_tm.pressure) * 12.0f;
            if (s_tm.status == GS_CLEANING && s_tm.pressure < 0.5f && fmodf(t, 15.0f) >= 10.0f) pump = 0;
        } else if (s_tm.status == GS_HOT_WATER) {
            pump = 35.0f;
        }
        if (pump < 0) pump = 0;
        if (pump > 100) pump = 100;
        s_sim.pump_f += (pump - s_sim.pump_f) * fminf(dt / 0.15f, 1.0f);
        s_tm.pump_pct = (uint8_t)lroundf(s_sim.pump_f);

        float valve = 0.0f;
        if (s_tm.status == GS_STEAM)
            valve = (s_sim.purge && now < s_sim.purge_until) ? 100.0f : s_tm.steam_power;
        s_sim.valve_f = approach(s_sim.valve_f, valve, 120.0f, dt);
        s_tm.steam_valve_pct = (uint8_t)lroundf(s_sim.valve_f);
    }

    /* --- derived status ------------------------------------------ */
    if (s_tm.status == GS_HEATING || s_tm.status == GS_READY) {
        float err = fabsf(s_tm.brew_temp - brew_set);   /* hysteresis: no flapping */
        if (s_tm.status == GS_HEATING && err < 0.8f)      s_tm.status = GS_READY;
        else if (s_tm.status == GS_READY && err > 2.0f)   s_tm.status = GS_HEATING;
    }

    s_tm.faults = s_sim.water_ok ? FAULT_NONE : FAULT_WATER_EMPTY;
}

static void sim_start(gaggia_status_t st)
{
    if (s_tm.status != GS_READY && s_tm.status != GS_HEATING) return;
    if (!s_sim.water_ok && st != GS_STEAM) return;
    s_sim.t0_ms = s_sim.last_ms;
    s_sim.absorbed_ml = 0;
    s_tm.status  = st;
    s_tm.volume  = 0;
    s_tm.flow    = 0;
    s_tm.shot_ms = 0;
    s_tm.phase   = (st == GS_BREWING) ? PH_PREINFUSION : PH_IDLE;
    s_tm.manual  = false;
    s_tm.target_bar = 0;
}

#endif /* GAGGIA_SIM */

/* ------------------------------------------------------------------ */
/* Commands                                                            */
/* ------------------------------------------------------------------ */

#if GAGGIA_SIM
static bool session_active(void)
{
    return s_tm.status == GS_BREWING || s_tm.status == GS_STEAM ||
           s_tm.status == GS_HOT_WATER || s_tm.status == GS_CLEANING;
}
#endif

void gaggia_cmd_brew(bool start)
{
    TX("tx brew %s", start ? "start" : "stop");
#if GAGGIA_SIM
    if (start) sim_start(GS_BREWING);
    else if (s_tm.status == GS_BREWING) { s_tm.phase = PH_DONE; s_tm.shot_count++; sim_end_session(); }
#else
    gaggia_link_send_brew(start ? BREW_PROFILE : BREW_STOP, 0);
#endif
}

void gaggia_cmd_brew_manual(bool start)
{
    TX("tx manual %s @ %.1f bar", start ? "start" : "stop", (double)s_tm.manual_bar);
#if GAGGIA_SIM
    if (start) {
        sim_start(GS_BREWING);
        if (s_tm.status == GS_BREWING) { s_tm.manual = true; s_tm.phase = PH_EXTRACTION; }
    } else if (s_tm.status == GS_BREWING) {
        s_tm.phase = PH_DONE; s_tm.shot_count++; sim_end_session();
    }
#else
    gaggia_link_send_brew(start ? BREW_MANUAL : BREW_STOP, s_tm.manual_bar);
#endif
}

void gaggia_cmd_manual_pressure(float bar)
{
    if (bar < 0) bar = 0;
    if (bar > GAGGIA_MANUAL_MAX_BAR) bar = GAGGIA_MANUAL_MAX_BAR;
    s_tm.manual_bar = bar;   /* echoed back by STM32 in telemetry later */
#if !GAGGIA_SIM
    gaggia_link_send_pressure(bar);
#endif
}

/* +/- buttons and presets on the Manual screen: setpoint changes, rotor follows */
void gaggia_ui_manual_pressure(float bar)
{
    gaggia_cmd_manual_pressure(bar);
    if (s_knob_mode == KNOB_PRESSURE) knob_port_set_target(knob_pct_of_current_target());
}

void gaggia_cmd_steam(bool start)
{
    TX("tx steam %s", start ? "start" : "stop");
#if GAGGIA_SIM
    if (start) sim_start(GS_STEAM);
    else if (s_tm.status == GS_STEAM) sim_end_session();
#else
    gaggia_link_send_steam(start);
#endif
}

static void steam_power_apply(uint8_t pct)
{
    if (pct > 100) pct = 100;
    s_tm.steam_power = pct;   /* requested opening; STM32 drives the PWM */
#if !GAGGIA_SIM
    static uint8_t sent = 0xFF;          /* knob streams values: send changes only */
    if (pct != sent) { sent = pct; gaggia_link_send_valve(pct); }
#endif
}

void gaggia_cmd_steam_power(uint8_t pct)
{
    steam_power_apply(pct);
    knob_port_set_target(s_tm.steam_power);   /* rotor follows the screen */
}

/* Knob -> model. Runs on the UI task from gaggia_tick(). */
static uint8_t knob_pct_of_current_target(void)
{
    if (s_knob_mode == KNOB_PRESSURE)
        return (uint8_t)lroundf(s_tm.manual_bar * 100.0f / GAGGIA_MANUAL_MAX_BAR);
    return s_tm.steam_power;
}

void gaggia_knob_set_mode(gaggia_knob_mode_t m)
{
    if (m == s_knob_mode) return;
    s_knob_mode = m;
    uint8_t dummy;
    (void)knob_input_take(&dummy);                   /* drop a stale value */
    knob_port_set_target(knob_pct_of_current_target());
    glog(LOG_I, "knob -> %s", m == KNOB_PRESSURE ? "pressure" : "steam valve");
}

gaggia_knob_mode_t gaggia_knob_mode(void) { return s_knob_mode; }

static void knob_poll(uint32_t now)
{
    uint8_t pct;
    if (!knob_input_take(&pct)) return;
    knob_input_mark(now);

    if (s_knob_mode == KNOB_PRESSURE) {
        float bar = roundf(pct * GAGGIA_MANUAL_MAX_BAR / 100.0f * 10.0f) / 10.0f;  /* 0.1 bar */
        if (fabsf(bar - s_tm.manual_bar) >= 0.05f) gaggia_cmd_manual_pressure(bar);
        return;   /* never starts steam in this mode */
    }

    steam_power_apply(pct);

    if (!s_store.settings.knob_auto_steam) return;
    /* Edge-triggered, like the original handle: start on closed->open, stop
       on open->closed. Never decide from s_tm.status: with the real link the
       STM32 reports GS_STEAM a frame or two later, and every knob tick in
       between would resend "start". */
    bool open = pct >= GAGGIA_KNOB_DEADBAND;
    static bool was_open;
    if (open == was_open) return;
    was_open = open;
    glog(LOG_I, "knob %s (%u%%)", open ? "opened" : "closed", pct);
    if (open  && (s_tm.status == GS_READY || s_tm.status == GS_HEATING)) gaggia_cmd_steam(true);
    if (!open && s_tm.status == GS_STEAM)                                gaggia_cmd_steam(false);
}

bool gaggia_knob_recent(uint32_t window_ms)
{
    uint32_t t = knob_input_last_ms();
    return t && (s_tick_ms - t) < window_ms;
}

void gaggia_cmd_purge(void)
{
    TX("tx purge");
#if !GAGGIA_SIM
    gaggia_link_send_purge(1500);
#endif
#if GAGGIA_SIM
    if (s_tm.status == GS_STEAM) { s_sim.purge = true; s_sim.purge_until = s_sim.last_ms + 1500; }
#endif
}

void gaggia_cmd_hot_water(bool start)
{
    TX("tx hot water %s %u ml", start ? "start" : "stop", s_store.settings.hot_water_ml);
#if GAGGIA_SIM
    if (start) sim_start(GS_HOT_WATER);
    else if (s_tm.status == GS_HOT_WATER) sim_end_session();
#else
    gaggia_link_send_hot_water(start, s_store.settings.hot_water_ml);
#endif
}

void gaggia_cmd_backflush(bool start)
{
    TX("tx backflush %s", start ? "start" : "stop");
#if GAGGIA_SIM
    if (start) { sim_start(GS_CLEANING); s_tm.clean_total = 5; s_tm.clean_cycle = 1; }
    else if (s_tm.status == GS_CLEANING) sim_end_session();
#else
    gaggia_link_send_backflush(start);
#endif
}

void gaggia_cmd_standby(bool enter)
{
    TX("tx standby %s", enter ? "enter" : "exit");
#if GAGGIA_SIM
    if (enter) { if (session_active()) return; s_tm.status = GS_STANDBY; }
    else if (s_tm.status == GS_STANDBY) s_tm.status = GS_HEATING;
#else
    gaggia_link_send_standby(enter);
#endif
}

void gaggia_cmd_reset_shot_counter(void)
{
    TX("tx reset shot counter");
#if GAGGIA_SIM
    s_tm.shot_count = 0;
#else
    gaggia_link_send_reset_counter();   /* counter lives on the STM32 */
#endif
}

/* ------------------------------------------------------------------ */

void gaggia_init(const gaggia_store_t *saved)
{
    if (saved && saved->magic == GAGGIA_STORE_MAGIC &&
        saved->version == GAGGIA_STORE_VERSION &&
        saved->profile_count > 0 && saved->profile_count <= GAGGIA_MAX_PROFILES) {
        s_store = *saved;
        if (s_store.settings.active_profile >= s_store.profile_count)
            s_store.settings.active_profile = 0;
    } else {
        load_defaults();
    }
    s_dirty = false;
    glog(LOG_I, "boot fw %s, %u profiles, store %s", GAGGIA_FW_VERSION, s_store.profile_count,
         (saved && s_store.magic == saved->magic && saved->version == GAGGIA_STORE_VERSION) ? "loaded" : "defaults");
#if GAGGIA_SIM
    sim_init();
#else
    memset(&s_tm, 0, sizeof(s_tm));
    s_tm.status = GS_BOOT;
#endif
}

static void track_changes(void)
{
    const gaggia_telemetry_t *t = &s_tm;

    if (t->status == GS_BREWING && t->pressure > s_shot_peak_bar) s_shot_peak_bar = t->pressure;

    if (t->status != s_prev_status) {
        glog(LOG_I, "status %s -> %s", gaggia_status_str(s_prev_status), gaggia_status_str(t->status));
        if (t->status == GS_BREWING) s_shot_peak_bar = 0;
        s_prev_status = t->status;
    }
    if (t->phase != s_prev_phase) {
        if (t->phase == PH_DONE)
            glog(LOG_I, "shot done %.1f s, %.0f ml, peak %.1f bar%s",
                 t->shot_ms / 1000.0, (double)t->volume, (double)s_shot_peak_bar,
                 t->manual ? " (manual)" : "");
        else if (t->phase != PH_IDLE)
            glog(LOG_I, "phase -> %s @ %.1f s", gaggia_phase_str(t->phase), t->shot_ms / 1000.0);
        s_prev_phase = t->phase;
    }
    if (t->faults != s_prev_faults) {
        uint16_t up = t->faults & ~s_prev_faults, down = s_prev_faults & ~t->faults;
        if (up)   glog(LOG_E, "fault set 0x%02X%s", up, (up & FAULT_WATER_EMPTY) ? " (water empty)" : "");
        if (down) glog(LOG_W, "fault cleared 0x%02X", down);
        s_prev_faults = t->faults;
    }
    if (t->link_ok != s_prev_link) {
        glog(t->link_ok ? LOG_I : LOG_E, "stm32 link %s", t->link_ok ? "up" : "LOST");
        s_prev_link = t->link_ok;
    }
}

void gaggia_tick(uint32_t now_ms)
{
    s_tick_ms = now_ms;
    glog_set_now(now_ms);
    knob_poll(now_ms);
#if GAGGIA_SIM
    sim_step(now_ms);
    /* pretend a telemetry frame arrives every 50 ms */
    while (now_ms - s_link.last_rx_ms >= 50) { s_link.rx_frames++; s_link.last_rx_ms += 50; }
#else
    gaggia_link_tick(now_ms);
    if (s_link_seen && now_ms - s_link.last_rx_ms > GAGGIA_LINK_TIMEOUT_MS && s_tm.link_ok) {
        s_tm.link_ok = false;
        s_tm.faults |= FAULT_LINK_LOST;
        s_link.timeouts++;
    }
#endif
    track_changes();
    glog_csv_tick(now_ms, &s_tm);
}
