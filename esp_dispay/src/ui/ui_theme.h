/**
 * ui_theme.h — palette, fonts, shared styles.
 *
 * Palette is taken from the machine itself: roasted-bean dark surfaces,
 * crema as the single warm accent, and three "instrument" colours that
 * always mean the same thing everywhere: ember = temperature,
 * steel blue = pressure, sage = flow.
 */
#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- palette ---- */
#define C_BG          lv_color_hex(0x15100D)   /* roast            */
#define C_SURFACE     lv_color_hex(0x221914)   /* portafilter      */
#define C_SURFACE_HI  lv_color_hex(0x2E231C)   /* pressed / raised */
#define C_LINE        lv_color_hex(0x3A2D25)
#define C_TEXT        lv_color_hex(0xF3E9DF)
#define C_TEXT_DIM    lv_color_hex(0xA8968A)
#define C_TEXT_MUTED  lv_color_hex(0x6B5B50)

#define C_CREMA       lv_color_hex(0xD9A066)   /* accent           */
#define C_CREMA_DEEP  lv_color_hex(0xB27238)
#define C_ON_CREMA    lv_color_hex(0x2A1A0E)

#define C_TEMP        lv_color_hex(0xE8673A)   /* ember            */
#define C_PRESS       lv_color_hex(0x5AA9C9)   /* steel blue       */
#define C_FLOW        lv_color_hex(0x8BC48A)   /* sage             */
#define C_STEAM       lv_color_hex(0xB9D3E0)

#define C_OK          lv_color_hex(0x8BC48A)
#define C_WARN        lv_color_hex(0xE9B949)
#define C_ERR         lv_color_hex(0xE5534B)

/* ---- fonts (enable sizes in lv_conf.h) ---- */
#define F_XS     (&lv_font_montserrat_12)
#define F_S      (&lv_font_montserrat_14)
#define F_M      (&lv_font_montserrat_16)
#define F_L      (&lv_font_montserrat_20)
#define F_XL     (&lv_font_montserrat_28)
#define F_HUGE   (&lv_font_montserrat_40)
#define F_GIANT  (&lv_font_montserrat_48)

/* ---- geometry ---- */
#define UI_W          480
#define UI_H          320
#define UI_BAR_H      34      /* status bar         */
#define UI_NAV_H      58      /* bottom navigation  */
#define UI_PAD        10
#define UI_CONTENT_Y  (UI_BAR_H + 6)
#define UI_CONTENT_H  (UI_H - UI_BAR_H - UI_NAV_H - 12)
#define UI_R_BIG      18
#define UI_R_SMALL    12

/* ---- shared styles ---- */
extern lv_style_t st_screen;
extern lv_style_t st_card;
extern lv_style_t st_card_pressed;
extern lv_style_t st_btn;
extern lv_style_t st_btn_pressed;
extern lv_style_t st_btn_accent;
extern lv_style_t st_btn_danger;
extern lv_style_t st_btn_disabled;
extern lv_style_t st_chip;

void ui_theme_init(void);

#ifdef __cplusplus
}
#endif
