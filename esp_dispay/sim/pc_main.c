/**
 * Headless PC harness: runs the real UI + model simulator on virtual time,
 * injects touches and dumps raw RGB565 frames for screenshots.
 *
 *   pio run -e native_sim -t exec        (frames -> ./sim_out)
 *   python3 sim/raw2png.py sim_out       (.raw -> .png)
 *   or without PlatformIO: make -C sim && ./sim/gaggia_sim sim_out
 */
#include "lvgl.h"
#include "ui.h"
#include "ui_hal.h"
#include "gaggia_model.h"
#include "sim_tick.h"
#include "knob_input.h"
#include "gaggia_log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#define W 480
#define H 320

static uint32_t   s_ms;
static lv_color_t s_fb[W * H];
static lv_color_t s_buf[W * 40];
static int        s_tx, s_ty, s_pressed;
static const char *s_out = "sim_out";
static uint8_t    s_bright = 100;
static FILE      *s_serial;
static void serial_sink(const char *line) { if (s_serial) fprintf(s_serial, "%s\n", line); }

uint32_t sim_millis(void) { return s_ms; }

void ui_hal_set_brightness(uint8_t pct) { s_bright = pct; }
uint32_t ui_hal_free_heap(void) { return 214 * 1024; }
static int32_t s_clock_off_min = 8 * 60 + 30;
bool ui_hal_get_time(int *h, int *m) { uint32_t t = (uint32_t)(s_clock_off_min + (int32_t)(s_ms / 60000)) % 1440; *h = t / 60; *m = t % 60; return true; }
void ui_hal_set_time(int h, int m) { s_clock_off_min = h * 60 + m - (int32_t)(s_ms / 60000); }
void ui_hal_beep(uint16_t ms) { (void)ms; }

static void flush(lv_disp_drv_t *d, const lv_area_t *a, lv_color_t *px)
{
    for (int y = a->y1; y <= a->y2; y++)
        for (int x = a->x1; x <= a->x2; x++)
            s_fb[y * W + x] = *px++;
    lv_disp_flush_ready(d);
}

static void touch(lv_indev_drv_t *d, lv_indev_data_t *data)
{
    (void)d;
    data->point.x = s_tx;
    data->point.y = s_ty;
    data->state = s_pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

static void run(uint32_t ms)
{
    uint32_t end = s_ms + ms;
    while (s_ms < end) {
        s_ms += 5;
        gaggia_tick(s_ms);
        lv_timer_handler();
    }
}

static void tap(int x, int y)
{
    s_tx = x; s_ty = y; s_pressed = 1; run(120);
    s_pressed = 0; run(300);
}

static void shot(const char *name)
{
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.raw", s_out, name);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    fwrite(s_fb, sizeof(s_fb), 1, f);
    fclose(f);
    printf("shot %s  (t=%.1fs, bright=%u, status=%s)\n", name, s_ms / 1000.0,
           s_bright, gaggia_status_str(gaggia_tm()->status));
}

int main(int argc, char **argv)
{
    if (argc > 1) s_out = argv[1];
    mkdir(s_out, 0755);   /* fine if it already exists */

    lv_init();
    static lv_disp_draw_buf_t db;
    lv_disp_draw_buf_init(&db, s_buf, NULL, W * 40);
    static lv_disp_drv_t dd;
    lv_disp_drv_init(&dd);
    dd.hor_res = W; dd.ver_res = H; dd.flush_cb = flush; dd.draw_buf = &db;
    lv_disp_drv_register(&dd);
    static lv_indev_drv_t id;
    lv_indev_drv_init(&id);
    id.type = LV_INDEV_TYPE_POINTER; id.read_cb = touch;
    lv_indev_drv_register(&id);

    char sp[256];
    snprintf(sp, sizeof(sp), "%s/serial.log", s_out);
    s_serial = fopen(sp, "w");
    glog_set_sink(serial_sink);
    gaggia_init(NULL);
    ui_init();

    run(6000);   shot("01_heating");
    run(40000);  shot("02_home_ready");

    tap(350, 225);                 /* Brew */
    run(9000);   shot("03_brew_preinf");
    run(14000);  shot("04_brew_extraction");
    run(30000);  shot("05_brew_done");
    tap(410, 290);                 /* Done */
    run(400);

    tap(434, 223);                 /* Manual */
    run(400);    shot("05b_manual_idle");
    tap(344, 294);                 /* Start pump */
    run(9000);
    for (int p = 75; p >= 50; p -= 5) { knob_input_report((uint8_t)p); run(80); }  /* knob -> 6 bar */
    run(600);    shot("05c_manual_running");
    tap(344, 294);                 /* Stop */
    run(500);
    tap(36, 294);                  /* back */
    run(400);

    tap(144, 290);                 /* nav: Profiles */
    run(400);    shot("06_profiles");
    tap(80, 150);                  /* Blooming */
    run(300);    shot("07_profiles_blooming");
    tap(420, 210);                 /* a param tile */
    run(400);    shot("08_number_editor");
    tap(160, 245);                 /* Cancel */
    run(300);

    tap(48, 290);                  /* Home: knob should pull us to Steam */
    run(400);
    for (int p = 0; p <= 35; p += 5) { knob_input_report((uint8_t)p); run(60); }
    run(600);    shot("09_steam_knob_turning");
    for (int p = 35; p <= 80; p += 5) { knob_input_report((uint8_t)p); run(60); }
    run(4000);   shot("09b_steam_knob_settled");
    for (int p = 80; p >= 0; p -= 10) { knob_input_report((uint8_t)p); run(50); }
    run(800);    shot("09c_steam_knob_closed");

    tap(336, 290);                 /* nav: Water */
    run(400);
    tap(350, 225);                 /* Dispense */
    run(12000);  shot("10_water");
    run(30000);

    tap(432, 290);                 /* nav: Settings */
    run(400);    shot("11_settings_boiler");
    s_tx = 330; s_ty = 220; s_pressed = 1; run(60);          /* drag page up */
    for (int i = 0; i < 12; i++) { s_ty -= 8; run(20); }
    s_pressed = 0; run(600);
    shot("11b_settings_preheat");

    tap(70, 132);                  /* Machine */
    run(300);    shot("11d_settings_machine");
    tap(400, 72);                  /* Clock value */
    run(400);
    for (int i = 0; i < 3; i++) tap(310, 96);    /* minutes up */
    shot("11e_clock_editor");
    tap(315, 252);                 /* Set */
    run(300);
    tap(70, 90);                   /* Display */
    run(300);    shot("12_settings_display");
    tap(70, 210);                  /* About */
    run(300);    shot("13_settings_about");
    tap(70, 170);                  /* Service */
    run(300);    shot("14_settings_service");
    tap(400, 82);                  /* Backflush Start */
    run(300);    shot("15_confirm_backflush");
    tap(316, 220);                 /* Start */
    run(6000);   shot("16_backflush_banner");
    tap(430, 50);                  /* Stop in banner */
    run(500);

    tap(70, 170);                  /* Service again (still there) */
    tap(430, 190);                 /* sim tank empty */
    run(300);
    tap(48, 290);                  /* Home */
    run(600);    shot("17_home_no_water");
    tap(350, 225);
    run(200);    shot("18_toast");

    gaggia_sim_set_water(true);
    run(500);
    s_tx = 50; s_ty = 17; s_pressed = 1; run(700);    /* long-press status text */
    s_pressed = 0; run(400);
    tap(304, 55);                  /* CSV off -> 1 Hz */
    tap(304, 55);                  /* -> 5 Hz */
    run(1500);   shot("19_debug_idle");
    tap(36, 55);                   /* back */
    tap(48, 290);                  /* Home */
    tap(350, 225);                 /* Brew */
    run(15000);
    s_tx = 50; s_ty = 17; s_pressed = 1; run(700);
    s_pressed = 0; run(1000);
    shot("20_debug_brewing");

    if (s_serial) fclose(s_serial);
    return 0;
}
