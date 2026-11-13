/**
 * knob_input.h — bridge between the smart knob (FOC task) and the UI/model.
 *
 *   FOC task  --knob_input_report(pct)-->  [latest value]  --gaggia_tick()--> model -> UI
 *   UI drag   --gaggia_cmd_steam_power()--> knob_port_set_target(pct) --> FOC moves the rotor
 *
 * knob_input_report() is safe to call from another task/core (single-byte
 * store + flag). The model consumes it in gaggia_tick(), on the UI task,
 * so LVGL is never touched from the FOC task.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- called by the knob side (FOC task) ---- */
void knob_input_report(uint8_t pct);      /* 0..100 % of valve travel */

/* ---- called by the model (UI task) ---- */
bool     knob_input_take(uint8_t *pct);   /* true once per new value */
uint32_t knob_input_last_ms(void);        /* when the knob last moved (0 = never) */
void     knob_input_mark(uint32_t now_ms);

/* ---- implemented by the knob driver; weak no-op by default ----
 * Move the rotor to pct so the knob physically follows a change made on
 * the touchscreen (FOC position mode / detent re-centre). */
void knob_port_set_target(uint8_t pct);

#ifdef __cplusplus
}
#endif
