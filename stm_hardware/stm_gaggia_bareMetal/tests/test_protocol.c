/* PC test for Core/Src/protocol.c. Includes the .c to reach static functions. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../Core/Src/protocol.c"

#define I16_MIN ((int16_t)-32768) /* protocol.h redefines INT16_MIN, don't rely on it */

static int failures;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

int main(void)
{
    /* CRC */
    CHECK(protocol_crc16((const uint8_t *)"123456789", 9) == 0x29B1);

    /* put_* little endian */
    uint8_t b[4] = {0};
    uint8_t *r = put_u16(b, 0x1234);
    CHECK(b[0] == 0x34 && b[1] == 0x12 && r == b + 2);
    r = put_u32(b, 0x12345678);
    CHECK(b[0] == 0x78 && b[1] == 0x56 && b[2] == 0x34 && b[3] == 0x12 && r == b + 4);

    /* temp_to_int */
    CHECK(temp_to_int(25.19f, 1) == 252);
    CHECK(temp_to_int(93.0f, 1) == 930);
    CHECK(temp_to_int(25.19f, 0) == I16_MIN);
    CHECK(temp_to_int(NAN, 1) == I16_MIN);
    CHECK(temp_to_int(5000.0f, 1) == I16_MIN);
    CHECK(temp_to_int(-5.0f, 1) == I16_MIN);
    CHECK(temp_to_int(200.0f, 1) == I16_MIN);

    /* pack: invalid args */
    system_snapshot_t s;
    memset(&s, 0, sizeof s);
    uint8_t buf[64];
    CHECK(protocol_pack_telemetry(&s, buf, 46) == 0);
    CHECK(protocol_pack_telemetry(&s, NULL, sizeof buf) == 0);
    CHECK(protocol_pack_telemetry(NULL, buf, sizeof buf) == 0);

    /* reference frame */
    s.up_time_ms = 100000; s.seq = 5; s.flowmeter = 0; s.faults = 0x0060;
    s.temp_brew_boiler = 93.0f; s.temp_steam_boiler = 135.0f; s.temp_preheat_boiler = 80.0f;
    s.state_ntc_boiler = 7; s.rpm_pump = 3750; s.pressure_pump = 900;
    s.amplitude_steam_probe = 2753; s.amplitude_min_level = 500; s.amplitude_max_level = 2700;
    s.machine_mode = 0; s.state_ssr_boiler = 0; s.state_pump = 2; s.percent_pump = 50;
    s.state_pwm_valve = 1; s.percent_pwm_valve = 0; s.valves = 0x05;
    s.state_steam_level_probe = 1; s.state_tank_level_probe = 0;

    static const uint8_t expected[47] = {
        0xAA,0x55,0x01,0x01,0x28,0xA0,0x86,0x01,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x60,0x00,0xA2,0x03,0x46,0x05,0x20,0x03,0xA6,0x0E,0x84,0x03,0xC1,0x0A,0xF4,
        0x01,0x8C,0x0A,0x00,0x07,0x00,0x02,0x32,0x01,0x00,0x05,0x01,0x00,0xBC,0xE8};

    memset(buf, 0xEE, sizeof buf);
    CHECK(protocol_pack_telemetry(&s, buf, sizeof buf) == 47);
    CHECK(memcmp(buf, expected, 47) == 0);

    /* CRC over buf[2..44] */
    uint16_t crc = protocol_crc16(buf + 2, 43);
    CHECK(buf[45] == (uint8_t)crc && buf[46] == (uint8_t)(crc >> 8));

    if (failures == 0)
        printf("ALL OK\n");
    return failures != 0;
}
