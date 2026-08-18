#include "recovery.h"
#include "internal.h"
#include "port.h"

#include <string.h>

typedef struct {
    fdir_entity_desc_t desc;
    uint8_t used;
    uint8_t restart_count;
    uint8_t watchdog_restart_count;
} fdir_entity_slot_t;

static fdir_entity_slot_t g_entities[FDIR_ENTITY_CAP];
static uint8_t g_entity_count;
static fdir_mode_t g_mode;


fdir_status_t fdir_init(const fdir_config_t *config)
{
    if (&fdir_get_now_ms == NULL || &fdir_post_failure == NULL || &fdir_isolate_current_worker == NULL) {
        return FDIR_ERR_PARAM;
    }

    fdir_internal_set_config(config);

    // Reset static buffers
    fdir_health_init();
    fdir_subsystems_init();

    memset(g_entities, 0, sizeof(g_entities));
    g_entity_count = 0U;
    g_mode = FDIR_MODE_NOMINAL;
    return FDIR_OK;
}
