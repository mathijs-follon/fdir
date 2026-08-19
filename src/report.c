#include "report.h"
#include "health.h"
#include "internal.h"
#include "port.h"

#include <string.h>

void fdir_report_fault(fdir_entity_id_t entity, fdir_reason_t reason, uint16_t error_code, const char *detail)
{
    fdir_failure_report_t report;

    memset(&report, 0, sizeof(report));
    report.entity = entity;
    report.reason = reason;
    report.error_code = error_code;
    report.timestamp_ms = fdir_get_now_ms();
    fdir_internal_copy_detail(report.detail, sizeof(report.detail), detail);

    fdir_health_set(entity, FDIR_HEALTH_FAILED, error_code, detail);


    (void)fdir_submit_failure(&report);

    fdir_isolate_current_worker();
}

void fdir_pause_if_failed(fdir_entity_id_t entity)
{
    const fdir_health_snapshot_t *snapshot = fdir_health_snapshot(entity);

    if (snapshot != NULL && snapshot->health == FDIR_HEALTH_FAILED) {
        fdir_isolate_current_worker();
    }
}
