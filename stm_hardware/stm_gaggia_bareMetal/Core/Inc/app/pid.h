#ifndef PID_H
#define PID_H

#include "main.h"
#include "stdbool.h"

typedef struct
{
    float set_point;
    float kp;
    float ki;
    float kd;
    float last_error;
    float integral;
    float max_out;
    float min_out;
    float dt;
    bool update_last_error;
} pid_ctrl_t;

float pid_update(pid_ctrl_t *p, float measured);
HAL_StatusTypeDef pid_init(pid_ctrl_t *p, float pk, float ik, float dk, float dt, float max, float min);

#endif