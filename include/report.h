#ifndef FDIR_REPORT_H
#define FDIR_REPORT_H

#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Enqueue a failure report for the supervisor. Does not change entity health.
 * Prefer fdir_report_fault() from worker context.
 * Returns FDIR_ERR_BUSY when the internal queue is full and the report is critical.
 */
fdir_status_t fdir_submit_failure(const fdir_failure_report_t *report);

/**
 * Mark entity failed and enqueue for the supervisor (call fdir_supervisor_tick).
 * Returns FDIR_ERR_BUSY when the internal queue rejects a critical report.
 */
fdir_status_t fdir_report_fault(fdir_entity_id_t entity, fdir_reason_t reason,
                                uint16_t error_code, const char *detail);

fdir_status_t fdir_report_fault_ex(fdir_entity_id_t entity, fdir_reason_t reason,
                                   uint16_t error_code, const char *detail,
                                   uint8_t flags);

/** Non-zero if any enqueue failed due to a full internal failure queue. */
fdir_bool_t fdir_failure_queue_full_latched(void);

#ifdef __cplusplus
}
#endif

#endif /* FDIR_REPORT_H */
