#include "subsystem.h"
#include "internal.h"
#include <string.h>

typedef struct {
    fdir_subsystem_desc_t desc;
    uint8_t available;
    uint8_t degraded;
    uint8_t used;
} fdir_subsystem_slot_t;

static fdir_subsystem_slot_t g_subs[FDIR_SUBSYSTEM_CAP];
static uint8_t g_sub_count;

void fdir_subsystems_init(void)
{
    memset(g_subs, 0, sizeof(g_subs));
    g_sub_count = 0U;
}

static fdir_subsystem_slot_t *slot_of(fdir_subsystem_id_t id)
{
    if (id >= g_sub_count || !g_subs[id].used) {
        return NULL;
    }
    return &g_subs[id];
}

uint8_t fdir_subsystems_critical_unavailable_count(void)
{
    uint8_t i;
    uint8_t count = 0U;

    for (i = 0U; i < g_sub_count; i++) {
        if (g_subs[i].used && g_subs[i].desc.is_critical_path && !g_subs[i].available) {
            count++;
        }
    }
    return count;
}

fdir_status_t fdir_subsystem_register(const fdir_subsystem_desc_t *desc, fdir_subsystem_id_t *out_id)
{
    fdir_subsystem_id_t id;

    if (desc == NULL || out_id == NULL) {
        return FDIR_ERR_PARAM;
    }
    if (g_sub_count >= FDIR_SUBSYSTEM_CAP) {
        return FDIR_ERR_FULL;
    }

    id = g_sub_count++;
    g_subs[id].desc = *desc;
    g_subs[id].used = 1U;
    g_subs[id].available = 1U;
    g_subs[id].degraded = 0U;
    *out_id = id;
    return FDIR_OK;
}

const char *fdir_subsystem_name(fdir_subsystem_id_t id)
{
    const fdir_subsystem_slot_t *slot = slot_of(id);

    if (slot == NULL) {
        return "unknown";
    }
    if (slot->desc.name == NULL) {
        return "unnamed";
    }
    return slot->desc.name;
}

fdir_bool_t fdir_subsystem_is_available(fdir_subsystem_id_t id)
{
    const fdir_subsystem_slot_t *slot = slot_of(id);

    return (slot != NULL && slot->available) ? 1U : 0U;
}

fdir_bool_t fdir_subsystem_is_degraded(fdir_subsystem_id_t id)
{
    const fdir_subsystem_slot_t *slot = slot_of(id);

    return (slot != NULL && slot->degraded) ? 1U : 0U;
}

fdir_bool_t fdir_subsystem_is_critical_path(fdir_subsystem_id_t id)
{
    const fdir_subsystem_slot_t *slot = slot_of(id);

    return (slot != NULL && slot->desc.is_critical_path) ? 1U : 0U;
}

void fdir_subsystem_mark_available(fdir_subsystem_id_t id)
{
    fdir_subsystem_slot_t *slot = slot_of(id);

    if (slot == NULL) {
        return;
    }
    slot->available = 1U;
    slot->degraded = 0U;
}

void fdir_subsystem_mark_unavailable(fdir_subsystem_id_t id)
{
    fdir_subsystem_slot_t *slot = slot_of(id);

    if (slot == NULL) {
        return;
    }
    slot->available = 0U;
    slot->degraded = 1U;
}

void fdir_subsystem_mark_degraded(fdir_subsystem_id_t id)
{
    fdir_subsystem_slot_t *slot = slot_of(id);

    if (slot == NULL) {
        return;
    }
    slot->degraded = 1U;
}
