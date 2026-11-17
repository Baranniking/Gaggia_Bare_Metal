#include "esp_link.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_uart.h"
#include "string.h"
#include "math.h"
#define SIZE_RX_BUF 256

static uint8_t tx_buf[PROTO_MAX_FRAME];
static uint8_t rx_buf[SIZE_RX_BUF];

uint16_t count_rx_frame = 0;
uint16_t last_rx_type = 0;
uint16_t last_rx_len = 0;

static proto_parser_t rx_parser;
static UART_HandleTypeDef *link_esp_uart = NULL;
static volatile bool flag_tx_uart;
static HAL_StatusTypeDef shipment_status;

HAL_StatusTypeDef init_esp_link(UART_HandleTypeDef *huart)
{
    if (huart == NULL)
        return HAL_ERROR;
    link_esp_uart = huart;
    proto_parser_init(&rx_parser);
    return HAL_UART_Receive_DMA(link_esp_uart, rx_buf, sizeof(rx_buf));
}

bool esp_link_tx_ready()
{
    if (link_esp_uart != NULL && flag_tx_uart == false)
    {
        return true;
    }
    else return false;
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

HAL_StatusTypeDef esp_link_send(uint8_t type, const void *data, uint8_t len)
{
    static uint8_t seq = 0;

    if ((data == NULL && len != 0) || link_esp_uart == NULL)
        return HAL_ERROR;
    if (flag_tx_uart)
        return HAL_BUSY;
    size_t size_buf = proto_encode(tx_buf, sizeof(tx_buf), type, seq, data, len);
    if (size_buf == 0)
        return HAL_ERROR;
    flag_tx_uart = true;
    shipment_status = HAL_UART_Transmit_DMA(link_esp_uart, tx_buf, (uint16_t)size_buf);
    if (shipment_status != HAL_OK)
    {
        flag_tx_uart = false;
        return shipment_status;
    }
    seq += 1;
    return HAL_OK;
}

bool esp_link_poll(proto_frame_t *t)
{
    if (link_esp_uart == NULL)
        return false;
    static uint8_t rx_tail = 0;
    uint16_t rx_dma_count = __HAL_DMA_GET_COUNTER(link_esp_uart->hdmarx);
    uint8_t rx_head = (SIZE_RX_BUF - rx_dma_count) % SIZE_RX_BUF;
    while (rx_tail != rx_head)
    {
        if (proto_feed(&rx_parser, rx_buf[rx_tail], t))
        {
            count_rx_frame += 1;
            last_rx_len = t->len;
            last_rx_type = t->type;
            rx_tail = (rx_tail + 1) % SIZE_RX_BUF;
            return true;
        }

        rx_tail = (rx_tail + 1) % SIZE_RX_BUF;
    }
    return false;
}