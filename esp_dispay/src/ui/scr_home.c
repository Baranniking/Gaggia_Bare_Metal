/**
 * scr_home.c — dashboard: brew boiler gauge, steam & pressure, active
 * profile and the big Brew button.
 */
#include "ui.h"
#include "ui_screens.h"
#include "ui_widgets.h"

#include <math.h>

static ui_gauge_t s_gauge;
static lv_obj_t *s_heat_icon;
static lv_obj_t *s_steam_val, *s_steam_bar;
static lv_obj_t *s_press_val, *s_press_bar;
static lv_obj_t *s_prof_name, *s_prof_meta;
static lv_obj_t *s_brew_btn, *s_brew_sub;
static bool      s_brew_allowed;

/* ---- actions ---- */

static void brew_temp_saved(float v, void *u)
{
    LV_UNUSED(u);
    brew_profile_t *p = gaggia_active_profile();
    if (!p) return;
    p->temp_c = v;
    gaggia_commit();
    scr_home_enter();
}

static void gauge_card_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    brew_profile_t *p = gaggia_active_profile();
    if (!p) return;
    ui_number_req_t r = {
        .title = "Brew temperature", .unit = "°C",
        .value = p->temp_c, .min = 85, .max = 100, .step = 0.5f, .decimals = 1,
    };
    ui_number_editor(&r, brew_temp_saved, NULL);
}

static void profile_card_cb(lv_event_t *e) { LV_UNUSED(e); ui_show(SCR_PROFILES); }
static void manual_cb(lv_event_t *e)       { LV_UNUSED(e); ui_show(SCR_MANUAL); }

static void brew_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    const gaggia_telemetry_t *t = gaggia_tm();
    if (t->faults & FAULT_WATER_EMPTY) { ui_toast(LV_SYMBOL_TINT "  Refill the water tank first", C_ERR); return; }
    if (!s_brew_allowed)               { ui_toast("Boiler is still heating", C_TEMP); return; }
    gaggia_cmd_brew(true);
}

/* ---- build ---- */

static lv_obj_t *mini_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, const char *title,
                           lv_color_t accent, bool steam_icon, lv_obj_t **value_out)
{
    lv_obj_t *c = ui_card(parent, x, y, 118, 80, NULL, NULL);
    lv_obj_set_style_pad_all(c, 10, 0);

    lv_obj_t *t = ui_label(c, F_XS, C_TEXT_DIM, title);
    if (steam_icon) {
        lv_obj_t *ic = ui_steam_icon(c, accent);
        lv_obj_align(ic, LV_ALIGN_TOP_RIGHT, 2, -3);
    } else {
        lv_obj_t *u = ui_label(c, F_XS, accent, "bar");
        lv_obj_align(u, LV_ALIGN_TOP_RIGHT, 0, 0);
    }
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

    *value_out = ui_label(c, F_XL, C_TEXT, "--");
    lv_obj_align(*value_out, LV_ALIGN_LEFT_MID, 0, 6);
    return c;
}

lv_obj_t *scr_home_create(void)
{
    lv_obj_t *scr = ui_screen_base();
    const lv_coord_t y0 = UI_CONTENT_Y;

    /* --- brew boiler gauge --- */
    lv_obj_t *gc = ui_card(scr, UI_PAD, y0, 206, 214, gauge_card_cb, NULL);
    ui_gauge_create(&s_gauge, gc, 172, 20, 110, C_TEMP, "°C");
    lv_obj_align(s_gauge.root, LV_ALIGN_TOP_MID, 0, -2);

    lv_obj_t *cap = ui_label(gc, F_XS, C_TEXT_DIM, "Brew boiler");
    lv_obj_align(cap, LV_ALIGN_BOTTOM_LEFT, 2, 2);
    s_heat_icon = ui_label(gc, F_XS, C_TEXT_MUTED, LV_SYMBOL_CHARGE " heater");
    lv_obj_align(s_heat_icon, LV_ALIGN_BOTTOM_RIGHT, -2, 2);

    /* --- right column --- */
    const lv_coord_t xr = UI_PAD + 206 + 10;
    const lv_coord_t wr = UI_W - xr - UI_PAD;

    lv_obj_t *sc = mini_card(scr, xr, y0, "Steam", C_STEAM, true, &s_steam_val);
    s_steam_bar = ui_bar(sc, 96, 4, C_STEAM);
    lv_obj_align(s_steam_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *pc = mini_card(scr, xr + 126, y0, "Pressure", C_PRESS, false, &s_press_val);
    s_press_bar = ui_bar(pc, 96, 4, C_PRESS);
    lv_obj_align(s_press_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    /* --- active profile --- */
    lv_obj_t *prc = ui_card(scr, xr, y0 + 88, wr, 56, profile_card_cb, NULL);
    lv_obj_set_style_pad_hor(prc, 14, 0);
    lv_obj_set_style_pad_ver(prc, 8, 0);
    lv_obj_t *pl = ui_label(prc, F_XS, C_TEXT_DIM, "Profile");
    lv_obj_align(pl, LV_ALIGN_TOP_LEFT, 0, 0);
    s_prof_name = ui_label(prc, F_M, C_TEXT, "");
    lv_obj_align(s_prof_name, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *arr = ui_label(prc, F_S, C_TEXT_DIM, LV_SYMBOL_RIGHT);
    lv_obj_align(arr, LV_ALIGN_RIGHT_MID, 0, 0);
    s_prof_meta = ui_label(prc, F_XS, C_TEXT_DIM, "");
    lv_obj_align(s_prof_meta, LV_ALIGN_RIGHT_MID, -22, 0);

    /* --- brew --- */
    s_brew_btn = ui_btn(scr, NULL, &st_btn_accent, brew_cb, NULL);
    lv_obj_set_pos(s_brew_btn, xr, y0 + 152);
    lv_obj_set_size(s_brew_btn, wr - 80, 62);
    lv_obj_set_style_radius(s_brew_btn, UI_R_BIG, 0);
    lv_obj_t *bl = ui_label(s_brew_btn, F_L, C_ON_CREMA, LV_SYMBOL_PLAY "  Brew");
    lv_obj_set_style_text_color(bl, lv_color_hex(0x2A1A0E), 0);
    lv_obj_align(bl, LV_ALIGN_LEFT_MID, 10, 0);
    s_brew_sub = ui_label(s_brew_btn, F_XS, C_ON_CREMA, "");
    lv_obj_align(s_brew_sub, LV_ALIGN_RIGHT_MID, -10, 0);

    lv_obj_t *mb = ui_btn(scr, NULL, NULL, manual_cb, NULL);
    lv_obj_set_pos(mb, xr + wr - 72, y0 + 152);
    lv_obj_set_size(mb, 72, 62);
    lv_obj_set_style_radius(mb, UI_R_BIG, 0);
    lv_obj_t *mi = ui_label(mb, F_L, C_CREMA, LV_SYMBOL_EDIT);
    lv_obj_align(mi, LV_ALIGN_CENTER, 0, -9);
    lv_obj_t *mt = ui_label(mb, F_XS, C_TEXT_DIM, "Manual");
    lv_obj_align(mt, LV_ALIGN_CENTER, 0, 15);

    return scr;
}

void scr_home_enter(void)
{
    brew_profile_t *p = gaggia_active_profile();
    if (!p) return;
    lv_label_set_text(s_prof_name, p->name);
    lv_label_set_text_fmt(s_prof_meta, "%d° / %u ml", (int)lroundf(p->temp_c), p->target_ml);
    lv_obj_align(s_prof_meta, LV_ALIGN_RIGHT_MID, -22, 0);
}

void scr_home_update(const gaggia_telemetry_t *t)
{
    float set = gaggia_brew_setpoint();
    ui_gauge_set(&s_gauge, t->brew_temp, set);
    ui_gauge_set_color(&s_gauge, fabsf(t->brew_temp - set) < 1.0f ? C_CREMA : C_TEMP);

    lv_obj_set_style_text_color(s_heat_icon, t->brew_heater_on ? C_TEMP : C_TEXT_MUTED, 0);

    ui_set_text_fmt_if_changed(s_steam_val, "%d°", (int)lroundf(t->steam_temp));
    float sk = (t->steam_temp - 20.0f) / (gaggia_settings()->steam_set - 20.0f);
    lv_bar_set_value(s_steam_bar, (int32_t)(fminf(fmaxf(sk, 0), 1) * 1000), LV_ANIM_ON);

    ui_set_text_fmt_if_changed(s_press_val, "%.1f", (double)t->pressure);
    lv_bar_set_value(s_press_bar, (int32_t)(fminf(fmaxf(t->pressure / 12.0f, 0), 1) * 1000), LV_ANIM_ON);

    bool allowed = (t->status == GS_READY) && !(t->faults & FAULT_WATER_EMPTY);
    if (allowed != s_brew_allowed || lv_label_get_text(s_brew_sub)[0] == 0) {
        s_brew_allowed = allowed;
        if (allowed) {
            lv_obj_remove_style(s_brew_btn, &st_btn_disabled, 0);
            lv_obj_set_style_text_color(lv_obj_get_child(s_brew_btn, 0), C_ON_CREMA, 0);
            lv_obj_set_style_text_color(s_brew_sub, C_ON_CREMA, 0);
        } else {
            lv_obj_add_style(s_brew_btn, &st_btn_disabled, 0);
            lv_obj_set_style_text_color(lv_obj_get_child(s_brew_btn, 0), C_TEXT_MUTED, 0);
            lv_obj_set_style_text_color(s_brew_sub, C_TEXT_DIM, 0);
        }
    }
    brew_profile_t *p = gaggia_active_profile();
    if (allowed && p) ui_set_text_fmt_if_changed(s_brew_sub, "%u ml", p->target_ml);
    else if (t->faults & FAULT_WATER_EMPTY) ui_set_text_fmt_if_changed(s_brew_sub, "%s", "no water");
    else ui_set_text_fmt_if_changed(s_brew_sub, "%s", "heating");
}
