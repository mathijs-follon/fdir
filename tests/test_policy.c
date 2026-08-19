#include "test_harness.h"

#include "fdir.h"

#include <stdint.h>
#include <string.h>

static uint32_t g_now_ms;
static int g_restart_calls;
static int g_event_calls;
static fdir_event_t g_last_event;
static fdir_event_t g_failure_event;
static int g_has_failure;

static uint32_t port_now(void)
{
    return g_now_ms;
}

static void port_emit(const fdir_event_t *event)
{
    if (event->kind == FDIR_EVENT_FAILURE || event->kind == FDIR_EVENT_WATCHDOG) {
        g_failure_event = *event;
        g_has_failure = 1;
    }
    g_last_event = *event;
    g_event_calls++;
}

static void port_reboot(const char *reason)
{
    (void)reason;
}

static fdir_port_t policy_test_port(void)
{
    fdir_port_t port = {
        .get_now_ms     = port_now,
        .emit_event     = port_emit,
        .request_reboot = port_reboot,
    };
    return port;
}

static int restart_ok(fdir_entity_id_t id, void *user)
{
    (void)id;
    (void)user;
    g_restart_calls++;
    return 0;
}

static fdir_entity_id_t setup_entity(uint8_t max_restarts)
{
    fdir_config_t cfg = fdir_config_default();
    fdir_entity_desc_t desc;
    fdir_entity_id_t id;

    fdir_port_t port = policy_test_port();

    g_now_ms = 0U;
    g_restart_calls = 0;
    g_event_calls = 0;
    g_has_failure = 0;

    (void)fdir_init(&cfg, &port);

    desc.name = "worker";
    desc.max_restarts = max_restarts;
    desc.max_watchdog_restarts = 1U;
    desc.on_exhausted = FDIR_ACTION_DEGRADE;
    desc.linked_subsystem = FDIR_SUBSYSTEM_NONE;
    desc.restart = restart_ok;
    desc.decide = NULL;
    desc.user = NULL;
    (void)fdir_entity_register(&desc, &id);
    return id;
}

static fdir_failure_report_t failure_of(fdir_entity_id_t id, fdir_reason_t reason)
{
    fdir_failure_report_t report;

    memset(&report, 0, sizeof(report));
    report.entity = id;
    report.reason = reason;
    report.error_code = 1U;
    report.timestamp_ms = g_now_ms;
    return report;
}

TEST(test_duplicate_failure_is_latched)
{
    fdir_entity_id_t id = setup_entity(0U);
    fdir_failure_report_t report = failure_of(id, FDIR_REASON_IO_ERROR);
    int calls_before;

    fdir_handle_failure(&report);
    ASSERT_EQ_INT(fdir_health_fault_is_latched(id, FDIR_REASON_IO_ERROR), 1);
    ASSERT_EQ_INT(fdir_health_fault_is_latched(id, FDIR_REASON_TIMEOUT), 0);
    calls_before = g_event_calls;
    fdir_handle_failure(&report);

    ASSERT_EQ_INT(g_event_calls, calls_before);
}

TEST(test_watchdog_not_repeated_while_latched)
{
    fdir_config_t cfg = fdir_config_default();
    fdir_entity_desc_t desc;
    fdir_entity_id_t id = 0;

    fdir_port_t port = policy_test_port();

    g_now_ms = 0U;
    g_event_calls = 0;
    (void)fdir_init(&cfg, &port);

    desc.name = "worker";
    desc.max_restarts = 0U;
    desc.max_watchdog_restarts = 0U;
    desc.on_exhausted = FDIR_ACTION_DEGRADE;
    desc.linked_subsystem = FDIR_SUBSYSTEM_NONE;
    desc.restart = restart_ok;
    desc.decide = NULL;
    desc.user = NULL;
    (void)fdir_entity_register(&desc, &id);

    g_now_ms = 2000U;
    fdir_check_watchdogs();
    g_event_calls = 0;
    fdir_check_watchdogs();

    ASSERT_EQ_INT(g_event_calls, 0);
}

TEST(test_deescalate_system_mode)
{
    setup_entity(1U);

    fdir_enter_safe_mode();
    ASSERT_EQ_INT(fdir_system_mode(), FDIR_MODE_SAFE);
    ASSERT_EQ_INT(fdir_deescalate_system_mode(), FDIR_OK);
    ASSERT_EQ_INT(fdir_system_mode(), FDIR_MODE_DEGRADED);
    ASSERT_EQ_INT(fdir_deescalate_system_mode(), FDIR_OK);
    ASSERT_EQ_INT(fdir_system_mode(), FDIR_MODE_NOMINAL);
    ASSERT_EQ_INT(fdir_deescalate_system_mode(), FDIR_ERR_STATE);
}

TEST(test_set_system_mode_ground)
{
    setup_entity(1U);

    ASSERT_EQ_INT(fdir_set_system_mode(FDIR_MODE_DEGRADED), FDIR_OK);
    ASSERT_EQ_INT(fdir_system_mode(), FDIR_MODE_DEGRADED);
    ASSERT_EQ_INT(fdir_set_system_mode(FDIR_MODE_REBOOT_PENDING), FDIR_ERR_STATE);
}

TEST(test_restart_emits_cleared_anomaly)
{
    fdir_entity_id_t id = setup_entity(2U);
    fdir_failure_report_t report = failure_of(id, FDIR_REASON_TIMEOUT);

    fdir_handle_failure(&report);
    ASSERT_EQ_INT(g_restart_calls, 1);
    ASSERT_EQ_INT(g_event_calls, 3);
    ASSERT_EQ_INT(g_last_event.kind, FDIR_EVENT_RESTART);
}

TEST(test_log_note_emits_event)
{
    setup_entity(1U);
    g_event_calls = 0;
    fdir_log_note("hello");
    ASSERT_EQ_INT(g_event_calls, 1);
    ASSERT_EQ_INT(g_last_event.kind, FDIR_EVENT_NOTE);
    ASSERT_STR_EQ(g_last_event.detail, "hello");
}

TEST(test_log_queue_overflow_emits_event)
{
    setup_entity(1U);
    g_event_calls = 0;
    fdir_log_queue_overflow(3U, 2U, 10U);
    ASSERT_EQ_INT(g_event_calls, 1);
    ASSERT_EQ_INT(g_last_event.kind, FDIR_EVENT_QUEUE_OVERFLOW);
    ASSERT_EQ_INT(g_last_event.severity, FDIR_SEVERITY_WARNING);
}

TEST(test_set_system_mode_rejects_invalid)
{
    setup_entity(1U);
    ASSERT_EQ_INT(fdir_set_system_mode((fdir_mode_t)99), FDIR_ERR_PARAM);
}

TEST(test_deescalate_from_reboot_pending_fails)
{
    setup_entity(1U);
    fdir_try_reboot("pending");
    ASSERT_EQ_INT(fdir_deescalate_system_mode(), FDIR_ERR_STATE);
}

TEST(test_init_failed_severity_error)
{
    fdir_entity_id_t id = setup_entity(0U);
    fdir_failure_report_t report = failure_of(id, FDIR_REASON_INIT_FAILED);

    g_has_failure = 0;
    fdir_handle_failure(&report);
    ASSERT_EQ_INT(g_has_failure, 1);
    ASSERT_EQ_INT(g_failure_event.severity, FDIR_SEVERITY_ERROR);
}

int main(void)
{
    RUN(test_duplicate_failure_is_latched);
    RUN(test_watchdog_not_repeated_while_latched);
    RUN(test_deescalate_system_mode);
    RUN(test_set_system_mode_ground);
    RUN(test_restart_emits_cleared_anomaly);
    RUN(test_log_note_emits_event);
    RUN(test_log_queue_overflow_emits_event);
    RUN(test_set_system_mode_rejects_invalid);
    RUN(test_deescalate_from_reboot_pending_fails);
    RUN(test_init_failed_severity_error);
    return test_harness_summary();
}
