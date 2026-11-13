/**
 * scr_water.c — hot water: choose an amount, dispense, watch progress.
 */
#include "ui.h"
#include "ui_screens.h"
#include "ui_widgets.h"

#include <math.h>

static const uint16_t k_presets[] = { 100, 150, 250 };
#define N_PRESETS (sizeof(k_presets) / sizeof(k_presets[0]))

static lv_obj_t *s_arc, *s_val, *s_sub;
static lv_obj_t *s_amount, *s_preset[N_PRESETS];
static lv_obj_t *s_temp;
static lv_obj_t *s_start_btn, *s_start_lbl;
static bool      s_running;
static bool      s_restyle = true;   /* force button restyle on next update */
static ui_duty_t s_pump;
static bool      s_had_run;   /* volume belongs to the last hot-water run */

static void refresh_amount(void)
{
    uint16_t ml = gaggia_settings()->hot_water_ml;
    lv_label_set_text_fmt(s_amount, "%u", ml);
    for (size_t i = 0; i < N_PRESETS; i++) {
        bool on = (k_presets[i] == ml);
        lv_obj_set_style_bg_color(s_preset[i], on ? C_PRESS : C_SURFACE_HI, 0);
        lv_obj_set_style_text_color(s_preset[i], on ? C_ON_CREMA : C_TEXT, 0);
    }
    if (!s_running) lv_label_set_text_fmt(s_sub, "of %u ml", ml);
}

static void set_amount(int ml)
{
    if (ml < 20)  ml = 20;
    if (ml > 500) ml = 500;
    gaggia_settings()->hot_water_ml = (uint16_t)ml;
    gaggia_commit();
    refresh_amount();
}

static void minus_cb(lv_event_t *e)  { LV_UNUSED(e); set_amount(gaggia_settings()->hot_water_ml - 10); }
static void plus_cb(lv_event_t *e)   { LV_UNUSED(e); set_amount(gaggia_settings()->hot_water_ml + 10); }
static void preset_cb(lv_event_t *e) { set_amount((int)(intptr_t)lv_event_get_user_data(e)); }

static void start_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    const gaggia_telemetry_t *t = gaggia_tm();
    if (t->status == GS_HOT_WATER) { gaggia_cmd_hot_water(false); return; }
    if (t->faults & FAULT_WATER_EMPTY) { ui_toast(LV_SYMBOL_TINT "  Refill the water tank first", C_ERR); return; }
    if (t->status != GS_READY && t->status != GS_HEATING) { ui_toast("Machine is busy", C_WARN); return; }
    gaggia_cmd_hot_water(true);
}

static lv_obj_t *round_btn(lv_obj_t *parent, const char *sym, lv_event_cb_t cb)
{
    lv_obj_t *b = ui_btn(parent, sym, NULL, cb, NULL);
    lv_obj_set_size(b, 48, 48);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
    return b;
}

lv_obj_t *scr_water_create(void)
{
    lv_obj_t *scr = ui_screen_base();
    const lv_coord_t y0 = UI_CONTENT_Y;

    /* progress */
    lv_obj_t *pc = ui_card(scr, UI_PAD, y0, 206, 214, NULL, NULL);
    s_arc = lv_arc_create(pc);
    lv_obj_remove_style(s_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(s_arc, 172, 172);
    lv_obj_align(s_arc, LV_ALIGN_TOP_MID, 0, -2);
    lv_arc_set_rotation(s_arc, 270);
    lv_arc_set_bg_angles(s_arc, 0, 360);
    lv_arc_set_range(s_arc, 0, 1000);
    lv_arc_set_value(s_arc, 0);
    lv_obj_set_style_arc_width(s_arc, 9, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, C_LINE, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 9, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, C_PRESS, LV_PART_INDICATOR);

    s_val = ui_label(s_arc, F_HUGE, C_TEXT, "0");
    lv_obj_align(s_val, LV_ALIGN_CENTER, 0, -6);
    s_sub = ui_label(s_arc, F_XS, C_TEXT_DIM, "");
    lv_obj_align(s_sub, LV_ALIGN_CENTER, 0, 26);

    ui_duty_create(&s_pump, pc, 112, "Pump", C_PRESS);
    lv_obj_align(s_pump.root, LV_ALIGN_BOTTOM_LEFT, 2, 2);
    s_temp = ui_label(pc, F_XS, C_TEXT_DIM, "");
    lv_obj_align(s_temp, LV_ALIGN_BOTTOM_RIGHT, -2, 2);

    const lv_coord_t xr = UI_PAD + 216;
    const lv_coord_t wr = UI_W - xr - UI_PAD;

    /* amount */
    lv_obj_t *ac = ui_card(scr, xr, y0, wr, 144, NULL, NULL);
    lv_obj_set_style_pad_all(ac, 12, 0);
    lv_obj_t *t = ui_label(ac, F_S, C_TEXT, "Amount");
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *mb = round_btn(ac, LV_SYMBOL_MINUS, minus_cb);
    lv_obj_align(mb, LV_ALIGN_LEFT_MID, 0, -6);
    lv_obj_t *pb = round_btn(ac, LV_SYMBOL_PLUS, plus_cb);
    lv_obj_align(pb, LV_ALIGN_RIGHT_MID, 0, -6);

    s_amount = ui_label(ac, F_HUGE, C_PRESS, "");
    lv_obj_align(s_amount, LV_ALIGN_CENTER, 0, -10);
    lv_obj_t *u = ui_label(ac, F_XS, C_TEXT_DIM, "ml");
    lv_obj_align(u, LV_ALIGN_CENTER, 0, 16);

    lv_obj_t *row = ui_panel(ac, 0, 0, wr - 24, 30);
    lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    for (size_t i = 0; i < N_PRESETS; i++) {
        char buf[12];
        lv_snprintf(buf, sizeof(buf), "%u ml", k_presets[i]);
        lv_obj_t *b = ui_btn(row, buf, NULL, preset_cb, (void *)(intptr_t)k_presets[i]);
        lv_obj_set_size(b, (wr - 24 - 16) / 3, 30);
        lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_all(b, 0, 0);
        lv_obj_set_style_text_font(b, F_XS, 0);
        s_preset[i] = b;
    }

    /* start */
    s_start_btn = ui_btn(scr, NULL, NULL, start_cb, NULL);
    lv_obj_set_pos(s_start_btn, xr, y0 + 152);
    lv_obj_set_size(s_start_btn, wr, 62);
    lv_obj_set_style_radius(s_start_btn, UI_R_BIG, 0);
    s_start_lbl = ui_label(s_start_btn, F_L, C_ON_CREMA, "");
    lv_obj_center(s_start_lbl);

    return scr;
}

void scr_water_enter(void)
{
    s_restyle = true;
    if (gaggia_tm()->status != GS_HOT_WATER) s_had_run = false;
    refresh_amount();
}

void scr_water_update(const gaggia_telemetry_t *t)
{
    bool running = (t->status == GS_HOT_WATER);
    if (running) s_had_run = true;
    float vol = s_had_run ? t->volume : 0.0f;
    uint16_t target = gaggia_settings()->hot_water_ml;

    if (running != s_running || s_restyle) {
        s_running = running;
        s_restyle = false;
        lv_obj_set_style_bg_color(s_start_btn, running ? C_ERR : C_PRESS, 0);
        lv_obj_set_style_bg_grad_dir(s_start_btn, LV_GRAD_DIR_NONE, 0);
        lv_obj_set_style_text_color(s_start_lbl, running ? lv_color_white() : C_ON_CREMA, 0);
        lv_label_set_text(s_start_lbl, running ? LV_SYMBOL_STOP "  Stop" : LV_SYMBOL_TINT "  Dispense");
        if (running) lv_label_set_text(s_sub, "dispensing");
        else         lv_label_set_text_fmt(s_sub, "of %u ml", target);
    }

    ui_set_text_fmt_if_changed(s_val, "%d", (int)lroundf(vol));
    float k = vol / (float)target;
    lv_arc_set_value(s_arc, (int16_t)(fminf(fmaxf(k, 0), 1) * 1000));
    ui_set_text_fmt_if_changed(s_temp, "%d°C", (int)lroundf(t->steam_temp));
    ui_duty_set(&s_pump, t->pump_pct);
}
