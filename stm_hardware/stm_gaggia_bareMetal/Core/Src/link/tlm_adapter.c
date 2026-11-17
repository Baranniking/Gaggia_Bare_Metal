#include "tlm_adapter.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <math.h>

#define TEMP_NO_DATA ((int16_t)-32768)

static int16_t temp_to_int(float temp, uint8_t state_ntc, uint8_t scale)
{
    if (!isfinite(temp) || !state_ntc)
        return TEMP_NO_DATA;
    if (temp >= 200 || temp <= -5)
        return TEMP_NO_DATA;
    temp = temp * scale;
    return (int16_t)lroundf(temp);
}

void tlm_from_snapshot(const system_snapshot_t *snapshot, msg_telemetry_t *tlm)
{
    if (tlm == NULL || snapshot == NULL)
        return;
    memset(tlm, 0, sizeof(*tlm));

    tlm->uptime_s = snapshot->up_time_ms / 1000;
    tlm->status = 0;                              /* gaggia_status_t */
    tlm->phase = 0;                               /* brew_phase_t    */
    tlm->pump_pct = snapshot->percent_pump;       /* actual PWM duty */
    tlm->valve_pct = snapshot->percent_pwm_valve; /* actual steam valve PWM duty */
    tlm->brew_c_x100 = temp_to_int(snapshot->temp_brew_boiler, (snapshot->state_ntc_boiler & NTC_BIT_BREW) != 0, 100);
    tlm->steam_c_x10 = temp_to_int(snapshot->temp_steam_boiler, (snapshot->state_ntc_boiler & NTC_BIT_STEAM) != 0, 10);
    tlm->preheat_c_x10 = temp_to_int(snapshot->temp_preheat_boiler, (snapshot->state_ntc_boiler & NTC_BIT_PREHEAT) != 0, 10);
    tlm->bar_x100 = 0;
    tlm->target_bar_x100 = 0;
    tlm->flow_x100 = 0;      /* ml/s */
    tlm->volume_x10 = 0;     /* ml   */
    tlm->manual_bar_x10 = 0; /* echo of the manual setpoint in use */
    tlm->shot_ms = 0;
    tlm->faults = 0;      /* gaggia_fault_t bits */
    tlm->flags = 0;       /* TLM_F_* */
    tlm->steam_power = 0; /* echo of the requested valve opening */
    tlm->clean_cycle = 0;
    tlm->clean_total = 0;
    tlm->reserved = 0;
    tlm->shot_count = 0;
}