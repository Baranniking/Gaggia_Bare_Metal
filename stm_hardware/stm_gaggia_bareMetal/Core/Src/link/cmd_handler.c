#include "cmd_handler.h"
#include "esp_link.h"
#include "stm32f4xx_hal_def.h"
#include <string.h>

static uint32_t reset_cause = 0;
static bool link_up = false;

HAL_StatusTypeDef ack_state;

uint32_t count_ack_ok;
uint32_t count_ack_error;
uint32_t count_ack_busy;

void reset_cause_save()
{
    reset_cause = RCC->CSR;
    __HAL_RCC_CLEAR_RESET_FLAGS();
}

uint32_t reset_cause_get()
{
    return reset_cause;
}

HAL_StatusTypeDef cmd_send_hello()
{
    msg_hello_t h;
    HAL_StatusTypeDef state = HAL_OK;
    if (!link_up)
    {
        memset(&h, 0, sizeof(h));
        h.proto_ver = PROTO_VERSION;
        h.fw_patch = PATCH;
        h.fw_minor = MINOR;
        h.fw_major = MAJOR;
        h.reset_cause = reset_cause_get();
        state = esp_link_send(MSG_HELLO, &h, sizeof(h));
    }
    if (state == HAL_OK)
    {
        link_up = true;
    }
    return state;
}

void cmd_handler(const proto_frame_t *t)
{
    if (t == NULL)
        return;
    msg_ack_t ack;
    cmd_send_hello();
    switch (t->type)
    {
    case CMD_PING:
        if (t->len == 0)
        {
            ack.result = ACK_OK;
        }
        else
        {
            ack.result = ACK_BAD_LEN;
        }

        break;
    default:
        ack.result = ACK_UNKNOWN;
        break;
    }

    ack.seq = t->seq;
    ack_state = esp_link_send(MSG_ACK, &ack, sizeof(ack));
    switch (ack_state)
    {
    case HAL_OK:
        count_ack_ok++;
        break;
    case HAL_ERROR:
        count_ack_error++;
        break;
    case HAL_BUSY:
        count_ack_busy++;
        break;

    default:
        break;
    }
}