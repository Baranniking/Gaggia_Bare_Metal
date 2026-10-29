#ifndef STEAM_PWM_VALVE_H
#define STEAM_PWM_VALVE_H

#include "main.h"
#include <stdint.h>
#include <sys/types.h>

typedef enum
{
OUT_VALVE_STEAM_FAULT,
OUT_VALVE_STEAM_ON,
OUT_VALVE_STEAM_OFF,
}out_steam_valve_state_t;

typedef struct
{
    out_steam_valve_state_t state;
    TIM_HandleTypeDef *htim;
    uint8_t percent;
    uint32_t channel;
    uint32_t open_limits_ms;
    uint32_t time_open_start;
} steam_pwm_valve_t;


HAL_StatusTypeDef init_steam_pwm_valve(steam_pwm_valve_t *valve, TIM_HandleTypeDef *htim, uint32_t channel);
out_steam_valve_state_t steam_pwm_valve_stop(steam_pwm_valve_t *valve);
out_steam_valve_state_t steam_pwm_valve_set(steam_pwm_valve_t *valve, uint8_t percent);
out_steam_valve_state_t steam_pwm_valve_update(steam_pwm_valve_t *valve);
out_steam_valve_state_t steam_pwm_valve_reset_fault(steam_pwm_valve_t *valve);
const char *pwm_valve_steam_state_text(out_steam_valve_state_t state);


#endif