#include "gaggia_log.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define GLOG_LOCK()   do {} while (0)
#define GLOG_UNLOCK() do {} while (0)

static log_entry_t s_ring[GLOG_SIZE];
static uint16_t    s_head;      /* next write slot */
static uint16_t    s_count;
static uint32_t    s_version;
static uint32_t    s_now;
static glog_sink_t s_sink;

static csv_rate_t  s_csv = CSV_OFF;
static uint32_t    s_csv_last;
static bool        s_csv_header;

static const char k_lv_chr[] = { 'I', 'W', 'E' };

void glog_set_now(uint32_t now_ms)  { s_now = now_ms; }
void glog_set_sink(glog_sink_t f)   { s_sink = f; }
uint32_t glog_version(void)         { return s_version; }
uint16_t glog_count(void)           { return s_count; }

const log_entry_t *glog_get(uint16_t i)
{
    if (i >= s_count) return NULL;
    uint16_t oldest = (uint16_t)((s_head + GLOG_SIZE - s_count) % GLOG_SIZE);
    return &s_ring[(oldest + i) % GLOG_SIZE];
}

void glog_clear(void)
{
    GLOG_LOCK();
    s_head = s_count = 0;
    s_version++;
    GLOG_UNLOCK();
}

void glog(log_level_t lv, const char *fmt, ...)
{
    GLOG_LOCK();
    log_entry_t *e = &s_ring[s_head];
    e->ms = s_now;
    e->level = (uint8_t)lv;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->msg, sizeof(e->msg), fmt, ap);
    va_end(ap);
    for (char *c = e->msg; *c; c++) if (*c == '#') *c = ' ';  /* '#' = LVGL recolor marker */

    s_head = (uint16_t)((s_head + 1) % GLOG_SIZE);
    if (s_count < GLOG_SIZE) s_count++;
    s_version++;
    GLOG_UNLOCK();

    if (s_sink) {
        char line[GLOG_MSG_LEN + 24];
        snprintf(line, sizeof(line), "# %c %lu.%03lu %s", k_lv_chr[lv % 3],
                 (unsigned long)(e->ms / 1000), (unsigned long)(e->ms % 1000), e->msg);
        s_sink(line);
    }
}

/* ------------------------------------------------------------------ */
/* CSV                                                                 */
/* ------------------------------------------------------------------ */

static const uint16_t k_csv_period[CSV_RATE_COUNT] = { 0, 1000, 200, 50 };
static const char    *k_csv_name[CSV_RATE_COUNT]   = { "CSV off", "CSV 1 Hz", "CSV 5 Hz", "CSV 20 Hz" };

void glog_csv_set(csv_rate_t r)
{
    if (r >= CSV_RATE_COUNT) r = CSV_OFF;
    if (s_csv == CSV_OFF) s_csv_header = false;   /* header once per enable */
    s_csv = r;
    glog(LOG_I, "serial csv: %s", k_csv_name[r]);
}

csv_rate_t  glog_csv_get(void)            { return s_csv; }
const char *glog_csv_name(csv_rate_t r)   { return r < CSV_RATE_COUNT ? k_csv_name[r] : "?"; }

void glog_csv_tick(uint32_t now, const gaggia_telemetry_t *t)
{
    if (s_csv == CSV_OFF || !s_sink) return;
    if (now - s_csv_last < k_csv_period[s_csv]) return;
    s_csv_last = now;

    char line[200];
    if (!s_csv_header) {
        s_csv_header = true;
        s_sink("t_ms,status,phase,brew_c,steam_c,preheat_c,bar,target_bar,flow_mls,vol_ml,"
               "pump_pct,valve_pct,knob_pct,heaters,faults,manual,manual_bar");
    }
    snprintf(line, sizeof(line),
             "%lu,%d,%d,%.2f,%.1f,%.1f,%.2f,%.2f,%.2f,%.1f,%u,%u,%u,%u,%u,%d,%.1f",
             (unsigned long)now, (int)t->status, (int)t->phase,
             (double)t->brew_temp, (double)t->steam_temp, (double)t->preheat_temp,
             (double)t->pressure, (double)t->target_bar, (double)t->flow, (double)t->volume,
             t->pump_pct, t->steam_valve_pct, t->steam_power,
             (unsigned)(t->brew_heater_on | (t->steam_heater_on << 1) | (t->preheat_heater_on << 2)),
             t->faults, t->manual, (double)t->manual_bar);
    s_sink(line);
}
