#include "esp_link.h"

static uint8_t tx_buf[PROTOCOL_FRAME_LEN];
static UART_HandleTypeDef *esp_link_uart = NULL;
HAL_StatusTypeDef init_esp_link(UART_HandleTypeDef *huart)
{
    if (huart == NULL)
        return HAL_ERROR;
    esp_link_uart = huart;
    return HAL_OK;
}

HAL_StatusTypeDef esp_link_send_telemetry(const system_snapshot_t *snapshot)
{
    if (snapshot == NULL || esp_link_uart == NULL)
        return HAL_ERROR;
    size_t size_buf = protocol_pack_telemetry(snapshot, tx_buf, sizeof(tx_buf));
    if(size_buf == 0) return HAL_ERROR;
    return HAL_UART_Transmit(esp_link_uart, tx_buf, (uint16_t)size_buf, 20);
}