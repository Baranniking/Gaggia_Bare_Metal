#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/cdefs.h>
#include "gaggia_proto.h"

uint8_t frame[PROTO_MAX_FRAME];
uint8_t byte_ack[2] = {1, 0};
msg_ack_t ack = {5, 0};

proto_parser_t parser;
proto_frame_t decoded;

size_t t;


static void dump(const uint8_t *buf, size_t n)
{
    printf("size = %zu\n", n);
    for (size_t i = 0; i < n; i++)
    {
        printf("byte = %02X\n", buf[i]);
    }
}

static size_t proto_assist(const uint8_t *buf, proto_parser_t *parser, size_t len)
{
    size_t accept = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (proto_feed(parser, buf[i], &decoded))
        {
            printf("ready at byte #: %zu\n", i);
            printf("type = %u, seq = %u, len = %u\n", decoded.type, decoded.seq, decoded.len);
            accept += 1;
        }
    }
    printf("frames_ok = %u\n", (unsigned)parser->frames_ok);
    return accept;
}

    int main(void)
    {
        uint8_t bad[PROTO_MAX_FRAME];
        uint8_t g[4 + PROTO_MAX_FRAME] = {0x00, 0xAA, 0xAA, 0x13};
        t = proto_encode(frame, sizeof(frame), MSG_ACK, 5, byte_ack, sizeof(byte_ack));
        // proto_parser_init(&parser);
        // size_t n = proto_assist(frame, &parser, t);
        //  memcpy(bad, frame, t);
        //  bad[5] ^= 1;
        //  proto_parser_init(&parser);
        //  n = proto_assist(bad, &parser, t);
        memcpy(g + 4, frame, t);
        proto_parser_init(&parser);
        size_t n = proto_assist(g, &parser, t + 4);

        printf("accepted = %zu, crc_errors = %u\n", n, (unsigned)parser.crc_errors);

        return 0;
    }
