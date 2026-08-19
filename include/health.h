#ifndef FDIR_HEALTH_H
#define FDIR_HEALTH_H

#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif

void fdir_health_heartbeat_notify(fdir_entity_id_t id);

void fdir_health_set(fdir_entity_id_t id, fdir_health_t health, uint16_t error_code, const char *detail);

/** Clear error state and mark OK. Does not clear fault latch or restart budgets. */
void fdir_health_reset(fdir_entity_id_t id);

/**
 * Pointer into live health storage. Numeric fields are usually coherent; detail[]
 * may tear if read concurrently with fdir_health_set / fdir_report_fault.
 * Prefer fdir_health_snapshot_copy() when a port lock is configured or tasks
 * overlap readers and writers.
 */
const fdir_health_snapshot_t *fdir_health_snapshot(fdir_entity_id_t id);

/** Copy health state under port sync. Returns 0 if id is out of range or out is NULL. */
fdir_bool_t fdir_health_snapshot_copy(fdir_entity_id_t id, fdir_health_snapshot_t *out);

fdir_bool_t fdir_health_is_stale(fdir_entity_id_t id, uint32_t now_ms, uint32_t max_age_ms);

/** Non-zero when entity+reason is latched after supervisor handling (query only). */
fdir_bool_t fdir_health_fault_is_latched(fdir_entity_id_t id, fdir_reason_t reason);

#ifdef __cplusplus
}
#endif

#endif /* FDIR_HEALTH_H */
