/* Link self-test (host, real-link mode):
 *   gcc -std=c11 -DGAGGIA_SIM=0 -I src/app -I src/proto sim/test_link.c src/app/gaggia_model.c \
 *       src/app/gaggia_link.c src/app/gaggia_log.c src/app/knob_input.c src/proto/gaggia_proto.c -lm && ./a.out */
#include "gaggia_model.h"
#include "gaggia_link.h"
#include "gaggia_proto.h"
#include "gaggia_log.h"
#include "knob_input.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
static uint8_t txlog[4096]; static size_t txn;
static size_t tx(const uint8_t *d, size_t n){ memcpy(txlog+txn,d,n); txn+=n; return n; }
static void sink(const char *l){ printf("  %s\n", l); }
/* decode everything the ESP sent */
static uint8_t lastpl[PROTO_MAX_PAYLOAD];
static int count_type(uint8_t type, proto_frame_t *last){
    static proto_parser_t p; proto_parser_init(&p); proto_frame_t f; int c=0;
    for(size_t i=0;i<txn;i++) if(proto_feed(&p,txlog[i],&f) && f.type==type){
        c++; if(last){ memcpy(lastpl,f.payload,f.len); *last=f; last->payload=lastpl; } }
    return c;
}
static void stm_send(uint8_t type, const void *pl, uint8_t len){
    uint8_t fr[PROTO_MAX_FRAME]; size_t n=proto_encode(fr,sizeof fr,type,1,pl,len); gaggia_link_rx(fr,n);
}
int main(void){
    glog_set_sink(sink);
    gaggia_init(NULL);
    gaggia_link_init(tx);
    msg_hello_t h={PROTO_VERSION,1,0,0,0x0C000000}; stm_send(MSG_HELLO,&h,sizeof h);
    assert(count_type(CMD_SET_PARAMS,NULL)==1 && count_type(CMD_SET_PROFILE,NULL)==1);
    msg_telemetry_t t={0}; t.status=WIRE_ST_READY; t.brew_c_x100=9302; t.bar_x100=0; t.uptime_s=12;
    t.flags=TLM_F_BREW_HEATER; t.preheat_c_x10=801; t.shot_count=1300;
    stm_send(MSG_TELEMETRY,&t,sizeof t); gaggia_tick(100);
    const gaggia_telemetry_t *m=gaggia_tm();
    assert(m->status==GS_READY && m->link_ok && m->brew_heater_on && m->shot_count==1300);
    assert(m->brew_temp>93.01f && m->brew_temp<93.03f && m->preheat_temp>80.0f);
    gaggia_cmd_brew(true); proto_frame_t f; assert(count_type(CMD_BREW,&f)==1);
    cmd_brew_t b; memcpy(&b,f.payload,sizeof b); assert(b.action==BREW_PROFILE);
    /* knob on the steam valve: only changes go out */
    knob_input_report(40); gaggia_tick(150); knob_input_report(40); gaggia_tick(200); knob_input_report(55); gaggia_tick(250);
    printf("valve frames: %d\n", count_type(CMD_STEAM_VALVE,&f)); assert(f.payload[0]==55);
    /* knob in pressure mode */
    gaggia_knob_set_mode(KNOB_PRESSURE); knob_input_report(75); gaggia_tick(300);
    assert(count_type(CMD_SET_PRESSURE,&f)>=1); cmd_u16_t pr; memcpy(&pr,f.payload,2); assert(pr.value==90);
    /* corrupt frame counted */
    uint8_t fr[PROTO_MAX_FRAME]; size_t n=proto_encode(fr,sizeof fr,MSG_TELEMETRY,2,&t,sizeof t); fr[8]^=0xFF; gaggia_link_rx(fr,n);
    assert(gaggia_link_stats()->crc_errors==1);
    /* link timeout */
    gaggia_tick(100+PROTO_LINK_TIMEOUT_MS+50); assert(!gaggia_tm()->link_ok && (gaggia_tm()->faults & FAULT_LINK_LOST));
    printf("LINK TEST OK: rx frames %lu, crc errors %lu, timeouts %lu, tx bytes %zu\n",
      (unsigned long)gaggia_link_stats()->rx_frames,(unsigned long)gaggia_link_stats()->crc_errors,
      (unsigned long)gaggia_link_stats()->timeouts, txn);
    return 0;
}
