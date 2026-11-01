/* Force-included (-include) before the firmware headers. Defines their include
 * guards so the real HAL-dependent headers become no-ops, and supplies the
 * few types system_snapshot.h needs. Core/ stays untouched. */
#ifndef HAL_STUBS_H
#define HAL_STUBS_H
#include <stdint.h>
#include <stddef.h>

#define __MAIN_H
#define BLDC_PUMP_H
#define __NTC_SENSOR_H__
#define STEAM_PWM_VALVE_H
#define __VALVES_H__
#define WATER_LEVEL_TANK_H
#define __WATER_LEVEL_STEAM_H__

typedef struct { int dummy; } bldc_pump_t;
typedef struct { int dummy; } ntc_sensor_t;
typedef struct { int dummy; } steam_pwm_valve_t;
typedef struct { int dummy; } water_level_tank_t;
typedef struct { int dummy; } water_probe_level_t;
#endif
