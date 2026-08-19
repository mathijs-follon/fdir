/*
 * Strong overrides for the fdir weak port hooks, wired to FreeRTOS primitives.
 */
#include "fdir.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include <stdio.h>

extern QueueHandle_t g_failure_queue;

uint32_t fdir_get_now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

int fdir_submit_failure(const fdir_failure_report_t *report)
{
    if (xQueueSend(g_failure_queue, report, 0) == pdTRUE) {
        return 0;
    }
    return -1;
}

void fdir_isolate_current_worker(void)
{
    vTaskSuspend(NULL);
}

void fdir_emit_event(const fdir_event_t *event)
{
    static const char *const kind_names[] = {
        "FAILURE", "MODE_CHANGE", "RESTART", "WATCHDOG", "NOTE", "QUEUE_OVF",
    };
    const char *kind = (event->kind < 6U) ? kind_names[event->kind] : "?";
    printf("[fdir] %-12s mode=%u entity=%u reason=%u \"%s\"\n",
           kind, (unsigned)event->mode, (unsigned)event->entity,
           (unsigned)event->reason, event->detail);
}

void fdir_request_reboot(const char *reason)
{
    printf("[fdir] REBOOT requested: %s\n", reason != NULL ? reason : "(null)");
    vTaskEndScheduler();
}

void vAssertCalled(const char *file, unsigned long line)
{
    printf("ASSERT failed: %s:%lu\n", file, line);
    vTaskEndScheduler();
}
