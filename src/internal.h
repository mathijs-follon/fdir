#ifndef FDIR_INTERNAL_H
#define FDIR_INTERNAL_H

#include <stddef.h>
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif


void fdir_internal_set_config(const fdir_config_t *config);

fdir_config_t* fdir_internal_get_config(void);

void fdir_internal_copy_detail(char *dst, size_t dst_len, const char *src);

void fdir_subsystems_init(void);

void fdir_health_init(void);

void fdir_health_set_entity_limit(uint8_t limit);

void fdir_internal_emit(fdir_event_kind_t kind, fdir_entity_id_t entity, fdir_reason_t reason, uint16_t error_code, const char *detail);

void fdir_internal_emit_mode(fdir_event_kind_t kind, fdir_mode_t mode, fdir_entity_id_t entity, fdir_reason_t reason, uint16_t error_code, const char *detail);


#ifdef __cplusplus
}
#endif

#endif /* FDIR_INTERNAL_H */
