#ifndef FDIR_HEALTH_H
#define FDIR_HEALTH_H

#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif

void fdir_health_heartbeat_notify(fdir_entity_id_t id);

void fdir_health_set(fdir_entity_id_t id, fdir_health_t health, uint16_t error_code, const char *detail);

void fdir_health_reset(fdir_entity_id_t id);

const fdir_health_snapshot_t *fdir_health_snapshot(fdir_entity_id_t id);

fdir_bool_t fdir_health_is_stale(fdir_entity_id_t id, uint32_t now_ms, uint32_t max_age_ms);

#ifdef __cplusplus
}
#endif

#endif /* FDIR_HEALTH_H */
