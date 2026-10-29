#ifndef WATER_LEVEL_TANK_H
#define WATER_LEVEL_TANK_H

#include "main.h"
#include "adc_dma.h"
typedef enum
{
    UNKNOWN_STATE,
    ERROR_TANK,
    FULL_TANK,
    LOW_TANK,
    EMPTY_TANK,
} state_water_level_tank_probe_t;

typedef struct
{
    state_water_level_tank_probe_t state;
    uint16_t amplitude_min_level;
    uint16_t amplitude_max_level;
} water_level_tank_t;

HAL_StatusTypeDef init_water_level_tank(water_level_tank_t *tank_probe);
state_water_level_tank_probe_t water_tank_level_tank_update(water_level_tank_t *tank_probe, uint16_t min_level_amplitude, uint16_t max_level_amplitude);
const char *water_tank_probe_state_text(state_water_level_tank_probe_t state);


#endif