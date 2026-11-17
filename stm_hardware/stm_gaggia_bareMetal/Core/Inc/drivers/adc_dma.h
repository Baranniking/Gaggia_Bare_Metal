#ifndef __ADC_DMA_H__
#define __ADC_DMA_H__

#include "main.h"

typedef enum 
{
        ntc_preheat,
        ntc_steam,
        ntc_coffee,
        water_level_steam,
        water_level_tank_min,
        water_level_tank_max,
        numb_of_ranks,
} dma_rank_t;


HAL_StatusTypeDef adc_dma_init(ADC_HandleTypeDef *hadc);
float adc_dma_get_raw(dma_rank_t rank);
uint16_t adc_dma_get_amplitude(dma_rank_t rank);



#endif /* __ADC_DMA_H__ */
