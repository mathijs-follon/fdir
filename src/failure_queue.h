#ifndef FDIR_FAILURE_QUEUE_INTERNAL_H
#define FDIR_FAILURE_QUEUE_INTERNAL_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FDIR_FAILURE_QUEUE_CAP
#define FDIR_FAILURE_QUEUE_CAP 8
#endif

void fdir_failure_queue_init(void);

/** 0 on success, -1 if full (sets full latch). Caller must hold port sync. */
int fdir_failure_queue_put_unsafe(const fdir_failure_report_t *report);

/** 0 on success, -1 if full (sets full latch). */
int fdir_failure_queue_put(const fdir_failure_report_t *report);

/** Pending entries (for tests). */
uint8_t fdir_failure_queue_pending_count(void);

/** 0 on success, -1 if empty. */
int fdir_failure_queue_get(fdir_failure_report_t *out);

fdir_bool_t fdir_failure_queue_full_latched(void);

#ifdef __cplusplus
}
#endif

#endif /* FDIR_FAILURE_QUEUE_INTERNAL_H */
