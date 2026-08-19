#include "subsystem.h"
#include "subsystem_internal.h"
#include "internal.h"
#include <string.h>

typedef struct {
    fdir_subsystem_desc_t desc;
    char                  name[FDIR_NAME_SIZE];
    uint8_t               available;
    uint8_t               degraded;
    uint8_t               used;
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

    fdir_port_sync_enter();

    if (desc == NULL || out_id == NULL) {
        fdir_port_sync_exit();
        return FDIR_ERR_PARAM;
    }
    if (g_sub_count >= FDIR_SUBSYSTEM_CAP) {
        fdir_port_sync_exit();
        return FDIR_ERR_FULL;
    }

    id = g_sub_count++;
    g_subs[id].desc = (fdir_subsystem_desc_t){0};
    fdir_internal_copy_detail(g_subs[id].name, sizeof(g_subs[id].name), desc->name);
    g_subs[id].desc.name = g_subs[id].name;
    g_subs[id].desc.is_critical_path = desc->is_critical_path;
    g_subs[id].desc.on_entity_exhausted = desc->on_entity_exhausted;
    g_subs[id].desc.user = desc->user;
    g_subs[id].used = 1U;
    g_subs[id].available = 1U;
    g_subs[id].degraded = 0U;
    *out_id = id;

    fdir_port_sync_exit();
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

void fdir_subsystem_mark_available_unsafe(fdir_subsystem_id_t id)
{
    fdir_subsystem_slot_t *slot = slot_of(id);

    if (slot != NULL) {
        slot->available = 1U;
        slot->degraded = 0U;
    }
}

void fdir_subsystem_mark_unavailable_unsafe(fdir_subsystem_id_t id)
{
    fdir_subsystem_slot_t *slot = slot_of(id);

    if (slot != NULL) {
        slot->available = 0U;
        slot->degraded = 1U;
    }
}

void fdir_subsystem_mark_degraded_unsafe(fdir_subsystem_id_t id)
{
    fdir_subsystem_slot_t *slot = slot_of(id);

    if (slot != NULL) {
        slot->degraded = 1U;
    }
}

void fdir_subsystem_mark_available(fdir_subsystem_id_t id)
{
    fdir_port_sync_enter();
    if (slot_of(id) != NULL) {
        fdir_subsystem_mark_available_unsafe(id);
        fdir_internal_emit_subsystem_state(id, FDIR_ENTITY_NONE, FDIR_REASON_USER, 0U,
                                           FDIR_ANOMALY_CLEARED, "available");
    }
    fdir_port_sync_exit();
}

void fdir_subsystem_mark_unavailable(fdir_subsystem_id_t id)
{
    fdir_port_sync_enter();
    if (slot_of(id) != NULL) {
        fdir_subsystem_mark_unavailable_unsafe(id);
        fdir_internal_emit_subsystem_state(id, FDIR_ENTITY_NONE, FDIR_REASON_USER, 0U,
                                           FDIR_ANOMALY_RAISED, "unavailable");
    }
    fdir_port_sync_exit();
}

void fdir_subsystem_mark_degraded(fdir_subsystem_id_t id)
{
    fdir_port_sync_enter();
    if (slot_of(id) != NULL) {
        fdir_subsystem_mark_degraded_unsafe(id);
        fdir_internal_emit_subsystem_state(id, FDIR_ENTITY_NONE, FDIR_REASON_USER, 0U,
                                           FDIR_ANOMALY_RAISED, "degraded");
    }
    fdir_port_sync_exit();
}

fdir_action_t fdir_subsystem_on_entity_exhausted(fdir_subsystem_id_t sub, fdir_entity_id_t entity,
                                                 const fdir_failure_report_t *report)
{
    const fdir_subsystem_slot_t *slot = slot_of(sub);

    if (slot == NULL || slot->desc.on_entity_exhausted == NULL) {
        return FDIR_ACTION_NONE;
    }
    return slot->desc.on_entity_exhausted(sub, entity, report, slot->desc.user);
}
