#include "bldc_pump.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_tim.h"

#define PP 42
#define TACHO_RPM_FACTOR 1875
#define TACHO_NO_PULSE_MS 1000
#define TACHO_PERIOD_MS 100
#define TACHO_MIN_RPM 150
#define PUMP_RESET_HOLD_MS 1000

HAL_StatusTypeDef pump_init(bldc_pump_t *pump, TIM_HandleTypeDef *htim, uint32_t channel_pin, uint16_t pin_tacho)
{
    if (pump == NULL || htim == NULL)
        return HAL_ERROR;
    uint32_t current_time = HAL_GetTick();
    HAL_StatusTypeDef status_pwm = HAL_ERROR;
    pump->htim = htim;
    pump->channel = channel_pin;
    pump->tacho_pin = pin_tacho;
    pump->reset_active = false;
    pump->percent = 0;
    pump->rpm = 0;
    pump->impulse_tacho = 0;
    pump->last_meas_impulse_tacho = 0;
    pump->time_reset_start = 0;
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

state_pump_t pump_set(bldc_pump_t *pump, uint8_t percent)
{
    uint32_t pulse = 0;
    if (pump == NULL || pump->htim == NULL)
    {
        return PUMP_FAULT;
    }

    if (percent >= 100)
    {
        percent = 100;
    }

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

    if (pump->state == PUMP_FAULT)
    {
        return PUMP_FAULT;
    }

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

state_pump_t update_pump_tacho(bldc_pump_t *pump)
{
    if (pump == NULL || pump->htim == NULL)
    {
        return PUMP_FAULT;
    }
    uint32_t tacho_impulse = pump->impulse_tacho;
    uint32_t currentTime = HAL_GetTick();
    if (currentTime - pump->time_last_meas >= TACHO_PERIOD_MS)
    {
        uint32_t result_impulse = tacho_impulse - pump->last_meas_impulse_tacho;
        uint32_t result_meas_time = currentTime - pump->time_last_meas;
        pump->rpm = (result_impulse * TACHO_RPM_FACTOR) / result_meas_time;
        pump->last_meas_impulse_tacho = tacho_impulse;
        pump->time_last_meas = currentTime;
        if (result_impulse > 0 && pump->rpm >= TACHO_MIN_RPM)
        {
            pump->time_last_pulse = currentTime;
        }
    }
    if (pump->state == PUMP_RUN && currentTime - pump->time_last_pulse >= TACHO_NO_PULSE_MS)
    {
        pump_stop(pump);
        pump->state = PUMP_FAULT;
        pump->rpm = 0;
    }
    return pump->state;
}

state_pump_t reset_fault_pump(bldc_pump_t *pump)
{
    if (pump == NULL || pump->htim == NULL)
    {
        return PUMP_FAULT;
    }
    if (pump->state != PUMP_FAULT)
    {
        pump->reset_active = false;
    }
    if (pump->state == PUMP_FAULT)
    {
        __HAL_TIM_SET_COMPARE(pump->htim, pump->channel, 0);
        uint32_t currentTime = HAL_GetTick();
        if (!pump->reset_active)
        {
            pump->time_reset_start = currentTime;
            pump->reset_active = true;
        }
        if (currentTime - pump->time_reset_start >= PUMP_RESET_HOLD_MS)
        {
            pump->state = PUMP_STOP;
            pump->time_last_pulse = currentTime;
            pump->reset_active = false;
        }
    }
    return pump->state;
}

const char *pump_state_text(state_pump_t state)
{
    switch (state)
    {
    case PUMP_FAULT:
        return "PUMP FAULT";
    case PUMP_STOP:
        return "PUMP STOP";
    case PUMP_RUN:
        return "PUMP RUN";
    default:
        return "?";
    }
}
