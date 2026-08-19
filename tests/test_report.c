#include "test_harness.h"
#include "test_port.h"

#include "fdir.h"
#include "failure_queue.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t g_now_ms;
static int g_restart_calls;

static uint32_t port_now(void)
{
    return g_now_ms;
}

static void port_emit(const fdir_event_t *event)
{
    (void)event;
}

static void port_reboot(const char *reason)
{
    (void)reason;
}

static int sensor_restart(fdir_entity_id_t id, void *user)
{
    (void)id;
    (void)user;
    g_restart_calls++;
    return 0;
}

static fdir_port_t report_test_port(void)
{
    fdir_port_t port = {
        .get_now_ms     = port_now,
        .emit_event     = port_emit,
        .request_reboot = port_reboot,
    };
    return port;
}

static fdir_status_t init_report_test(void)
{
    fdir_config_t cfg = fdir_config_default();
    fdir_port_t port = report_test_port();

    g_now_ms = 0U;
    g_restart_calls = 0;
    return fdir_init(&cfg, &port);
}

static fdir_entity_id_t register_entity_named(const char *name)
{
    fdir_entity_desc_t desc;
    fdir_entity_id_t id;

    memset(&desc, 0, sizeof(desc));
    desc.name = name;
    desc.max_restarts = 1U;
    desc.max_watchdog_restarts = 1U;
    desc.on_exhausted = FDIR_ACTION_DEGRADE;
    desc.linked_subsystem = FDIR_SUBSYSTEM_NONE;
    desc.restart = sensor_restart;
    desc.decide = NULL;
    desc.user = NULL;

    if (fdir_entity_register(&desc, &id) != FDIR_OK) {
        fprintf(stderr, "  FAIL %s:%d: fdir_entity_register failed\n", __FILE__, __LINE__);
        g_tests_failed++;
        return 0;
    }
    return id;
}

static fdir_entity_id_t setup_entity(void)
{
    if (init_report_test() != FDIR_OK) {
        fprintf(stderr, "  FAIL %s:%d: fdir_init failed\n", __FILE__, __LINE__);
        g_tests_failed++;
        return 0;
    }
    return register_entity_named("sensor");
}

TEST(test_report_fault_enqueues_for_supervisor)
{
    fdir_entity_id_t id = setup_entity();

    g_now_ms = 1234U;
    fdir_report_fault(id, FDIR_REASON_IO_ERROR, 55U, "disk");

    ASSERT_EQ_INT(g_restart_calls, 0);

    fdir_supervisor_tick();

    ASSERT_EQ_INT(g_restart_calls, 1);
}

TEST(test_report_fault_marks_health_failed)
{
    fdir_entity_id_t id = setup_entity();
    const fdir_health_snapshot_t *snapshot;

    fdir_report_fault(id, FDIR_REASON_TIMEOUT, 7U, "late");

    snapshot = fdir_health_snapshot(id);
    ASSERT(snapshot != NULL);
    ASSERT_EQ_INT(snapshot->health, FDIR_HEALTH_FAILED);
    ASSERT_EQ_INT(snapshot->error_code, 7U);
    ASSERT_STR_EQ(snapshot->detail, "late");
}

TEST(test_entity_may_run_false_after_report_fault)
{
    fdir_entity_id_t id = setup_entity();

    fdir_report_fault(id, FDIR_REASON_USER, 0U, "stop");

    ASSERT_EQ_INT(fdir_entity_may_run(id), 0);
}

TEST(test_critical_fault_returns_busy_when_queue_full)
{
    fdir_entity_id_t ids[FDIR_FAILURE_QUEUE_CAP + 1U];
    fdir_entity_id_t overflow_id;
    fdir_status_t status;
    const fdir_health_snapshot_t *snapshot;
    unsigned i;

    if (init_report_test() != FDIR_OK) {
        fprintf(stderr, "  FAIL %s:%d: fdir_init failed\n", __FILE__, __LINE__);
        g_tests_failed++;
        return;
    }

    for (i = 0U; i < FDIR_FAILURE_QUEUE_CAP; i++) {
        char name[16];
        (void)snprintf(name, sizeof(name), "e%u", i);
        ids[i] = register_entity_named(name);
        status = fdir_report_fault(ids[i], FDIR_REASON_IO_ERROR, (uint16_t)i, "full");
        ASSERT_EQ_INT(status, FDIR_OK);
    }

    overflow_id = register_entity_named("overflow");
    status = fdir_report_fault(overflow_id, FDIR_REASON_IO_ERROR, 99U, "busy");
    ASSERT_EQ_INT(status, FDIR_ERR_BUSY);
    ASSERT_EQ_INT(fdir_failure_queue_full_latched(), 1);

    snapshot = fdir_health_snapshot(overflow_id);
    ASSERT(snapshot != NULL);
    ASSERT_EQ_INT(snapshot->health, FDIR_HEALTH_OK);
}

TEST(test_report_fault_retry_after_supervisor_drain)
{
    fdir_entity_id_t ids[FDIR_FAILURE_QUEUE_CAP + 1U];
    fdir_entity_id_t overflow_id;
    fdir_status_t status;
    const fdir_health_snapshot_t *snapshot;
    unsigned i;

    if (init_report_test() != FDIR_OK) {
        fprintf(stderr, "  FAIL %s:%d: fdir_init failed\n", __FILE__, __LINE__);
        g_tests_failed++;
        return;
    }

    for (i = 0U; i < FDIR_FAILURE_QUEUE_CAP; i++) {
        char name[16];
        (void)snprintf(name, sizeof(name), "r%u", i);
        ids[i] = register_entity_named(name);
        status = fdir_report_fault(ids[i], FDIR_REASON_IO_ERROR, (uint16_t)i, "full");
        ASSERT_EQ_INT(status, FDIR_OK);
    }

    overflow_id = register_entity_named("retry");
    ASSERT_EQ_INT(fdir_report_fault(overflow_id, FDIR_REASON_IO_ERROR, 99U, "busy"), FDIR_ERR_BUSY);

    fdir_supervisor_tick();

    status = fdir_report_fault(overflow_id, FDIR_REASON_IO_ERROR, 99U, "retry");
    ASSERT_EQ_INT(status, FDIR_OK);

    snapshot = fdir_health_snapshot(overflow_id);
    ASSERT(snapshot != NULL);
    ASSERT_EQ_INT(snapshot->health, FDIR_HEALTH_FAILED);
}

TEST(test_submit_failure_enqueues_without_changing_health)
{
    fdir_entity_id_t id = setup_entity();
    fdir_failure_report_t report;
    fdir_status_t status;
    const fdir_health_snapshot_t *snapshot;

    memset(&report, 0, sizeof(report));
    report.entity = id;
    report.reason = FDIR_REASON_IO_ERROR;
    report.error_code = 12U;
    report.timestamp_ms = 500U;
    report.flags = FDIR_FAULT_FLAG_CRITICAL;
    (void)strncpy(report.detail, "manual", sizeof(report.detail) - 1U);

    status = fdir_submit_failure(&report);
    ASSERT_EQ_INT(status, FDIR_OK);

    snapshot = fdir_health_snapshot(id);
    ASSERT(snapshot != NULL);
    ASSERT_EQ_INT(snapshot->health, FDIR_HEALTH_OK);
    ASSERT_EQ_INT(fdir_entity_may_run(id), 1);

    fdir_supervisor_tick();

    ASSERT_EQ_INT(g_restart_calls, 1);
    snapshot = fdir_health_snapshot(id);
    ASSERT(snapshot != NULL);
    ASSERT_EQ_INT(snapshot->health, FDIR_HEALTH_OK);
}

TEST(test_submit_failure_null_param)
{
    ASSERT_EQ_INT(fdir_submit_failure(NULL), FDIR_ERR_PARAM);
}

TEST(test_submit_failure_supervision_disabled)
{
    fdir_entity_id_t id = setup_entity();
    fdir_failure_report_t report;

    memset(&report, 0, sizeof(report));
    report.entity = id;
    report.reason = FDIR_REASON_IO_ERROR;
    report.flags = FDIR_FAULT_FLAG_CRITICAL;

    (void)fdir_set_supervision_enabled(0U);
    ASSERT_EQ_INT(fdir_submit_failure(&report), FDIR_OK);
    fdir_supervisor_tick();
    ASSERT_EQ_INT(g_restart_calls, 0);
    (void)fdir_set_supervision_enabled(1U);
}

TEST(test_submit_failure_noncritical_queue_full)
{
    fdir_entity_id_t ids[FDIR_FAILURE_QUEUE_CAP];
    fdir_failure_report_t report;
    fdir_status_t status;
    unsigned i;

    if (init_report_test() != FDIR_OK) {
        g_tests_failed++;
        return;
    }

    for (i = 0U; i < FDIR_FAILURE_QUEUE_CAP; i++) {
        char name[16];
        (void)snprintf(name, sizeof(name), "q%u", i);
        ids[i] = register_entity_named(name);
        status = fdir_report_fault(ids[i], FDIR_REASON_IO_ERROR, (uint16_t)i, "fill");
        ASSERT_EQ_INT(status, FDIR_OK);
    }

    memset(&report, 0, sizeof(report));
    report.entity = ids[0];
    report.reason = FDIR_REASON_USER;
    report.error_code = 1U;
    report.flags = 0U;
    report.detail[0] = 'n';
    report.detail[1] = '\0';

    status = fdir_submit_failure(&report);
    ASSERT_EQ_INT(status, FDIR_ERR_STATE);
}

TEST(test_report_fault_skips_when_already_failed)
{
    fdir_entity_id_t id = setup_entity();
    fdir_status_t status;

    ASSERT_EQ_INT(fdir_report_fault(id, FDIR_REASON_IO_ERROR, 1U, "first"), FDIR_OK);
    status = fdir_report_fault(id, FDIR_REASON_TIMEOUT, 2U, "second");
    ASSERT_EQ_INT(status, FDIR_OK);
    ASSERT_EQ_INT(fdir_health_snapshot(id)->health, FDIR_HEALTH_FAILED);
}

TEST(test_report_fault_skips_when_latched)
{
    fdir_entity_id_t id = setup_entity();

    ASSERT_EQ_INT(fdir_report_fault(id, FDIR_REASON_IO_ERROR, 1U, "first"), FDIR_OK);
    fdir_supervisor_tick();
    ASSERT_EQ_INT(g_restart_calls, 1);

    ASSERT_EQ_INT(fdir_report_fault(id, FDIR_REASON_IO_ERROR, 2U, "latched"), FDIR_OK);
    ASSERT_EQ_INT(g_restart_calls, 1);
}

TEST(test_report_fault_no_duplicate_enqueue_before_supervisor)
{
    fdir_entity_id_t id = setup_entity();

    ASSERT_EQ_INT(fdir_report_fault(id, FDIR_REASON_IO_ERROR, 1U, "first"), FDIR_OK);
    ASSERT_EQ_INT(fdir_failure_queue_pending_count(), 1U);
    ASSERT_EQ_INT(fdir_report_fault(id, FDIR_REASON_IO_ERROR, 2U, "dup"), FDIR_OK);
    ASSERT_EQ_INT(fdir_failure_queue_pending_count(), 1U);
}

int main(void)
{
    RUN(test_report_fault_enqueues_for_supervisor);
    RUN(test_report_fault_marks_health_failed);
    RUN(test_entity_may_run_false_after_report_fault);
    RUN(test_critical_fault_returns_busy_when_queue_full);
    RUN(test_report_fault_retry_after_supervisor_drain);
    RUN(test_submit_failure_enqueues_without_changing_health);
    RUN(test_submit_failure_null_param);
    RUN(test_submit_failure_supervision_disabled);
    RUN(test_submit_failure_noncritical_queue_full);
    RUN(test_report_fault_skips_when_already_failed);
    RUN(test_report_fault_skips_when_latched);
    RUN(test_report_fault_no_duplicate_enqueue_before_supervisor);
    return test_harness_summary();
}
