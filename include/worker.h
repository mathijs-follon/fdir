#ifndef FDIR_WORKER_H
#define FDIR_WORKER_H

#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Worker loop gate. Non-zero when the entity may take work this iteration.
 *
 * When system mode is SAFE or REBOOT_PENDING, sends a heartbeat and returns zero
 * (workers must stay alive for watchdogs but must not do work). When entity
 * health is FAILED, returns zero without heartbeating.
 *
 * Prefer this over fdir_entity_may_run() in worker loops.
 */
fdir_bool_t fdir_worker_may_run(fdir_entity_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* FDIR_WORKER_H */
