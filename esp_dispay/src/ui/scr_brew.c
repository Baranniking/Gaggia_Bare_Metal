/**
 * scr_brew.c — live shot: timer, phase, pressure/flow chart over the
 * profile's target curve, and a big Stop button.
 */
#include "ui.h"
#include "ui_screens.h"
#include "ui_widgets.h"
#include "ui_hal.h"

#include <math.h>

#define SAMPLE_MS      250
#define MAX_POINTS     260   /* 65 s */

static lv_obj_t *s_timer, *s_timer_unit;
static lv_obj_t *s_phase_chip, *s_phase_lbl, *s_prof_lbl;
static lv_obj_t *s_temp_val;
static lv_obj_t *s_chart, *s_x_end_lbl;
static lv_chart_series_t *s_ser_target, *s_ser_press, *s_ser_flow;
static lv_obj_t *s_p_val, *s_f_val, *s_v_val;
static lv_obj_t *s_vol_lbl, *s_vol_bar, *s_target_lbl;
static lv_obj_t *s_stop_btn, *s_stop_lbl;

static ui_duty_t s_pump;

static int      s_last_idx;
static uint16_t s_points;
static bool     s_finished;

/* ---- actions ---- */

static void stop_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (s_finished || gaggia_tm()->status != GS_BREWING) ui_show(SCR_HOME);
    else gaggia_cmd_brew(false);
}

/* ---- build helpers ---- */

static lv_obj_t *value_tile(lv_obj_t *parent, lv_coord_t y, const char *title,
                            const char *unit, lv_color_t color)
{
    lv_obj_t *c = ui_card(parent, UI_W - UI_PAD - 120, y, 120, 50, NULL, NULL);
    lv_obj_set_style_pad_hor(c, 10, 0);
    lv_obj_set_style_pad_ver(c, 6, 0);
    lv_obj_set_style_radius(c, UI_R_SMALL, 0);
    lv_obj_set_style_border_side(c, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_width(c, 3, 0);
    lv_obj_set_style_border_color(c, color, 0);
    lv_obj_set_style_border_opa(c, LV_OPA_COVER, 0);

    lv_obj_t *t = ui_label(c, F_XS, C_TEXT_DIM, title);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, -2);
    lv_obj_t *u = ui_label(c, F_XS, C_TEXT_MUTED, unit);
    lv_obj_align(u, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_t *v = ui_label(c, F_L, C_TEXT, "0.0");
    lv_obj_align(v, LV_ALIGN_BOTTOM_LEFT, 0, 2);
    return v;
}

static lv_obj_t *legend(lv_obj_t *parent, const char *txt, lv_color_t c)
{
    lv_obj_t *l = ui_label(parent, F_XS, c, txt);
    return l;
}

lv_obj_t *scr_brew_create(void)
{
    lv_obj_t *scr = ui_screen_base();

    /* --- header --- */
    s_timer = ui_label(scr, F_GIANT, C_TEXT, "0.0");
    lv_obj_set_pos(s_timer, UI_PAD + 4, UI_BAR_H + 2);
    s_timer_unit = ui_label(scr, F_M, C_TEXT_DIM, "s");

    s_phase_chip = lv_obj_create(scr);
    lv_obj_remove_style_all(s_phase_chip);
    lv_obj_add_style(s_phase_chip, &st_chip, 0);
    lv_obj_set_size(s_phase_chip, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(s_phase_chip, C_CREMA, 0);
    s_phase_lbl = ui_label(s_phase_chip, F_S, C_ON_CREMA, "Pre-infusion");
    lv_obj_set_pos(s_phase_chip, 150, UI_BAR_H + 8);

    s_prof_lbl = ui_label(scr, F_XS, C_TEXT_DIM, "");
    lv_obj_set_pos(s_prof_lbl, 152, UI_BAR_H + 36);

    ui_duty_create(&s_pump, scr, 110, "Pump", C_PRESS);
    lv_obj_set_pos(s_pump.root, 286, UI_BAR_H + 12);

    lv_obj_t *tl = ui_label(scr, F_XS, C_TEXT_DIM, "Group");
    lv_obj_align(tl, LV_ALIGN_TOP_RIGHT, -UI_PAD - 4, UI_BAR_H + 6);
    s_temp_val = ui_label(scr, F_L, C_TEMP, "--");
    lv_obj_align(s_temp_val, LV_ALIGN_TOP_RIGHT, -UI_PAD - 4, UI_BAR_H + 24);

    /* --- chart --- */
    const lv_coord_t cy = UI_BAR_H + 64;
    lv_obj_t *cc = ui_card(scr, UI_PAD, cy, 330, 158, NULL, NULL);
    lv_obj_set_style_pad_all(cc, 8, 0);

    s_chart = lv_chart_create(cc);
    lv_obj_set_size(s_chart, 314 - 18, 124);
    lv_obj_align(s_chart, LV_ALIGN_BOTTOM_RIGHT, 0, -6);
    lv_obj_set_style_bg_opa(s_chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_chart, 0, 0);
    lv_obj_set_style_pad_all(s_chart, 0, 0);
    lv_obj_set_style_line_color(s_chart, C_LINE, LV_PART_MAIN);
    lv_obj_set_style_line_opa(s_chart, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_line_width(s_chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(s_chart, 0, LV_PART_INDICATOR);
    lv_obj_clear_flag(s_chart, LV_OBJ_FLAG_CLICKABLE);
    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(s_chart, 4, 0);
    lv_chart_set_update_mode(s_chart, LV_CHART_UPDATE_MODE_CIRCULAR);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 120);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_SECONDARY_Y, 0, 80);
    lv_chart_set_point_count(s_chart, 160);

    s_ser_target = lv_chart_add_series(s_chart, lv_color_mix(C_CREMA, C_SURFACE, LV_OPA_50),
                                       LV_CHART_AXIS_PRIMARY_Y);
    s_ser_flow   = lv_chart_add_series(s_chart, C_FLOW,  LV_CHART_AXIS_SECONDARY_Y);
    s_ser_press  = lv_chart_add_series(s_chart, C_PRESS, LV_CHART_AXIS_PRIMARY_Y);

    /* axis hints */
    lv_obj_t *y12 = ui_label(cc, F_XS, C_TEXT_MUTED, "12");
    lv_obj_align_to(y12, s_chart, LV_ALIGN_OUT_LEFT_TOP, -4, -6);
    lv_obj_t *y6 = ui_label(cc, F_XS, C_TEXT_MUTED, "6");
    lv_obj_align_to(y6, s_chart, LV_ALIGN_OUT_LEFT_MID, -4, 0);
    lv_obj_t *y0 = ui_label(cc, F_XS, C_TEXT_MUTED, "0");
    lv_obj_align_to(y0, s_chart, LV_ALIGN_OUT_LEFT_BOTTOM, -4, 6);
    s_x_end_lbl = ui_label(cc, F_XS, C_TEXT_MUTED, "");
    lv_obj_align(s_x_end_lbl, LV_ALIGN_BOTTOM_RIGHT, 0, 8);

    lv_obj_t *lg = ui_panel(cc, 0, 0, 300, 16);
    lv_obj_align(lg, LV_ALIGN_TOP_RIGHT, 0, -4);
    lv_obj_set_flex_flow(lg, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(lg, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(lg, 12, 0);
    legend(lg, "bar", C_PRESS);
    legend(lg, "ml/s", C_FLOW);
    legend(lg, "target", C_CREMA);

    /* --- value tiles --- */
    s_p_val = value_tile(scr, cy,       "Pressure", "bar",  C_PRESS);
    s_f_val = value_tile(scr, cy + 54,  "Flow",     "ml/s", C_FLOW);
    s_v_val = value_tile(scr, cy + 108, "In cup",   "ml",   C_CREMA);

    /* --- bottom: volume progress + stop --- */
    const lv_coord_t by = cy + 166;
    s_vol_lbl = ui_label(scr, F_S, C_TEXT, "");
    lv_obj_set_pos(s_vol_lbl, UI_PAD + 4, by + 2);
    s_target_lbl = ui_label(scr, F_XS, C_TEXT_DIM, "");
    lv_obj_align(s_target_lbl, LV_ALIGN_TOP_LEFT, UI_PAD + 330 - 120, by + 4);
    lv_obj_set_width(s_target_lbl, 116);
    lv_obj_set_style_text_align(s_target_lbl, LV_TEXT_ALIGN_RIGHT, 0);

    s_vol_bar = ui_bar(scr, 330, 10, C_CREMA);
    lv_obj_set_pos(s_vol_bar, UI_PAD, by + 26);

    s_stop_btn = ui_btn(scr, NULL, &st_btn_danger, stop_cb, NULL);
    lv_obj_set_pos(s_stop_btn, UI_W - UI_PAD - 120, by);
    lv_obj_set_size(s_stop_btn, 120, 44);
    s_stop_lbl = ui_label(s_stop_btn, F_M, lv_color_white(), LV_SYMBOL_STOP "  Stop");
    lv_obj_center(s_stop_lbl);

    return scr;
}

static void set_finished(bool fin)
{
    s_finished = fin;
    lv_obj_remove_style(s_stop_btn, &st_btn_danger, 0);
    lv_obj_remove_style(s_stop_btn, &st_btn_accent, 0);
    lv_obj_add_style(s_stop_btn, fin ? &st_btn_accent : &st_btn_danger, 0);
    lv_obj_set_style_text_color(s_stop_lbl, fin ? C_ON_CREMA : lv_color_white(), 0);
    lv_label_set_text(s_stop_lbl, fin ? LV_SYMBOL_OK "  Done" : LV_SYMBOL_STOP "  Stop");
}

void scr_brew_enter(void)
{
    brew_profile_t *p = gaggia_active_profile();
    float span = p ? (float)p->max_s : 40.0f;
    if (span < 20) span = 20;
    s_points = (uint16_t)(span * 1000 / SAMPLE_MS);
    if (s_points > MAX_POINTS) s_points = MAX_POINTS;

    lv_chart_set_point_count(s_chart, s_points);
    lv_chart_set_all_value(s_chart, s_ser_press, LV_CHART_POINT_NONE);
    lv_chart_set_all_value(s_chart, s_ser_flow, LV_CHART_POINT_NONE);
    for (uint16_t i = 0; i < s_points; i++) {
        float tb = gaggia_profile_target_bar(p, i * SAMPLE_MS / 1000.0f);
        lv_chart_set_value_by_id(s_chart, s_ser_target, i, (lv_coord_t)(tb * 10));
    }
    lv_chart_refresh(s_chart);
    lv_label_set_text_fmt(s_x_end_lbl, "%d s", (int)(s_points * SAMPLE_MS / 1000));

    s_last_idx = -1;
    lv_label_set_text(s_prof_lbl, p ? p->name : "");
    lv_label_set_text(s_timer, "0.0");
    lv_obj_align_to(s_timer_unit, s_timer, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -8);
    lv_bar_set_value(s_vol_bar, 0, LV_ANIM_OFF);
    set_finished(false);
}

static void plot(const gaggia_telemetry_t *t)
{
    int idx = (int)(t->shot_ms / SAMPLE_MS);
    if (idx >= s_points) idx = s_points - 1;
    if (idx <= s_last_idx) return;
    for (int i = s_last_idx + 1; i <= idx; i++) {
        lv_chart_set_value_by_id(s_chart, s_ser_press, i, (lv_coord_t)lroundf(t->pressure * 10));
        lv_chart_set_value_by_id(s_chart, s_ser_flow,  i, (lv_coord_t)lroundf(t->flow * 10));
    }
    s_last_idx = idx;
    lv_chart_refresh(s_chart);
}

void scr_brew_update(const gaggia_telemetry_t *t)
{
    brew_profile_t *p = gaggia_active_profile();
    bool brewing = (t->status == GS_BREWING);

    if (brewing) {
        plot(t);
        uint32_t ds = t->shot_ms / 100;
        ui_set_text_fmt_if_changed(s_timer, "%lu.%lu", (unsigned long)(ds / 10), (unsigned long)(ds % 10));
        lv_obj_align_to(s_timer_unit, s_timer, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -8);
    } else if (!s_finished && t->phase == PH_DONE) {
        set_finished(true);
        if (gaggia_settings()->sound) ui_hal_beep(150);
    }

    /* phase chip */
    lv_color_t chip = C_CREMA;
    switch (t->phase) {
    case PH_PREINFUSION: chip = C_PRESS; break;
    case PH_SOAK:        chip = C_FLOW;  break;
    case PH_EXTRACTION:  chip = C_CREMA; break;
    case PH_DONE:        chip = C_OK;    break;
    default:             chip = C_TEXT_MUTED; break;
    }
    lv_obj_set_style_bg_color(s_phase_chip, chip, 0);
    ui_set_text_fmt_if_changed(s_phase_lbl, "%s", gaggia_phase_str(brewing || t->phase == PH_DONE ? t->phase : PH_IDLE));

    ui_duty_set(&s_pump, t->pump_pct);
    ui_set_text_fmt_if_changed(s_temp_val, "%.1f°", (double)t->brew_temp);
    ui_set_text_fmt_if_changed(s_p_val, "%.1f", (double)t->pressure);
    ui_set_text_fmt_if_changed(s_f_val, "%.1f", (double)t->flow);
    ui_set_text_fmt_if_changed(s_v_val, "%d", (int)lroundf(t->volume));

    if (p) {
        ui_set_text_fmt_if_changed(s_vol_lbl, "%d of %u ml", (int)lroundf(t->volume), p->target_ml);
        float k = t->volume / (float)p->target_ml;
        lv_bar_set_value(s_vol_bar, (int32_t)(fminf(k, 1.0f) * 1000), LV_ANIM_ON);
        if (brewing) ui_set_text_fmt_if_changed(s_target_lbl, "target %.1f bar", (double)t->target_bar);
        else         ui_set_text_fmt_if_changed(s_target_lbl, "%s", "");
    }
}
