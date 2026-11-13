/**
 * scr_steam.c — steam boiler gauge, valve opening (mirrors the smart knob,
 * can also be dragged), start/stop and purge.
 */
#include "ui.h"
#include "ui_screens.h"
#include "ui_widgets.h"

#include <math.h>

static ui_gauge_t s_gauge;
static lv_obj_t *s_heat_icon;
static lv_obj_t *s_valve_arc, *s_valve_val, *s_timer;
static lv_obj_t *s_start_btn, *s_start_lbl, *s_purge_btn;
static bool      s_running = false;
static bool      s_restyle = true;   /* force button restyle on next update */
static ui_duty_t s_valve_pwm;
static lv_obj_t *s_src_lbl;   /* "Valve" / "Knob" — who is driving */

static void valve_cb(lv_event_t *e)
{
    lv_obj_t *arc = lv_event_get_target(e);
    gaggia_cmd_steam_power((uint8_t)lv_arc_get_value(arc));
}

static void steam_set_saved(float v, void *u)
{
    LV_UNUSED(u);
    gaggia_settings()->steam_set = v;
    gaggia_commit();
}

static void gauge_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_number_req_t r = {
        .title = "Steam temperature", .unit = "°C",
        .value = gaggia_settings()->steam_set, .min = 115, .max = 150, .step = 1, .decimals = 0,
    };
    ui_number_editor(&r, steam_set_saved, NULL);
}

static void start_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    const gaggia_telemetry_t *t = gaggia_tm();
    if (t->status == GS_STEAM) { gaggia_cmd_steam(false); return; }
    if (t->status != GS_READY && t->status != GS_HEATING) {
        ui_toast("Machine is busy", C_WARN);
        return;
    }
    if (t->steam_temp < gaggia_settings()->steam_set - 10)
        ui_toast("Steam boiler still heating up", C_TEMP);
    gaggia_cmd_steam(true);
}

static void purge_cb(lv_event_t *e) { LV_UNUSED(e); gaggia_cmd_purge(); }

lv_obj_t *scr_steam_create(void)
{
    lv_obj_t *scr = ui_screen_base();
    const lv_coord_t y0 = UI_CONTENT_Y;

    lv_obj_t *gc = ui_card(scr, UI_PAD, y0, 206, 214, gauge_cb, NULL);
    ui_gauge_create(&s_gauge, gc, 172, 20, 160, C_STEAM, "°C");
    lv_obj_align(s_gauge.root, LV_ALIGN_TOP_MID, 0, -2);
    lv_obj_t *cap = ui_label(gc, F_XS, C_TEXT_DIM, "Steam boiler");
    lv_obj_align(cap, LV_ALIGN_BOTTOM_LEFT, 2, 2);
    s_heat_icon = ui_label(gc, F_XS, C_TEXT_MUTED, LV_SYMBOL_CHARGE " heater");
    lv_obj_align(s_heat_icon, LV_ALIGN_BOTTOM_RIGHT, -2, 2);

    const lv_coord_t xr = UI_PAD + 216;
    const lv_coord_t wr = UI_W - xr - UI_PAD;

    /* valve */
    lv_obj_t *vc = ui_card(scr, xr, y0, wr, 144, NULL, NULL);
    lv_obj_set_style_pad_all(vc, 10, 0);

    s_valve_arc = lv_arc_create(vc);
    lv_obj_set_size(s_valve_arc, 120, 120);
    lv_obj_align(s_valve_arc, LV_ALIGN_LEFT_MID, 0, 2);
    lv_arc_set_rotation(s_valve_arc, 135);
    lv_arc_set_bg_angles(s_valve_arc, 0, 270);
    lv_arc_set_range(s_valve_arc, 0, 100);
    lv_obj_set_style_arc_width(s_valve_arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_valve_arc, C_LINE, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_valve_arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_valve_arc, C_STEAM, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_valve_arc, C_TEXT, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_valve_arc, 3, LV_PART_KNOB);
    lv_obj_set_ext_click_area(s_valve_arc, 10);
    lv_obj_add_event_cb(s_valve_arc, valve_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_valve_val = ui_label(s_valve_arc, F_XL, C_TEXT, "0");
    lv_obj_align(s_valve_val, LV_ALIGN_CENTER, 0, -2);
    lv_obj_t *pct = ui_label(s_valve_arc, F_XS, C_TEXT_DIM, "% open");
    lv_obj_align(pct, LV_ALIGN_CENTER, 0, 22);

    s_src_lbl = ui_label(vc, F_S, C_TEXT_DIM, LV_SYMBOL_REFRESH " Knob");
    lv_obj_align(s_src_lbl, LV_ALIGN_TOP_RIGHT, -4, 4);
    /* actual PWM on the valve MOSFET (ramps, purge = 100 %) vs. requested arc */
    ui_duty_create(&s_valve_pwm, vc, 92, "PWM", C_STEAM);
    lv_obj_align(s_valve_pwm.root, LV_ALIGN_TOP_RIGHT, -4, 32);

    s_timer = ui_label(vc, F_XL, C_TEXT_DIM, "0:00");
    lv_obj_align(s_timer, LV_ALIGN_BOTTOM_RIGHT, -4, 0);

    /* buttons */
    s_start_btn = ui_btn(scr, NULL, &st_btn_accent, start_cb, NULL);
    lv_obj_set_pos(s_start_btn, xr, y0 + 152);
    lv_obj_set_size(s_start_btn, wr - 92, 62);
    lv_obj_set_style_radius(s_start_btn, UI_R_BIG, 0);
    s_start_lbl = ui_label(s_start_btn, F_L, C_ON_CREMA, "Start steam");
    lv_obj_center(s_start_lbl);

    s_purge_btn = ui_btn(scr, "Purge", NULL, purge_cb, NULL);
    lv_obj_set_pos(s_purge_btn, xr + wr - 84, y0 + 152);
    lv_obj_set_size(s_purge_btn, 84, 62);
    lv_obj_set_style_radius(s_purge_btn, UI_R_BIG, 0);

    return scr;
}

void scr_steam_enter(void)
{
    if (gaggia_settings()->touch_arcs) lv_obj_add_flag(s_valve_arc, LV_OBJ_FLAG_CLICKABLE);
    else                               lv_obj_clear_flag(s_valve_arc, LV_OBJ_FLAG_CLICKABLE);
    s_restyle = true;
}

void scr_steam_update(const gaggia_telemetry_t *t)
{
    ui_gauge_set(&s_gauge, t->steam_temp, gaggia_settings()->steam_set);
    lv_obj_set_style_text_color(s_heat_icon, t->steam_heater_on ? C_TEMP : C_TEXT_MUTED, 0);

    if (!lv_obj_has_state(s_valve_arc, LV_STATE_PRESSED) &&
        lv_arc_get_value(s_valve_arc) != t->steam_power)
        lv_arc_set_value(s_valve_arc, t->steam_power);
    ui_set_text_fmt_if_changed(s_valve_val, "%d", lv_arc_get_value(s_valve_arc));
    ui_duty_set(&s_valve_pwm, t->steam_valve_pct);

    /* the knob is the primary input: show when it is the one moving the valve */
    bool knob = gaggia_knob_recent(1500);
    lv_obj_set_style_text_color(s_src_lbl, knob ? C_CREMA : C_TEXT_DIM, 0);
    lv_obj_set_style_bg_color(s_valve_arc, knob ? C_CREMA : C_TEXT, LV_PART_KNOB);

    bool running = (t->status == GS_STEAM);
    if (running) {
        uint32_t s = t->shot_ms / 1000;
        ui_set_text_fmt_if_changed(s_timer, "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
    }
    if (running != s_running || s_restyle) {
        s_running = running;
        s_restyle = false;
        lv_obj_remove_style(s_start_btn, &st_btn_accent, 0);
        lv_obj_remove_style(s_start_btn, &st_btn_danger, 0);
        lv_obj_add_style(s_start_btn, running ? &st_btn_danger : &st_btn_accent, 0);
        lv_label_set_text(s_start_lbl, running ? LV_SYMBOL_STOP "  Stop" : "Start steam");
        lv_obj_set_style_text_color(s_start_lbl, running ? lv_color_white() : C_ON_CREMA, 0);
        lv_obj_set_style_text_color(s_timer, running ? C_STEAM : C_TEXT_MUTED, 0);
        ui_btn_set_enabled(s_purge_btn, running);
        if (!running) lv_label_set_text(s_timer, "0:00");
    }
}
