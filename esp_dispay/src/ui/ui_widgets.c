#include "ui_widgets.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

/* ================================================================== */
/* Primitives                                                          */
/* ================================================================== */

lv_obj_t *ui_panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

lv_obj_t *ui_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                  lv_event_cb_t on_click, void *user)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_add_style(c, &st_card, 0);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, w, h);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    if (on_click) {
        lv_obj_add_style(c, &st_card_pressed, LV_STATE_PRESSED);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(c, on_click, LV_EVENT_CLICKED, user);
    } else {
        lv_obj_clear_flag(c, LV_OBJ_FLAG_CLICKABLE);
    }
    return c;
}

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, text ? text : "");
    return l;
}

lv_obj_t *ui_btn(lv_obj_t *parent, const char *text, lv_style_t *variant,
                 lv_event_cb_t on_click, void *user)
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_add_style(b, &st_btn, 0);
    if (variant) lv_obj_add_style(b, variant, 0);
    lv_obj_add_style(b, &st_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(b, &st_btn_disabled, LV_STATE_DISABLED);
    if (on_click) lv_obj_add_event_cb(b, on_click, LV_EVENT_CLICKED, user);
    if (text) {
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, text);
        lv_obj_center(l);
    }
    return b;
}

void ui_btn_set_enabled(lv_obj_t *btn, bool en)
{
    if (en) lv_obj_clear_state(btn, LV_STATE_DISABLED);
    else    lv_obj_add_state(btn, LV_STATE_DISABLED);
}

lv_obj_t *ui_bar(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, lv_color_t color)
{
    lv_obj_t *b = lv_bar_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, C_LINE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_anim_time(b, 250, 0);
    lv_bar_set_range(b, 0, 1000);
    return b;
}

/* Three wavy strokes — LVGL's built-in symbol font has no steam glyph. */
static const lv_point_t k_wave[3][5] = {
    { {2, 18}, {5, 14}, {2, 10}, {5, 6}, {3, 2} },
    { {9, 18}, {12, 14}, {9, 10}, {12, 6}, {10, 2} },
    { {16, 18}, {19, 14}, {16, 10}, {19, 6}, {17, 2} },
};

lv_obj_t *ui_steam_icon(lv_obj_t *parent, lv_color_t color)
{
    lv_obj_t *box = ui_panel(parent, 0, 0, 22, 20);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < 3; i++) {
        lv_obj_t *ln = lv_line_create(box);
        lv_line_set_points(ln, k_wave[i], 5);
        lv_obj_set_style_line_width(ln, 2, 0);
        lv_obj_set_style_line_rounded(ln, true, 0);
        lv_obj_set_style_line_color(ln, color, 0);
    }
    return box;
}

void ui_set_text_fmt_if_changed(lv_obj_t *label, const char *fmt, ...)
{
    char buf[64];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (strcmp(lv_label_get_text(label), buf) != 0) lv_label_set_text(label, buf);
}

/* ================================================================== */
/* Boiler gauge                                                        */
/* ================================================================== */

#define GAUGE_START 135
#define GAUGE_SWEEP 270

void ui_gauge_create(ui_gauge_t *g, lv_obj_t *parent, lv_coord_t size,
                     float min, float max, lv_color_t color, const char *unit)
{
    memset(g, 0, sizeof(*g));
    g->min = min; g->max = max;
    g->last_val10 = g->last_set10 = INT16_MIN;

    g->root = ui_panel(parent, 0, 0, size, size);
    lv_obj_clear_flag(g->root, LV_OBJ_FLAG_CLICKABLE);

    /* Tick ring, inside the value arc */
    lv_coord_t inner = size - 36;
    lv_obj_t *m = lv_meter_create(g->root);
    lv_obj_remove_style_all(m);
    lv_obj_set_size(m, inner, inner);
    lv_obj_center(m);
    lv_obj_clear_flag(m, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_text_opa(m, LV_OPA_TRANSP, LV_PART_TICKS);
    lv_meter_scale_t *sc = lv_meter_add_scale(m);
    lv_meter_set_scale_ticks(m, sc, 37, 1, 5, C_TEXT_MUTED);
    lv_meter_set_scale_major_ticks(m, sc, 9, 2, 9, C_TEXT_DIM, 0);
    lv_meter_set_scale_range(m, sc, 0, 100, GAUGE_SWEEP, GAUGE_START);

    /* Value arc */
    g->arc = lv_arc_create(g->root);
    lv_obj_remove_style(g->arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(g->arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(g->arc, size, size);
    lv_obj_center(g->arc);
    lv_obj_set_style_pad_all(g->arc, 0, 0);
    lv_obj_set_style_arc_width(g->arc, 9, LV_PART_MAIN);
    lv_obj_set_style_arc_color(g->arc, C_LINE, LV_PART_MAIN);
    lv_obj_set_style_arc_width(g->arc, 9, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(g->arc, color, LV_PART_INDICATOR);
    lv_arc_set_rotation(g->arc, GAUGE_START);
    lv_arc_set_bg_angles(g->arc, 0, GAUGE_SWEEP);
    lv_arc_set_range(g->arc, 0, 1000);
    lv_arc_set_value(g->arc, 0);

    /* Setpoint marker: a short fat arc segment on the same circle */
    g->marker = lv_arc_create(g->root);
    lv_obj_remove_style(g->marker, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(g->marker, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(g->marker, size + 8, size + 8);
    lv_obj_center(g->marker);
    lv_obj_set_style_pad_all(g->marker, 0, 0);
    lv_obj_set_style_arc_opa(g->marker, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(g->marker, 17, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(g->marker, C_TEXT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(g->marker, true, LV_PART_INDICATOR);
    lv_arc_set_mode(g->marker, LV_ARC_MODE_NORMAL);
    lv_arc_set_rotation(g->marker, 0);
    lv_arc_set_bg_angles(g->marker, 0, 360);
    lv_arc_set_angles(g->marker, 0, 0);

    /* Center text */
    g->value = ui_label(g->root, F_HUGE, C_TEXT, "--");
    lv_obj_align(g->value, LV_ALIGN_CENTER, -6, -4);

    g->unit = ui_label(g->root, F_M, C_TEXT_DIM, unit);
    lv_obj_align_to(g->unit, g->value, LV_ALIGN_OUT_RIGHT_TOP, 2, 4);

    g->sub = ui_label(g->root, F_XS, C_TEXT_DIM, "");
    lv_obj_align(g->sub, LV_ALIGN_CENTER, 0, 30);
}

static float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

void ui_gauge_set(ui_gauge_t *g, float value, float setpoint)
{
    int16_t v10 = (int16_t)lroundf(value * 10.0f);
    int16_t s10 = (int16_t)lroundf(setpoint * 10.0f);

    if (v10 != g->last_val10) {
        g->last_val10 = v10;
        float k = clamp01((value - g->min) / (g->max - g->min));
        lv_arc_set_value(g->arc, (int16_t)(k * 1000));
        if (value >= 100.0f) lv_label_set_text_fmt(g->value, "%d", (int)lroundf(value));
        else                 lv_label_set_text_fmt(g->value, "%d.%d", v10 / 10, abs(v10 % 10));
        lv_obj_align(g->value, LV_ALIGN_CENTER, -6, -4);
        lv_obj_align_to(g->unit, g->value, LV_ALIGN_OUT_RIGHT_TOP, 2, 4);
    }
    if (s10 != g->last_set10) {
        g->last_set10 = s10;
        float k = clamp01((setpoint - g->min) / (g->max - g->min));
        int ang = (GAUGE_START + (int)(k * GAUGE_SWEEP)) % 360;
        lv_arc_set_angles(g->marker, (ang + 359) % 360, (ang + 1) % 360);
        if (setpoint >= 100.0f) lv_label_set_text_fmt(g->sub, "target %d", (int)lroundf(setpoint));
        else                    lv_label_set_text_fmt(g->sub, "target %d.%d", s10 / 10, abs(s10 % 10));
    }
}

void ui_gauge_set_color(ui_gauge_t *g, lv_color_t c)
{
    lv_obj_set_style_arc_color(g->arc, c, LV_PART_INDICATOR);
}

/* ================================================================== */
/* Duty bar                                                            */
/* ================================================================== */

void ui_duty_create(ui_duty_t *d, lv_obj_t *parent, lv_coord_t w,
                    const char *title, lv_color_t color)
{
    d->last = -1;
    d->root = ui_panel(parent, 0, 0, w, 30);
    lv_obj_clear_flag(d->root, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *t = ui_label(d->root, F_XS, C_TEXT_DIM, title);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);
    d->val = ui_label(d->root, F_XS, C_TEXT, "0%");
    lv_obj_align(d->val, LV_ALIGN_TOP_RIGHT, 0, 0);

    d->bar = ui_bar(d->root, w, 6, color);
    lv_obj_set_style_anim_time(d->bar, 120, 0);
    lv_obj_align(d->bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

void ui_duty_set(ui_duty_t *d, uint8_t pct)
{
    if (pct > 100) pct = 100;
    if (d->last == pct) return;
    d->last = pct;
    lv_label_set_text_fmt(d->val, "%u%%", pct);
    lv_obj_set_style_text_color(d->val, pct ? C_TEXT : C_TEXT_MUTED, 0);
    lv_bar_set_value(d->bar, pct * 10, LV_ANIM_ON);
}

/* ================================================================== */
/* Overlays                                                            */
/* ================================================================== */

static lv_obj_t *overlay_create(void)
{
    lv_obj_t *ov = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(ov);
    lv_obj_set_size(ov, UI_W, UI_H);
    lv_obj_set_style_bg_color(ov, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ov, LV_OPA_60, 0);
    lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);      /* swallow touches */
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_fade_in(ov, 150, 0);
    return ov;
}

static lv_obj_t *overlay_panel(lv_obj_t *ov, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *p = ui_card(ov, 0, 0, w, h, NULL, NULL);
    lv_obj_set_style_bg_color(p, C_SURFACE, 0);
    lv_obj_set_style_border_color(p, C_CREMA_DEEP, 0);
    lv_obj_set_style_border_opa(p, LV_OPA_50, 0);
    lv_obj_set_style_shadow_width(p, 40, 0);
    lv_obj_set_style_shadow_color(p, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(p, LV_OPA_70, 0);
    lv_obj_center(p);
    return p;
}

/* ---- number editor ---- */

typedef struct {
    ui_number_req_t req;
    ui_number_cb_t  cb;
    void           *user;
    lv_obj_t       *overlay;
    lv_obj_t       *value_lbl;
    lv_obj_t       *bar;
} num_ctx_t;

static void num_render(num_ctx_t *c)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "%.*f", c->req.decimals, (double)c->req.value);
    lv_label_set_text(c->value_lbl, buf);
    float k = (c->req.value - c->req.min) / (c->req.max - c->req.min);
    lv_bar_set_value(c->bar, (int32_t)(clamp01(k) * 1000), LV_ANIM_ON);
}

static void num_step(num_ctx_t *c, int dir)
{
    float v = c->req.value + dir * c->req.step;
    if (v < c->req.min) v = c->req.min;
    if (v > c->req.max) v = c->req.max;
    /* snap to step grid to kill float drift */
    v = roundf(v / c->req.step) * c->req.step;
    c->req.value = v;
    num_render(c);
}

static void num_minus_cb(lv_event_t *e) { num_step(lv_event_get_user_data(e), -1); }
static void num_plus_cb(lv_event_t *e)  { num_step(lv_event_get_user_data(e), +1); }

static void num_cancel_cb(lv_event_t *e)
{
    num_ctx_t *c = lv_event_get_user_data(e);
    lv_obj_del_async(c->overlay);
}

static void num_ok_cb(lv_event_t *e)
{
    num_ctx_t *c = lv_event_get_user_data(e);
    if (c->cb) c->cb(c->req.value, c->user);
    lv_obj_del_async(c->overlay);
}

static void num_free_cb(lv_event_t *e) { lv_mem_free(lv_event_get_user_data(e)); }

static lv_obj_t *round_step_btn(lv_obj_t *parent, const char *sym, num_ctx_t *c, lv_event_cb_t cb)
{
    lv_obj_t *b = ui_btn(parent, sym, NULL, NULL, NULL);
    lv_obj_set_size(b, 62, 62);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_text_font(b, F_L, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, c);
    lv_obj_add_event_cb(b, cb, LV_EVENT_LONG_PRESSED_REPEAT, c);
    return b;
}

void ui_number_editor(const ui_number_req_t *req, ui_number_cb_t cb, void *user)
{
    num_ctx_t *c = lv_mem_alloc(sizeof(num_ctx_t));
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->req = *req; c->cb = cb; c->user = user;
    if (c->req.step <= 0) c->req.step = 1;

    c->overlay = overlay_create();
    lv_obj_add_event_cb(c->overlay, num_free_cb, LV_EVENT_DELETE, c);

    lv_obj_t *p = overlay_panel(c->overlay, 330, 236);

    lv_obj_t *t = ui_label(p, F_M, C_TEXT_DIM, req->title);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 0);

    round_step_btn(p, LV_SYMBOL_MINUS, c, num_minus_cb);
    lv_obj_align(lv_obj_get_child(p, -1), LV_ALIGN_LEFT_MID, 0, -14);
    round_step_btn(p, LV_SYMBOL_PLUS, c, num_plus_cb);
    lv_obj_align(lv_obj_get_child(p, -1), LV_ALIGN_RIGHT_MID, 0, -14);

    c->value_lbl = ui_label(p, F_GIANT, C_CREMA, "");
    lv_obj_align(c->value_lbl, LV_ALIGN_CENTER, 0, -22);
    lv_obj_t *u = ui_label(p, F_S, C_TEXT_DIM, req->unit ? req->unit : "");
    lv_obj_align(u, LV_ALIGN_CENTER, 0, 14);

    c->bar = ui_bar(p, 290, 4, C_CREMA);
    lv_obj_align(c->bar, LV_ALIGN_CENTER, 0, 36);

    lv_obj_t *cancel = ui_btn(p, "Cancel", NULL, num_cancel_cb, c);
    lv_obj_set_size(cancel, 140, 44);
    lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *ok = ui_btn(p, "Save", &st_btn_accent, num_ok_cb, c);
    lv_obj_set_size(ok, 140, 44);
    lv_obj_align(ok, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    num_render(c);
}

/* ---- time editor: HH : MM, each with its own +/- ---- */

typedef struct {
    int h, m;
    ui_time_cb_t cb;
    void *user;
    lv_obj_t *overlay, *h_lbl, *m_lbl;
} time_ctx_t;

static void time_render(time_ctx_t *c)
{
    lv_label_set_text_fmt(c->h_lbl, "%02d", c->h);
    lv_label_set_text_fmt(c->m_lbl, "%02d", c->m);
}

static void time_btn_cb(lv_event_t *e)
{
    lv_obj_t *b = lv_event_get_target(e);
    time_ctx_t *c = lv_event_get_user_data(e);
    int code = (int)(intptr_t)lv_obj_get_user_data(b);   /* 0 h-, 1 h+, 2 m-, 3 m+ */
    if (code == 0) c->h = (c->h + 23) % 24;
    if (code == 1) c->h = (c->h + 1) % 24;
    if (code == 2) c->m = (c->m + 59) % 60;
    if (code == 3) c->m = (c->m + 1) % 60;
    time_render(c);
}

static void time_ok_cb(lv_event_t *e)
{
    time_ctx_t *c = lv_event_get_user_data(e);
    if (c->cb) c->cb(c->h, c->m, c->user);
    lv_obj_del_async(c->overlay);
}

static void time_cancel_cb(lv_event_t *e)
{
    time_ctx_t *c = lv_event_get_user_data(e);
    lv_obj_del_async(c->overlay);
}

static lv_obj_t *time_col(lv_obj_t *p, time_ctx_t *c, lv_coord_t x, int code_minus, lv_obj_t **lbl)
{
    lv_obj_t *col = ui_panel(p, x, 30, 96, 140);
    for (int i = 0; i < 2; i++) {
        lv_obj_t *b = ui_btn(col, i ? LV_SYMBOL_DOWN : LV_SYMBOL_UP, NULL, NULL, NULL);
        lv_obj_set_size(b, 96, 36);
        lv_obj_align(b, i ? LV_ALIGN_BOTTOM_MID : LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_user_data(b, (void *)(intptr_t)(i ? code_minus : code_minus + 1));
        lv_obj_add_event_cb(b, time_btn_cb, LV_EVENT_CLICKED, c);
        lv_obj_add_event_cb(b, time_btn_cb, LV_EVENT_LONG_PRESSED_REPEAT, c);
    }
    *lbl = ui_label(col, F_GIANT, C_CREMA, "00");
    lv_obj_center(*lbl);
    return col;
}

void ui_time_editor(const char *title, int hour, int minute, ui_time_cb_t cb, void *user)
{
    time_ctx_t *c = lv_mem_alloc(sizeof(time_ctx_t));
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->h = hour; c->m = minute; c->cb = cb; c->user = user;

    c->overlay = overlay_create();
    lv_obj_add_event_cb(c->overlay, num_free_cb, LV_EVENT_DELETE, c);
    lv_obj_t *p = overlay_panel(c->overlay, 300, 248);

    lv_obj_t *t = ui_label(p, F_M, C_TEXT_DIM, title);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 0);

    time_col(p, c, 20, 0, &c->h_lbl);
    time_col(p, c, 160, 2, &c->m_lbl);
    lv_obj_t *colon = ui_label(p, F_GIANT, C_TEXT_DIM, ":");
    lv_obj_align(colon, LV_ALIGN_TOP_MID, 0, 72);

    lv_obj_t *cancel = ui_btn(p, "Cancel", NULL, time_cancel_cb, c);
    lv_obj_set_size(cancel, 126, 40);
    lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *ok = ui_btn(p, "Set", &st_btn_accent, time_ok_cb, c);
    lv_obj_set_size(ok, 126, 40);
    lv_obj_align(ok, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    time_render(c);
}

/* ---- confirm ---- */

typedef struct {
    ui_confirm_cb_t cb;
    void *user;
    lv_obj_t *overlay;
} confirm_ctx_t;

static void confirm_no_cb(lv_event_t *e)
{
    confirm_ctx_t *c = lv_event_get_user_data(e);
    lv_obj_del_async(c->overlay);
}
static void confirm_yes_cb(lv_event_t *e)
{
    confirm_ctx_t *c = lv_event_get_user_data(e);
    if (c->cb) c->cb(c->user);
    lv_obj_del_async(c->overlay);
}

void ui_confirm(const char *title, const char *text, const char *ok_text,
                bool danger, ui_confirm_cb_t on_ok, void *user)
{
    confirm_ctx_t *c = lv_mem_alloc(sizeof(confirm_ctx_t));
    if (!c) return;
    c->cb = on_ok; c->user = user;
    c->overlay = overlay_create();
    lv_obj_add_event_cb(c->overlay, num_free_cb, LV_EVENT_DELETE, c);

    lv_obj_t *p = overlay_panel(c->overlay, 330, 180);
    lv_obj_t *t = ui_label(p, F_L, C_TEXT, title);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 4, 2);
    lv_obj_t *d = ui_label(p, F_S, C_TEXT_DIM, text);
    lv_label_set_long_mode(d, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(d, 290);
    lv_obj_align(d, LV_ALIGN_TOP_LEFT, 4, 36);

    lv_obj_t *no = ui_btn(p, "Cancel", NULL, confirm_no_cb, c);
    lv_obj_set_size(no, 140, 44);
    lv_obj_align(no, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *yes = ui_btn(p, ok_text, danger ? &st_btn_danger : &st_btn_accent, confirm_yes_cb, c);
    lv_obj_set_size(yes, 140, 44);
    lv_obj_align(yes, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

/* ---- toast ---- */

static lv_obj_t *s_toast;

static void toast_deleted_cb(lv_event_t *e)
{
    if (lv_event_get_target(e) == s_toast) s_toast = NULL;
}

void ui_toast(const char *text, lv_color_t accent)
{
    if (s_toast) lv_obj_del(s_toast);

    s_toast = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_toast);
    lv_obj_add_style(s_toast, &st_chip, 0);
    lv_obj_set_style_bg_color(s_toast, C_SURFACE_HI, 0);
    lv_obj_set_style_border_width(s_toast, 1, 0);
    lv_obj_set_style_border_color(s_toast, accent, 0);
    lv_obj_set_style_pad_hor(s_toast, 16, 0);
    lv_obj_set_style_pad_ver(s_toast, 10, 0);
    lv_obj_set_style_shadow_width(s_toast, 24, 0);
    lv_obj_set_style_shadow_opa(s_toast, LV_OPA_60, 0);
    lv_obj_set_size(s_toast, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(s_toast, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *l = ui_label(s_toast, F_S, C_TEXT, text);
    LV_UNUSED(l);
    lv_obj_align(s_toast, LV_ALIGN_BOTTOM_MID, 0, -(UI_NAV_H + 10));
    lv_obj_add_event_cb(s_toast, toast_deleted_cb, LV_EVENT_DELETE, NULL);

    /* one opa animation only: a second lv_obj_fade_* would replace the first */
    lv_obj_fade_out(s_toast, 300, 2200);
    lv_obj_del_delayed(s_toast, 2550);
}

/* ================================================================== */
/* Rows                                                                */
/* ================================================================== */

lv_obj_t *ui_row(lv_obj_t *list, const char *title, const char *hint)
{
    lv_obj_t *r = lv_obj_create(list);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, LV_PCT(100), hint ? 56 : 48);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_set_style_border_color(r, C_LINE, 0);
    lv_obj_set_style_pad_hor(r, 4, 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t = ui_label(r, F_M, C_TEXT, title);
    if (hint) {
        lv_obj_align(t, LV_ALIGN_LEFT_MID, 0, -9);
        lv_obj_t *h = ui_label(r, F_XS, C_TEXT_MUTED, hint);
        lv_obj_align(h, LV_ALIGN_LEFT_MID, 0, 12);
    } else {
        lv_obj_align(t, LV_ALIGN_LEFT_MID, 0, 0);
    }
    return r;
}

lv_obj_t *ui_row_value(lv_obj_t *list, const char *title, const char *hint,
                       lv_event_cb_t on_click, void *user)
{
    lv_obj_t *r = ui_row(list, title, hint);
    lv_obj_t *b = ui_btn(r, NULL, NULL, on_click, user);
    lv_obj_set_size(b, LV_SIZE_CONTENT, 36);
    lv_obj_set_style_pad_hor(b, 14, 0);
    lv_obj_set_style_min_width(b, 90, 0);
    lv_obj_align(b, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_t *v = ui_label(b, F_M, C_CREMA, "");
    lv_obj_center(v);
    return v;
}

static void style_switch(lv_obj_t *sw)
{
    lv_obj_set_style_bg_color(sw, C_LINE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, C_CREMA, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, C_TEXT, LV_PART_KNOB);
    lv_obj_set_style_pad_all(sw, -3, LV_PART_KNOB);
}

lv_obj_t *ui_row_switch(lv_obj_t *list, const char *title, const char *hint, bool on,
                        lv_event_cb_t on_change, void *user)
{
    lv_obj_t *r = ui_row(list, title, hint);
    lv_obj_t *sw = lv_switch_create(r);
    style_switch(sw);
    lv_obj_set_size(sw, 54, 28);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -4, 0);
    if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
    if (on_change) lv_obj_add_event_cb(sw, on_change, LV_EVENT_VALUE_CHANGED, user);
    return sw;
}

lv_obj_t *ui_row_slider(lv_obj_t *list, const char *title, int32_t min, int32_t max,
                        int32_t val, lv_event_cb_t on_change, void *user)
{
    lv_obj_t *r = ui_row(list, title, NULL);
    lv_obj_t *s = lv_slider_create(r);
    lv_obj_set_size(s, 150, 8);
    lv_obj_align(s, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_set_style_bg_color(s, C_LINE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s, C_CREMA, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s, C_TEXT, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s, 6, LV_PART_KNOB);
    lv_obj_set_ext_click_area(s, 16);
    lv_slider_set_range(s, min, max);
    lv_slider_set_value(s, val, LV_ANIM_OFF);
    if (on_change) lv_obj_add_event_cb(s, on_change, LV_EVENT_VALUE_CHANGED, user);
    return s;
}

lv_obj_t *ui_row_action(lv_obj_t *list, const char *title, const char *hint,
                        const char *btn_text, bool danger, lv_event_cb_t on_click, void *user)
{
    lv_obj_t *r = ui_row(list, title, hint);
    lv_obj_t *b = ui_btn(r, btn_text, danger ? &st_btn_danger : NULL, on_click, user);
    lv_obj_set_size(b, LV_SIZE_CONTENT, 36);
    lv_obj_set_style_pad_hor(b, 16, 0);
    lv_obj_set_style_text_font(b, F_S, 0);
    lv_obj_align(b, LV_ALIGN_RIGHT_MID, 0, 0);
    return b;
}
