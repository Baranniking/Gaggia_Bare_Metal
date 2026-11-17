#include "ntc_sensor.h"

#define T0 273.15
#define ADC_RESOLUTION 4075.0f // сменить имя константы на более подходящее, так как это не разрешение АЦП, а максимальное значение АЦП в данном случае

void init_ntc_sensor(ntc_sensor_t *sensor, float adc_value) {
        sensor->temp = NAN; 
        sensor->ntc_status = false; // Indicate that the NTC sensor is not functioning properly
        sensor->t0 = T0;
}

float get_temperature(ntc_sensor_t *sensor, float adc_value) {
    sensor->raw = adc_value;
    if(sensor->raw <= 0 || sensor->raw >= ADC_RESOLUTION) { // в будущем сделать проверку по температуре, не только по сырому значению АЦП, так как при обрыве цепи термистора, АЦП может выдавать значения в пределах диапазона, но температура будет некорректной
        sensor->temp = NAN; // Invalid reading, set to absolute zero
        sensor->ntc_status = false; // Indicate that the NTC sensor is not functioning properly
        return sensor->temp; // Return an error value
    }
    float r = (sensor->r_pull * sensor->raw) / (4095.0f - sensor->raw); // Calculate resistance of the NTC thermistor
    float lr = logf(r);
    sensor->temp = 1.0f /(sensor->sh_a + sensor->sh_b * lr + sensor->sh_c * lr * lr) - sensor->t0; // Convert to Celsius
    sensor->ntc_status = true; // сделать проверку на конечное число
    return sensor->temp;
}


