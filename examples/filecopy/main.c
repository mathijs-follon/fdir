/*
 * fcopy - parallel directory copy with fdir-based fault recovery
 *
 * Usage: fcopy <src> <dst> [--workers N]
 *
 * Each worker thread copies files from the job queue and heartbeats fdir.
 * On I/O failure fdir handles restart/degrade/safe escalation automatically.
 */

#include "fdir.h"
#include "worker.h"

#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void fport_queue_init(void);
int  fport_queue_pop(fdir_failure_report_t *out);


static job_queue_t  g_queue;
static worker_ctx_t g_workers[WORKER_MAX];
static int          g_nworkers = 4;

static char g_src_root[JOB_PATH_MAX];
static char g_dst_root[JOB_PATH_MAX];

static volatile int g_running = 1;

static int worker_restart_cb(fdir_entity_id_t id, void *user)
{
    worker_ctx_t *ctx = user;
    (void)id;
    fprintf(stderr, "[fcopy] restarting worker %d\n", ctx->index);
    /* join the old thread (it has exited via fdir_isolate_current_worker) */
    pthread_join(ctx->thread, NULL);
    return worker_start(ctx);
}

static int walk_cb(const char *fpath, const struct stat *sb, int typeflag,
                   struct FTW *ftwbuf)
{
    (void)sb;
    (void)ftwbuf;

    if (typeflag != FTW_F) {
        /* directory: create it in dst tree */
        if (typeflag == FTW_D) {
            const char *rel = fpath + strlen(g_src_root);
            char dst[JOB_PATH_MAX];
            snprintf(dst, sizeof(dst), "%s%s", g_dst_root, rel);
            mkdir(dst, 0755);
        }
        return 0;
    }

    const char *rel = fpath + strlen(g_src_root);
    copy_job_t job;
    snprintf(job.src, sizeof(job.src), "%s", fpath);
    snprintf(job.dst, sizeof(job.dst), "%s%s", g_dst_root, rel);
    job_queue_push(&g_queue, &job);
    return 0;
}

static void *supervisor_thread(void *arg)
{
    (void)arg;
    const struct timespec interval = { .tv_sec = 0, .tv_nsec = 200 * 1000 * 1000 };

    while (g_running) {
        fdir_failure_report_t report;
        while (fport_queue_pop(&report)) {
            fdir_handle_failure(&report);
        }
        fdir_check_watchdogs();

        if (fdir_system_mode() >= FDIR_MODE_SAFE) {
            fprintf(stderr, "[fcopy] system entered SAFE mode, aborting\n");
            g_running = 0;
            job_queue_finish(&g_queue);
            break;
        }

        nanosleep(&interval, NULL);
    }
    return NULL;
}

static void usage(const char *argv0)
{
    fprintf(stderr, "usage: %s <src> <dst> [--workers N]\n", argv0);
}

static int parse_args(int argc, char **argv)
{
    if (argc < 3) {
        usage(argv[0]);
        return -1;
    }
    strncpy(g_src_root, argv[1], JOB_PATH_MAX - 1);
    strncpy(g_dst_root, argv[2], JOB_PATH_MAX - 1);

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--workers") == 0 && i + 1 < argc) {
            g_nworkers = atoi(argv[++i]);
            if (g_nworkers < 1 || g_nworkers > WORKER_MAX) {
                fprintf(stderr, "workers must be 1..%d\n", WORKER_MAX);
                return -1;
            }
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return -1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (parse_args(argc, argv) != 0) {
        return 1;
    }

    /* strip trailing slash from src so relative paths work correctly */
    size_t slen = strlen(g_src_root);
    if (slen > 1 && g_src_root[slen - 1] == '/') {
        g_src_root[slen - 1] = '\0';
    }

    struct stat st;
    if (stat(g_src_root, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "fcopy: '%s' is not a directory\n", g_src_root);
        return 1;
    }

    if (mkdir(g_dst_root, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "fcopy: cannot create '%s': %s\n", g_dst_root, strerror(errno));
        return 1;
    }

    fport_queue_init();
    job_queue_init(&g_queue);

    fdir_config_t cfg = fdir_config_default();
    cfg.health_check_period_ms            = 2000;
    cfg.missed_heartbeat_tolerance        = 3;
    cfg.safe_mode_critical_failure_threshold = (uint8_t)(g_nworkers);

    if (fdir_init(&cfg) != FDIR_OK) {
        fprintf(stderr, "fcopy: fdir_init failed\n");
        return 1;
    }

    for (int i = 0; i < g_nworkers; i++) {
        g_workers[i].index = i;
        g_workers[i].queue = &g_queue;

        fdir_entity_desc_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.name                  = "worker";
        desc.max_restarts          = 2;
        desc.max_watchdog_restarts = 1;
        desc.on_exhausted          = FDIR_ACTION_UNAVAILABLE;
        desc.linked_subsystem      = FDIR_SUBSYSTEM_NONE;
        desc.restart               = worker_restart_cb;
        desc.user                  = &g_workers[i];

        if (fdir_entity_register(&desc, &g_workers[i].entity) != FDIR_OK) {
            fprintf(stderr, "fcopy: fdir_entity_register failed for worker %d\n", i);
            return 1;
        }

        fdir_health_heartbeat_notify(g_workers[i].entity);

        if (worker_start(&g_workers[i]) != 0) {
            fprintf(stderr, "fcopy: failed to start worker %d\n", i);
            return 1;
        }
    }

    pthread_t sup;
    pthread_create(&sup, NULL, supervisor_thread, NULL);

    nftw(g_src_root, walk_cb, 64, FTW_PHYS);
    job_queue_finish(&g_queue);

    worker_wait_all(g_workers, g_nworkers);

    g_running = 0;
    pthread_join(sup, NULL);

    fdir_mode_t mode = fdir_system_mode();
    if (mode >= FDIR_MODE_SAFE) {
        fprintf(stderr, "fcopy: completed in degraded state (mode=%u)\n", (unsigned)mode);
        return 1;
    }

    printf("fcopy: done\n");
    return 0;
}
