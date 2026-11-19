#include "pid.h"
#include "stm32f4xx_hal_def.h"

HAL_StatusTypeDef init_pid(pid_ctrl_t *p, float pk, float ik, float dk, float dt, float max, float min)
{
    if (p == NULL || min > max)
        return HAL_ERROR;
    if (pk < 0 || ik < 0 || dk < 0 || dt <= 0)
        return HAL_ERROR;
    p->kp = pk;
    p->ki = ik;
    p->kd = dk;
    p->integral = 0;
    p->last_error = 0;
    p->max_out = max;
    p->min_out = min;
    p->set_point = 0;
    p->dt = dt;
    p->update_last_error = false;
    return HAL_OK;
}

float pid_update(pid_ctrl_t *p, float measured)
{
    if (p == NULL)
        return 0;
    float error = p->set_point - measured;
    if (!p->update_last_error)
    {
        p->last_error = error;
        p->update_last_error = true;
    }

    float p_part = p->kp * error;
    float i_part = 0;
    float d_part = 0;
    if (p->ki > 0)
        p->integral += error * p->dt;
    {
        if (p->integral > p->max_out / p->ki)
            p->integral = p->max_out / p->ki;
        if (p->integral < p->min_out / p->ki)
            p->integral = p->min_out / p->ki;
        i_part = p->ki * p->integral;
    }
    d_part = p->kd * (error - p->last_error) / p->dt;
    float out = p_part + i_part + d_part;
    if (out > p->max_out)
        out = p->max_out;
    if (out < p->min_out)
        out = p->min_out;
    p->last_error = error;
    return out;
}

HAL_StatusTypeDef pid_reset(pid_ctrl_t *p)
{
    if (p == NULL)
        return HAL_ERROR;
    p->integral = 0;
    p->last_error = 0;
    p->update_last_error = false;
    return HAL_OK;
}

HAL_StatusTypeDef pid_set_gains(pid_ctrl_t *p, float kp, float ki, float kd)
{
    if (p == NULL)
        return HAL_ERROR;
    if (kp < 0 || ki < 0 || kd < 0)
        return HAL_ERROR;
    p->integral = 0;
    p->kp = kp;
    p->ki = ki;
    p->kd = kd;
    return HAL_OK;
}