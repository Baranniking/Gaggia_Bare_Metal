#include "water_level_tank.h"
#define WATER_TANK_WET_THRESHOLD 1650
#define AMP_NO_SIGNAL 100

HAL_StatusTypeDef init_water_level_tank(water_level_tank_t *tank_probe)
{
    if (tank_probe == NULL)
        return HAL_ERROR;
    tank_probe->amplitude_max_level = 0;
    tank_probe->amplitude_min_level = 0;
    tank_probe->state = UNKNOWN_STATE;
    return HAL_OK;
}

static state_water_level_tank_probe_t water_tank_classify(uint16_t min_level_amplitude, uint16_t max_level_amplitude)
{
    if (min_level_amplitude < AMP_NO_SIGNAL || max_level_amplitude < AMP_NO_SIGNAL)
        return UNKNOWN_STATE;
    if (max_level_amplitude < WATER_TANK_WET_THRESHOLD && min_level_amplitude >= WATER_TANK_WET_THRESHOLD)
        return ERROR_TANK;
    if (max_level_amplitude < WATER_TANK_WET_THRESHOLD && min_level_amplitude < WATER_TANK_WET_THRESHOLD)
        return FULL_TANK;
    if (max_level_amplitude >= WATER_TANK_WET_THRESHOLD && min_level_amplitude < WATER_TANK_WET_THRESHOLD)
        return LOW_TANK;
    if (max_level_amplitude >= WATER_TANK_WET_THRESHOLD && min_level_amplitude >= WATER_TANK_WET_THRESHOLD)
        return EMPTY_TANK;
    return UNKNOWN_STATE;
}

state_water_level_tank_probe_t water_tank_level_tank_update(water_level_tank_t *tank_probe, uint16_t min_level_amplitude, uint16_t max_level_amplitude)
{
    if (tank_probe == NULL)
        return UNKNOWN_STATE;
    tank_probe->amplitude_min_level = min_level_amplitude;
    tank_probe->amplitude_max_level = max_level_amplitude;
    tank_probe->state = water_tank_classify(min_level_amplitude, max_level_amplitude);
    return tank_probe->state;
}

const char *water_tank_probe_state_text(state_water_level_tank_probe_t state)
{
    switch (state)
    {
    case UNKNOWN_STATE:
        return "UNKNOWN";
    case FULL_TANK:
        return "TANK FULL";
    case LOW_TANK:
        return "TANK LOW";
    case EMPTY_TANK:
        return "TANK EMTPY";
    case ERROR_TANK:
        return "ERROR TANK";
    default:
        return "?";
    }
}