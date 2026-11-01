#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "main.h"
#include <stddef.h>
#include <math.h>
#include "system_snapshot.h"

#define START_BYTE1 0xAA
#define START_BYTE2 0x55
#define FORMAT_VERSION 1
#define TELEMETRY_TYPE 0x01
#define CRC_POLY 0x1021 
#define TEMP_NO_DATA 0x8000
#define PROTOCOL_FRAME_LEN 47
#define PROTOCOL_FRAME_HEAD_PLUS_DATA 45
#define PROTOCOL_DATA_LEN 40
#define NTC_BIT_BREW 2
#define NTC_BIT_STEAM 1
#define NTC_BIT_PREHEAT 0


size_t protocol_pack_telemetry(const system_snapshot_t *snapshot, uint8_t *buf, size_t buf_size);
#endif