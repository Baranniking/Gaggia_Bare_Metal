#include "protocol.h"

static uint16_t protocol_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= ((uint16_t)data[i] << 8);
        for (int k = 0; k < 8; k++)
        {
            if (crc & 0x8000)
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            else
                crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static uint8_t *put_u8(uint8_t *p, uint8_t value)
{
    p[0] = (uint8_t)value;
    return p + 1;
}

static uint8_t *put_u16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    return p + 2;
}

static uint8_t *put_u32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
    return p + 4;
}

static int16_t temp_to_int(float temp, uint8_t state_ntc)
{
    if (!isfinite(temp) || !state_ntc)
        return TEMP_NO_DATA;
    if (temp >= 200 || temp <= -5)
        return TEMP_NO_DATA;
    temp = temp * 10;
    return (int16_t)lroundf(temp);
}

size_t protocol_pack_telemetry(const system_snapshot_t *snapshot, uint8_t *buf, size_t buf_size)
{
    if (snapshot == NULL || buf == NULL || buf_size < PROTOCOL_FRAME_LEN)
        return 0;
    uint8_t *p = buf;
    p = put_u8(p, START_BYTE1);
    p = put_u8(p, START_BYTE2);
    p = put_u8(p, FORMAT_VERSION);
    p = put_u8(p, TELEMETRY_TYPE);
    p = put_u8(p, PROTOCOL_DATA_LEN);
    p = put_u32(p, snapshot->up_time_ms);
    p = put_u32(p, snapshot->seq);
    p = put_u32(p, snapshot->flowmeter);
    p = put_u16(p, snapshot->faults);
    uint8_t state_brew_boiler = (snapshot->state_ntc_boiler >> NTC_BIT_BREW) & 1u;
    uint8_t state_steam_boiler = (snapshot->state_ntc_boiler >> NTC_BIT_STEAM) & 1u;
    uint8_t state_preheat_boiler = (snapshot->state_ntc_boiler >> NTC_BIT_PREHEAT) & 1u;
    p = put_u16(p, (uint16_t)temp_to_int(snapshot->temp_brew_boiler, state_brew_boiler));
    p = put_u16(p, (uint16_t)temp_to_int(snapshot->temp_steam_boiler, state_steam_boiler));
    p = put_u16(p, (uint16_t)temp_to_int(snapshot->temp_preheat_boiler, state_preheat_boiler));
    p = put_u16(p, snapshot->rpm_pump);
    p = put_u16(p, snapshot->pressure_pump);
    p = put_u16(p, snapshot->amplitude_steam_probe);
    p = put_u16(p, snapshot->amplitude_min_level);
    p = put_u16(p, snapshot->amplitude_max_level);
    p = put_u8(p, snapshot->machine_mode);
    p = put_u8(p, snapshot->state_ntc_boiler);
    p = put_u8(p, snapshot->state_ssr_boiler);
    p = put_u8(p, snapshot->state_pump);
    p = put_u8(p, snapshot->percent_pump);
    p = put_u8(p, snapshot->state_pwm_valve);
    p = put_u8(p, snapshot->percent_pwm_valve);
    p = put_u8(p, snapshot->valves);
    p = put_u8(p, snapshot->state_steam_level_probe);
    p = put_u8(p, snapshot->state_tank_level_probe);

    if (p != buf + PROTOCOL_FRAME_HEAD_PLUS_DATA)
        return 0;
    uint16_t crc = protocol_crc16(buf + 2, (size_t)(p - (buf + 2)));
    p = put_u16(p, crc);
    return PROTOCOL_FRAME_LEN;
}