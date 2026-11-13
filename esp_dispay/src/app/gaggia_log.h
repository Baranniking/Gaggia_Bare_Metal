/**
 * gaggia_log.h — debug log: event ring buffer + telemetry CSV stream.
 *
 *  - glog(LOG_I, "fmt", ...) stores a timestamped line in a RAM ring (shown on
 *    the Telemetry screen) and forwards it to the sink as "# I 12.345 msg".
 *  - With CSV enabled, glog_csv_tick() emits one telemetry row per period to
 *    the sink (header line first). Paste into SerialPlot / a spreadsheet.
 *
 * Call from the UI task only. If the link task ever logs, wrap the ring
 * access with a mutex (GLOG_LOCK/UNLOCK in gaggia_log.c).
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "gaggia_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { LOG_I = 0, LOG_W, LOG_E } log_level_t;

#define GLOG_SIZE     64
#define GLOG_MSG_LEN  60

typedef struct {
    uint32_t ms;
    uint8_t  level;
    char     msg[GLOG_MSG_LEN];
} log_entry_t;

typedef void (*glog_sink_t)(const char *line);   /* line has no trailing '\n' */

void               glog(log_level_t lv, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void               glog_set_now(uint32_t now_ms);  /* time base, set by gaggia_tick */
void               glog_set_sink(glog_sink_t sink);
uint32_t           glog_version(void);             /* bumps on every entry/clear */
uint16_t           glog_count(void);
const log_entry_t *glog_get(uint16_t i);           /* 0 = oldest */
void               glog_clear(void);

typedef enum { CSV_OFF = 0, CSV_1HZ, CSV_5HZ, CSV_20HZ, CSV_RATE_COUNT } csv_rate_t;
void        glog_csv_set(csv_rate_t r);
csv_rate_t  glog_csv_get(void);
const char *glog_csv_name(csv_rate_t r);
void        glog_csv_tick(uint32_t now_ms, const gaggia_telemetry_t *t);

#ifdef __cplusplus
}
#endif
