/**
 * ui_widgets.h — small reusable building blocks for every screen.
 */
#pragma once
#include "lvgl.h"
#include "ui_theme.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- primitives ---- */
lv_obj_t *ui_panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h);
lv_obj_t *ui_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                  lv_event_cb_t on_click, void *user);
lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text);
lv_obj_t *ui_btn(lv_obj_t *parent, const char *text, lv_style_t *variant,
                 lv_event_cb_t on_click, void *user);
void      ui_btn_set_enabled(lv_obj_t *btn, bool en);
lv_obj_t *ui_bar(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, lv_color_t color);
lv_obj_t *ui_steam_icon(lv_obj_t *parent, lv_color_t color);
void      ui_set_text_fmt_if_changed(lv_obj_t *label, const char *fmt, ...);

/* ---- boiler gauge (arc + tick ring + setpoint marker) ---- */
typedef struct {
    lv_obj_t *root;
    lv_obj_t *arc;
    lv_obj_t *marker;
    lv_obj_t *value;
    lv_obj_t *unit;
    lv_obj_t *sub;
    float     min, max;
    int16_t   last_val10, last_set10;
} ui_gauge_t;

void ui_gauge_create(ui_gauge_t *g, lv_obj_t *parent, lv_coord_t size,
                     float min, float max, lv_color_t color, const char *unit);
void ui_gauge_set(ui_gauge_t *g, float value, float setpoint);
void ui_gauge_set_color(ui_gauge_t *g, lv_color_t c);

/* ---- actuator duty bar: "Pump  62%" + thin bar (PWM outputs) ---- */
typedef struct {
    lv_obj_t *root;
    lv_obj_t *bar;
    lv_obj_t *val;
    int16_t   last;
} ui_duty_t;

void ui_duty_create(ui_duty_t *d, lv_obj_t *parent, lv_coord_t w,
                    const char *title, lv_color_t color);
void ui_duty_set(ui_duty_t *d, uint8_t pct);

/* ---- overlays (live on lv_layer_top) ---- */
typedef void (*ui_number_cb_t)(float value, void *user);
typedef struct {
    const char *title;
    const char *unit;
    float value, min, max, step;
    uint8_t decimals;
} ui_number_req_t;

void ui_number_editor(const ui_number_req_t *req, ui_number_cb_t cb, void *user);

typedef void (*ui_time_cb_t)(int hour, int minute, void *user);
void ui_time_editor(const char *title, int hour, int minute, ui_time_cb_t cb, void *user);

typedef void (*ui_confirm_cb_t)(void *user);
void ui_confirm(const char *title, const char *text, const char *ok_text,
                bool danger, ui_confirm_cb_t on_ok, void *user);

void ui_toast(const char *text, lv_color_t accent);

/* ---- settings-style rows ---- */
lv_obj_t *ui_row(lv_obj_t *list, const char *title, const char *hint);
lv_obj_t *ui_row_value(lv_obj_t *list, const char *title, const char *hint,
                       lv_event_cb_t on_click, void *user);   /* returns value label */
lv_obj_t *ui_row_switch(lv_obj_t *list, const char *title, const char *hint, bool on,
                        lv_event_cb_t on_change, void *user); /* returns switch */
lv_obj_t *ui_row_slider(lv_obj_t *list, const char *title, int32_t min, int32_t max,
                        int32_t val, lv_event_cb_t on_change, void *user);
lv_obj_t *ui_row_action(lv_obj_t *list, const char *title, const char *hint,
                        const char *btn_text, bool danger, lv_event_cb_t on_click, void *user);

#ifdef __cplusplus
}
#endif
