#ifndef ESP_LINK_H
#define ESP_LINK_H
#include "main.h"
#include "system_snapshot.h"
#include "gaggia_proto.h"
#include "stdbool.h"

HAL_StatusTypeDef init_esp_link(UART_HandleTypeDef *huart);
HAL_StatusTypeDef esp_link_send(uint8_t type, const void *data, uint8_t len);
bool esp_link_tx_ready();
bool esp_link_poll(proto_frame_t *t);

#endif