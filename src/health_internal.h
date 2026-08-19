#ifndef FDIR_HEALTH_INTERNAL_H
#define FDIR_HEALTH_INTERNAL_H

#include "health.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Unsynchronised; caller must hold fdir_port_sync or run single-threaded. */
fdir_bool_t fdir_health_fault_is_latched_unsafe(fdir_entity_id_t id, fdir_reason_t reason);

fdir_bool_t fdir_health_latch_fault_unsafe(fdir_entity_id_t id, fdir_reason_t reason);

fdir_reason_t fdir_health_clear_latch_unsafe(fdir_entity_id_t id);

void fdir_health_set_unsafe(fdir_entity_id_t id, fdir_health_t health, uint16_t error_code,
                            const char *detail);

void fdir_health_reset_unsafe(fdir_entity_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* FDIR_HEALTH_INTERNAL_H */
