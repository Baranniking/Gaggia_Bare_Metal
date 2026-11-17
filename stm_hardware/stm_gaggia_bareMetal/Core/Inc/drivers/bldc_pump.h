#ifndef BLDC_PUMP_H
#define BLDC_PUMP_H
#include "main.h"
#include <stdint.h>
#include "stdbool.h"

typedef enum
{
    PUMP_FAULT,
    PUMP_STOP,
    PUMP_RUN,
} state_pump_t;

typedef struct
{
    state_pump_t state;
    TIM_HandleTypeDef *htim;
    bool reset_active;
    uint8_t percent;
    uint16_t rpm;
    uint16_t tacho_pin;
    volatile uint32_t impulse_tacho;
    uint32_t time_reset_start;
    uint32_t last_meas_impulse_tacho;
    uint32_t time_last_meas;
    uint32_t time_last_pulse;
    uint32_t channel;
} bldc_pump_t;

HAL_StatusTypeDef pump_init(bldc_pump_t *pump, TIM_HandleTypeDef *htim, uint32_t channel_pin, uint16_t pin_tacho);
state_pump_t pump_set(bldc_pump_t *pump, uint8_t percent);
state_pump_t pump_stop(bldc_pump_t *pump);
state_pump_t update_pump_tacho(bldc_pump_t *pump);
state_pump_t reset_fault_pump(bldc_pump_t *pump);
    const char *pump_state_text(state_pump_t state);

#endif