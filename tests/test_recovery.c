#include "test_harness.h"

#include "fdir.h"

#include <stdint.h>

static uint32_t g_now_ms;
static int g_restart_calls;
static fdir_entity_id_t g_last_restart_id;
static int g_restart_result;
static int g_reboot_calls;
static char g_last_reboot_reason[FDIR_DETAIL_SIZE];
static int g_event_calls;
static fdir_event_t g_last_event;

uint32_t fdir_get_now_ms(void)
{
    return g_now_ms;
}

int fdir_submit_failure(const fdir_failure_report_t *report)
{
    (void)report;
    return 0;
}

void fdir_isolate_current_worker(void) {}

void fdir_emit_event(const fdir_event_t *event)
{
    g_last_event = *event;
    g_event_calls++;
}

void fdir_request_reboot(const char *reason)
{
    g_reboot_calls++;
    if (reason == NULL) {
        g_last_reboot_reason[0] = '\0';
        return;
    }
    for (int i = 0; i < FDIR_DETAIL_SIZE - 1 && reason[i] != '\0'; i++) {
        g_last_reboot_reason[i] = reason[i];
        g_last_reboot_reason[i + 1] = '\0';
    }
}

static int restart_ok(fdir_entity_id_t id, void *user)
{
    (void)user;
    g_restart_calls++;
    g_last_restart_id = id;
    return g_restart_result;
}

static void reset_fixtures(void)
{
    g_now_ms = 0U;
    g_restart_calls = 0;
    g_last_restart_id = FDIR_ENTITY_NONE;
    g_restart_result = 0;
    g_reboot_calls = 0;
    g_last_reboot_reason[0] = '\0';
    g_event_calls = 0;
}

static fdir_entity_id_t init_one_entity(uint8_t max_restarts,
                                        uint8_t max_watchdog_restarts,
                                        fdir_action_t on_exhausted)
{
    fdir_config_t cfg = fdir_config_default();
    fdir_entity_desc_t desc;
    fdir_entity_id_t id;

    reset_fixtures();
    cfg.health_check_period_ms = 1000U;
    cfg.missed_heartbeat_tolerance = 2U;
    cfg.safe_mode_critical_failure_threshold = 2U;
    (void)fdir_init(&cfg);

    desc.name = "worker";
    desc.max_restarts = max_restarts;
    desc.max_watchdog_restarts = max_watchdog_restarts;
    desc.on_exhausted = on_exhausted;
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

    report.entity = id;
    report.reason = reason;
    report.error_code = 17U;
    report.timestamp_ms = g_now_ms;
    report.detail[0] = 'x';
    report.detail[1] = '\0';
    return report;
}

TEST(test_entity_register_increments_count)
{
    fdir_entity_id_t id = init_one_entity(2U, 1U, FDIR_ACTION_DEGRADE);

    ASSERT_EQ_INT(id, 0);
    ASSERT_EQ_INT(fdir_entity_count(), 1);
    ASSERT_STR_EQ(fdir_entity_name(id), "worker");
}

TEST(test_handle_failure_restarts_entity)
{
    fdir_entity_id_t id = init_one_entity(2U, 1U, FDIR_ACTION_DEGRADE);
    fdir_failure_report_t report = failure_of(id, FDIR_REASON_IO_ERROR);

    fdir_health_set(id, FDIR_HEALTH_DEGRADED, 9U, "fault");
    fdir_handle_failure(&report);

    ASSERT_EQ_INT(g_restart_calls, 1);
    ASSERT_EQ_INT(g_last_restart_id, id);
    ASSERT_EQ_INT(fdir_system_mode(), FDIR_MODE_NOMINAL);
    ASSERT_EQ_INT(fdir_health_snapshot(id)->health, FDIR_HEALTH_OK);
}

TEST(test_restart_budget_exhaustion_enters_degraded)
{
    fdir_entity_id_t id = init_one_entity(1U, 1U, FDIR_ACTION_DEGRADE);
    fdir_failure_report_t report = failure_of(id, FDIR_REASON_IO_ERROR);

    fdir_handle_failure(&report);
    fdir_handle_failure(&report);

    ASSERT_EQ_INT(g_restart_calls, 1);
    ASSERT_EQ_INT(fdir_system_mode(), FDIR_MODE_DEGRADED);
}

TEST(test_watchdog_uses_watchdog_budget)
{
    fdir_entity_id_t id = init_one_entity(5U, 1U, FDIR_ACTION_DEGRADE);
    fdir_failure_report_t report = failure_of(id, FDIR_REASON_WATCHDOG);

    fdir_handle_failure(&report);
    ASSERT_EQ_INT(g_restart_calls, 1);
    ASSERT_EQ_INT(fdir_system_mode(), FDIR_MODE_NOMINAL);

    fdir_handle_failure(&report);
    ASSERT_EQ_INT(g_restart_calls, 1);
    ASSERT_EQ_INT(fdir_system_mode(), FDIR_MODE_DEGRADED);
}

TEST(test_invalid_entity_failure_degrades_system)
{
    fdir_failure_report_t report;

    init_one_entity(1U, 1U, FDIR_ACTION_DEGRADE);
    report = failure_of(99U, FDIR_REASON_IO_ERROR);
    fdir_handle_failure(&report);
    ASSERT_EQ_INT(fdir_system_mode(), FDIR_MODE_DEGRADED);
}

TEST(test_restart_failure_falls_back_to_on_exhausted)
{
    fdir_entity_id_t id = init_one_entity(3U, 1U, FDIR_ACTION_DEGRADE);
    fdir_failure_report_t report = failure_of(id, FDIR_REASON_IO_ERROR);

    g_restart_result = -1;
    fdir_handle_failure(&report);

    ASSERT_EQ_INT(g_restart_calls, 1);
    ASSERT_EQ_INT(fdir_system_mode(), FDIR_MODE_DEGRADED);
}

TEST(test_try_reboot_sets_mode_and_calls_hook)
{
    init_one_entity(1U, 1U, FDIR_ACTION_DEGRADE);

    fdir_try_reboot("manual");

    ASSERT_EQ_INT(fdir_system_mode(), FDIR_MODE_REBOOT_PENDING);
    ASSERT_EQ_INT(g_reboot_calls, 1);
    ASSERT_STR_EQ(g_last_reboot_reason, "manual");
}

TEST(test_unavailable_action_escalates_to_safe_with_two_critical_subsystems)
{
    fdir_config_t cfg = fdir_config_default();
    fdir_subsystem_desc_t sub_desc;
    fdir_entity_desc_t desc;
    fdir_subsystem_id_t sub_a;
    fdir_subsystem_id_t sub_b;
    fdir_entity_id_t id_a;
    fdir_entity_id_t id_b;
    fdir_failure_report_t report_a;
    fdir_failure_report_t report_b;

    reset_fixtures();
    cfg.safe_mode_critical_failure_threshold = 2U;
    (void)fdir_init(&cfg);

    sub_desc.name = "a";
    sub_desc.is_critical_path = 1U;
    ASSERT_EQ_INT(fdir_subsystem_register(&sub_desc, &sub_a), FDIR_OK);
    sub_desc.name = "b";
    ASSERT_EQ_INT(fdir_subsystem_register(&sub_desc, &sub_b), FDIR_OK);

    desc.name = "ea";
    desc.max_restarts = 0U;
    desc.max_watchdog_restarts = 0U;
    desc.on_exhausted = FDIR_ACTION_UNAVAILABLE;
    desc.linked_subsystem = sub_a;
    desc.restart = restart_ok;
    desc.decide = NULL;
    desc.user = NULL;
    ASSERT_EQ_INT(fdir_entity_register(&desc, &id_a), FDIR_OK);

    desc.name = "eb";
    desc.linked_subsystem = sub_b;
    ASSERT_EQ_INT(fdir_entity_register(&desc, &id_b), FDIR_OK);

    report_a = failure_of(id_a, FDIR_REASON_IO_ERROR);
    fdir_handle_failure(&report_a);
    ASSERT_EQ_INT(fdir_system_mode(), FDIR_MODE_DEGRADED);

    report_b = failure_of(id_b, FDIR_REASON_IO_ERROR);
    fdir_handle_failure(&report_b);
    ASSERT_EQ_INT(fdir_system_mode(), FDIR_MODE_SAFE);
}

TEST(test_check_watchdogs_raises_restart)
{
    fdir_entity_id_t id = init_one_entity(2U, 1U, FDIR_ACTION_DEGRADE);

    g_now_ms = 0U;
    fdir_health_heartbeat_notify(id);
    g_now_ms = 2500U;
    fdir_check_watchdogs();

    ASSERT_EQ_INT(g_restart_calls, 1);
    ASSERT_EQ_INT(g_last_restart_id, id);
}

int main(void)
{
    RUN(test_entity_register_increments_count);
    RUN(test_handle_failure_restarts_entity);
    RUN(test_restart_budget_exhaustion_enters_degraded);
    RUN(test_watchdog_uses_watchdog_budget);
    RUN(test_invalid_entity_failure_degrades_system);
    RUN(test_restart_failure_falls_back_to_on_exhausted);
    RUN(test_try_reboot_sets_mode_and_calls_hook);
    RUN(test_unavailable_action_escalates_to_safe_with_two_critical_subsystems);
    RUN(test_check_watchdogs_raises_restart);
    return test_harness_summary();
}
