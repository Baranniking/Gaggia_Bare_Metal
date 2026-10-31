#ifndef SYSTEM_SNAPSHOT_H
#define SYSTEM_SNAPSHOT_H
#include "main.h"
#include "bldc_pump.h"
#include "ntc_sensor.h"
#include "steam_pwm_valve.h"
#include "valves.h"
#include "water_level_tank.h"
#include "water_level_steam.h"
#include <stdint.h>


#define FAULT_PUMP         (1u << 0)
#define FAULT_STEAM_VALVE  (1u << 1)
#define FAULT_NTC_PREHEAT  (1u << 2)
#define FAULT_NTC_STEAM    (1u << 3)
#define FAULT_NTC_BREW     (1u << 4)
#define FAULT_STEAM_PROBE  (1u << 5)  
#define FAULT_TANK_SENSOR  (1u << 6)  
#define FAULT_TANK_EMPTY   (1u << 7)  

typedef struct
{
    float temp_brew_boiler;    // celsius;
    float temp_steam_boiler;   // celsius;
    float temp_preheat_boiler; // celsius;
    uint32_t up_time_ms;       // time;
    uint32_t flowmeter;        // impulse;
    uint32_t seq; // sample;
    uint16_t faults;
    uint16_t rpm_pump; // rpm;
    uint16_t pressure_pump; // bar * 100;
    uint16_t amplitude_steam_probe; // adc_value;
    uint16_t amplitude_min_level; // adc_value;
    uint16_t amplitude_max_level;// adc_value;
    uint8_t machine_mode; // machine mode;
    uint8_t state_ntc_boiler; // // bit 0 - preheat, bit 1 - steam, bit 2 - brew; 1 = OK, 0 = FAULT
    uint8_t state_ssr_boiler; // // bit 0 - preheat, bit 1 - steam, bit 2 - brew; 1 = ON, 0 = OFF
    uint8_t state_pump; // 0x0 : pump fault, 0x1 : pump stop, 0x2 : pump run;
    uint8_t percent_pump; // percent pump
    uint8_t state_pwm_valve;  // 0x0 : pwm_valve fault, 0x1 : pwm_valve stop, 0x2 : pwm_valve run;
    uint8_t percent_pwm_valve; // percent valve
    uint8_t valves; // (1u << 0) : GROUP_VALVE, (1u << 1) : FILL_STEAM_VALVE, (1u << 2) : HOT_WATER_VALVE, (1u << 3) : AIR_VENTING_VALVE;
    uint8_t state_steam_level_probe; // 0x0 : PROBE_UNKNOWN , 0x1 : PROBE_DRY, 0x2 : PROBE_WET;
    uint8_t state_tank_level_probe; // 0x0 :UNKNOWN_STATE , 0x1 : ERROR_TANK, 0x2 : FULL_TANK, 0x3 : LOW_TANK, 0x4: EMPTY_TANK;
} system_snapshot_t;

void snapshot_update(system_snapshot_t *snapshot, const ntc_sensor_t *ntc_brew,
                     const ntc_sensor_t *ntc_steam,
                     const ntc_sensor_t *ntc_preheat,
                     const bldc_pump_t *bldc_pump,
                     const steam_pwm_valve_t *steam_pwm_valve,
                     const water_level_tank_t *level_tank,
                     const water_probe_level_t *level_steam);

#endif