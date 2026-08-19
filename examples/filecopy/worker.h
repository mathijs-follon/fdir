#ifndef FILECOPY_WORKER_H
#define FILECOPY_WORKER_H

#include "fdir.h"

#include <pthread.h>
#include <stddef.h>

#define WORKER_MAX       16
#define JOB_PATH_MAX     4096
#define JOB_QUEUE_CAP    4096

typedef struct {
    char src[JOB_PATH_MAX];
    char dst[JOB_PATH_MAX];
} copy_job_t;

/*
 * Shared job queue. The tree walker pushes jobs; workers pop them.
 * Marked done when the walker has finished and the queue is drained.
 */
typedef struct {
    copy_job_t  jobs[JOB_QUEUE_CAP];
    int         head;
    int         tail;
    int         count;
    int         producer_done; /* set by walker when tree walk is complete */
    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
} job_queue_t;

/* per-worker context */
typedef struct {
    int            index;
    fdir_entity_id_t entity;
    pthread_t      thread;
    job_queue_t   *queue;
    int            active; /* set to 0 when the worker exits */
} worker_ctx_t;

void job_queue_init(job_queue_t *q);
void job_queue_push(job_queue_t *q, const copy_job_t *job);
int  job_queue_pop(job_queue_t *q, copy_job_t *out);   /* returns 0 if drained */
void job_queue_finish(job_queue_t *q);

int  worker_start(worker_ctx_t *ctx);
void worker_wait_all(worker_ctx_t *workers, int n);

#endif /* FILECOPY_WORKER_H */
