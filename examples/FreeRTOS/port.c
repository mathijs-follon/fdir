/*
 * Platform port hooks wired to FreeRTOS primitives.
 */
#include "fdir.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

static uint32_t port_now(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void port_emit(const fdir_event_t *event)
{
    static const char *const kind_names[] = {
        "FAILURE", "MODE_CHANGE", "RESTART", "WATCHDOG", "NOTE", "QUEUE_OVF",
    };
    const char *kind = (event->kind < 6U) ? kind_names[event->kind] : "?";
    printf("[fdir] %-12s mode=%u entity=%u reason=%u \"%s\"\n",
           kind, (unsigned)event->mode, (unsigned)event->entity,
           (unsigned)event->reason, event->detail);
}

static void port_reboot(const char *reason)
{
    printf("[fdir] REBOOT requested: %s\n", reason != NULL ? reason : "(null)");
    vTaskEndScheduler();
}

fdir_port_t fdir_app_port(void)
{
    fdir_port_t port = {
        .get_now_ms     = port_now,
        .emit_event     = port_emit,
        .request_reboot = port_reboot,
    };
    return port;
}

void vAssertCalled(const char *file, unsigned long line)
{
    printf("ASSERT failed: %s:%lu\n", file, line);
    vTaskEndScheduler();
}
