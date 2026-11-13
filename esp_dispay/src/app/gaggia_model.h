/**
 * gaggia_model.h — machine state as seen by the ESP32 (UI side).
 *
 * The UI never talks to UART directly. It reads a telemetry snapshot and
 * calls gaggia_cmd_*(). Today the backend is a simulator (GAGGIA_SIM=1);
 * later gaggia_on_telemetry() is fed by the STM32 link task and the
 * gaggia_cmd_*() functions serialize packets instead.
 *
 * Threading: all functions are meant to be called from the LVGL/UI task.
 * When the UART RX task appears, guard gaggia_on_telemetry() + the copy in
 * gaggia_tm() with a mutex (see GAGGIA_LOCK/UNLOCK in gaggia_model.c).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GAGGIA_SIM
#define GAGGIA_SIM 1
#endif

#define GAGGIA_FW_VERSION    "0.1.0-ui"
#define GAGGIA_MAX_PROFILES  6
#define GAGGIA_NAME_LEN      20
#define GAGGIA_MANUAL_MAX_BAR 12.0f
#define GAGGIA_MANUAL_MAX_S   90      /* safety stop for manual shots */

typedef enum {
    GS_BOOT = 0,     /* waiting for STM32 link              */
    GS_HEATING,
    GS_READY,
    GS_BREWING,
    GS_STEAM,
    GS_HOT_WATER,
    GS_CLEANING,
    GS_STANDBY,
    GS_FAULT,
} gaggia_status_t;

typedef enum {
    PH_IDLE = 0,
    PH_PREINFUSION,
    PH_SOAK,
    PH_EXTRACTION,
    PH_DONE,
} brew_phase_t;

typedef enum {
    FAULT_NONE          = 0,
    FAULT_WATER_EMPTY   = 1u << 0,
    FAULT_BREW_SENSOR   = 1u << 1,
    FAULT_STEAM_SENSOR  = 1u << 2,
    FAULT_OVERTEMP      = 1u << 3,
    FAULT_LINK_LOST     = 1u << 4,
} gaggia_fault_t;

/* Everything the STM32 reports (~50 ms) + a few ESP-local fields. */
typedef struct {
    gaggia_status_t status;
    brew_phase_t    phase;
    float    brew_temp;      /* °C            */
    float    steam_temp;     /* °C            */
    float    preheat_temp;   /* °C, preheat boiler */
    bool     preheat_heater_on;
    float    pressure;       /* bar           */
    float    flow;           /* ml/s          */
    float    volume;         /* ml, this shot / this hot-water run */
    float    target_bar;     /* profile target at current shot time */
    uint32_t shot_ms;        /* elapsed in current shot / session  */
    uint8_t  steam_power;    /* 0..100 %, requested valve opening (smart knob / UI) */
    uint8_t  steam_valve_pct;/* 0..100 %, actual PWM duty on the steam valve MOSFET */
    uint8_t  pump_pct;       /* 0..100 %, BLDC pump PWM duty                        */
    bool     manual;         /* current brew is a manual (live pressure) shot       */
    float    manual_bar;     /* manual mode pressure setpoint                       */
    uint8_t  clean_cycle;    /* backflush: current cycle, 1-based   */
    uint8_t  clean_total;
    bool     brew_heater_on;
    bool     steam_heater_on;
    bool     link_ok;
    uint16_t faults;         /* gaggia_fault_t bitmask */
    uint32_t shot_count;     /* lifetime */
    uint32_t uptime_s;
} gaggia_telemetry_t;

typedef struct {
    char     name[GAGGIA_NAME_LEN];
    float    preinf_s;       /* pre-infusion duration          */
    float    preinf_bar;     /* pre-infusion pressure          */
    float    soak_s;         /* pump off, puck blooms          */
    float    brew_bar;       /* extraction pressure at start   */
    float    end_bar;        /* ...declining linearly to this  */
    float    temp_c;         /* brew setpoint for this profile */
    uint16_t target_ml;      /* stop at this cup volume        */
    uint16_t max_s;          /* safety stop                    */
} brew_profile_t;

typedef struct {
    float    steam_set;      /* °C */
    float    preheat_set;    /* °C, preheat boiler setpoint */
    bool     preheat_on;     /* preheat boiler enabled */
    float    temp_offset;    /* group head offset, °C */
    float    kp, ki, kd;     /* brew boiler PID       */
    uint16_t hot_water_ml;
    uint8_t  brightness;     /* 10..100 % */
    uint16_t dim_after_s;    /* 0 = never */
    uint16_t auto_off_min;   /* 0 = never */
    bool     sound;
    bool     eco;            /* steam boiler off until requested */
    bool     knob_auto_steam;/* turning the knob off 0 starts steam, back to 0 stops */
    bool     touch_arcs;     /* arcs draggable by finger; off = smart knob only */
    uint8_t  active_profile;
} gaggia_settings_t;

/* Blob that goes to NVS as-is. Bump GAGGIA_STORE_VERSION on layout change. */
#define GAGGIA_STORE_MAGIC   0x47474131u   /* "GGA1" */
#define GAGGIA_STORE_VERSION 4   /* v2 knob_auto_steam, v3 preheat, v4 touch_arcs */
typedef struct {
    uint32_t          magic;
    uint16_t          version;
    uint8_t           profile_count;
    gaggia_settings_t settings;
    brew_profile_t    profiles[GAGGIA_MAX_PROFILES];
} gaggia_store_t;

/* ---- lifecycle ---- */
void gaggia_init(const gaggia_store_t *saved_or_null);
void gaggia_tick(uint32_t now_ms);            /* call often (UI timer) */

/* ---- state ---- */
const gaggia_telemetry_t *gaggia_tm(void);
void gaggia_on_telemetry(const gaggia_telemetry_t *t); /* link RX path */

/* ---- settings / profiles (UI edits in place, then commit) ---- */
gaggia_settings_t *gaggia_settings(void);
uint8_t            gaggia_profile_count(void);
brew_profile_t    *gaggia_profile(uint8_t idx);
brew_profile_t    *gaggia_active_profile(void);
void               gaggia_select_profile(uint8_t idx);
void               gaggia_commit(void);        /* mark dirty + push to STM32 */
float              gaggia_brew_setpoint(void); /* active profile temp + offset */

/* ---- persistence helpers ---- */
const gaggia_store_t *gaggia_store(void);
bool  gaggia_store_dirty(void);
void  gaggia_store_clear_dirty(void);

/* ---- commands ---- */
void gaggia_cmd_brew(bool start);
void gaggia_cmd_brew_manual(bool start);        /* pump holds manual_bar until stopped */
void gaggia_cmd_manual_pressure(float bar);     /* live setpoint, 0..12 bar            */
void gaggia_ui_manual_pressure(float bar);      /* same, from touch: also moves the rotor */
void gaggia_cmd_steam(bool start);
void gaggia_cmd_steam_power(uint8_t pct);   /* from the touchscreen; knob follows */
void gaggia_cmd_purge(void);
void gaggia_cmd_hot_water(bool start);
void gaggia_cmd_backflush(bool start);
void gaggia_cmd_standby(bool enter);
void gaggia_cmd_reset_shot_counter(void);

/* ---- preheat boiler limits ---- */
#define GAGGIA_PREHEAT_MIN 40.0f
#define GAGGIA_PREHEAT_MAX 95.0f

/* ---- STM32 link statistics (debug screen) ---- */
typedef struct {
    uint32_t rx_frames;
    uint32_t crc_errors;
    uint32_t timeouts;
    uint32_t tx_cmds;
    uint32_t last_rx_ms;
} gaggia_link_stats_t;

const gaggia_link_stats_t *gaggia_link_stats(void);
void gaggia_link_crc_error(void);   /* call from the frame parser */

#define GAGGIA_LINK_TIMEOUT_MS 500

/* ---- smart knob ----
 * One physical knob, two jobs. The UI picks the job by screen:
 *   KNOB_STEAM     valve opening 0..100 %   (default, everywhere)
 *   KNOB_PRESSURE  manual pressure 0..GAGGIA_MANUAL_MAX_BAR (Manual screen)
 * On every mode switch the rotor is moved to the current value of the new
 * target (knob_port_set_target), so the knob never "jumps" the setpoint. */
typedef enum { KNOB_STEAM = 0, KNOB_PRESSURE } gaggia_knob_mode_t;

#define GAGGIA_KNOB_DEADBAND 3   /* % — below this the knob counts as closed */
void               gaggia_knob_set_mode(gaggia_knob_mode_t m);
gaggia_knob_mode_t gaggia_knob_mode(void);
bool               gaggia_knob_recent(uint32_t window_ms);   /* knob moved within window */

/* ---- helpers ---- */
float       gaggia_profile_target_bar(const brew_profile_t *p, float t_s);
float       gaggia_profile_duration(const brew_profile_t *p); /* nominal */
const char *gaggia_status_str(gaggia_status_t s);
const char *gaggia_phase_str(brew_phase_t p);

#if GAGGIA_SIM
void gaggia_sim_set_water(bool ok);
#endif

#ifdef __cplusplus
}
#endif
