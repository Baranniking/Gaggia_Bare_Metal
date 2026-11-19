#include "boiler_ctrl.h"
#include "adc_dma.h"
#include "gaggia_proto.h"
#include "pid.h"
#include "stm32f4xx_hal_def.h"
#include <sys/types.h>
#define BOILER_ABS_MAX_TEMP 150

HAL_StatusTypeDef init_boiler(boiler_t *b, ntc_sensor_t *s, pid_ctrl_t *p, id_heater_t id, float max_temp, float min_temp)
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
    b->enable_boiler = false;
    return HAL_OK;
}

HAL_StatusTypeDef boiler_set_target(boiler_t *b, float target_temp)
{
    if (b == NULL || b->pid == NULL)
        return HAL_ERROR;
    if (b->temp_max < target_temp || b->temp_min > target_temp)
    {
        b->alarm = true;
        heater_stop_all();
        return HAL_ERROR;
    }
    if (b->alarm)
    {
        heater_stop_all();
        return HAL_ERROR;
    }
    b->pid->set_point = target_temp;
    return HAL_OK;
}

HAL_StatusTypeDef boiler_ctrl_update(boiler_t *b)
{
    if (b == NULL || b->sensor == NULL || b->pid == NULL)
        return HAL_ERROR;
    if (!b->enable_boiler)
    {
        heater_set(b->id, 0);
        return HAL_OK;
    }
    if (b->alarm)
    {
        heater_stop_all();
        return HAL_ERROR;
    }
    if (!b->sensor->ntc_status)
    {
        b->bad_count++;
        heater_set(b->id, 0);
        if (b->bad_count >= 3)
        {
            b->alarm = true;
            heater_stop_all();
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

HAL_StatusTypeDef disable_boiler(boiler_t *b)
{
    if (b == NULL)
        return HAL_ERROR;
    b->enable_boiler = false;
    return HAL_OK;
}

HAL_StatusTypeDef enable_boiler(boiler_t *b)
{
    if (b == NULL)
        return HAL_ERROR;
    if (!b->enable_boiler)
    {
        if (pid_reset(b->pid) != HAL_OK)
            return HAL_ERROR;
        b->enable_boiler = true;
    }
    return HAL_OK;
}

HAL_StatusTypeDef boiler_apply_param(const cmd_params_t *p, boiler_t *b)
{
    if (p == NULL || b == NULL)
        return HAL_ERROR;
    float kp = p->kp_x100 / 100.0f;
    float ki = p->ki_x1000 / 1000.0f;
    float kd = p->kd_x10 / 10.0f;

    switch (b->id)
    {
    case PRE_HEATER:
        if (p->preheat_on)
        {
            float temp_preheat = p->preheat_set_x10 / 10.0f;
            if (boiler_set_target(b, temp_preheat) != HAL_OK)
                return HAL_ERROR;
            enable_boiler(b);
        }
        else
        {
            disable_boiler(b);
        }
        break;
    case STEAM_HEATER:
    {
        float temp_steam = p->steam_set_x10 / 10.0f;
        if (boiler_set_target(b, temp_steam) != HAL_OK)
            return HAL_ERROR;
        enable_boiler(b);
    }
    break;
    case COFFEE_HEATER:
    {
        if (pid_set_gains(b->pid, kp, ki, kd) != HAL_OK)
            return HAL_ERROR;
        float temp_coffee = p->brew_set_x10 / 10.0f;
        if (boiler_set_target(b, temp_coffee) != HAL_OK)
            return HAL_ERROR;
        enable_boiler(b);
    }
    break;
    default:
        return HAL_ERROR;
    }
    return HAL_OK;
}