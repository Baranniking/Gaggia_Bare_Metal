#include "heater.h"
#include "cmd_handler.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_gpio.h"
#include <stdbool.h>

static heater_t heater[NUM_OF_HEATER] =
    {
        [PRE_HEATER] = {0, ssr_pre_heat_Pin, 0, ssr_pre_heat_GPIO_Port, 0},
        [STEAM_HEATER] = {0, ssr_steam_heat_Pin, 0, ssr_steam_heat_GPIO_Port, 0},
        [COFFEE_HEATER] = {0, ssr_coffee_heat_Pin, 0, ssr_coffee_heat_GPIO_Port, 0}};

HAL_StatusTypeDef heaters_init(void)
{
    HAL_StatusTypeDef init_heaters_state = HAL_OK;
    for (int i = 0; i < NUM_OF_HEATER; i++)
    {
        if (heater[i].port == NULL)
        {
            init_heaters_state = HAL_ERROR;
            heater[i].power = 0;
            heater[i].heater_alarm = true;
        }
        else
        {
            HAL_GPIO_WritePin(heater[i].port, heater[i].pin, GPIO_PIN_RESET);
            heater[i].power = 0;
            heater[i].heater_alarm = false;
            heater[i].last_set_time = 0;
        }
    }
    return init_heaters_state;
}

HAL_StatusTypeDef heater_set(id_heater_t id, uint16_t set)
{

    if (id >= NUM_OF_HEATER)
        return HAL_ERROR;
    if (heater[id].heater_alarm)
        return HAL_ERROR;
    if (set > 1000)
        set = 1000;
    heater[id].power = set;
    heater[id].last_set_time = HAL_GetTick();
    return HAL_OK;
}

static HAL_StatusTypeDef heater_update(id_heater_t id)
{

    if (id >= NUM_OF_HEATER)
    {
        return HAL_ERROR;
    }
    if (heater[id].port == NULL)
    {
        return HAL_ERROR;
    }
    if (heater[id].heater_alarm == true)
    {
        HAL_GPIO_WritePin(heater[id].port, heater[id].pin, GPIO_PIN_RESET);
        return HAL_ERROR;
    }

    uint32_t time_now = HAL_GetTick();
    if (heater[id].power > 0 && (time_now - heater[id].last_set_time) > 2000)
    {
        heater[id].power = 0;
        HAL_GPIO_WritePin(heater[id].port, heater[id].pin, GPIO_PIN_RESET);
        heater[id].heater_alarm = true;
        return HAL_ERROR;
    }
    uint8_t position = (time_now % 2000) / 10;
    uint8_t threshold = heater[id].power / 5;
    if (position < threshold)
    {
        HAL_GPIO_WritePin(heater[id].port, heater[id].pin, GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(heater[id].port, heater[id].pin, GPIO_PIN_RESET);
    }
    return HAL_OK;
}

HAL_StatusTypeDef heater_update_all(void)
{
    HAL_StatusTypeDef status = HAL_OK;
    for (id_heater_t i = 0; i < NUM_OF_HEATER; i++)
    {
        if (heater_update(i) != HAL_OK)
            status = HAL_ERROR;
    }
    return status;
}

HAL_StatusTypeDef heater_stop_all(void)
{
    HAL_StatusTypeDef status = HAL_OK;
    for (id_heater_t i = 0; i < NUM_OF_HEATER; i++)
    {
        if (heater[i].port != NULL)
        {
            heater[i].power = 0;
            heater[i].heater_alarm = true;
            HAL_GPIO_WritePin(heater[i].port, heater[i].pin, GPIO_PIN_RESET);
        }
        else
        {
            status = HAL_ERROR;
        }
    }
    return status;
}

HAL_StatusTypeDef heater_reset_fault(id_heater_t id)
{
    if (heater[id].port == NULL)
    {
        return HAL_ERROR;
    }
    if (id >= NUM_OF_HEATER)
    {
        return HAL_ERROR;
    }
    heater[id].power = 0;
    heater[id].last_set_time = HAL_GetTick();
    heater[id].heater_alarm = false;
    return HAL_OK;
}