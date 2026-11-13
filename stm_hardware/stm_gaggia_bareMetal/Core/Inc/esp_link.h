#ifndef ESP_LINK_H
#define ESP_LINK_H
#include "main.h"
#include "system_snapshot.h"
#include "gaggia_proto.h"
#include "stdbool.h"

HAL_StatusTypeDef init_esp_link(UART_HandleTypeDef *huart);
HAL_StatusTypeDef esp_link_send_telemetry(const msg_telemetry_t *tlm);

#endif