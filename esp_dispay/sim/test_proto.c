/* Protocol self-test (host): gcc -std=c11 -I src/proto sim/test_proto.c src/proto/gaggia_proto.c && ./a.out */
#include "gaggia_proto.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
int main(void){
    /* CRC-16/CCITT-FALSE check value */
    assert(proto_crc16((const uint8_t*)"123456789",9,0xFFFF)==0x29B1);
    msg_telemetry_t t; memset(&t,0,sizeof t); t.status=3; t.bar_x100=912; t.shot_ms=28400; t.uptime_s=77;
    uint8_t f[PROTO_MAX_FRAME]; size_t n=proto_encode(f,sizeof f,MSG_TELEMETRY,7,&t,sizeof t);
    assert(n==sizeof t+7);
    proto_parser_t p; proto_parser_init(&p); proto_frame_t fr; int got=0;
    uint8_t junk[]={0x00,0xAA,0x12,0xAA}; for(size_t i=0;i<sizeof junk;i++) proto_feed(&p,junk[i],&fr);
    for(size_t i=0;i<n;i++) if(proto_feed(&p,f[i],&fr)) got++;
    assert(got==1 && fr.type==MSG_TELEMETRY && fr.seq==7 && fr.len==sizeof t);
    msg_telemetry_t r; memcpy(&r,fr.payload,sizeof r); assert(r.bar_x100==912 && r.shot_ms==28400);
    f[10]^=1; got=0; for(size_t i=0;i<n;i++) if(proto_feed(&p,f[i],&fr)) got++;
    assert(got==0 && p.crc_errors==1);
    n=proto_encode(f,sizeof f,CMD_PING,9,NULL,0); got=0; for(size_t i=0;i<n;i++) if(proto_feed(&p,f[i],&fr)) got++;
    assert(got==1 && fr.len==0 && fr.type==CMD_PING);
    printf("proto ok: telemetry frame %zu bytes, crc check 0x29B1\n", sizeof t+7);
    return 0;
}
