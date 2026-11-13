/**
 * ui.h — public entry points of the Gaggia UI.
 */
#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SCR_HOME = 0,
    SCR_BREW,
    SCR_MANUAL,
    SCR_PROFILES,
    SCR_STEAM,
    SCR_WATER,
    SCR_SETTINGS,
    SCR_DEBUG,
    SCR_COUNT
} ui_screen_id_t;

void           ui_init(void);
void           ui_show(ui_screen_id_t id);
ui_screen_id_t ui_current(void);
void           ui_apply_brightness(void);   /* after settings change */

#ifdef __cplusplus
}
#endif
