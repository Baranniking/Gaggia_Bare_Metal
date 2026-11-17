#ifndef __NTC_SENSOR_H__
#define __NTC_SENSOR_H__

#include "main.h"
#include "stdint.h"
#include "stdbool.h"
#include "math.h"

typedef struct 
{
    float temp; 
    uint16_t raw;
    float sh_a; // Steinhart-Hart coefficient A
    float sh_b; // Steinhart-Hart coefficient B 
    float sh_c; // Steinhart-Hart coefficient C
    float r_pull; // Pull-up resistor value in ohms
    float t0; // Reference temperature in Kelvin
    bool ntc_status; // true if the NTC sensor is connected and functioning properly, false otherwise

} ntc_sensor_t;

void init_ntc_sensor(ntc_sensor_t *sensor, float adc_value);
float get_temperature(ntc_sensor_t *sensor, float adc_value);


#endif /* __NTC_SENSOR_H__*/
