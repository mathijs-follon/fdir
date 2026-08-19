#include "test_harness.h"

#include "fdir.h"

#include <stdint.h>
#include <string.h>

static uint32_t g_now_ms;
static int g_restart_calls;
static fdir_entity_id_t g_last_restart_id;
static int g_restart_result;
static int g_reboot_calls;
static char g_last_reboot_reason[FDIR_DETAIL_SIZE];
static int g_event_calls;
static fdir_event_t g_last_event;
static int g_subsystem_event_seen;
static fdir_event_t g_subsystem_event;

static uint32_t port_now(void)
{
    return g_now_ms;
}

static void port_emit(const fdir_event_t *event)
{
    g_last_event = *event;
    g_event_calls++;
    if (event->level == FDIR_LEVEL_SUBSYSTEM) {
        g_subsystem_event = *event;
        g_subsystem_event_seen = 1;
    }
}

static void port_reboot(const char *reason)
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

static fdir_port_t recovery_test_port(void)
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
    g_subsystem_event_seen = 0;
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
    {
        fdir_port_t port = recovery_test_port();
        (void)fdir_init(&cfg, &port);
    }

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

    memset(&report, 0, sizeof(report));
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
    {
        fdir_port_t port = recovery_test_port();
        (void)fdir_init(&cfg, &port);
    }

    sub_desc = (fdir_subsystem_desc_t){0};
    sub_desc.name = "a";
    sub_desc.is_critical_path = 1U;
    ASSERT_EQ_INT(fdir_subsystem_register(&sub_desc, &sub_a), FDIR_OK);
    sub_desc.name = "b";
    ASSERT_EQ_INT(fdir_subsystem_register(&sub_desc, &sub_b), FDIR_OK);

    desc = (fdir_entity_desc_t){0};
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

TEST(test_unlimited_restarts_never_exhaust_budget)
{
    fdir_entity_id_t id = init_one_entity(FDIR_RESTART_UNLIMITED, 1U, FDIR_ACTION_DEGRADE);
    fdir_failure_report_t report = failure_of(id, FDIR_REASON_IO_ERROR);
    int i;

    for (i = 0; i < 10; i++) {
        fdir_handle_failure(&report);
    }

    ASSERT_EQ_INT(g_restart_calls, 10);
    ASSERT_EQ_INT(fdir_system_mode(), FDIR_MODE_NOMINAL);
}

TEST(test_unavailable_action_emits_subsystem_event)
{
    fdir_config_t cfg = fdir_config_default();
    fdir_subsystem_desc_t sub_desc;
    fdir_entity_desc_t desc;
    fdir_subsystem_id_t sub;
    fdir_entity_id_t id;
    fdir_failure_report_t report;

    reset_fixtures();
    {
        fdir_port_t port = recovery_test_port();
        (void)fdir_init(&cfg, &port);
    }

    sub_desc = (fdir_subsystem_desc_t){0};
    sub_desc.name = "payload";
    sub_desc.is_critical_path = 0U;
    ASSERT_EQ_INT(fdir_subsystem_register(&sub_desc, &sub), FDIR_OK);

    desc = (fdir_entity_desc_t){0};
    desc.name = "sensor";
    desc.max_restarts = 0U;
    desc.max_watchdog_restarts = 0U;
    desc.on_exhausted = FDIR_ACTION_UNAVAILABLE;
    desc.linked_subsystem = sub;
    desc.restart = restart_ok;
    desc.decide = NULL;
    desc.user = NULL;
    ASSERT_EQ_INT(fdir_entity_register(&desc, &id), FDIR_OK);

    report = failure_of(id, FDIR_REASON_IO_ERROR);
    fdir_handle_failure(&report);

    ASSERT_EQ_INT(g_subsystem_event_seen, 1);
    ASSERT_EQ_INT(g_subsystem_event.level, FDIR_LEVEL_SUBSYSTEM);
    ASSERT_EQ_INT(g_subsystem_event.subsystem, sub);
    ASSERT_EQ_INT(g_subsystem_event.entity, id);
    ASSERT_STR_EQ(g_subsystem_event.detail, "unavailable");
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

TEST(test_entity_register_rejects_null)
{
    fdir_entity_id_t id = 0U;

    init_one_entity(1U, 1U, FDIR_ACTION_DEGRADE);
    ASSERT_EQ_INT(fdir_entity_register(NULL, &id), FDIR_ERR_PARAM);
    ASSERT_EQ_INT(fdir_entity_register(&(fdir_entity_desc_t){ .name = "x" }, NULL), FDIR_ERR_PARAM);
}

TEST(test_entity_register_rejects_full)
{
    fdir_config_t cfg = fdir_config_default();
    fdir_entity_desc_t desc;
    fdir_entity_id_t id;
    unsigned i;

    reset_fixtures();
    {
        fdir_port_t port = recovery_test_port();
        (void)fdir_init(&cfg, &port);
    }

    desc = (fdir_entity_desc_t){0};
    desc.name = "fill";
    desc.max_restarts = 0U;
    desc.on_exhausted = FDIR_ACTION_DEGRADE;
    desc.restart = restart_ok;

    for (i = 0U; i < FDIR_ENTITY_CAP; i++) {
        ASSERT_EQ_INT(fdir_entity_register(&desc, &id), FDIR_OK);
    }
    ASSERT_EQ_INT(fdir_entity_register(&desc, &id), FDIR_ERR_FULL);
}

TEST(test_entity_name_unknown)
{
    init_one_entity(1U, 1U, FDIR_ACTION_DEGRADE);
    ASSERT_STR_EQ(fdir_entity_name(99U), "unknown");
}

TEST(test_restart_null_callback_falls_back)
{
    fdir_config_t cfg = fdir_config_default();
    fdir_entity_desc_t desc;
    fdir_entity_id_t id;
    fdir_failure_report_t report;

    reset_fixtures();
    {
        fdir_port_t port = recovery_test_port();
        (void)fdir_init(&cfg, &port);
    }

    desc = (fdir_entity_desc_t){0};
    desc.name = "no_restart";
    desc.max_restarts = 1U;
    desc.on_exhausted = FDIR_ACTION_DEGRADE;
    desc.restart = NULL;
    (void)fdir_entity_register(&desc, &id);

    report = failure_of(id, FDIR_REASON_IO_ERROR);
    fdir_handle_failure(&report);

    ASSERT_EQ_INT(g_restart_calls, 0);
    ASSERT_EQ_INT(fdir_system_mode(), FDIR_MODE_DEGRADED);
}

TEST(test_degrade_marks_linked_subsystem)
{
    fdir_config_t cfg = fdir_config_default();
    fdir_subsystem_desc_t sub_desc;
    fdir_entity_desc_t desc;
    fdir_subsystem_id_t sub;
    fdir_entity_id_t id;
    fdir_failure_report_t report;

    reset_fixtures();
    {
        fdir_port_t port = recovery_test_port();
        (void)fdir_init(&cfg, &port);
    }

    sub_desc = (fdir_subsystem_desc_t){0};
    sub_desc.name = "payload";
    ASSERT_EQ_INT(fdir_subsystem_register(&sub_desc, &sub), FDIR_OK);

    desc = (fdir_entity_desc_t){0};
    desc.name = "sensor";
    desc.max_restarts = 0U;
    desc.on_exhausted = FDIR_ACTION_DEGRADE;
    desc.linked_subsystem = sub;
    desc.restart = restart_ok;
    ASSERT_EQ_INT(fdir_entity_register(&desc, &id), FDIR_OK);

    report = failure_of(id, FDIR_REASON_IO_ERROR);
    fdir_handle_failure(&report);

    ASSERT_EQ_INT(fdir_subsystem_is_degraded(sub), 1);
    ASSERT_EQ_INT(fdir_system_mode(), FDIR_MODE_DEGRADED);
}

static fdir_action_t force_safe(fdir_subsystem_id_t sub, fdir_entity_id_t entity,
                                const fdir_failure_report_t *report, void *user)
{
    (void)sub;
    (void)entity;
    (void)report;
    (void)user;
    return FDIR_ACTION_SAFE;
}

TEST(test_subsystem_policy_override)
{
    fdir_config_t cfg = fdir_config_default();
    fdir_subsystem_desc_t sub_desc;
    fdir_entity_desc_t desc;
    fdir_subsystem_id_t sub;
    fdir_entity_id_t id;
    fdir_failure_report_t report;

    reset_fixtures();
    {
        fdir_port_t port = recovery_test_port();
        (void)fdir_init(&cfg, &port);
    }

    sub_desc = (fdir_subsystem_desc_t){0};
    sub_desc.name = "policy";
    sub_desc.on_entity_exhausted = force_safe;
    ASSERT_EQ_INT(fdir_subsystem_register(&sub_desc, &sub), FDIR_OK);

    desc = (fdir_entity_desc_t){0};
    desc.name = "sensor";
    desc.max_restarts = 0U;
    desc.on_exhausted = FDIR_ACTION_DEGRADE;
    desc.linked_subsystem = sub;
    desc.restart = restart_ok;
    ASSERT_EQ_INT(fdir_entity_register(&desc, &id), FDIR_OK);

    report = failure_of(id, FDIR_REASON_IO_ERROR);
    fdir_handle_failure(&report);

    ASSERT_EQ_INT(fdir_system_mode(), FDIR_MODE_SAFE);
    ASSERT_EQ_INT(g_subsystem_event_seen, 1);
    ASSERT_EQ_INT(g_subsystem_event.subsystem, sub);
}

TEST(test_on_exhausted_reboot_action)
{
    fdir_entity_id_t id = init_one_entity(0U, 0U, FDIR_ACTION_REBOOT);
    fdir_failure_report_t report = failure_of(id, FDIR_REASON_IO_ERROR);

    report.detail[0] = 'x';
    report.detail[1] = '\0';
    fdir_handle_failure(&report);

    ASSERT_EQ_INT(fdir_system_mode(), FDIR_MODE_REBOOT_PENDING);
    ASSERT_EQ_INT(g_reboot_calls, 1);
    ASSERT_STR_EQ(g_last_reboot_reason, "x");
}

TEST(test_handle_failure_null_report)
{
    init_one_entity(1U, 1U, FDIR_ACTION_DEGRADE);
    fdir_handle_failure(NULL);
    ASSERT_EQ_INT(g_event_calls, 0);
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
    RUN(test_unlimited_restarts_never_exhaust_budget);
    RUN(test_unavailable_action_emits_subsystem_event);
    RUN(test_check_watchdogs_raises_restart);
    RUN(test_entity_register_rejects_null);
    RUN(test_entity_register_rejects_full);
    RUN(test_entity_name_unknown);
    RUN(test_restart_null_callback_falls_back);
    RUN(test_degrade_marks_linked_subsystem);
    RUN(test_subsystem_policy_override);
    RUN(test_on_exhausted_reboot_action);
    RUN(test_handle_failure_null_report);
    return test_harness_summary();
}
