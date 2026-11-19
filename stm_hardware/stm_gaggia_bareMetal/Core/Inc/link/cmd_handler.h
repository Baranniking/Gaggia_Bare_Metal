#ifndef CMD_HANDLER_H
#define CMD_HANDLER_H

#include "main.h"
#include "gaggia_proto.h"
#include "esp_link.h"
#include "boiler_ctrl.h"

#define PATCH 0
#define MINOR 1
#define MAJOR 0


void cmd_handler(const proto_frame_t *t);
HAL_StatusTypeDef cmd_send_hello();
void reset_cause_save();
uint32_t reset_cause_get();
HAL_StatusTypeDef init_cmd_handler(boiler_t *pre, boiler_t *st, boiler_t *cof);


#endif