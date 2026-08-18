#ifndef FDIR_PORT_H
#define FDIR_PORT_H

#include "types.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Weak defined (extern "C" __attribute__((weak))), to be overridden by the port implementation */

uint32_t fdir_get_now_ms(void);

int fidr_post_failure(const fdir_failure_report_t *report);

void fdir_isolate_current_worker(void);

void fdir_emit_event(const fdir_event_t *event);

void fdir_request_reboot(const char *reason);

#ifdef __cplusplus
}
#endif

#endif /* FDIR_PORT_H */
