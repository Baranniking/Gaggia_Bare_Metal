#ifndef BOILER_CTRL_H
#define BOILER_CTRL_H
#include "heater.h"
#include "main.h"
#include "pid.h"
#include "ntc_sensor.h"

typedef struct
{
    ntc_sensor_t *sensor;
    pid_ctrl_t *pid;
    id_heater_t id;
    float temp_min;
    float temp_max;
    uint8_t bad_count;
    bool alarm;
} boiler_t;

HAL_StatusTypeDef boiler_init(boiler_t *b, ntc_sensor_t *s, pid_ctrl_t *p, id_heater_t id, float max_temp, float min_temp);
HAL_StatusTypeDef boiler_set_target(boiler_t *b, float target_temp);

#endif