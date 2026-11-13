/**
 * gaggia_proto.c — framing + CRC. SHARED FILE (ESP32 and STM32).
 */
#include "gaggia_proto.h"
#include <string.h>

uint16_t proto_crc16(const uint8_t *d, size_t n, uint16_t crc)
{
    while (n--) {
        crc ^= (uint16_t)(*d++) << 8;
        for (int i = 0; i < 8; i++)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

size_t proto_encode(uint8_t *out, size_t cap, uint8_t type, uint8_t seq,
                    const void *payload, uint8_t len)
{
    if (len > PROTO_MAX_PAYLOAD || cap < (size_t)len + PROTO_OVERHEAD) return 0;
    out[0] = PROTO_SOF0;
    out[1] = PROTO_SOF1;
    out[2] = len;
    out[3] = type;
    out[4] = seq;
    if (len) memcpy(&out[5], payload, len);
    uint16_t crc = proto_crc16(&out[2], (size_t)len + 3u, 0xFFFFu);
    out[5 + len] = (uint8_t)(crc & 0xFF);
    out[6 + len] = (uint8_t)(crc >> 8);
    return (size_t)len + PROTO_OVERHEAD;
}

enum { ST_SOF0, ST_SOF1, ST_LEN, ST_TYPE, ST_SEQ, ST_PAYLOAD, ST_CRC_L, ST_CRC_H };

void proto_parser_init(proto_parser_t *p)
{
    memset(p, 0, sizeof(*p));
}

bool proto_feed(proto_parser_t *p, uint8_t b, proto_frame_t *out)
{
    switch (p->state) {
    case ST_SOF0: if (b == PROTO_SOF0) p->state = ST_SOF1; break;
    case ST_SOF1: p->state = (b == PROTO_SOF1) ? ST_LEN : (b == PROTO_SOF0 ? ST_SOF1 : ST_SOF0); break;
    case ST_LEN:
        if (b > PROTO_MAX_PAYLOAD) { p->len_errors++; p->state = ST_SOF0; break; }
        p->len = b; p->state = ST_TYPE; break;
    case ST_TYPE: p->type = b; p->state = ST_SEQ; break;
    case ST_SEQ:
        p->seq = b; p->idx = 0;
        p->state = p->len ? ST_PAYLOAD : ST_CRC_L; break;
    case ST_PAYLOAD:
        p->buf[p->idx++] = b;
        if (p->idx >= p->len) p->state = ST_CRC_L;
        break;
    case ST_CRC_L: p->crc_lo = b; p->state = ST_CRC_H; break;
    case ST_CRC_H: {
        p->state = ST_SOF0;
        uint8_t hdr[3] = { p->len, p->type, p->seq };
        uint16_t crc = proto_crc16(hdr, 3, 0xFFFFu);
        crc = proto_crc16(p->buf, p->len, crc);
        if (crc != (uint16_t)(p->crc_lo | (b << 8))) { p->crc_errors++; break; }
        p->frames_ok++;
        out->type = p->type; out->seq = p->seq; out->len = p->len; out->payload = p->buf;
        return true;
    }
    default: p->state = ST_SOF0; break;
    }
    return false;
}
