#include "water_level_steam.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_tim.h"
#include "tim.h"

#define WATER_WET_THRESHOLD 1650 //650-1650
#define AMP_NO_SIGNAL 100


HAL_StatusTypeDef init_water_level(water_probe_level_t *probe, TIM_HandleTypeDef *htim, uint32_t channel){
    if(htim == NULL || probe == NULL) return HAL_ERROR;
    probe->htim = htim;
    probe->channel = channel;
    probe->state = PROBE_UNKNOWN;
    probe->amplitude = 0;
    return HAL_TIM_PWM_Start(probe->htim, probe->channel);
}

static State_water_level_t water_probe_classif(uint16_t ampl){
 if(ampl < AMP_NO_SIGNAL) return PROBE_UNKNOWN;
 if(ampl < WATER_WET_THRESHOLD) return PROBE_WET;
 return PROBE_DRY;
}

State_water_level_t water_probe_update(water_probe_level_t *probe, uint16_t ampl){
    if(probe == NULL) return PROBE_UNKNOWN;
    probe->amplitude = ampl;
    probe->state = water_probe_classif(probe->amplitude);
    return probe->state;
}

const char *water_probe_state_text(State_water_level_t state)
{
    switch (state) {
        case PROBE_UNKNOWN: return "UNKNOWN";
        case PROBE_DRY:     return "DRY";
        case PROBE_WET:     return "WET";
        default:            return "?";
    }
}

