#include "esp_link.h"
#include "stm32f4xx_hal_def.h"

static uint8_t tx_buf[PROTOCOL_FRAME_LEN];
static UART_HandleTypeDef *link_esp_uart = NULL;
static volatile bool flag_tx_uart;
static HAL_StatusTypeDef shipment_status;

HAL_StatusTypeDef init_esp_link(UART_HandleTypeDef *huart)
{
    if (huart == NULL)
        return HAL_ERROR;
    link_esp_uart = huart;
    return HAL_OK;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == link_esp_uart)
    {
        flag_tx_uart = false;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == link_esp_uart)
    {
        flag_tx_uart = false;
    } 
}

HAL_StatusTypeDef esp_link_send_telemetry(const system_snapshot_t *snapshot)
{
    if (snapshot == NULL || link_esp_uart == NULL)
        return HAL_ERROR;
    if (flag_tx_uart)
        return HAL_BUSY;
    size_t size_buf = protocol_pack_telemetry(snapshot, tx_buf, sizeof(tx_buf));
    if (size_buf == 0)
        return HAL_ERROR;
    flag_tx_uart = true;
    shipment_status = HAL_UART_Transmit_DMA(link_esp_uart, tx_buf, (uint16_t)size_buf);
    if (shipment_status != HAL_OK)
    {
        flag_tx_uart = false;
        return shipment_status;
    }
    return HAL_OK;
}