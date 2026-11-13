/**
 * scr_profiles.c — pick and tune brew profiles. List on the left, the
 * selected profile's pressure curve and parameters on the right.
 */
#include "ui.h"
#include "ui_screens.h"
#include "ui_widgets.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <math.h>

#define LIST_W     150
#define PREVIEW_PTS 60

typedef struct {
    const char *caption;
    const char *title;     /* editor title */
    const char *unit;
    size_t      offset;
    bool        is_u16;
    float       min, max, step;
    uint8_t     decimals;
} param_def_t;

static const param_def_t k_params[] = {
    { "Pre-inf s", "Pre-infusion time",     "seconds", offsetof(brew_profile_t, preinf_s),   false, 0,  15, 0.5f, 1 },
    { "Pre bar",   "Pre-infusion pressure", "bar",     offsetof(brew_profile_t, preinf_bar), false, 0,   6, 0.5f, 1 },
    { "Bloom s",   "Bloom (pump off)",      "seconds", offsetof(brew_profile_t, soak_s),     false, 0,  20, 1.0f, 0 },
    { "Peak bar",  "Peak pressure",         "bar",     offsetof(brew_profile_t, brew_bar),   false, 3,  12, 0.5f, 1 },
    { "End bar",   "Pressure at the end",   "bar",     offsetof(brew_profile_t, end_bar),    false, 2,  12, 0.5f, 1 },
    { "Temp °C",   "Brew temperature",      "°C",      offsetof(brew_profile_t, temp_c),     false, 85, 100, 0.5f, 1 },
    { "Cup ml",    "Stop at volume",        "ml",      offsetof(brew_profile_t, target_ml),  true, 15,  120, 1.0f, 0 },
    { "Limit s",   "Time limit",            "seconds", offsetof(brew_profile_t, max_s),      true, 15,  90, 1.0f, 0 },
};
#define N_PARAMS (sizeof(k_params) / sizeof(k_params[0]))

static lv_obj_t *s_list;
static lv_obj_t *s_items[GAGGIA_MAX_PROFILES];
static lv_obj_t *s_add_btn;
static lv_obj_t *s_name, *s_use_btn, *s_use_lbl;
static lv_obj_t *s_chart, *s_chart_end;
static lv_chart_series_t *s_ser;
static lv_obj_t *s_param_val[N_PARAMS];
static uint8_t   s_view;   /* profile being shown */

static void refresh_detail(void);
static void rebuild_list(void);

/* ---- param access ---- */

static float param_get(const brew_profile_t *p, const param_def_t *d)
{
    const uint8_t *b = (const uint8_t *)p + d->offset;
    return d->is_u16 ? (float)*(const uint16_t *)b : *(const float *)b;
}

static void param_set(brew_profile_t *p, const param_def_t *d, float v)
{
    uint8_t *b = (uint8_t *)p + d->offset;
    if (d->is_u16) *(uint16_t *)b = (uint16_t)lroundf(v);
    else           *(float *)b = v;
}

/* ---- callbacks ---- */

static void param_saved(float v, void *user)
{
    const param_def_t *d = user;
    brew_profile_t *p = gaggia_profile(s_view);
    if (!p) return;
    param_set(p, d, v);
    gaggia_commit();
    refresh_detail();
    rebuild_list();
}

static void param_cb(lv_event_t *e)
{
    const param_def_t *d = lv_event_get_user_data(e);
    brew_profile_t *p = gaggia_profile(s_view);
    if (!p) return;
    ui_number_req_t r = {
        .title = d->title, .unit = d->unit, .value = param_get(p, d),
        .min = d->min, .max = d->max, .step = d->step, .decimals = d->decimals,
    };
    ui_number_editor(&r, param_saved, (void *)d);
}

static void item_cb(lv_event_t *e)
{
    s_view = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    rebuild_list();
    refresh_detail();
}

static void use_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (gaggia_settings()->active_profile == s_view) return;
    gaggia_select_profile(s_view);
    char buf[48];
    snprintf(buf, sizeof(buf), LV_SYMBOL_OK "  %s selected", gaggia_profile(s_view)->name);
    ui_toast(buf, C_CREMA);
    rebuild_list();
    refresh_detail();
}

static void add_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    gaggia_store_t *st = (gaggia_store_t *)gaggia_store();   /* owned by model */
    if (st->profile_count >= GAGGIA_MAX_PROFILES) {
        ui_toast("Profile slots are full", C_WARN);
        return;
    }
    brew_profile_t *dst = &st->profiles[st->profile_count];
    *dst = *gaggia_profile(s_view);
    snprintf(dst->name, sizeof(dst->name), "Custom %u", st->profile_count + 1);
    s_view = st->profile_count;
    st->profile_count++;
    gaggia_commit();
    rebuild_list();
    refresh_detail();
}

/* ---- list ---- */

static void rebuild_list(void)
{
    uint8_t active = gaggia_settings()->active_profile;
    for (int i = 0; i < GAGGIA_MAX_PROFILES; i++) {
        lv_obj_t *it = s_items[i];
        brew_profile_t *p = gaggia_profile((uint8_t)i);
        if (!p) { lv_obj_add_flag(it, LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_clear_flag(it, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t *name = lv_obj_get_child(it, 0);
        lv_obj_t *meta = lv_obj_get_child(it, 1);
        lv_label_set_text(name, p->name);
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f bar, %u ml", (double)p->brew_bar, p->target_ml);
        lv_label_set_text(meta, buf);   /* lv_snprintf has no %f */

        bool viewing = (i == s_view);
        bool in_use = (i == active);
        lv_obj_set_style_bg_color(it, viewing ? C_SURFACE_HI : C_SURFACE, 0);
        lv_obj_set_style_border_color(it, in_use ? C_CREMA : C_LINE, 0);
        lv_obj_set_style_border_width(it, in_use ? 2 : 1, 0);
        lv_obj_set_style_text_color(name, in_use ? C_CREMA : C_TEXT, 0);
    }
    lv_obj_move_to_index(s_add_btn, -1);
    if (gaggia_profile_count() >= GAGGIA_MAX_PROFILES) lv_obj_add_flag(s_add_btn, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(s_add_btn, LV_OBJ_FLAG_HIDDEN);
}

/* ---- detail ---- */

static void refresh_detail(void)
{
    brew_profile_t *p = gaggia_profile(s_view);
    if (!p) return;
    bool in_use = (gaggia_settings()->active_profile == s_view);

    lv_label_set_text(s_name, p->name);

    lv_obj_remove_style(s_use_btn, &st_btn_accent, 0);
    if (in_use) {
        lv_label_set_text(s_use_lbl, LV_SYMBOL_OK " In use");
        lv_obj_set_style_text_color(s_use_lbl, C_CREMA, 0);
    } else {
        lv_obj_add_style(s_use_btn, &st_btn_accent, 0);
        lv_label_set_text(s_use_lbl, "Use");
        lv_obj_set_style_text_color(s_use_lbl, C_ON_CREMA, 0);
    }

    float span = (float)p->max_s;
    for (int i = 0; i < PREVIEW_PTS; i++) {
        float t = span * i / (PREVIEW_PTS - 1);
        lv_chart_set_value_by_id(s_chart, s_ser, i, (lv_coord_t)lroundf(gaggia_profile_target_bar(p, t) * 10));
    }
    lv_chart_refresh(s_chart);
    lv_label_set_text_fmt(s_chart_end, "%u s", p->max_s);
    lv_obj_align_to(s_chart_end, s_chart, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, -2);

    for (size_t i = 0; i < N_PARAMS; i++) {
        const param_def_t *d = &k_params[i];
        char buf[16];
        snprintf(buf, sizeof(buf), "%.*f", d->decimals, (double)param_get(p, d));
        lv_label_set_text(s_param_val[i], buf);
    }
}

/* ---- build ---- */

lv_obj_t *scr_profiles_create(void)
{
    lv_obj_t *scr = ui_screen_base();
    const lv_coord_t y0 = UI_CONTENT_Y;
    const lv_coord_t h  = UI_CONTENT_H + 2;

    /* list */
    s_list = ui_panel(scr, UI_PAD, y0, LIST_W, h);
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_list, 6, 0);

    for (int i = 0; i < GAGGIA_MAX_PROFILES; i++) {
        lv_obj_t *it = ui_card(s_list, 0, 0, LIST_W, 48, item_cb, (void *)(intptr_t)i);
        lv_obj_set_style_radius(it, UI_R_SMALL, 0);
        lv_obj_set_style_pad_hor(it, 12, 0);
        lv_obj_set_style_pad_ver(it, 6, 0);
        lv_obj_t *n = ui_label(it, F_S, C_TEXT, "");
        lv_obj_align(n, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_t *m = ui_label(it, F_XS, C_TEXT_DIM, "");
        lv_obj_align(m, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        s_items[i] = it;
    }
    s_add_btn = ui_btn(s_list, LV_SYMBOL_PLUS "  Copy as new", NULL, add_cb, NULL);
    lv_obj_set_size(s_add_btn, LIST_W, 40);
    lv_obj_set_style_text_font(s_add_btn, F_XS, 0);
    lv_obj_set_style_bg_opa(s_add_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_add_btn, 1, 0);
    lv_obj_set_style_border_color(s_add_btn, C_LINE, 0);
    lv_obj_set_style_text_color(s_add_btn, C_TEXT_DIM, 0);

    /* detail */
    const lv_coord_t xd = UI_PAD + LIST_W + 8;
    const lv_coord_t wd = UI_W - xd - UI_PAD;
    lv_obj_t *dc = ui_card(scr, xd, y0, wd, h, NULL, NULL);
    lv_obj_set_style_pad_all(dc, 10, 0);
    const lv_coord_t iw = wd - 20;

    s_name = ui_label(dc, F_L, C_TEXT, "");
    lv_obj_align(s_name, LV_ALIGN_TOP_LEFT, 2, 2);

    s_use_btn = ui_btn(dc, NULL, NULL, use_cb, NULL);
    lv_obj_set_size(s_use_btn, 88, 30);
    lv_obj_set_style_radius(s_use_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(s_use_btn, 0, 0);
    lv_obj_align(s_use_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    s_use_lbl = ui_label(s_use_btn, F_S, C_ON_CREMA, "Use");
    lv_obj_center(s_use_lbl);

    s_chart = lv_chart_create(dc);
    lv_obj_set_size(s_chart, iw, 52);
    lv_obj_align(s_chart, LV_ALIGN_TOP_LEFT, 0, 38);
    lv_obj_set_style_bg_opa(s_chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_chart, 0, 0);
    lv_obj_set_style_pad_all(s_chart, 0, 0);
    lv_obj_set_style_line_color(s_chart, C_LINE, LV_PART_MAIN);
    lv_obj_set_style_line_width(s_chart, 3, LV_PART_ITEMS);
    lv_obj_set_style_size(s_chart, 0, LV_PART_INDICATOR);
    lv_obj_clear_flag(s_chart, LV_OBJ_FLAG_CLICKABLE);
    lv_chart_set_div_line_count(s_chart, 3, 0);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 120);
    lv_chart_set_point_count(s_chart, PREVIEW_PTS);
    s_ser = lv_chart_add_series(s_chart, C_CREMA, LV_CHART_AXIS_PRIMARY_Y);
    s_chart_end = ui_label(dc, F_XS, C_TEXT_MUTED, "");
    lv_obj_align_to(s_chart_end, s_chart, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, -2);

    /* parameter grid 4 x 2 */
    const lv_coord_t gap = 6;
    const lv_coord_t tw = (iw - 3 * gap) / 4;
    const lv_coord_t th = 46;
    const lv_coord_t gy = 104;
    for (size_t i = 0; i < N_PARAMS; i++) {
        lv_coord_t x = (lv_coord_t)(i % 4) * (tw + gap);
        lv_coord_t y = gy + (lv_coord_t)(i / 4) * (th + gap);
        lv_obj_t *t = ui_btn(dc, NULL, NULL, param_cb, (void *)&k_params[i]);
        lv_obj_set_pos(t, x, y);
        lv_obj_set_size(t, tw, th);
        lv_obj_set_style_pad_all(t, 6, 0);
        lv_obj_set_style_radius(t, 10, 0);
        lv_obj_t *c = ui_label(t, F_XS, C_TEXT_DIM, k_params[i].caption);
        lv_obj_align(c, LV_ALIGN_TOP_LEFT, 0, -2);
        s_param_val[i] = ui_label(t, F_M, C_CREMA, "");
        lv_obj_align(s_param_val[i], LV_ALIGN_BOTTOM_LEFT, 0, 2);
    }

    return scr;
}

void scr_profiles_enter(void)
{
    s_view = gaggia_settings()->active_profile;
    rebuild_list();
    refresh_detail();
}

void scr_profiles_update(const gaggia_telemetry_t *t) { LV_UNUSED(t); }
