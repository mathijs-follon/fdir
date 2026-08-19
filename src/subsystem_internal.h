#ifndef FDIR_SUBSYSTEM_INTERNAL_H
#define FDIR_SUBSYSTEM_INTERNAL_H

#include "subsystem.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Unsynchronised; caller must hold fdir_port_sync or run single-threaded. */
void fdir_subsystem_mark_available_unsafe(fdir_subsystem_id_t id);

void fdir_subsystem_mark_unavailable_unsafe(fdir_subsystem_id_t id);

void fdir_subsystem_mark_degraded_unsafe(fdir_subsystem_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* FDIR_SUBSYSTEM_INTERNAL_H */
