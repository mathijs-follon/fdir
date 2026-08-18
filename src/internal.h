#ifndef FDIR_INTERNAL_H
#define FDIR_INTERNAL_H

#include <stddef.h>
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif


void fdir_internal_set_config(const fdir_config_t *config);

void fdir_internal_copy_detail(char *dst, size_t dst_len, const char *src);

void fdir_internal_set_entity_limit(uint8_t count);

void fdir_subsystems_init(void);

void fdir_health_init(void);

#ifdef __cplusplus
}
#endif

#endif /* FDIR_INTERNAL_H */
