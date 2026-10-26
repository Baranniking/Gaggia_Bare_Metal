#include "adc_dma.h"
#include <stdint.h>
#include <math.h>

#define OVER_SAMPLE_COUNT 16

static volatile uint16_t adc_dma_buffer[OVER_SAMPLE_COUNT][numb_of_ranks];

HAL_StatusTypeDef adc_dma_init(ADC_HandleTypeDef *hadc){
    HAL_StatusTypeDef status = HAL_OK;
    status = HAL_ADC_Start_DMA(hadc, (uint32_t *) adc_dma_buffer, OVER_SAMPLE_COUNT * numb_of_ranks);
    return status;

}   

float adc_dma_get_raw(dma_rank_t rank){
    uint32_t sum = 0;
    if(rank >= numb_of_ranks){
        return -1.0f; // Invalid rank
    }
    for(int i = 0; i < OVER_SAMPLE_COUNT; i++){
        sum += adc_dma_buffer[i][rank];
    }
    return (float)sum / OVER_SAMPLE_COUNT;
}

uint16_t adc_dma_get_amplitude(dma_rank_t rank){
    if(rank >= numb_of_ranks) return 0;// переделать через хал проверку, и отдать через структуру 
    uint16_t checkNumber = 0; 
    uint16_t min_sideAmplitude = adc_dma_buffer[0][rank];
    uint16_t max_sideAmplitude = adc_dma_buffer[0][rank];
    for(int i = 1; i < OVER_SAMPLE_COUNT; i++){
        checkNumber = adc_dma_buffer[i][rank];
        if(min_sideAmplitude > checkNumber){
        min_sideAmplitude = checkNumber;
        }if(max_sideAmplitude < checkNumber){
        max_sideAmplitude = checkNumber;
        }
    }
    return max_sideAmplitude - min_sideAmplitude;

} 


