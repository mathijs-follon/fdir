#include "internal.h"
#include "config.h"
#include "port.h"
#include "types.h"
#include <string.h>

static fdir_config_t g_config;


void fdir_internal_set_config(const fdir_config_t *config)
{
    if (config == NULL) {
        g_config = fdir_config_default();
        return;
    }
    g_config = *config;
}

fdir_config_t* fdir_internal_get_config(void)
{
    return &g_config;
}



void fdir_internal_copy_detail(char *dst, size_t dst_len, const char *src)
{
    if (dst == NULL || dst_len == 0U) {
        return;
    }

    dst[0] = '\0';
    if (src == NULL) {
        return;
    }

    (void)strncpy(dst, src, dst_len - 1U);
    dst[dst_len - 1U] = '\0';
}


void fdir_internal_emit_mode(fdir_event_kind_t kind, fdir_mode_t mode,
                             fdir_entity_id_t entity, fdir_reason_t reason,
                             uint16_t error_code, const char *detail)
{
    fdir_event_t event;

    memset(&event, 0, sizeof(event));
    event.kind = kind;
    event.mode = mode;
    event.entity = entity;
    event.reason = reason;
    event.error_code = error_code;
    event.timestamp_ms = fdir_get_now_ms();
    fdir_internal_copy_detail(event.detail, sizeof(event.detail), detail);
    fdir_emit_event(&event);
}

void fdir_internal_emit(fdir_event_kind_t kind, fdir_entity_id_t entity,
                        fdir_reason_t reason, uint16_t error_code,
                        const char *detail)
{
    fdir_internal_emit_mode(kind, FDIR_MODE_NOMINAL, entity, reason, error_code,
                            detail);
}
