#include "knob_input.h"

static volatile uint8_t s_val;
static volatile bool    s_new;
static uint32_t         s_last_ms;

void knob_input_report(uint8_t pct)
{
    if (pct > 100) pct = 100;
    s_val = pct;          /* byte store is atomic on Xtensa */
    s_new = true;
}

bool knob_input_take(uint8_t *pct)
{
    if (!s_new) return false;
    s_new = false;
    *pct = s_val;         /* a newer value can only set s_new again */
    return true;
}

uint32_t knob_input_last_ms(void)       { return s_last_ms; }
void     knob_input_mark(uint32_t now)  { s_last_ms = now ? now : 1; }

__attribute__((weak)) void knob_port_set_target(uint8_t pct) { (void)pct; }
