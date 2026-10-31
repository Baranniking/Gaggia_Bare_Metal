#include "system_snapshot.h"
#include "valves.h"

static void fill_ntc(system_snapshot_t *snapshot, const ntc_sensor_t *ntc_brew, const ntc_sensor_t *ntc_steam, const ntc_sensor_t *ntc_preheat)
{
    snapshot->state_ntc_boiler = 0;
    if (ntc_brew->ntc_status)
        snapshot->state_ntc_boiler |= (1u << 2);
    else
        snapshot->faults |= FAULT_NTC_BREW;
    if (ntc_steam->ntc_status)
        snapshot->state_ntc_boiler |= (1u << 1);
    else
        snapshot->faults |= FAULT_NTC_STEAM;
    if (ntc_preheat->ntc_status)
        snapshot->state_ntc_boiler |= (1u << 0);
    else
        snapshot->faults |= FAULT_NTC_PREHEAT;
    snapshot->temp_brew_boiler = ntc_brew->temp;
    snapshot->temp_steam_boiler = ntc_steam->temp;
    snapshot->temp_preheat_boiler = ntc_preheat->temp;
}

static void fill_pump(system_snapshot_t *snapshot, const bldc_pump_t *bldc_pump)
{
    snapshot->rpm_pump = bldc_pump->rpm;
    snapshot->percent_pump = bldc_pump->percent;
    snapshot->pressure_pump = 0; // заглушка
    snapshot->state_pump = bldc_pump->state;
    if (snapshot->state_pump == PUMP_FAULT)
        snapshot->faults |= FAULT_PUMP;
}

static void fill_steam_pwm_valve(system_snapshot_t *snapshot, const steam_pwm_valve_t *steam_pwm_valve)
{
    snapshot->state_pwm_valve = steam_pwm_valve->state;
    if (snapshot->state_pwm_valve == OUT_VALVE_STEAM_FAULT)
        snapshot->faults |= FAULT_STEAM_VALVE;
    snapshot->percent_pwm_valve = steam_pwm_valve->percent;
}

static void fill_valves(system_snapshot_t *snapshot)
{
    snapshot->valves = 0;
    for (uint8_t i = 0; i < NUM_OF_VALVE; i++)
    {
        if (valve_is_open(i))
            snapshot->valves |= (1u << i);
    }
}

static void fill_steam_probe(system_snapshot_t *snapshot, const water_probe_level_t *level_steam)
{
    snapshot->state_steam_level_probe = level_steam->state;
    if (snapshot->state_steam_level_probe == PROBE_UNKNOWN)
        snapshot->faults |= FAULT_STEAM_PROBE;
    snapshot->amplitude_steam_probe = level_steam->amplitude;
}

static void fill_tank(system_snapshot_t *snapshot, const water_level_tank_t *level_tank)
{
    snapshot->state_tank_level_probe = level_tank->state;
    if (snapshot->state_tank_level_probe == ERROR_TANK || snapshot->state_tank_level_probe == UNKNOWN_STATE)
        snapshot->faults |= FAULT_TANK_SENSOR;
    if (snapshot->state_tank_level_probe == EMPTY_TANK)
        snapshot->faults |= FAULT_TANK_EMPTY;
    snapshot->amplitude_min_level = level_tank->amplitude_min_level;
    snapshot->amplitude_max_level = level_tank->amplitude_max_level;
}

static void fill_stubs(system_snapshot_t *snapshot)
{
    snapshot->flowmeter = 0;        // заглушка
    snapshot->machine_mode = 0;     // заглушка
    snapshot->state_ssr_boiler = 0; // заглушка
}

void snapshot_update(system_snapshot_t *snapshot, const ntc_sensor_t *ntc_brew,
                     const ntc_sensor_t *ntc_steam,
                     const ntc_sensor_t *ntc_preheat,
                     const bldc_pump_t *bldc_pump,
                     const steam_pwm_valve_t *steam_pwm_valve,
                     const water_level_tank_t *level_tank,
                     const water_probe_level_t *level_steam)
{
    if (snapshot == NULL || ntc_brew == NULL || ntc_steam == NULL || ntc_preheat == NULL ||
        bldc_pump == NULL || steam_pwm_valve == NULL || level_tank == NULL || level_steam == NULL)
        return;

    snapshot->up_time_ms = HAL_GetTick();
    snapshot->seq++;
    snapshot->faults = 0;

    fill_ntc(snapshot, ntc_brew, ntc_steam, ntc_preheat);
    fill_pump(snapshot, bldc_pump);
    fill_steam_pwm_valve(snapshot, steam_pwm_valve);
    fill_valves(snapshot);
    fill_steam_probe(snapshot, level_steam);
    fill_tank(snapshot, level_tank);
    fill_stubs(snapshot);
}