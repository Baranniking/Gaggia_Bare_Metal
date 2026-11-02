#ifndef ESP_LINK_H
#define ESP_LINK_H
#include "main.h"
#include "protocol.h"
#include "system_snapshot.h"

HAL_StatusTypeDef init_esp_link(UART_HandleTypeDef *huart);
HAL_StatusTypeDef esp_link_send_telemetry(const system_snapshot_t *snapshot);

#endif