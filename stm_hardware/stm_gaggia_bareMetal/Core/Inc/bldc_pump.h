#ifndef BLDC_PUMP_H
#define BLDC_PUMP_H
#include "main.h"
#include <stdint.h>

typedef enum {
    PUMP_FAULT,
    PUMP_STOP,
    PUMP_RUN,
}state_pump_t;

typedef struct 
{
    state_pump_t state;
    TIM_HandleTypeDef *htim;
    uint8_t percent;
    uint16_t rpm;
    volatile uint32_t impulse_tacho;
    uint32_t last_meas_impulse_tacho;
    uint32_t time_last_meas;
    uint32_t time_last_pulse;
    uint32_t channel; 
}bldc_pump_t;

HAL_StatusTypeDef init_bldc_pump(bldc_pump_t *pump, TIM_HandleTypeDef *htim, uint32_t channel);
state_pump_t set_pump(bldc_pump_t *pump, uint8_t percent);
state_pump_t pump_stop(bldc_pump_t *pump);
#endif