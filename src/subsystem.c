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
