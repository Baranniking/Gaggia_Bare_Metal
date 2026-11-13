#ifndef TLM_ADAPTER_H
#define TLM_ADAPTER_H

#include "main.h"
#include "system_snapshot.h"
#include "gaggia_proto.h"

void tlm_from_snapshot(const system_snapshot_t *snapshot, msg_telemetry_t *tlm);




#endif

