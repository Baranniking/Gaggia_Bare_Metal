/**
 * ui_screens.h — internal: every screen module exposes the same trio.
 *   create() builds the widget tree once, at boot
 *   enter()  is called each time the screen becomes active
 *   update() is called every UI tick (~100 ms) while active
 */
#pragma once
#include "lvgl.h"
#include "gaggia_model.h"

#define SCREEN_DECL(name)                                  \
    lv_obj_t *scr_##name##_create(void);                   \
    void      scr_##name##_enter(void);                    \
    void      scr_##name##_update(const gaggia_telemetry_t *t);

SCREEN_DECL(home)
SCREEN_DECL(brew)
SCREEN_DECL(manual)
SCREEN_DECL(profiles)
SCREEN_DECL(steam)
SCREEN_DECL(water)
SCREEN_DECL(settings)
SCREEN_DECL(debug)

/* screen background with the content area reserved under the status bar */
lv_obj_t *ui_screen_base(void);
