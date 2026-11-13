//**************************************************************************************************
//  Gaggia Bare Metal — UI firmware for WT32-SC01 Plus (ESP32-S3, ST7796 8-bit parallel, FT6336)
//
//  Layers:
//    main.cpp     board: LovyanGFX panel/touch, LVGL port, NVS, ui_hal_* implementation
//    src/app/     gaggia_model: machine state, settings, profiles, commands (+ simulator)
//    src/ui/      LVGL screens; talk only to gaggia_model and ui_hal
//
//  Display / touch config is the one from the productbakery template, unchanged.
//**************************************************************************************************

#define LGFX_USE_V1
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7796  _panel_instance;
    lgfx::Bus_Parallel8 _bus_instance;
    lgfx::Light_PWM     _light_instance;
    lgfx::Touch_FT5x06  _touch_instance;   // FT6336U

public:
    LGFX(void)
    {
        {
            auto cfg = _bus_instance.config();
            cfg.freq_write = 20000000;
            cfg.pin_wr = 47;
            cfg.pin_rd = -1;
            cfg.pin_rs = 0;
            cfg.pin_d0 = 9;
            cfg.pin_d1 = 46;
            cfg.pin_d2 = 3;
            cfg.pin_d3 = 8;
            cfg.pin_d4 = 18;
            cfg.pin_d5 = 17;
            cfg.pin_d6 = 16;
            cfg.pin_d7 = 15;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = -1;
            cfg.pin_rst = 4;
            cfg.pin_busy = -1;
            cfg.memory_width = 320;
            cfg.memory_height = 480;
            cfg.panel_width = 320;
            cfg.panel_height = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;
            cfg.readable = true;
            cfg.invert = true;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = true;
            _panel_instance.config(cfg);
        }
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = 45;
            cfg.invert = false;
            cfg.freq = 44100;
            cfg.pwm_channel = 0;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }
        {
            auto cfg = _touch_instance.config();
            cfg.x_min = 0;
            cfg.x_max = 319;
            cfg.y_min = 0;
            cfg.y_max = 479;
            cfg.pin_int = 7;
            cfg.bus_shared = false;
            cfg.offset_rotation = 0;
            cfg.i2c_port = 0;
            cfg.i2c_addr = 0x38;
            cfg.pin_sda = 6;
            cfg.pin_scl = 5;
            cfg.freq = 400000;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }
        setPanel(&_panel_instance);
    }
};

#include <lvgl.h>
#include "ui.h"
#include "ui_hal.h"
#include "gaggia_model.h"
#include "knob_input.h"
#include "gaggia_log.h"
#if !GAGGIA_SIM
#include "gaggia_link.h"
#endif

static LGFX tft;

#define SCREEN_W   480
#define SCREEN_H   320
#define BUF_LINES  40      // 2 x 480*40*2 B = 75 KB of internal DMA-capable RAM

static lv_disp_draw_buf_t s_draw_buf;
static Preferences        s_prefs;
static uint32_t           s_dirty_since;

//------------------------------------------------------------------ LVGL port

static void disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.writePixels((lgfx::rgb565_t *)&color_p->full, w * h);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

static void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    uint16_t x, y;
    if (tft.getTouch(&x, &y)) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

static void lvgl_port_init(void)
{
    lv_init();

    const size_t px = SCREEN_W * BUF_LINES;
    auto *b1 = (lv_color_t *)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    auto *b2 = (lv_color_t *)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    lv_disp_draw_buf_init(&s_draw_buf, b1, b2, px);   // b2 == NULL -> single buffer, still works

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = SCREEN_W;
    disp_drv.ver_res  = SCREEN_H;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register(&indev_drv);
}

//------------------------------------------------------------------ persistence (NVS)

static void store_load_and_init(void)
{
    static gaggia_store_t saved;
    s_prefs.begin("gaggia", false);
    size_t n = s_prefs.getBytes("store", &saved, sizeof(saved));
    gaggia_init(n == sizeof(saved) ? &saved : nullptr);   // model validates magic/version
}

static void store_save_if_needed(void)
{
    if (!gaggia_store_dirty()) { s_dirty_since = 0; return; }
    uint32_t now = millis();
    if (!s_dirty_since) { s_dirty_since = now; return; }
    if (now - s_dirty_since < 3000) return;               // debounce flash writes
    s_prefs.putBytes("store", gaggia_store(), sizeof(gaggia_store_t));
    gaggia_store_clear_dirty();
    s_dirty_since = 0;
}

//------------------------------------------------------------------ ui_hal

extern "C" void ui_hal_set_brightness(uint8_t pct)
{
    if (pct > 100) pct = 100;
    tft.setBrightness((uint8_t)(pct * 255 / 100));
}

extern "C" uint32_t ui_hal_free_heap(void)
{
    return heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
}

extern "C" bool ui_hal_get_time(int *hour, int *minute)
{
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    if (t.tm_year + 1900 >= 2024) { *hour = t.tm_hour; *minute = t.tm_min; return true; }
#if GAGGIA_SIM
    uint32_t m = 8 * 60 + 30 + millis() / 60000;          // fake clock for the demo
    *hour = (m / 60) % 24; *minute = m % 60;
    return true;
#else
    return false;
#endif
}

extern "C" void ui_hal_set_time(int hour, int minute)
{
    // No RTC battery on the WT32: keep today's date if we have one, else a fixed one.
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    if (t.tm_year + 1900 < 2024) { memset(&t, 0, sizeof(t)); t.tm_year = 2026 - 1900; t.tm_mday = 1; }
    t.tm_hour = hour; t.tm_min = minute; t.tm_sec = 0;
    struct timeval tv = { mktime(&t), 0 };
    settimeofday(&tv, nullptr);
#if !GAGGIA_SIM
    gaggia_link_send_rtc((uint32_t)tv.tv_sec);   // STM32 RTC keeps it on VBAT
#endif
}

extern "C" void ui_hal_beep(uint16_t ms) { (void)ms; }   // no buzzer wired yet

//------------------------------------------------------------------ smart knob (FOC)
//
//  Plug the existing FOC project in here. From its task (any core):
//
//      float deg = foc_get_angle_deg();                 // your encoder angle
//      knob_input_report(angle_to_pct(deg));            // 0..100 %, thread-safe
//
//  and implement the reverse direction so the rotor follows a drag on the
//  touchscreen (overrides the weak no-op in knob_input.c):
//
//      extern "C" void knob_port_set_target(uint8_t pct) { foc_set_target(pct_to_angle(pct)); }
//
//  Keep the FOC loop on core 0 and LVGL on core 1 (Arduino loop()).

//------------------------------------------------------------------ STM32 link (UART1)
//
//  WT32-SC01 Plus extension header exposes IO10..IO14 and IO21.
//  CHECK against your board/header before wiring. 3.3 V logic both sides,
//  cross TX/RX, common GND.
#ifndef GAGGIA_UART_RX
#define GAGGIA_UART_RX 10      // ESP RX  <- STM32 TX
#endif
#ifndef GAGGIA_UART_TX
#define GAGGIA_UART_TX 11      // ESP TX  -> STM32 RX
#endif

#if !GAGGIA_SIM
static size_t link_tx(const uint8_t *data, size_t len)
{
    return Serial1.write(data, len);
}

static void link_init(void)
{
    Serial1.setRxBufferSize(1024);
    Serial1.begin(PROTO_BAUD, SERIAL_8N1, GAGGIA_UART_RX, GAGGIA_UART_TX);
    gaggia_link_init(link_tx);
}

static void link_poll(void)
{
    uint8_t buf[128];
    int n;
    while ((n = Serial1.available()) > 0) {
        if (n > (int)sizeof(buf)) n = sizeof(buf);
        n = Serial1.readBytes(buf, n);
        gaggia_link_rx(buf, (size_t)n);
    }
}
#endif

//------------------------------------------------------------------ debug log -> USB serial

static void log_sink(const char *line)
{
    Serial.println(line);   // "# I 12.345 msg" events, CSV rows without '#'
}

//------------------------------------------------------------------ setup / loop

void setup()
{
    Serial.begin(115200);

    tft.begin();
    tft.setRotation(1);
    tft.setBrightness(0);          // ui_init() applies the stored brightness

    glog_set_sink(log_sink);       // before init so the boot line is printed
    lvgl_port_init();
    store_load_and_init();
#if !GAGGIA_SIM
    link_init();
#endif
    ui_init();
}

void loop()
{
#if !GAGGIA_SIM
    link_poll();
#endif
    gaggia_tick(millis());
    lv_timer_handler();
    store_save_if_needed();
    delay(5);
}
