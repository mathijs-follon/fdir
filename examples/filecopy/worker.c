#include "worker.h"
#include "fdir.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define COPY_BUF_SIZE (256 * 1024)

void job_queue_init(job_queue_t *q)
{
    memset(q, 0, sizeof(*q));
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void job_queue_push(job_queue_t *q, const copy_job_t *job)
{
    pthread_mutex_lock(&q->lock);
    while (q->count == JOB_QUEUE_CAP) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    q->jobs[q->tail] = *job;
    q->tail = (q->tail + 1) % JOB_QUEUE_CAP;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

/* returns 1 if a job was returned, 0 if queue is drained and producer done */
int job_queue_pop(job_queue_t *q, copy_job_t *out)
{
    pthread_mutex_lock(&q->lock);
    while (q->count == 0 && !q->producer_done) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    if (q->count == 0) {
        pthread_cond_broadcast(&q->not_empty); /* wake other waiting workers */
        pthread_mutex_unlock(&q->lock);
        return 0;
    }
    *out = q->jobs[q->head];
    q->head = (q->head + 1) % JOB_QUEUE_CAP;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return 1;
}

void job_queue_finish(job_queue_t *q)
{
    pthread_mutex_lock(&q->lock);
    q->producer_done = 1;
    pthread_cond_broadcast(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

static int copy_file(const char *src, const char *dst)
{
    static __thread char buf[COPY_BUF_SIZE];

    int src_fd = open(src, O_RDONLY);
    if (src_fd < 0) {
        return -1;
    }

    struct stat st;
    if (fstat(src_fd, &st) < 0) {
        close(src_fd);
        return -1;
    }

    int dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
    if (dst_fd < 0) {
        close(src_fd);
        return -1;
    }

    ssize_t n;
    while ((n = read(src_fd, buf, sizeof(buf))) > 0) {
        const char *p = buf;
        while (n > 0) {
            ssize_t w = write(dst_fd, p, (size_t)n);
            if (w < 0) {
                close(src_fd);
                close(dst_fd);
                return -1;
            }
            p += w;
            n -= w;
        }
    }

    close(src_fd);
    close(dst_fd);
    return (n < 0) ? -1 : 0;
}

static void *worker_thread(void *arg)
{
    worker_ctx_t *ctx = arg;
    copy_job_t job;

    for (;;) {
        if (!fdir_worker_may_run(ctx->entity)) {
            usleep(200000);
            continue;
        }

        if (!job_queue_pop(ctx->queue, &job)) {
            break;
        }

        if (copy_file(job.src, job.dst) != 0) {
            fdir_report_fault(ctx->entity, FDIR_REASON_IO_ERROR,
                              (uint16_t)errno, job.src);
            break;
        }

        fdir_health_heartbeat_notify(ctx->entity);
    }

    ctx->active = 0;
    return NULL;
}

int worker_start(worker_ctx_t *ctx)
{
    ctx->active = 1;
    return pthread_create(&ctx->thread, NULL, worker_thread, ctx);
}

void worker_wait_all(worker_ctx_t *workers, int n)
{
    for (int i = 0; i < n; i++) {
        if (workers[i].active) {
            pthread_join(workers[i].thread, NULL);
        }
    }
}
