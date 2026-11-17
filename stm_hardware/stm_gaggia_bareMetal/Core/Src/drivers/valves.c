#include "valves.h"
#include "main.h"
#include "stm32f4xx_hal_gpio.h"
#include <stdint.h>

#define START_STATE 0

static valve_cfg_t valve_cfg[NUM_OF_VALVE] = {
    [GROUP_VALVE] = {brew_valve_GPIO_Port, brew_valve_Pin, GPIO_PIN_SET, GPIO_PIN_RESET, START_STATE},
    [FILL_STEAM_VALVE] = {fill_valve_GPIO_Port, fill_valve_Pin, GPIO_PIN_SET, GPIO_PIN_RESET, START_STATE},
    [HOT_WATER_VALVE] = {hot_water_GPIO_Port, hot_water_Pin, GPIO_PIN_SET, GPIO_PIN_RESET, START_STATE},
    [AIR_VENTING_VALVE] = {air_venting_valve_GPIO_Port, air_venting_valve_Pin, GPIO_PIN_SET, GPIO_PIN_RESET, START_STATE},
};

void valve_closed_all(void){
    
    for(int i = 0; i < NUM_OF_VALVE; i++){
        HAL_GPIO_WritePin(valve_cfg[i].GPIOx, valve_cfg[i].pin, valve_cfg[i].closed_level);
        valve_cfg[i].is_open = false;
    }
}

void valve_init(void){
    valve_closed_all();
}


HAL_StatusTypeDef valve_set_state(valve_id_t id, bool open){
        if(id >= NUM_OF_VALVE) return HAL_ERROR;
        GPIO_PinState level = open ? valve_cfg[id].open_level : valve_cfg[id].closed_level;
        HAL_GPIO_WritePin(valve_cfg[id].GPIOx, valve_cfg[id].pin, level);
        valve_cfg[id].is_open = open;
        return HAL_OK;
    }

bool valve_is_open(valve_id_t id){
    if(id >= NUM_OF_VALVE) return false;
    return valve_cfg[id].is_open;
}


