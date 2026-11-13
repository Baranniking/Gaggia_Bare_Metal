/**
 * ui_hal.h — the few things the UI needs from the board.
 * Implemented in main.cpp (or in the PC simulator).
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void     ui_hal_set_brightness(uint8_t pct);      /* 0..100 */
uint32_t ui_hal_free_heap(void);                  /* bytes  */
bool     ui_hal_get_time(int *hour, int *minute); /* false if clock not set */
void     ui_hal_set_time(int hour, int minute);   /* manual clock setting   */
void     ui_hal_beep(uint16_t ms);                /* no-op if no buzzer */

#ifdef __cplusplus
}
#endif
