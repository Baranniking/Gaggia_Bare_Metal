#ifndef __WATER_LEVEL_STEAM_H__
#define __WATER_LEVEL_STEAM_H__

#include "main.h"
#include "adc_dma.h"
#include "stdbool.h"

typedef enum {
    PROBE_UNKNOWN,
    PROBE_DRY,
    PROBE_WET,
    NUM_OF_STATE_FILL,
} State_water_level_t;

typedef struct
{
    TIM_HandleTypeDef *htim;
    State_water_level_t state;
    uint32_t channel;
    uint16_t amplitude;
}water_probe_level_t;


HAL_StatusTypeDef init_water_level(water_probe_level_t *probe, TIM_HandleTypeDef *htim, uint32_t channel);
State_water_level_t water_probe_update(water_probe_level_t *probe, uint16_t ampl);
const char *water_probe_state_text(State_water_level_t state);

#endif