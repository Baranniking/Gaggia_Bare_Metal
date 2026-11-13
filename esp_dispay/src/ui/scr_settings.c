/**
 * scr_settings.c — sectioned settings: sidebar on the left, scrollable
 * rows on the right. Every numeric row goes through the shared editor.
 */
#include "ui.h"
#include "ui_hal.h"
#include "ui_screens.h"
#include "ui_widgets.h"
#include "gaggia_log.h"

#include <stdio.h>
#include <math.h>

#define SIDE_W 128

typedef enum { SEC_BOILER, SEC_DISPLAY, SEC_MACHINE, SEC_SERVICE, SEC_ABOUT, SEC_COUNT } section_t;
static const char *k_sec_names[SEC_COUNT] = { "Boiler", "Display", "Machine", "Service", "About" };
static const char *k_sec_icons[SEC_COUNT] = {
    LV_SYMBOL_CHARGE, LV_SYMBOL_IMAGE, LV_SYMBOL_POWER, LV_SYMBOL_REFRESH, LV_SYMBOL_FILE
};

/* A numeric setting bound directly to a field in gaggia_settings_t. */
typedef struct {
    const char *title;
    const char *hint;
    const char *unit;
    float      *f;        /* one of f / u is set */
    uint16_t   *u;
    float       min, max, step;
    uint8_t     decimals;
    const char *zero_text;   /* e.g. "Never" when value == 0 */
    lv_obj_t   *label;
} num_setting_t;

static num_setting_t s_nums[12];
static uint8_t       s_num_count;

static lv_obj_t *s_side_btn[SEC_COUNT];
static lv_obj_t *s_pages[SEC_COUNT];
static section_t s_sec;

static lv_obj_t *s_preheat_now;
static lv_obj_t *s_clock_lbl;
static lv_obj_t *s_about_link, *s_about_uptime, *s_about_heap, *s_about_shots, *s_shots_lbl;

/* ------------------------------------------------------------------ */

static void num_render(num_setting_t *n)
{
    float v = n->f ? *n->f : (float)*n->u;
    if (n->zero_text && v == 0) { lv_label_set_text(n->label, n->zero_text); return; }
    char buf[24];
    snprintf(buf, sizeof(buf), "%.*f %s", n->decimals, (double)v, n->unit ? n->unit : "");
    lv_label_set_text(n->label, buf);
}

static void num_saved(float v, void *user)
{
    num_setting_t *n = user;
    if (n->f) *n->f = v; else *n->u = (uint16_t)lroundf(v);
    gaggia_commit();
    num_render(n);
}

static void num_click(lv_event_t *e)
{
    num_setting_t *n = lv_event_get_user_data(e);
    ui_number_req_t r = {
        .title = n->title, .unit = n->unit,
        .value = n->f ? *n->f : (float)*n->u,
        .min = n->min, .max = n->max, .step = n->step, .decimals = n->decimals,
    };
    ui_number_editor(&r, num_saved, n);
}

static void add_num(lv_obj_t *page, num_setting_t tmpl)
{
    if (s_num_count >= sizeof(s_nums) / sizeof(s_nums[0])) return;
    num_setting_t *n = &s_nums[s_num_count++];
    *n = tmpl;
    n->label = ui_row_value(page, n->title, n->hint, num_click, n);
    num_render(n);
}

/* ---- switch / slider / action callbacks ---- */

static void eco_cb(lv_event_t *e)
{
    gaggia_settings()->eco = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    gaggia_commit();
}

static void knob_auto_cb(lv_event_t *e)
{
    gaggia_settings()->knob_auto_steam = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    gaggia_commit();
}

static void preheat_on_cb(lv_event_t *e)
{
    gaggia_settings()->preheat_on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    gaggia_commit();
}

static void sound_cb(lv_event_t *e)
{
    gaggia_settings()->sound = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    gaggia_commit();
    if (gaggia_settings()->sound) ui_hal_beep(40);
}

static void bright_cb(lv_event_t *e)
{
    gaggia_settings()->brightness = (uint8_t)lv_slider_get_value(lv_event_get_target(e));
    gaggia_commit();
    ui_apply_brightness();
}

static void clock_saved(int h, int m, void *u)
{
    LV_UNUSED(u);
    ui_hal_set_time(h, m);
    glog(LOG_I, "clock set %02d:%02d", h, m);
    lv_label_set_text_fmt(s_clock_lbl, "%02d:%02d", h, m);
}

static void clock_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    int h = 12, m = 0;
    ui_hal_get_time(&h, &m);
    ui_time_editor("Set clock", h, m, clock_saved, NULL);
}

static void touch_arcs_cb(lv_event_t *e)
{
    gaggia_settings()->touch_arcs = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    gaggia_commit();
}

static void standby_yes(void *u) { LV_UNUSED(u); gaggia_cmd_standby(true); }
static void standby_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_confirm("Standby", "Heaters off, display dimmed. Touch the screen to wake the machine.",
               "Standby", false, standby_yes, NULL);
}

static void reset_shots_yes(void *u) { LV_UNUSED(u); gaggia_cmd_reset_shot_counter(); ui_toast("Shot counter reset", C_CREMA); }
static void reset_shots_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_confirm("Reset counter", "The lifetime shot counter will go back to zero.",
               "Reset", true, reset_shots_yes, NULL);
}

static void backflush_yes(void *u) { LV_UNUSED(u); gaggia_cmd_backflush(true); }
static void backflush_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    const gaggia_telemetry_t *t = gaggia_tm();
    if (t->status != GS_READY && t->status != GS_HEATING) { ui_toast("Machine is busy", C_WARN); return; }
    ui_confirm("Backflush",
               "Insert the blind basket with a little detergent. The pump runs 5 cycles of 10 s on, 5 s off.",
               "Start", false, backflush_yes, NULL);
}

static void debug_cb(lv_event_t *e) { LV_UNUSED(e); ui_show(SCR_DEBUG); }

static void descale_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_toast("Descale wizard: not implemented yet", C_WARN);
}

#if GAGGIA_SIM
static void sim_water_cb(lv_event_t *e)
{
    gaggia_sim_set_water(!lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED));
}
#endif

/* ---- sidebar ---- */

static void show_section(section_t sec)
{
    s_sec = sec;
    for (int i = 0; i < SEC_COUNT; i++) {
        bool on = (i == (int)sec);
        lv_obj_set_style_bg_opa(s_side_btn[i], on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(s_side_btn[i], on ? C_CREMA : C_TEXT_DIM, 0);
        if (on) lv_obj_clear_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
        else    lv_obj_add_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_scroll_to_y(s_pages[sec], 0, LV_ANIM_OFF);
}

static void side_cb(lv_event_t *e) { show_section((section_t)(intptr_t)lv_event_get_user_data(e)); }

static lv_obj_t *make_page(lv_obj_t *card)
{
    lv_obj_t *p = ui_panel(card, 0, 0, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(p, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(p, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_style_bg_color(p, C_CREMA_DEEP, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(p, 3, LV_PART_SCROLLBAR);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_right(p, 8, 0);
    return p;
}

static lv_obj_t *info_row(lv_obj_t *page, const char *title)
{
    lv_obj_t *r = ui_row(page, title, NULL);
    lv_obj_t *v = ui_label(r, F_S, C_TEXT_DIM, "");
    lv_obj_align(v, LV_ALIGN_RIGHT_MID, -4, 0);
    return v;
}

/* ------------------------------------------------------------------ */

lv_obj_t *scr_settings_create(void)
{
    lv_obj_t *scr = ui_screen_base();
    gaggia_settings_t *st = gaggia_settings();
    const lv_coord_t y0 = UI_CONTENT_Y;
    const lv_coord_t h  = UI_CONTENT_H + 2;

    /* sidebar */
    lv_obj_t *side = ui_panel(scr, UI_PAD, y0, SIDE_W, h);
    lv_obj_set_flex_flow(side, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(side, 4, 0);
    for (int i = 0; i < SEC_COUNT; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%s   %s", k_sec_icons[i], k_sec_names[i]);
        lv_obj_t *b = ui_btn(side, NULL, NULL, side_cb, (void *)(intptr_t)i);
        lv_obj_set_size(b, SIDE_W, 38);
        lv_obj_set_style_text_font(b, F_S, 0);
        lv_obj_set_style_pad_left(b, 12, 0);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, buf);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
        s_side_btn[i] = b;
    }

    /* content */
    const lv_coord_t xc = UI_PAD + SIDE_W + 8;
    lv_obj_t *card = ui_card(scr, xc, y0, UI_W - xc - UI_PAD, h, NULL, NULL);
    lv_obj_set_style_pad_all(card, 6, 0);
    lv_obj_set_style_pad_left(card, 12, 0);
    for (int i = 0; i < SEC_COUNT; i++) s_pages[i] = make_page(card);

    /* Boiler */
    lv_obj_t *p = s_pages[SEC_BOILER];
    add_num(p, (num_setting_t){ "Steam temperature", NULL, "°C", &st->steam_set, NULL, 115, 150, 1, 0 });
    ui_row_switch(p, "Preheat boiler", "Warms water before the brew boiler", st->preheat_on, preheat_on_cb, NULL);
    add_num(p, (num_setting_t){ "Preheat temperature", NULL, "°C", &st->preheat_set, NULL,
                                GAGGIA_PREHEAT_MIN, GAGGIA_PREHEAT_MAX, 1, 0 });
    lv_obj_t *pr = ui_row(p, "Preheat now", NULL);
    s_preheat_now = ui_label(pr, F_S, C_TEXT_DIM, "");
    lv_obj_align(s_preheat_now, LV_ALIGN_RIGHT_MID, -4, 0);
    add_num(p, (num_setting_t){ "Group offset", "Added to every profile", "°C", &st->temp_offset, NULL, -5, 5, 0.1f, 1 });
    ui_row_switch(p, "Eco steam", "Steam boiler heats on demand", st->eco, eco_cb, NULL);
    ui_row_switch(p, "Knob starts steam", "Opening the smart knob starts steam", st->knob_auto_steam, knob_auto_cb, NULL);
    add_num(p, (num_setting_t){ "PID Kp", "Brew boiler", "", &st->kp, NULL, 0, 50, 0.1f, 1 });
    add_num(p, (num_setting_t){ "PID Ki", "Brew boiler", "", &st->ki, NULL, 0, 5, 0.01f, 2 });
    add_num(p, (num_setting_t){ "PID Kd", "Brew boiler", "", &st->kd, NULL, 0, 200, 1, 0 });

    /* Display */
    p = s_pages[SEC_DISPLAY];
    ui_row_slider(p, "Brightness", 10, 100, st->brightness, bright_cb, NULL);
    add_num(p, (num_setting_t){ "Dim after", "Backlight drops when idle", "s", NULL, &st->dim_after_s, 0, 600, 15, 0, "Never" });
    ui_row_switch(p, "Sounds", NULL, st->sound, sound_cb, NULL);

    /* Machine */
    p = s_pages[SEC_MACHINE];
    s_clock_lbl = ui_row_value(p, "Clock", "Resets on power-off for now", clock_cb, NULL);
    ui_row_switch(p, "Touch moves arcs", "Off = smart knob only",
                  st->touch_arcs, touch_arcs_cb, NULL);
    add_num(p, (num_setting_t){ "Auto standby", "After no touch for", "min", NULL, &st->auto_off_min, 0, 240, 5, 0, "Never" });
    ui_row_action(p, "Standby now", NULL, LV_SYMBOL_POWER " Standby", false, standby_cb, NULL);
    lv_obj_t *r = ui_row(p, "Shots pulled", NULL);
    s_shots_lbl = ui_label(r, F_M, C_CREMA, "");
    lv_obj_align(s_shots_lbl, LV_ALIGN_RIGHT_MID, -4, 0);
    ui_row_action(p, "Reset counter", NULL, "Reset", true, reset_shots_cb, NULL);

    /* Service */
    p = s_pages[SEC_SERVICE];
    ui_row_action(p, "Backflush", "Blind basket + detergent", "Start", false, backflush_cb, NULL);
    ui_row_action(p, "Descale", "Guided, about 25 min", "Open", false, descale_cb, NULL);
    ui_row_action(p, "Telemetry log", "Live values, events, serial CSV", "Open", false, debug_cb, NULL);
#if GAGGIA_SIM
    ui_row_switch(p, "Sim: tank empty", "Simulator only", false, sim_water_cb, NULL);
#endif

    /* About */
    p = s_pages[SEC_ABOUT];
    lv_obj_t *fw = info_row(p, "Firmware");
    lv_label_set_text(fw, GAGGIA_FW_VERSION);
    s_about_link   = info_row(p, "Controller");
    s_about_uptime = info_row(p, "Uptime");
    s_about_heap   = info_row(p, "Free heap");
    s_about_shots  = info_row(p, "Shots");

    show_section(SEC_BOILER);
    return scr;
}

void scr_settings_enter(void)
{
    for (uint8_t i = 0; i < s_num_count; i++) num_render(&s_nums[i]);
}

void scr_settings_update(const gaggia_telemetry_t *t)
{
    if (s_sec == SEC_MACHINE) {
        int h, m;
        if (ui_hal_get_time(&h, &m)) ui_set_text_fmt_if_changed(s_clock_lbl, "%02d:%02d", h, m);
        else                         ui_set_text_fmt_if_changed(s_clock_lbl, "%s", "--:--");
    }
    ui_set_text_fmt_if_changed(s_shots_lbl, "%lu", (unsigned long)t->shot_count);
    if (s_sec == SEC_BOILER) {
        bool on = gaggia_settings()->preheat_on;
        if (on) ui_set_text_fmt_if_changed(s_preheat_now, "%.1f °C%s", (double)t->preheat_temp,
                                           t->preheat_heater_on ? "  " LV_SYMBOL_CHARGE : "");
        else    ui_set_text_fmt_if_changed(s_preheat_now, "%s", "off");
        lv_obj_set_style_text_color(s_preheat_now, t->preheat_heater_on ? C_TEMP : C_TEXT_DIM, 0);
    }
    if (s_sec != SEC_ABOUT) return;

    ui_set_text_fmt_if_changed(s_about_link, "%s", t->link_ok ? "STM32 connected" : "no link");
    lv_obj_set_style_text_color(s_about_link, t->link_ok ? C_OK : C_ERR, 0);
    uint32_t u = t->uptime_s;
    ui_set_text_fmt_if_changed(s_about_uptime, "%luh %02lum %02lus",
                               (unsigned long)(u / 3600), (unsigned long)(u / 60 % 60), (unsigned long)(u % 60));
    ui_set_text_fmt_if_changed(s_about_heap, "%lu KB", (unsigned long)(ui_hal_free_heap() / 1024));
    ui_set_text_fmt_if_changed(s_about_shots, "%lu", (unsigned long)t->shot_count);
}
