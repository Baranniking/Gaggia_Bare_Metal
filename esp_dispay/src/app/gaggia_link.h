/**
 * gaggia_link.h — ESP32 side of the STM32 link (used when GAGGIA_SIM=0).
 *
 *   UART RX bytes --> gaggia_link_rx() --> proto parser --> gaggia_on_telemetry()
 *   gaggia_cmd_*() --> gaggia_link_send_*() --> proto_encode() --> tx callback
 *
 * Everything runs in the UI task (Arduino loop): no locks needed. If you move
 * RX into its own task, push bytes through a FreeRTOS stream buffer instead
 * of calling gaggia_link_rx() from there.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "gaggia_proto.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t (*gaggia_link_tx_fn)(const uint8_t *data, size_t len);

void gaggia_link_init(gaggia_link_tx_fn tx);
void gaggia_link_rx(const uint8_t *data, size_t len);   /* feed raw UART bytes */
void gaggia_link_tick(uint32_t now_ms);                 /* keepalive ping       */
bool gaggia_link_raw_dump(bool on);                     /* hex frames -> log    */

/* typed senders (called by gaggia_model.c) */
void gaggia_link_send_params(void);                     /* CMD_SET_PARAMS       */
void gaggia_link_send_profile(uint8_t index);           /* CMD_SET_PROFILE      */
void gaggia_link_send_brew(uint8_t action, float manual_bar);
void gaggia_link_send_pressure(float bar);
void gaggia_link_send_steam(bool on);
void gaggia_link_send_valve(uint8_t pct);
void gaggia_link_send_purge(uint16_t ms);
void gaggia_link_send_hot_water(bool on, uint16_t ml);
void gaggia_link_send_backflush(bool on);
void gaggia_link_send_standby(bool on);
void gaggia_link_send_reset_counter(void);
void gaggia_link_send_rtc(uint32_t unix_s);

#ifdef __cplusplus
}
#endif
