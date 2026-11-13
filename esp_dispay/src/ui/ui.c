/**
 * ui.c — screen manager + persistent chrome (status bar, bottom nav,
 * fault banner, standby curtain) living on lv_layer_top().
 */
#include "ui.h"
#include "ui_hal.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_screens.h"
#include "gaggia_model.h"

#include <stdio.h>

#define UI_TICK_MS 100

typedef struct {
    lv_obj_t *(*create)(void);
    void      (*enter)(void);
    void      (*update)(const gaggia_telemetry_t *t);
    bool       show_nav;
    lv_obj_t  *obj;
} screen_t;

static screen_t s_screens[SCR_COUNT] = {
    [SCR_HOME]     = { scr_home_create,     scr_home_enter,     scr_home_update,     true  },
    [SCR_BREW]     = { scr_brew_create,     scr_brew_enter,     scr_brew_update,     false },
    [SCR_MANUAL]   = { scr_manual_create,   scr_manual_enter,   scr_manual_update,   false },
    [SCR_PROFILES] = { scr_profiles_create, scr_profiles_enter, scr_profiles_update, true  },
    [SCR_STEAM]    = { scr_steam_create,    scr_steam_enter,    scr_steam_update,    true  },
    [SCR_WATER]    = { scr_water_create,    scr_water_enter,    scr_water_update,    true  },
    [SCR_SETTINGS] = { scr_settings_create, scr_settings_enter, scr_settings_update, true  },
    [SCR_DEBUG]    = { scr_debug_create,    scr_debug_enter,    scr_debug_update,    false },
};

static ui_screen_id_t s_cur = SCR_COUNT;

/* chrome */
static lv_obj_t *s_bar, *s_dot, *s_status, *s_clock, *s_water, *s_link, *s_power;
static lv_obj_t *s_nav, *s_nav_btn[5], *s_nav_pill[5];
static lv_obj_t *s_banner, *s_banner_lbl, *s_banner_btn;
static lv_obj_t *s_curtain, *s_curtain_clock;

static gaggia_status_t s_prev_status = GS_BOOT;
static bool s_dimmed;

/* nav slot -> screen */
static const ui_screen_id_t k_nav_target[5] = {
    SCR_HOME, SCR_PROFILES, SCR_STEAM, SCR_WATER, SCR_SETTINGS
};

/* ------------------------------------------------------------------ */

lv_obj_t *ui_screen_base(void)
{
    lv_obj_t *s = lv_obj_create(NULL);
    lv_obj_remove_style_all(s);
    lv_obj_add_style(s, &st_screen, 0);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    return s;
}

static int nav_slot_of(ui_screen_id_t id)
{
    for (int i = 0; i < 5; i++) if (k_nav_target[i] == id) return i;
    return -1;
}

static void nav_highlight(ui_screen_id_t id)
{
    int slot = nav_slot_of(id);
    for (int i = 0; i < 5; i++) {
        bool on = (i == slot);
        lv_color_t c = on ? C_CREMA : C_TEXT_MUTED;
        lv_obj_set_style_text_color(s_nav_btn[i], c, 0);
        /* steam icon is lines, not text */
        lv_obj_t *icon = lv_obj_get_child(s_nav_btn[i], 0);
        uint32_t n = lv_obj_get_child_cnt(icon);
        for (uint32_t k = 0; k < n; k++)
            lv_obj_set_style_line_color(lv_obj_get_child(icon, k), c, 0);
        if (on) lv_obj_clear_flag(s_nav_pill[i], LV_OBJ_FLAG_HIDDEN);
        else    lv_obj_add_flag(s_nav_pill[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_show(ui_screen_id_t id)
{
    if (id >= SCR_COUNT || id == s_cur) return;
    screen_t *sc = &s_screens[id];
    s_cur = id;

    if (sc->show_nav) lv_obj_clear_flag(s_nav, LV_OBJ_FLAG_HIDDEN);
    else              lv_obj_add_flag(s_nav, LV_OBJ_FLAG_HIDDEN);
    nav_highlight(id);

    /* the smart knob sets pressure on the Manual screen, steam valve elsewhere */
    gaggia_knob_set_mode(id == SCR_MANUAL ? KNOB_PRESSURE : KNOB_STEAM);

    if (sc->enter) sc->enter();
    lv_scr_load_anim(sc->obj, LV_SCR_LOAD_ANIM_FADE_ON, 160, 0, false);
}

ui_screen_id_t ui_current(void) { return s_cur; }

/* ------------------------------------------------------------------ */
/* Status bar                                                          */
/* ------------------------------------------------------------------ */

static void standby_yes(void *u) { LV_UNUSED(u); gaggia_cmd_standby(true); }

static void power_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_confirm("Standby", "Heaters off, display dimmed. Touch the screen to wake the machine.",
               "Standby", false, standby_yes, NULL);
}

static void status_long_cb(lv_event_t *e) { LV_UNUSED(e); ui_show(SCR_DEBUG); }

static void build_status_bar(void)
{
    s_bar = ui_panel(lv_layer_top(), 0, 0, UI_W, UI_BAR_H);
    lv_obj_set_style_bg_color(s_bar, C_BG, 0);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, 0);

    s_dot = lv_obj_create(s_bar);
    lv_obj_remove_style_all(s_dot);
    lv_obj_set_size(s_dot, 9, 9);
    lv_obj_set_style_radius(s_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_dot, C_WARN, 0);
    lv_obj_align(s_dot, LV_ALIGN_LEFT_MID, 14, 0);

    s_status = ui_label(s_bar, F_S, C_TEXT, "Connecting");
    lv_obj_align(s_status, LV_ALIGN_LEFT_MID, 30, 0);
    /* hidden shortcut: long-press the status text -> telemetry log */
    lv_obj_add_flag(s_status, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_status, 12);
    lv_obj_add_event_cb(s_status, status_long_cb, LV_EVENT_LONG_PRESSED, NULL);

    s_clock = ui_label(s_bar, F_M, C_TEXT, "");
    lv_obj_align(s_clock, LV_ALIGN_CENTER, 0, 0);

    s_power = lv_btn_create(s_bar);
    lv_obj_remove_style_all(s_power);
    lv_obj_set_size(s_power, 40, UI_BAR_H);
    lv_obj_align(s_power, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_text_color(s_power, C_TEXT_DIM, 0);
    lv_obj_set_style_text_color(s_power, C_CREMA, LV_STATE_PRESSED);
    lv_obj_add_event_cb(s_power, power_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *pl = lv_label_create(s_power);
    lv_label_set_text(pl, LV_SYMBOL_POWER);
    lv_obj_center(pl);

    s_link = ui_label(s_bar, F_XS, C_TEXT_MUTED, "STM32");
    lv_obj_align(s_link, LV_ALIGN_RIGHT_MID, -52, 0);

    s_water = ui_label(s_bar, F_S, C_PRESS, LV_SYMBOL_TINT);
    lv_obj_align(s_water, LV_ALIGN_RIGHT_MID, -110, 0);
}

static lv_color_t status_color(gaggia_status_t s)
{
    switch (s) {
    case GS_READY:     return C_OK;
    case GS_HEATING:   return C_TEMP;
    case GS_BREWING:   return C_CREMA;
    case GS_STEAM:     return C_STEAM;
    case GS_HOT_WATER: return C_PRESS;
    case GS_CLEANING:  return C_PRESS;
    case GS_FAULT:     return C_ERR;
    default:           return C_WARN;
    }
}

static void update_status_bar(const gaggia_telemetry_t *t)
{
    lv_obj_set_style_bg_color(s_dot, status_color(t->status), 0);

    if (t->status == GS_HEATING) {
        float set = gaggia_brew_setpoint();
        int pct = (int)((t->brew_temp - 20.0f) * 100.0f / (set - 20.0f));
        if (pct < 0) pct = 0;
        if (pct > 99) pct = 99;
        ui_set_text_fmt_if_changed(s_status, "Heating  %d%%", pct);
    } else {
        ui_set_text_fmt_if_changed(s_status, "%s", gaggia_status_str(t->status));
    }

    int h, m;
    if (ui_hal_get_time(&h, &m)) ui_set_text_fmt_if_changed(s_clock, "%02d:%02d", h, m);
    else                         ui_set_text_fmt_if_changed(s_clock, "%s", "");

    bool water_ok = !(t->faults & FAULT_WATER_EMPTY);
    lv_obj_set_style_text_color(s_water, water_ok ? C_PRESS : C_ERR, 0);
    lv_obj_set_style_text_color(s_link, t->link_ok ? C_TEXT_DIM : C_ERR, 0);
}

/* ------------------------------------------------------------------ */
/* Bottom navigation                                                   */
/* ------------------------------------------------------------------ */

static void nav_cb(lv_event_t *e)
{
    int slot = (int)(intptr_t)lv_event_get_user_data(e);
    ui_show(k_nav_target[slot]);
}

static void build_nav(void)
{
    static const char *icons[5]  = { LV_SYMBOL_HOME, LV_SYMBOL_LIST, NULL, LV_SYMBOL_TINT, LV_SYMBOL_SETTINGS };
    static const char *labels[5] = { "Home", "Profiles", "Steam", "Water", "Settings" };

    s_nav = ui_panel(lv_layer_top(), 0, UI_H - UI_NAV_H, UI_W, UI_NAV_H);
    lv_obj_set_style_bg_color(s_nav, C_BG, 0);
    lv_obj_set_style_bg_opa(s_nav, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(s_nav, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(s_nav, 1, 0);
    lv_obj_set_style_border_color(s_nav, C_LINE, 0);

    const lv_coord_t w = UI_W / 5;
    for (int i = 0; i < 5; i++) {
        lv_obj_t *b = lv_btn_create(s_nav);
        lv_obj_remove_style_all(b);
        lv_obj_set_size(b, w, UI_NAV_H);
        lv_obj_set_pos(b, i * w, 0);
        lv_obj_set_style_bg_color(b, C_SURFACE, LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_add_event_cb(b, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        s_nav_btn[i] = b;

        lv_obj_t *icon;
        if (icons[i]) {
            icon = lv_label_create(b);
            lv_obj_set_style_text_font(icon, F_L, 0);
            lv_label_set_text(icon, icons[i]);
        } else {
            icon = ui_steam_icon(b, C_TEXT_MUTED);
        }
        lv_obj_align(icon, LV_ALIGN_CENTER, 0, -8);

        lv_obj_t *t = lv_label_create(b);
        lv_obj_set_style_text_font(t, F_XS, 0);
        lv_label_set_text(t, labels[i]);
        lv_obj_align(t, LV_ALIGN_CENTER, 0, 15);

        lv_obj_t *pill = lv_obj_create(b);
        lv_obj_remove_style_all(pill);
        lv_obj_set_size(pill, 28, 3);
        lv_obj_set_style_radius(pill, 2, 0);
        lv_obj_set_style_bg_color(pill, C_CREMA, 0);
        lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
        lv_obj_align(pill, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_add_flag(pill, LV_OBJ_FLAG_HIDDEN);
        s_nav_pill[i] = pill;
    }
}

/* ------------------------------------------------------------------ */
/* Banner (faults / background jobs)                                   */
/* ------------------------------------------------------------------ */

static void banner_btn_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    gaggia_cmd_backflush(false);
}

static void build_banner(void)
{
    s_banner = ui_panel(lv_layer_top(), UI_PAD, UI_BAR_H, UI_W - 2 * UI_PAD, 32);
    lv_obj_set_style_radius(s_banner, UI_R_SMALL, 0);
    lv_obj_set_style_bg_opa(s_banner, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_banner, C_ERR, 0);
    lv_obj_set_style_shadow_width(s_banner, 20, 0);
    lv_obj_set_style_shadow_opa(s_banner, LV_OPA_50, 0);

    s_banner_lbl = ui_label(s_banner, F_S, lv_color_white(), "");
    lv_obj_align(s_banner_lbl, LV_ALIGN_LEFT_MID, 12, 0);

    s_banner_btn = ui_btn(s_banner, "Stop", NULL, banner_btn_cb, NULL);
    lv_obj_set_size(s_banner_btn, 64, 24);
    lv_obj_set_style_text_font(s_banner_btn, F_XS, 0);
    lv_obj_set_style_pad_all(s_banner_btn, 0, 0);
    lv_obj_align(s_banner_btn, LV_ALIGN_RIGHT_MID, -4, 0);

    lv_obj_add_flag(s_banner, LV_OBJ_FLAG_HIDDEN);
}

static void update_banner(const gaggia_telemetry_t *t)
{
    const char *msg = NULL;
    lv_color_t bg = C_ERR, fg = lv_color_white();
    bool btn = false;
    static char buf[48];

    if (t->status == GS_CLEANING) {
        snprintf(buf, sizeof(buf), LV_SYMBOL_REFRESH "  Backflush  cycle %u of %u",
                 t->clean_cycle, t->clean_total);
        msg = buf; bg = C_PRESS; fg = C_ON_CREMA; btn = true;
    } else if (!t->link_ok && t->status != GS_BOOT) {
        msg = LV_SYMBOL_WARNING "  No connection to controller";
    } else if (t->faults & FAULT_WATER_EMPTY) {
        msg = LV_SYMBOL_TINT "  Water tank empty. Refill to brew.";
    } else if (t->faults & (FAULT_BREW_SENSOR | FAULT_STEAM_SENSOR)) {
        msg = LV_SYMBOL_WARNING "  Temperature sensor fault";
    } else if (t->faults & FAULT_OVERTEMP) {
        msg = LV_SYMBOL_WARNING "  Over-temperature. Heaters cut.";
    }

    if (!msg) { lv_obj_add_flag(s_banner, LV_OBJ_FLAG_HIDDEN); return; }

    lv_obj_clear_flag(s_banner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(s_banner, bg, 0);
    lv_obj_set_style_text_color(s_banner_lbl, fg, 0);
    ui_set_text_fmt_if_changed(s_banner_lbl, "%s", msg);
    if (btn) lv_obj_clear_flag(s_banner_btn, LV_OBJ_FLAG_HIDDEN);
    else     lv_obj_add_flag(s_banner_btn, LV_OBJ_FLAG_HIDDEN);
}

/* ------------------------------------------------------------------ */
/* Standby curtain                                                     */
/* ------------------------------------------------------------------ */

static void curtain_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    gaggia_cmd_standby(false);
}

static void build_curtain(void)
{
    s_curtain = ui_panel(lv_layer_top(), 0, 0, UI_W, UI_H);
    lv_obj_set_style_bg_color(s_curtain, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_curtain, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_curtain, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_curtain, curtain_cb, LV_EVENT_CLICKED, NULL);

    s_curtain_clock = ui_label(s_curtain, F_GIANT, C_TEXT_DIM, "");
    lv_obj_align(s_curtain_clock, LV_ALIGN_CENTER, 0, -16);
    lv_obj_t *h = ui_label(s_curtain, F_S, C_TEXT_MUTED, "Standby. Touch to wake.");
    lv_obj_align(h, LV_ALIGN_CENTER, 0, 30);

    lv_obj_add_flag(s_curtain, LV_OBJ_FLAG_HIDDEN);
}

static void update_curtain(const gaggia_telemetry_t *t)
{
    bool on = (t->status == GS_STANDBY);
    bool shown = !lv_obj_has_flag(s_curtain, LV_OBJ_FLAG_HIDDEN);
    if (on != shown) {
        if (on) { lv_obj_clear_flag(s_curtain, LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(s_curtain); }
        else    { lv_obj_add_flag(s_curtain, LV_OBJ_FLAG_HIDDEN); ui_show(SCR_HOME); }
        ui_hal_set_brightness(on ? 8 : gaggia_settings()->brightness);
    }
    if (on) {
        int h, m;
        if (ui_hal_get_time(&h, &m)) ui_set_text_fmt_if_changed(s_curtain_clock, "%02d:%02d", h, m);
        else                         ui_set_text_fmt_if_changed(s_curtain_clock, "%s", LV_SYMBOL_POWER);
    }
}

/* ------------------------------------------------------------------ */
/* Backlight dimming + auto-off                                        */
/* ------------------------------------------------------------------ */

void ui_apply_brightness(void)
{
    s_dimmed = false;
    ui_hal_set_brightness(gaggia_settings()->brightness);
}

static void update_power_saving(const gaggia_telemetry_t *t)
{
    const gaggia_settings_t *st = gaggia_settings();
    uint32_t idle_ms = lv_disp_get_inactive_time(NULL);
    bool busy = (t->status == GS_BREWING || t->status == GS_STEAM ||
                 t->status == GS_HOT_WATER || t->status == GS_CLEANING);

    if (t->status == GS_STANDBY) return;

    if (busy) lv_disp_trig_activity(NULL);

    if (st->dim_after_s && !s_dimmed && idle_ms > st->dim_after_s * 1000u) {
        s_dimmed = true;
        ui_hal_set_brightness(st->brightness / 5 + 2);
    } else if (s_dimmed && idle_ms < 500) {
        ui_apply_brightness();
    }

    if (st->auto_off_min && idle_ms > st->auto_off_min * 60000u &&
        (t->status == GS_READY || t->status == GS_HEATING)) {
        gaggia_cmd_standby(true);
    }
}

/* ------------------------------------------------------------------ */
/* Main UI tick                                                        */
/* ------------------------------------------------------------------ */

static void follow_machine_state(const gaggia_telemetry_t *t)
{
    if (t->status == s_prev_status) return;

    /* jump to the relevant screen when the machine starts something,
       e.g. brew started from a physical button */
    switch (t->status) {
    case GS_BREWING:   ui_show(t->manual ? SCR_MANUAL : SCR_BREW); break;
    case GS_STEAM:     ui_show(SCR_STEAM); break;
    case GS_HOT_WATER: ui_show(SCR_WATER); break;
    default: break;
    }
    if (s_prev_status == GS_HEATING && t->status == GS_READY) {
        ui_toast(LV_SYMBOL_OK "  Ready to brew", C_OK);
        if (gaggia_settings()->sound) ui_hal_beep(80);
    }
    s_prev_status = t->status;
}

static void ui_tick_cb(lv_timer_t *tm)
{
    LV_UNUSED(tm);
    const gaggia_telemetry_t *t = gaggia_tm();

    follow_machine_state(t);
    update_status_bar(t);
    update_banner(t);
    update_curtain(t);
    update_power_saving(t);

    if (s_cur < SCR_COUNT && s_screens[s_cur].update)
        s_screens[s_cur].update(t);
}

void ui_init(void)
{
    ui_theme_init();

    for (int i = 0; i < SCR_COUNT; i++)
        s_screens[i].obj = s_screens[i].create();

    build_status_bar();
    build_nav();
    build_banner();
    build_curtain();

    s_cur = SCR_COUNT;
    s_screens[SCR_HOME].enter();
    lv_disp_load_scr(s_screens[SCR_HOME].obj);
    s_cur = SCR_HOME;
    nav_highlight(SCR_HOME);

    ui_apply_brightness();
    lv_timer_create(ui_tick_cb, UI_TICK_MS, NULL);
    ui_tick_cb(NULL);
}
