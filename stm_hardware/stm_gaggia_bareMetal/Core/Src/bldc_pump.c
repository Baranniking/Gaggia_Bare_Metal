#include "bldc_pump.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_def.h"

#define PP 42

HAL_StatusTypeDef init_bldc_pump(bldc_pump_t *pump, TIM_HandleTypeDef *htim, uint32_t channel)
{
    if (pump == NULL || htim == NULL)
        return HAL_ERROR;
    uint32_t current_time = HAL_GetTick();
    HAL_StatusTypeDef status_pwm = HAL_ERROR;
    pump->htim = htim;
    pump->channel = channel;
    pump->percent = 0;
    pump->rpm = 0;
    pump->impulse_tacho = 0;
    pump->last_meas_impulse_tacho = 0;
    pump->time_last_meas = current_time;
    pump->time_last_pulse = current_time;
    pump->state = PUMP_FAULT;
    __HAL_TIM_SET_COMPARE(pump->htim, pump->channel, 0);
    if (HAL_TIM_PWM_Start(pump->htim, pump->channel) == HAL_OK)
    {
        pump->state = PUMP_STOP;
        status_pwm = HAL_OK;
    }
    return status_pwm;
}

state_pump_t set_pump(bldc_pump_t *pump, uint8_t percent)
{
    uint32_t pulse = 0;
    if (pump == NULL || pump->htim == NULL)
    {
        return PUMP_FAULT;
    }
    /////////////////////
    if (percent >= 100)
    {
        percent = 100;
    }

    ///////////////////
    if (percent < 5)
    {
        pump->percent = 0;
        __HAL_TIM_SET_COMPARE(pump->htim, pump->channel, 0);
        if (pump->state != PUMP_FAULT)
        {
            pump->state = PUMP_STOP;
        }
        return pump->state;
    }
    /////////////////////
    if (pump->state == PUMP_FAULT)
    {
        return PUMP_FAULT;
    }
    ///////////////////////
    pulse = percent * (pump->htim->Init.Period + 1) / 100;
    __HAL_TIM_SET_COMPARE(pump->htim, pump->channel, pulse);
    if (pump->state != PUMP_RUN)
    {
        pump->time_last_pulse = HAL_GetTick();
    }
    pump->percent = percent;
    return pump->state = PUMP_RUN;
}

state_pump_t pump_stop(bldc_pump_t *pump)
{
    if (pump == NULL || pump->htim == NULL)
    {
        return PUMP_FAULT;
    }
    __HAL_TIM_SET_COMPARE(pump->htim, pump->channel, 0);
    pump->percent = 0;
    if (pump->state != PUMP_FAULT)
    {
        pump->state = PUMP_STOP;
    }
    return pump->state;
}