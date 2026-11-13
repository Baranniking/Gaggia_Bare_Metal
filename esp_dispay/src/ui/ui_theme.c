#include "ui_theme.h"

lv_style_t st_screen;
lv_style_t st_card;
lv_style_t st_card_pressed;
lv_style_t st_btn;
lv_style_t st_btn_pressed;
lv_style_t st_btn_accent;
lv_style_t st_btn_danger;
lv_style_t st_btn_disabled;
lv_style_t st_chip;

static const lv_style_prop_t k_trans_props[] = {
    LV_STYLE_BG_COLOR, LV_STYLE_BORDER_COLOR, LV_STYLE_TRANSLATE_Y, 0
};
static lv_style_transition_dsc_t s_trans;

void ui_theme_init(void)
{
    /* Dark default theme as a base, then override what we care about. */
    lv_disp_t *d = lv_disp_get_default();
    lv_theme_t *th = lv_theme_default_init(d, C_CREMA, C_PRESS, true, F_S);
    lv_disp_set_theme(d, th);

    lv_style_transition_dsc_init(&s_trans, k_trans_props, lv_anim_path_ease_out, 120, 0, NULL);

    lv_style_init(&st_screen);
    lv_style_set_bg_color(&st_screen, C_BG);
    lv_style_set_bg_opa(&st_screen, LV_OPA_COVER);
    lv_style_set_text_color(&st_screen, C_TEXT);
    lv_style_set_text_font(&st_screen, F_S);
    lv_style_set_pad_all(&st_screen, 0);

    lv_style_init(&st_card);
    lv_style_set_bg_color(&st_card, C_SURFACE);
    lv_style_set_bg_opa(&st_card, LV_OPA_COVER);
    lv_style_set_radius(&st_card, UI_R_BIG);
    lv_style_set_border_width(&st_card, 1);
    lv_style_set_border_color(&st_card, C_LINE);
    lv_style_set_border_opa(&st_card, LV_OPA_60);
    lv_style_set_pad_all(&st_card, 12);
    lv_style_set_shadow_width(&st_card, 0);
    lv_style_set_text_color(&st_card, C_TEXT);
    lv_style_set_transition(&st_card, &s_trans);

    lv_style_init(&st_card_pressed);
    lv_style_set_bg_color(&st_card_pressed, C_SURFACE_HI);
    lv_style_set_border_color(&st_card_pressed, C_CREMA_DEEP);
    lv_style_set_translate_y(&st_card_pressed, 1);

    lv_style_init(&st_btn);
    lv_style_set_bg_color(&st_btn, C_SURFACE_HI);
    lv_style_set_bg_opa(&st_btn, LV_OPA_COVER);
    lv_style_set_radius(&st_btn, UI_R_SMALL);
    lv_style_set_border_width(&st_btn, 0);
    lv_style_set_shadow_width(&st_btn, 0);
    lv_style_set_text_color(&st_btn, C_TEXT);
    lv_style_set_text_font(&st_btn, F_M);
    lv_style_set_pad_all(&st_btn, 8);
    lv_style_set_transition(&st_btn, &s_trans);

    lv_style_init(&st_btn_pressed);
    lv_style_set_bg_color(&st_btn_pressed, C_LINE);
    lv_style_set_translate_y(&st_btn_pressed, 1);

    lv_style_init(&st_btn_accent);
    lv_style_set_bg_color(&st_btn_accent, C_CREMA);
    lv_style_set_bg_grad_color(&st_btn_accent, C_CREMA_DEEP);
    lv_style_set_bg_grad_dir(&st_btn_accent, LV_GRAD_DIR_VER);
    lv_style_set_text_color(&st_btn_accent, C_ON_CREMA);

    lv_style_init(&st_btn_danger);
    lv_style_set_bg_color(&st_btn_danger, C_ERR);
    lv_style_set_bg_grad_color(&st_btn_danger, lv_color_hex(0xB23A33));
    lv_style_set_bg_grad_dir(&st_btn_danger, LV_GRAD_DIR_VER);
    lv_style_set_text_color(&st_btn_danger, lv_color_white());

    lv_style_init(&st_btn_disabled);
    lv_style_set_bg_color(&st_btn_disabled, C_SURFACE_HI);
    lv_style_set_bg_grad_dir(&st_btn_disabled, LV_GRAD_DIR_NONE);
    lv_style_set_text_color(&st_btn_disabled, C_TEXT_MUTED);

    lv_style_init(&st_chip);
    lv_style_set_bg_color(&st_chip, C_SURFACE_HI);
    lv_style_set_bg_opa(&st_chip, LV_OPA_COVER);
    lv_style_set_radius(&st_chip, LV_RADIUS_CIRCLE);
    lv_style_set_pad_hor(&st_chip, 10);
    lv_style_set_pad_ver(&st_chip, 4);
    lv_style_set_text_font(&st_chip, F_XS);
    lv_style_set_border_width(&st_chip, 0);
}
