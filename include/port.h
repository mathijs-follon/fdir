#ifndef FDIR_PORT_H
#define FDIR_PORT_H

#include "types.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/*
 * Weak port hooks (__attribute__((weak)) in src/port.c). Provide strong
 * definitions in the integrating firmware to override the defaults.
 * Required (default abort()): fdir_get_now_ms, fdir_submit_failure,
 * fdir_isolate_current_worker.
 * Optional (default no-op): fdir_emit_event, fdir_request_reboot.
 * C++ users can skip these; the wrapper supplies a different interface.
 */

uint32_t fdir_get_now_ms(void);

int fdir_submit_failure(const fdir_failure_report_t *report);

void fdir_isolate_current_worker(void);

void fdir_emit_event(const fdir_event_t *event);

void fdir_request_reboot(const char *reason);

#ifdef __cplusplus
}
#endif

#endif /* FDIR_PORT_H */
