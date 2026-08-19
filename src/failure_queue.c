#include "failure_queue.h"
#include "internal.h"

#include <string.h>

static fdir_failure_report_t g_slots[FDIR_FAILURE_QUEUE_CAP];
static uint8_t g_head;
static uint8_t g_tail;
static uint8_t g_count;
static fdir_bool_t g_full_latched;

void fdir_failure_queue_init(void)
{
    memset(g_slots, 0, sizeof(g_slots));
    g_head = 0U;
    g_tail = 0U;
    g_count = 0U;
    g_full_latched = 0U;
}

int fdir_failure_queue_put(const fdir_failure_report_t *report)
{
    if (report == NULL) {
        return -1;
    }

    fdir_port_sync_enter();

    if (g_count >= FDIR_FAILURE_QUEUE_CAP) {
        g_full_latched = 1U;
        fdir_port_sync_exit();
        return -1;
    }

    g_slots[g_tail] = *report;
    g_tail = (uint8_t)((g_tail + 1U) % FDIR_FAILURE_QUEUE_CAP);
    g_count++;

    fdir_port_sync_exit();
    return 0;
}

int fdir_failure_queue_get(fdir_failure_report_t *out)
{
    if (out == NULL) {
        return -1;
    }

    fdir_port_sync_enter();

    if (g_count == 0U) {
        fdir_port_sync_exit();
        return -1;
    }

    *out = g_slots[g_head];
    g_head = (uint8_t)((g_head + 1U) % FDIR_FAILURE_QUEUE_CAP);
    g_count--;

    fdir_port_sync_exit();
    return 0;
}

fdir_bool_t fdir_failure_queue_full_latched(void)
{
    fdir_bool_t latched;

    fdir_port_sync_enter();
    latched = g_full_latched;
    fdir_port_sync_exit();
    return latched;
}
