#ifndef CMD_HANDLER_H
#define CMD_HANDLER_H

#include "main.h"
#include "gaggia_proto.h"
#include "esp_link.h"

#define PATCH 0
#define MINOR 1
#define MAJOR 0


void cmd_handler(const proto_frame_t *t);
HAL_StatusTypeDef cmd_send_hello();
void reset_cause_save();
uint32_t reset_cause_get();


#endif