#include "log.h"
#include <stdarg.h>
#include <stdio.h>

#define LOG_BUFFER_SIZE    128
#define LOG_TX_TIMEOUT_MS  50

static UART_HandleTypeDef *log_uart = NULL;

void log_init(UART_HandleTypeDef *huart)
{
    log_uart = huart;
}

HAL_StatusTypeDef log_write(const char *format, ...)
{
    if (log_uart == NULL) {
        return HAL_ERROR;
    }

    char local_buffer[LOG_BUFFER_SIZE];

    va_list args;
    va_start(args, format);
    int written = vsnprintf(local_buffer, sizeof(local_buffer), format, args);
    va_end(args);

    if (written < 0) {
        return HAL_ERROR;
    }

    uint16_t len;
    if ((size_t)written < sizeof(local_buffer)) {
        len = (uint16_t)written;
    } else {
        len = (uint16_t)(sizeof(local_buffer) - 1);
    }

    return HAL_UART_Transmit(log_uart, (uint8_t *)local_buffer, len, LOG_TX_TIMEOUT_MS);
}