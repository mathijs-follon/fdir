#include "fdir.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t g_fdir_mutex = PTHREAD_MUTEX_INITIALIZER;

static void port_lock(void)
{
    (void)pthread_mutex_lock(&g_fdir_mutex);
}

static void port_unlock(void)
{
    (void)pthread_mutex_unlock(&g_fdir_mutex);
}

static uint32_t port_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static void port_emit(const fdir_event_t *event)
{
    static const char *const modes[] = { "NOMINAL", "DEGRADED", "SAFE", "REBOOT_PENDING" };
    static const char *const kinds[] = { "FAILURE", "MODE_CHANGE", "RESTART", "WATCHDOG", "NOTE", "QUEUE_OVF" };
    const char *mode = (event->mode < 4U) ? modes[event->mode] : "?";
    const char *kind = (event->kind < 6U) ? kinds[event->kind] : "?";
    fprintf(stderr, "[fdir] %-12s mode=%-16s entity=%u \"%s\"\n",
            kind, mode, (unsigned)event->entity, event->detail);
}

static void port_reboot(const char *reason)
{
    fprintf(stderr, "[fdir] fatal: %s\n", reason != NULL ? reason : "(null)");
    _exit(1);
}

fdir_port_t fdir_app_port(void)
{
    fdir_port_t port = {
        .get_now_ms     = port_now,
        .emit_event     = port_emit,
        .request_reboot = port_reboot,
        .lock           = port_lock,
        .unlock         = port_unlock,
    };
    return port;
}
