#ifndef FDIR_SUBSYSTEM_H
#define FDIR_SUBSYSTEM_H

#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name; /**< Copied at registration; need not outlive fdir_subsystem_register(). */
    fdir_bool_t is_critical_path;
    /**
     * Subsystem-level handler (FDIR level 2). Called when a linked entity
     * exhausts its restart budget and the default action would be on_exhausted.
     * Return FDIR_ACTION_NONE to accept the entity default.
     */
    fdir_action_t (*on_entity_exhausted)(fdir_subsystem_id_t sub,
                                         fdir_entity_id_t entity,
                                         const fdir_failure_report_t *report,
                                         void *user);
    void *user;
} fdir_subsystem_desc_t;

uint8_t fdir_subsystems_critical_unavailable_count(void);

fdir_status_t fdir_subsystem_register(const fdir_subsystem_desc_t *desc, fdir_subsystem_id_t *out_id);

const char *fdir_subsystem_name(fdir_subsystem_id_t id);

fdir_bool_t fdir_subsystem_is_available(fdir_subsystem_id_t id);

fdir_bool_t fdir_subsystem_is_degraded(fdir_subsystem_id_t id);

fdir_bool_t fdir_subsystem_is_critical_path(fdir_subsystem_id_t id);

void fdir_subsystem_mark_available(fdir_subsystem_id_t id);

void fdir_subsystem_mark_unavailable(fdir_subsystem_id_t id);

void fdir_subsystem_mark_degraded(fdir_subsystem_id_t id);

fdir_action_t fdir_subsystem_on_entity_exhausted(fdir_subsystem_id_t sub,
                                                 fdir_entity_id_t entity,
                                                 const fdir_failure_report_t *report);

#ifdef __cplusplus
}
#endif

#endif /* FDIR_SUBSYSTEM_H */
