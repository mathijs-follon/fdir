#ifndef FDIR_PORT_H
#define FDIR_PORT_H

#include "types.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Platform integration hooks. Pass to fdir_init().
 *
 * All three callbacks are required (non-NULL); otherwise fdir_init() returns FDIR_ERR_PORT.
 * lock/unlock are optional: when set, the library serialises internal state
 * (including the internal failure queue) for multi-threaded use.
 */
typedef struct {
    uint32_t (*get_now_ms)(void);
    void (*emit_event)(const fdir_event_t *event);
    void (*request_reboot)(const char *reason);
    void (*lock)(void);
    void (*unlock)(void);
} fdir_port_t;

uint32_t fdir_get_now_ms(void);
void fdir_emit_event(const fdir_event_t *event);
void fdir_request_reboot(const char *reason);

#ifdef __cplusplus
}
#endif

#endif /* FDIR_PORT_H */
