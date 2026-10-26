#ifndef LOG_H
#define LOG_H

#include "main.h"

void log_init(UART_HandleTypeDef *huart);
HAL_StatusTypeDef log_write(const char *format, ...);

#endif /* LOG_H */