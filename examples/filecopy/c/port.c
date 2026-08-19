#include "fdir.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


#define FPORT_QUEUE_CAP 256

static struct {
    fdir_failure_report_t slots[FPORT_QUEUE_CAP];
    int                   head;
    int                   tail;
    int                   count;
    pthread_mutex_t       lock;
    pthread_cond_t        not_empty;
} g_fqueue;

void fport_queue_init(void)
{
    memset(&g_fqueue, 0, sizeof(g_fqueue));
    pthread_mutex_init(&g_fqueue.lock, NULL);
    pthread_cond_init(&g_fqueue.not_empty, NULL);
}

/* drain one report into *out; returns 1 if got one, 0 if empty */
int fport_queue_pop(fdir_failure_report_t *out)
{
    pthread_mutex_lock(&g_fqueue.lock);
    if (g_fqueue.count == 0) {
        pthread_mutex_unlock(&g_fqueue.lock);
        return 0;
    }
    *out = g_fqueue.slots[g_fqueue.head];
    g_fqueue.head = (g_fqueue.head + 1) % FPORT_QUEUE_CAP;
    g_fqueue.count--;
    pthread_mutex_unlock(&g_fqueue.lock);
    return 1;
}

uint32_t fdir_get_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

int fdir_submit_failure(const fdir_failure_report_t *report)
{
    pthread_mutex_lock(&g_fqueue.lock);
    if (g_fqueue.count == FPORT_QUEUE_CAP) {
        pthread_mutex_unlock(&g_fqueue.lock);
        return -1;
    }
    g_fqueue.slots[g_fqueue.tail] = *report;
    g_fqueue.tail = (g_fqueue.tail + 1) % FPORT_QUEUE_CAP;
    g_fqueue.count++;
    pthread_cond_signal(&g_fqueue.not_empty);
    pthread_mutex_unlock(&g_fqueue.lock);
    return 0;
}

void fdir_isolate_current_worker(void)
{
    pthread_exit(NULL);
}

void fdir_emit_event(const fdir_event_t *event)
{
    static const char *const modes[]   = { "NOMINAL", "DEGRADED", "SAFE", "REBOOT_PENDING" };
    static const char *const kinds[]   = { "FAILURE", "MODE_CHANGE", "RESTART", "WATCHDOG", "NOTE", "QUEUE_OVF" };
    const char *mode = (event->mode < 4U) ? modes[event->mode] : "?";
    const char *kind = (event->kind < 6U) ? kinds[event->kind] : "?";
    fprintf(stderr, "[fdir] %-12s mode=%-16s entity=%u \"%s\"\n",
            kind, mode, (unsigned)event->entity, event->detail);
}

void fdir_request_reboot(const char *reason)
{
    fprintf(stderr, "[fdir] fatal: %s\n", reason != NULL ? reason : "(null)");
    /* no hardware reset available; exit with failure so the caller notices */
    _exit(1);
}
