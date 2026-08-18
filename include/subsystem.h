#ifndef FDIR_SUBSYSTEM_H
#define FDIR_SUBSYSTEM_H

#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;
    fdir_bool_t is_critical_path;
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



#ifdef __cplusplus
}
#endif

#endif /* FDIR_SUBSYSTEM_H */
