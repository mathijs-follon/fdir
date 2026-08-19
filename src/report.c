#include "report.h"
#include "failure_queue.h"
#include "health.h"
#include "health_internal.h"
#include "internal.h"
#include "port.h"
#include "recovery.h"

#include <string.h>

static fdir_status_t enqueue_failure_unsafe(const fdir_failure_report_t *report)
{
    if (fdir_failure_queue_put_unsafe(report) != 0) {
        if ((report->flags & FDIR_FAULT_FLAG_CRITICAL) != 0U) {
            return FDIR_ERR_BUSY;
        }
        return FDIR_ERR_STATE;
    }
    return FDIR_OK;
}

fdir_status_t fdir_submit_failure(const fdir_failure_report_t *report)
{
    if (report == NULL) {
        return FDIR_ERR_PARAM;
    }
    if (fdir_supervision_enabled() == 0U) {
        return FDIR_OK;
    }
    if (fdir_failure_queue_put(report) != 0) {
        if ((report->flags & FDIR_FAULT_FLAG_CRITICAL) != 0U) {
            return FDIR_ERR_BUSY;
        }
        return FDIR_ERR_STATE;
    }
    return FDIR_OK;
}

static fdir_status_t report_fault_impl(fdir_entity_id_t entity, fdir_reason_t reason,
                                       uint16_t error_code, const char *detail, uint8_t flags)
{
    fdir_failure_report_t report;
    const fdir_health_snapshot_t *snapshot;
    fdir_status_t status;

    fdir_port_sync_enter();

    if (fdir_supervision_enabled_unsafe() == 0U) {
        fdir_port_sync_exit();
        return FDIR_OK;
    }

    if (fdir_health_fault_is_latched_unsafe(entity, reason)) {
        fdir_port_sync_exit();
        return FDIR_OK;
    }

    snapshot = fdir_health_snapshot(entity);
    if (snapshot != NULL && snapshot->health == FDIR_HEALTH_FAILED) {
        fdir_port_sync_exit();
        return FDIR_OK;
    }

    memset(&report, 0, sizeof(report));
    report.entity = entity;
    report.reason = reason;
    report.error_code = error_code;
    report.timestamp_ms = fdir_get_now_ms();
    report.flags = flags;
    fdir_internal_copy_detail(report.detail, sizeof(report.detail), detail);

    status = enqueue_failure_unsafe(&report);
    if (status != FDIR_OK) {
        fdir_port_sync_exit();
        return status;
    }

    fdir_health_set_unsafe(entity, FDIR_HEALTH_FAILED, error_code, detail);
    fdir_port_sync_exit();
    return FDIR_OK;
}

fdir_status_t fdir_report_fault(fdir_entity_id_t entity, fdir_reason_t reason,
                                uint16_t error_code, const char *detail)
{
    return report_fault_impl(entity, reason, error_code, detail, FDIR_FAULT_FLAG_CRITICAL);
}

fdir_status_t fdir_report_fault_ex(fdir_entity_id_t entity, fdir_reason_t reason,
                                   uint16_t error_code, const char *detail, uint8_t flags)
{
    return report_fault_impl(entity, reason, error_code, detail, flags);
}
