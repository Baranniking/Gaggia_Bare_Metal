#ifndef __VALVES_H__
#define __VALVES_H__
#include "adc_dma.h"
#include "main.h"
#include "stdbool.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_rcc.h"
#include "gpio.h"
#include <stdint.h>

typedef enum {
    GROUP_VALVE,
    FILL_STEAM_VALVE,
    HOT_WATER_VALVE,
    NUM_OF_VALVE
}valve_id_t;

typedef struct {
    GPIO_TypeDef *GPIOx; // GPIO port for the valve
    uint16_t pin;
    GPIO_PinState open_level;
    GPIO_PinState closed_level;
    bool is_open;
}valve_cfg_t;

void valve_init();
void valve_closed_all(void);
HAL_StatusTypeDef valve_set_state(valve_id_t id, bool open);
bool valve_is_open(valve_id_t id);



#endif