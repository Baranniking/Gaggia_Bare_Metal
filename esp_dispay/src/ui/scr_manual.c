/**
 * scr_manual.c — manual shot: the user sets pressure live (drag the arc
 * or +/-), the STM32 pump loop holds it. Shows actual pressure, flow,
 * volume and pump PWM duty, with a rolling chart.
 */
#include "ui.h"
#include "ui_hal.h"
#include "ui_screens.h"
#include "ui_widgets.h"

#include <math.h>
#include <stdio.h>

#define SAMPLE_MS   250
#define CHART_PTS   120      /* 30 s rolling window */
#define STEP_BAR    0.5f

static lv_obj_t *s_timer, *s_timer_unit, *s_chip, *s_chip_lbl, *s_temp_val;
static lv_obj_t *s_set_arc, *s_act_arc, *s_set_val, *s_act_val, *s_knob_lbl;
static lv_obj_t *s_chart;
static lv_chart_series_t *s_ser_set, *s_ser_press, *s_ser_flow;
static lv_obj_t *s_flow_val, *s_vol_val;
static ui_duty_t s_pump;
static lv_obj_t *s_back_btn, *s_go_btn, *s_go_lbl;

static bool     s_running;
static bool     s_had_run;     /* volume shown belongs to a manual shot */
static uint32_t s_last_sample;

/* ------------------------------------------------------------------ */

static void set_pressure(float bar)
{
    bar = roundf(bar / 0.1f) * 0.1f;
    gaggia_ui_manual_pressure(bar);     /* rotor follows touch changes */
}

static void arc_cb(lv_event_t *e)
{
    set_pressure(lv_arc_get_value(lv_event_get_target(e)) / 10.0f);
}

static void minus_cb(lv_event_t *e) { LV_UNUSED(e); set_pressure(gaggia_tm()->manual_bar - STEP_BAR); }
static void plus_cb(lv_event_t *e)  { LV_UNUSED(e); set_pressure(gaggia_tm()->manual_bar + STEP_BAR); }
static void preset_cb(lv_event_t *e) { set_pressure((float)(intptr_t)lv_event_get_user_data(e)); }

static void back_cb(lv_event_t *e) { LV_UNUSED(e); ui_show(SCR_HOME); }

static void go_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    const gaggia_telemetry_t *t = gaggia_tm();
    if (t->status == GS_BREWING) { gaggia_cmd_brew_manual(false); return; }
    if (t->faults & FAULT_WATER_EMPTY) { ui_toast(LV_SYMBOL_TINT "  Refill the water tank first", C_ERR); return; }
    if (t->status != GS_READY) {
        ui_toast(t->status == GS_HEATING ? "Boiler is still heating" : "Machine is busy", C_TEMP);
        return;
    }
    lv_chart_set_all_value(s_chart, s_ser_set, LV_CHART_POINT_NONE);
    lv_chart_set_all_value(s_chart, s_ser_press, LV_CHART_POINT_NONE);
    lv_chart_set_all_value(s_chart, s_ser_flow, LV_CHART_POINT_NONE);
    gaggia_cmd_brew_manual(true);
}

/* ------------------------------------------------------------------ */

static lv_obj_t *small_tile(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                            const char *title, const char *unit, lv_color_t color)
{
    lv_obj_t *c = ui_card(parent, x, y, w, 52, NULL, NULL);
    lv_obj_set_style_radius(c, UI_R_SMALL, 0);
    lv_obj_set_style_pad_hor(c, 8, 0);
    lv_obj_set_style_pad_ver(c, 6, 0);
    lv_obj_set_style_border_side(c, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_width(c, 3, 0);
    lv_obj_set_style_border_color(c, color, 0);
    lv_obj_set_style_border_opa(c, LV_OPA_COVER, 0);
    char buf[24];
    lv_snprintf(buf, sizeof(buf), "%s, %s", title, unit);   /* tiles are narrow */
    lv_obj_t *t = ui_label(c, F_XS, C_TEXT_DIM, buf);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, -2);
    lv_obj_t *v = ui_label(c, F_L, C_TEXT, "0");
    lv_obj_align(v, LV_ALIGN_BOTTOM_LEFT, 0, 2);
    return v;
}

static lv_obj_t *step_btn(lv_obj_t *parent, const char *sym, lv_event_cb_t cb)
{
    lv_obj_t *b = ui_btn(parent, sym, NULL, cb, NULL);
    lv_obj_set_size(b, 42, 42);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
    return b;
}

lv_obj_t *scr_manual_create(void)
{
    lv_obj_t *scr = ui_screen_base();

    /* --- header (same rhythm as the Brew screen) --- */
    s_timer = ui_label(scr, F_GIANT, C_TEXT, "0.0");
    lv_obj_set_pos(s_timer, UI_PAD + 4, UI_BAR_H + 2);
    s_timer_unit = ui_label(scr, F_M, C_TEXT_DIM, "s");
    lv_obj_align_to(s_timer_unit, s_timer, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -8);

    s_chip = lv_obj_create(scr);
    lv_obj_remove_style_all(s_chip);
    lv_obj_add_style(s_chip, &st_chip, 0);
    lv_obj_set_size(s_chip, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(s_chip, C_SURFACE_HI, 0);
    s_chip_lbl = ui_label(s_chip, F_S, C_CREMA, LV_SYMBOL_EDIT "  Manual");
    lv_obj_set_pos(s_chip, 150, UI_BAR_H + 8);
    lv_obj_t *hint = ui_label(scr, F_XS, C_TEXT_DIM, "pressure set by hand");
    lv_obj_set_pos(hint, 152, UI_BAR_H + 36);

    lv_obj_t *tl = ui_label(scr, F_XS, C_TEXT_DIM, "Group");
    lv_obj_align(tl, LV_ALIGN_TOP_RIGHT, -UI_PAD - 4, UI_BAR_H + 6);
    s_temp_val = ui_label(scr, F_L, C_TEMP, "--");
    lv_obj_align(s_temp_val, LV_ALIGN_TOP_RIGHT, -UI_PAD - 4, UI_BAR_H + 24);

    const lv_coord_t cy = UI_BAR_H + 64;

    /* --- pressure control --- */
    lv_obj_t *pc = ui_card(scr, UI_PAD, cy, 200, 166, NULL, NULL);
    lv_obj_set_style_pad_all(pc, 8, 0);

    s_set_arc = lv_arc_create(pc);
    lv_obj_set_size(s_set_arc, 146, 146);
    lv_obj_align(s_set_arc, LV_ALIGN_TOP_MID, 0, 0);
    lv_arc_set_rotation(s_set_arc, 135);
    lv_arc_set_bg_angles(s_set_arc, 0, 270);
    lv_arc_set_range(s_set_arc, 0, (int16_t)(GAGGIA_MANUAL_MAX_BAR * 10));
    lv_obj_set_style_arc_width(s_set_arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_set_arc, C_LINE, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_set_arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_set_arc, C_CREMA, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_set_arc, C_TEXT, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_set_arc, 4, LV_PART_KNOB);
    lv_obj_set_ext_click_area(s_set_arc, 12);
    lv_obj_add_event_cb(s_set_arc, arc_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* actual pressure: thin inner ring, read-only */
    s_act_arc = lv_arc_create(s_set_arc);
    lv_obj_remove_style(s_act_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_act_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(s_act_arc, 112, 112);
    lv_obj_center(s_act_arc);
    lv_arc_set_rotation(s_act_arc, 135);
    lv_arc_set_bg_angles(s_act_arc, 0, 270);
    lv_arc_set_range(s_act_arc, 0, (int16_t)(GAGGIA_MANUAL_MAX_BAR * 10));
    lv_arc_set_value(s_act_arc, 0);
    lv_obj_set_style_arc_width(s_act_arc, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_act_arc, C_LINE, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_act_arc, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_act_arc, C_PRESS, LV_PART_INDICATOR);

    s_set_val = ui_label(s_set_arc, F_XL, C_CREMA, "9.0");
    lv_obj_align(s_set_val, LV_ALIGN_CENTER, 0, -8);
    lv_obj_t *u = ui_label(s_set_arc, F_XS, C_TEXT_DIM, "bar set");
    lv_obj_align(u, LV_ALIGN_CENTER, 0, 14);
    s_act_val = ui_label(s_set_arc, F_XS, C_PRESS, "");
    lv_obj_align(s_act_val, LV_ALIGN_CENTER, 0, 32);

    s_knob_lbl = ui_label(pc, F_XS, C_TEXT_DIM, LV_SYMBOL_REFRESH " Knob");
    lv_obj_align(s_knob_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *mb = step_btn(pc, LV_SYMBOL_MINUS, minus_cb);
    lv_obj_align(mb, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *pb = step_btn(pc, LV_SYMBOL_PLUS, plus_cb);
    lv_obj_align(pb, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    /* --- chart --- */
    const lv_coord_t xr = UI_PAD + 208;
    const lv_coord_t wr = UI_W - xr - UI_PAD;
    lv_obj_t *cc = ui_card(scr, xr, cy, wr, 108, NULL, NULL);
    lv_obj_set_style_pad_all(cc, 8, 0);
    s_chart = lv_chart_create(cc);
    lv_obj_set_size(s_chart, wr - 16, 76);
    lv_obj_align(s_chart, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(s_chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_chart, 0, 0);
    lv_obj_set_style_pad_all(s_chart, 0, 0);
    lv_obj_set_style_line_color(s_chart, C_LINE, LV_PART_MAIN);
    lv_obj_set_style_line_width(s_chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(s_chart, 0, LV_PART_INDICATOR);
    lv_obj_clear_flag(s_chart, LV_OBJ_FLAG_CLICKABLE);
    lv_chart_set_div_line_count(s_chart, 3, 0);
    lv_chart_set_update_mode(s_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_point_count(s_chart, CHART_PTS);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 120);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_SECONDARY_Y, 0, 80);
    s_ser_set   = lv_chart_add_series(s_chart, lv_color_mix(C_CREMA, C_SURFACE, LV_OPA_50), LV_CHART_AXIS_PRIMARY_Y);
    s_ser_flow  = lv_chart_add_series(s_chart, C_FLOW,  LV_CHART_AXIS_SECONDARY_Y);
    s_ser_press = lv_chart_add_series(s_chart, C_PRESS, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(s_chart, s_ser_set, LV_CHART_POINT_NONE);
    lv_chart_set_all_value(s_chart, s_ser_press, LV_CHART_POINT_NONE);
    lv_chart_set_all_value(s_chart, s_ser_flow, LV_CHART_POINT_NONE);

    lv_obj_t *ct = ui_label(cc, F_XS, C_TEXT_DIM, "Last 30 s");
    lv_obj_align(ct, LV_ALIGN_TOP_LEFT, 0, -2);
    lv_obj_t *lg = ui_label(cc, F_XS, C_PRESS, "bar");
    lv_obj_align(lg, LV_ALIGN_TOP_RIGHT, -84, -2);
    lg = ui_label(cc, F_XS, C_FLOW, "ml/s");
    lv_obj_align(lg, LV_ALIGN_TOP_RIGHT, -44, -2);
    lg = ui_label(cc, F_XS, C_CREMA, "set");
    lv_obj_align(lg, LV_ALIGN_TOP_RIGHT, 0, -2);

    /* --- tiles: flow, volume, pump duty --- */
    const lv_coord_t ty = cy + 114;
    const lv_coord_t tw = (wr - 12) / 3;
    s_flow_val = small_tile(scr, xr,              ty, tw, "Flow",   "ml/s", C_FLOW);
    s_vol_val  = small_tile(scr, xr + tw + 6,     ty, tw, "Cup",    "ml",   C_CREMA);
    lv_obj_t *pt = ui_card(scr, xr + 2 * (tw + 6), ty, tw, 52, NULL, NULL);
    lv_obj_set_style_radius(pt, UI_R_SMALL, 0);
    lv_obj_set_style_pad_hor(pt, 6, 0);
    lv_obj_set_style_pad_ver(pt, 8, 0);
    ui_duty_create(&s_pump, pt, tw - 12, "Pump", C_PRESS);
    lv_obj_align(s_pump.root, LV_ALIGN_CENTER, 0, 0);

    /* --- bottom: presets + back + go --- */
    const lv_coord_t by = cy + 174;
    s_back_btn = ui_btn(scr, LV_SYMBOL_LEFT, NULL, back_cb, NULL);
    lv_obj_set_pos(s_back_btn, UI_PAD, by);
    lv_obj_set_size(s_back_btn, 52, 44);

    static const int presets[] = { 3, 6, 9 };
    for (int i = 0; i < 3; i++) {
        char b[8];
        lv_snprintf(b, sizeof(b), "%d", presets[i]);
        lv_obj_t *p = ui_btn(scr, b, NULL, preset_cb, (void *)(intptr_t)presets[i]);
        lv_obj_set_pos(p, UI_PAD + 60 + i * 48, by);
        lv_obj_set_size(p, 42, 44);
        lv_obj_set_style_text_color(p, C_CREMA, 0);
    }

    s_go_btn = ui_btn(scr, NULL, &st_btn_accent, go_cb, NULL);
    lv_obj_set_pos(s_go_btn, xr, by);
    lv_obj_set_size(s_go_btn, wr, 44);
    s_go_lbl = ui_label(s_go_btn, F_M, C_ON_CREMA, "");
    lv_obj_center(s_go_lbl);

    return scr;
}

void scr_manual_enter(void)
{
    if (gaggia_settings()->touch_arcs) lv_obj_add_flag(s_set_arc, LV_OBJ_FLAG_CLICKABLE);
    else                               lv_obj_clear_flag(s_set_arc, LV_OBJ_FLAG_CLICKABLE);
    s_running = !(gaggia_tm()->status == GS_BREWING);   /* force restyle */
    s_last_sample = 0;
    if (gaggia_tm()->status != GS_BREWING) s_had_run = false;
}

void scr_manual_update(const gaggia_telemetry_t *t)
{
    bool running = (t->status == GS_BREWING && t->manual);
    if (running) s_had_run = true;

    if (running != s_running) {
        s_running = running;
        lv_obj_remove_style(s_go_btn, &st_btn_accent, 0);
        lv_obj_remove_style(s_go_btn, &st_btn_danger, 0);
        lv_obj_add_style(s_go_btn, running ? &st_btn_danger : &st_btn_accent, 0);
        lv_obj_set_style_text_color(s_go_lbl, running ? lv_color_white() : C_ON_CREMA, 0);
        lv_label_set_text(s_go_lbl, running ? LV_SYMBOL_STOP "  Stop" : LV_SYMBOL_PLAY "  Start pump");
        ui_btn_set_enabled(s_back_btn, !running);
        lv_obj_set_style_bg_color(s_chip, running ? C_CREMA : C_SURFACE_HI, 0);
        lv_obj_set_style_text_color(s_chip_lbl, running ? C_ON_CREMA : C_CREMA, 0);
        if (!running && gaggia_settings()->sound) ui_hal_beep(120);
    }

    /* setpoint: don't fight the finger while the arc is being dragged */
    int16_t set10 = (int16_t)lroundf(t->manual_bar * 10);
    if (!lv_obj_has_state(s_set_arc, LV_STATE_PRESSED) && lv_arc_get_value(s_set_arc) != set10)
        lv_arc_set_value(s_set_arc, set10);
    ui_set_text_fmt_if_changed(s_set_val, "%.1f", (double)t->manual_bar);
    bool knob = gaggia_knob_recent(1500);
    lv_obj_set_style_text_color(s_knob_lbl, knob ? C_CREMA : C_TEXT_DIM, 0);
    lv_obj_set_style_bg_color(s_set_arc, knob ? C_CREMA : C_TEXT, LV_PART_KNOB);

    lv_arc_set_value(s_act_arc, (int16_t)lroundf(fminf(t->pressure, GAGGIA_MANUAL_MAX_BAR) * 10));
    ui_set_text_fmt_if_changed(s_act_val, "now %.1f", (double)t->pressure);

    ui_set_text_fmt_if_changed(s_temp_val, "%.1f°", (double)t->brew_temp);
    ui_set_text_fmt_if_changed(s_flow_val, "%.1f", (double)t->flow);
    ui_set_text_fmt_if_changed(s_vol_val, "%d", s_had_run ? (int)lroundf(t->volume) : 0);
    ui_duty_set(&s_pump, t->pump_pct);

    if (running) {
        uint32_t ds = t->shot_ms / 100;
        ui_set_text_fmt_if_changed(s_timer, "%lu.%lu", (unsigned long)(ds / 10), (unsigned long)(ds % 10));
        lv_obj_align_to(s_timer_unit, s_timer, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -8);

        if (t->shot_ms - s_last_sample >= SAMPLE_MS || t->shot_ms < s_last_sample) {
            s_last_sample = t->shot_ms;
            lv_chart_set_next_value(s_chart, s_ser_set,   (lv_coord_t)lroundf(t->target_bar * 10));
            lv_chart_set_next_value(s_chart, s_ser_press, (lv_coord_t)lroundf(t->pressure * 10));
            lv_chart_set_next_value(s_chart, s_ser_flow,  (lv_coord_t)lroundf(t->flow * 10));
        }
    }
}
