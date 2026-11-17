#include "steam_pwm_valve.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_tim.h"
#include <stdint.h>

HAL_StatusTypeDef init_steam_pwm_valve(steam_pwm_valve_t *valve, TIM_HandleTypeDef *htim, uint32_t channel)
{
    if (valve == NULL || htim == NULL)
    {
        return HAL_ERROR;
    }
    HAL_StatusTypeDef status_pwm = HAL_ERROR;
    valve->htim = htim;
    valve->channel = channel;
    valve->open_limits_ms = 60000;
    valve->percent = 0;
    valve->state = OUT_VALVE_STEAM_FAULT;
    valve->time_open_start = 0;
    __HAL_TIM_SET_COMPARE(valve->htim, valve->channel, 0);
    if (HAL_TIM_PWM_Start(valve->htim, valve->channel) == HAL_OK)
    {
        valve->state = OUT_VALVE_STEAM_OFF;
        status_pwm = HAL_OK;
    }
    return status_pwm;
}

out_steam_valve_state_t steam_pwm_valve_stop(steam_pwm_valve_t *valve)
{
    if (valve == NULL || valve->htim == NULL)
    {
        return OUT_VALVE_STEAM_FAULT;
    }
    __HAL_TIM_SET_COMPARE(valve->htim, valve->channel, 0);
    valve->percent = 0;
    if (valve->state != OUT_VALVE_STEAM_FAULT)
    {
        valve->state = OUT_VALVE_STEAM_OFF;
    }
    return valve->state;
}

out_steam_valve_state_t steam_pwm_valve_set(steam_pwm_valve_t *valve, uint8_t percent)
{
    uint32_t pulse = 0;
    if (valve == NULL || valve->htim == NULL)
    {
        return OUT_VALVE_STEAM_FAULT;
    }
    if (percent >= 100)
    {
        percent = 100;
    }
    if (percent == 0)
    {
        return steam_pwm_valve_stop(valve);
    }
    if (valve->state == OUT_VALVE_STEAM_FAULT)
    {
        return OUT_VALVE_STEAM_FAULT;
    }
    pulse = percent * (valve->htim->Init.Period + 1) / 100;
    __HAL_TIM_SET_COMPARE(valve->htim, valve->channel, pulse);
    if (valve->state != OUT_VALVE_STEAM_ON)
    {
        valve->time_open_start = HAL_GetTick();
    }
    valve->percent = percent;
    valve->state = OUT_VALVE_STEAM_ON;
    return valve->state;
}

out_steam_valve_state_t steam_pwm_valve_update(steam_pwm_valve_t *valve)
{
    if (valve == NULL || valve->htim == NULL)
    {
        return OUT_VALVE_STEAM_FAULT;
    }
    if (valve->state == OUT_VALVE_STEAM_ON)
    {
        uint32_t current_time = HAL_GetTick();
        if (current_time - valve->time_open_start >= valve->open_limits_ms)
        {
            steam_pwm_valve_stop(valve);
            valve->state = OUT_VALVE_STEAM_FAULT;
        }
    }
    return valve->state;
}

out_steam_valve_state_t steam_pwm_valve_reset_fault(steam_pwm_valve_t *valve)
{
    if (valve == NULL || valve->htim == NULL)
    {
        return OUT_VALVE_STEAM_FAULT;
    }
    if (valve->state == OUT_VALVE_STEAM_FAULT)
    {
        __HAL_TIM_SET_COMPARE(valve->htim, valve->channel, 0);
        valve->percent = 0;
        valve->state = OUT_VALVE_STEAM_OFF;
    }
    return valve->state;
}

const char *pwm_valve_steam_state_text(out_steam_valve_state_t valve)
{
    switch (valve)
    {
    case OUT_VALVE_STEAM_FAULT:
        return "PWM STEAM VALVE FAULT";
    case OUT_VALVE_STEAM_ON:
        return "PWM STEAM VALVE RUN";
    case OUT_VALVE_STEAM_OFF:
        return "PWM STEAM VALVE STOP";
    default:
        return "?";
    }
}