#ifndef HEATER_H
#define HEATER_H
#include "main.h"
#include "stm32f4xx_hal_def.h"
#include <stdint.h>
#include "stdbool.h"

typedef enum
{
    PRE_HEATER,
    STEAM_HEATER,
    COFFEE_HEATER,
    NUM_OF_HEATER
} id_heater_t;

typedef struct
{
    uint32_t last_set_time;
    uint16_t pin;
    uint16_t power;
    GPIO_TypeDef *port;
    bool heater_alarm;
} heater_t;

HAL_StatusTypeDef heaters_init();
HAL_StatusTypeDef heater_set(id_heater_t id, uint16_t set);
HAL_StatusTypeDef heater_reset_fault(id_heater_t id);
HAL_StatusTypeDef heater_stop_all(void);
HAL_StatusTypeDef heater_update_all(void);

#endif