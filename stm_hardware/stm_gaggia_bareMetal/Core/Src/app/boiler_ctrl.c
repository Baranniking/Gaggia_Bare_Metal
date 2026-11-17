#include "boiler_ctrl.h"
#include "adc_dma.h"
#include "stm32f4xx_hal_def.h"
#include <sys/types.h>
#define BOILER_ABS_MAX_TEMP 150

HAL_StatusTypeDef boiler_init(boiler_t *b, ntc_sensor_t *s, pid_ctrl_t *p, id_heater_t id, float max_temp, float min_temp)
{
    if (b == NULL || s == NULL || p == NULL || id >= NUM_OF_HEATER || max_temp <= min_temp || min_temp < 0 || max_temp > BOILER_ABS_MAX_TEMP)
        return HAL_ERROR;
    b->sensor = s;
    b->alarm = false;
    b->bad_count = 0;
    b->id = id;
    b->temp_max = max_temp;
    b->temp_min = min_temp;
    b->pid = p;
    return HAL_OK;
}

HAL_StatusTypeDef boiler_set_target(boiler_t *b, float target_temp)
{
    if (b == NULL || b->pid == NULL)
        return HAL_ERROR;
    if (b->temp_max < target_temp || b->temp_min > target_temp)
    {
        return HAL_ERROR;
    }
    if (b->alarm)
        return HAL_ERROR;
    b->pid->set_point = target_temp;
    return HAL_OK;
}

HAL_StatusTypeDef boiler_ctrl_update(boiler_t *b)
{
    if (b == NULL || b->sensor == NULL || b->pid == NULL)
        return HAL_ERROR;
    if (b->alarm)
    {
        heater_set(b->id, 0);
        return HAL_ERROR;
    }
    if (!b->sensor->ntc_status)
    {
        b->bad_count++;
        heater_set(b->id, 0);
        if (b->bad_count >= 3)
        {
            b->alarm = true;
            return HAL_ERROR;
        }
        return HAL_ERROR;
    }
    else
    {
        b->bad_count = 0;
    }
    float result = pid_update(b->pid, b->sensor->temp);
    if (result < 0)
        result = 0;
    else if (result > 1000)
        result = 1000;
    heater_set(b->id, (uint16_t)result);
    return HAL_OK;
}