#ifndef FDIR_INTERNAL_H
#define FDIR_INTERNAL_H

#include <stddef.h>
#include "port.h"
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

void fdir_internal_set_config(const fdir_config_t *config);

fdir_config_t *fdir_internal_get_config(void);

void fdir_internal_copy_detail(char *dst, size_t dst_len, const char *src);

void fdir_subsystems_init(void);

void fdir_health_init(void);

void fdir_health_set_entity_limit(uint8_t limit);

void fdir_port_sync_enter(void);

void fdir_port_sync_exit(void);

void fdir_internal_emit_event(const fdir_event_t *event);

void fdir_internal_emit_subsystem_state(fdir_subsystem_id_t sub,
                                        fdir_entity_id_t entity,
                                        fdir_reason_t reason,
                                        uint16_t error_code,
                                        fdir_anomaly_phase_t phase,
                                        const char *detail);

fdir_status_t fdir_port_bind(const fdir_port_t *port);

#ifdef __cplusplus
}
#endif

#endif /* FDIR_INTERNAL_H */
