/**
 * scr_debug.c — telemetry & event log for debugging.
 * Left: every telemetry field raw, live. Right: event log (newest at the
 * bottom, auto-scroll unless you scrolled up). Header: serial CSV rate,
 * pause, clear. Open from Settings > Service or long-press the status text.
 */
#include "ui.h"
#include "ui_screens.h"
#include "ui_widgets.h"
#include "gaggia_log.h"

#include <stdio.h>
#include <string.h>

#define LOG_SHOWN 40               /* lines rendered (ring keeps GLOG_SIZE) */

static lv_obj_t *s_vals, *s_log_box, *s_log_lbl;
static lv_obj_t *s_csv_lbl, *s_pause_lbl, *s_rate_lbl;
static bool      s_paused;
static uint32_t  s_log_ver = UINT32_MAX;
static uint32_t  s_rate_t0, s_rate_f0, s_fps;

static char s_buf[LOG_SHOWN * 84 + 1];   /* static: too big for the stack */

/* ---- header actions ---- */

static void back_cb(lv_event_t *e)  { LV_UNUSED(e); ui_show(SCR_SETTINGS); }
static void clear_cb(lv_event_t *e) { LV_UNUSED(e); glog_clear(); }

static void csv_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    glog_csv_set((csv_rate_t)((glog_csv_get() + 1) % CSV_RATE_COUNT));
    lv_label_set_text(s_csv_lbl, glog_csv_name(glog_csv_get()));
}

static void pause_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    s_paused = !s_paused;
    lv_label_set_text(s_pause_lbl, s_paused ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);
    lv_obj_set_style_text_color(s_pause_lbl, s_paused ? C_WARN : C_TEXT, 0);
}

static lv_obj_t *hdr_btn(lv_obj_t *scr, lv_coord_t x, lv_coord_t w, const char *txt,
                         lv_event_cb_t cb, lv_obj_t **lbl_out)
{
    lv_obj_t *b = ui_btn(scr, NULL, NULL, cb, NULL);
    lv_obj_set_pos(b, x, UI_BAR_H + 2);
    lv_obj_set_size(b, w, 34);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_t *l = ui_label(b, F_XS, C_TEXT, txt);
    lv_obj_center(l);
    if (lbl_out) *lbl_out = l;
    return b;
}

/* ---- build ---- */

lv_obj_t *scr_debug_create(void)
{
    lv_obj_t *scr = ui_screen_base();

    hdr_btn(scr, UI_PAD, 44, LV_SYMBOL_LEFT, back_cb, NULL);
    lv_obj_t *t = ui_label(scr, F_M, C_TEXT, "Telemetry");
    lv_obj_set_pos(t, UI_PAD + 54, UI_BAR_H + 10);
    s_rate_lbl = ui_label(scr, F_XS, C_TEXT_MUTED, "");
    lv_obj_set_pos(s_rate_lbl, UI_PAD + 148, UI_BAR_H + 13);

    hdr_btn(scr, 262, 84, "", csv_cb, &s_csv_lbl);
    hdr_btn(scr, 352, 52, LV_SYMBOL_PAUSE, pause_cb, &s_pause_lbl);
    hdr_btn(scr, 410, 60, LV_SYMBOL_TRASH, clear_cb, NULL);

    const lv_coord_t y = UI_BAR_H + 42;
    const lv_coord_t h = UI_H - y - 6;

    /* live values: two aligned multi-line labels */
    lv_obj_t *vc = ui_card(scr, UI_PAD, y, 196, h, NULL, NULL);
    lv_obj_set_style_pad_all(vc, 8, 0);
    lv_obj_set_style_radius(vc, UI_R_SMALL, 0);
    lv_obj_add_flag(vc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(vc, LV_SCROLLBAR_MODE_OFF);
    lv_obj_t *keys = ui_label(vc, F_XS, C_TEXT_MUTED,
        "status\nphase\nbrew\nsteam\npreheat\npressure\nflow/vol\npump/valve\n"
        "knob req\nheaters\nfaults\nshot/up\nlink rx\ncrc/t-out\ntx cmds");
    lv_obj_set_pos(keys, 0, 0);
    s_vals = ui_label(vc, F_XS, C_TEXT, "");
    lv_obj_set_pos(s_vals, 80, 0);

    /* event log */
    s_log_box = ui_card(scr, UI_PAD + 204, y, UI_W - UI_PAD * 2 - 204, h, NULL, NULL);
    lv_obj_set_style_pad_all(s_log_box, 8, 0);
    lv_obj_set_style_radius(s_log_box, UI_R_SMALL, 0);
    lv_obj_add_flag(s_log_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_log_box, LV_DIR_VER);
    lv_obj_set_style_bg_color(s_log_box, C_CREMA_DEEP, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(s_log_box, 3, LV_PART_SCROLLBAR);
    s_log_lbl = ui_label(s_log_box, F_XS, C_TEXT, "");
    lv_label_set_recolor(s_log_lbl, true);
    lv_label_set_long_mode(s_log_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_log_lbl, LV_PCT(100));

    return scr;
}

void scr_debug_enter(void)
{
    lv_label_set_text(s_csv_lbl, glog_csv_name(glog_csv_get()));
    s_log_ver = UINT32_MAX;   /* force rebuild */
}

/* ---- update ---- */

static void render_values(const gaggia_telemetry_t *t)
{
    const gaggia_settings_t   *st = gaggia_settings();
    const gaggia_link_stats_t *ls = gaggia_link_stats();
    char v[512];
    snprintf(v, sizeof(v),
             "%s (%d)\n%s (%d)%s\n"
             "%.2f / %.1f °C\n%.1f / %.0f °C\n%.1f / %.0f °C%s\n"
             "%.2f / %.2f bar\n%.2f / %.1f ml\n%u%% / %u%%\n%u%%\n"
             "B%d S%d P%d\n0x%04X\n%.1f / %lu s\n"
             "%lu (%lu fps)\n%lu / %lu\n%lu",
             gaggia_status_str(t->status), (int)t->status,
             gaggia_phase_str(t->phase), (int)t->phase, t->manual ? " man" : "",
             (double)t->brew_temp, (double)gaggia_brew_setpoint(),
             (double)t->steam_temp, (double)st->steam_set,
             (double)t->preheat_temp, (double)st->preheat_set, st->preheat_on ? "" : " off",
             (double)t->pressure, (double)t->target_bar,
             (double)t->flow, (double)t->volume,
             t->pump_pct, t->steam_valve_pct, t->steam_power,
             t->brew_heater_on, t->steam_heater_on, t->preheat_heater_on,
             t->faults, t->shot_ms / 1000.0, (unsigned long)t->uptime_s,
             (unsigned long)ls->rx_frames, (unsigned long)s_fps,
             (unsigned long)ls->crc_errors, (unsigned long)ls->timeouts,
             (unsigned long)ls->tx_cmds);
    if (strcmp(lv_label_get_text(s_vals), v) != 0) lv_label_set_text(s_vals, v);
}

static void render_log(void)
{
    static const char *k_lv[] = { "8BC48A I", "E9B949 W", "E5534B E" };
    uint16_t n = glog_count();
    uint16_t first = n > LOG_SHOWN ? n - LOG_SHOWN : 0;
    size_t len = 0;
    s_buf[0] = 0;

    for (uint16_t i = first; i < n; i++) {
        const log_entry_t *e = glog_get(i);
        int w = snprintf(s_buf + len, sizeof(s_buf) - len, "%s#6B5B50 %lu.%02lu# #%s# %s",
                         len ? "\n" : "",
                         (unsigned long)(e->ms / 1000), (unsigned long)(e->ms % 1000 / 10),
                         k_lv[e->level % 3], e->msg);
        if (w < 0 || (size_t)w >= sizeof(s_buf) - len) break;
        len += (size_t)w;
    }
    if (!n) snprintf(s_buf, sizeof(s_buf), "#6B5B50 log empty#");

    /* keep position if the user scrolled up to read */
    bool at_bottom = lv_obj_get_scroll_bottom(s_log_box) <= 8;
    lv_label_set_text(s_log_lbl, s_buf);
    lv_obj_update_layout(s_log_box);
    if (at_bottom) lv_obj_scroll_by(s_log_box, 0, -lv_obj_get_scroll_bottom(s_log_box), LV_ANIM_OFF);
}

void scr_debug_update(const gaggia_telemetry_t *t)
{
    /* telemetry frame rate, once per second */
    uint32_t now = lv_tick_get();
    if (now - s_rate_t0 >= 1000) {
        uint32_t f = gaggia_link_stats()->rx_frames;
        s_fps = (f - s_rate_f0) * 1000 / (now - s_rate_t0 ? now - s_rate_t0 : 1);
        s_rate_f0 = f; s_rate_t0 = now;
        ui_set_text_fmt_if_changed(s_rate_lbl, "%lu entries", (unsigned long)glog_count());
    }

    if (s_paused) return;
    render_values(t);
    if (glog_version() != s_log_ver) {
        s_log_ver = glog_version();
        render_log();
    }
}
