#include "test_harness.h"
#include "test_port.h"

#include "fdir.h"
#include "internal.h"

#include <string.h>

static fdir_event_t g_last_event;
static fdir_event_t g_failure_event;
static int          g_emit_count;
static int          g_has_failure_event;

static void port_emit(const fdir_event_t *e)
{
    if (e->kind == FDIR_EVENT_FAILURE) {
        g_failure_event = *e;
        g_has_failure_event = 1;
    }
    g_last_event = *e;
    g_emit_count++;
}

static fdir_port_t internal_test_port(void)
{
    fdir_port_t port = *test_port_default();
    port.emit_event = port_emit;
    return port;
}

static void init_port(void)
{
    fdir_config_t cfg = fdir_config_default();
    fdir_port_t port = internal_test_port();
    (void)fdir_init(&cfg, &port);
}

TEST(test_copy_detail_normal)
{
    char dst[16];
    fdir_internal_copy_detail(dst, sizeof(dst), "hello");
    ASSERT_STR_EQ(dst, "hello");
}

TEST(test_copy_detail_truncation)
{
    char dst[4];
    fdir_internal_copy_detail(dst, sizeof(dst), "toolong");
    ASSERT_STR_EQ(dst, "too");
    ASSERT_EQ_INT((int)dst[3], 0);
}

TEST(test_copy_detail_null_src)
{
    char dst[8] = "garbage";
    fdir_internal_copy_detail(dst, sizeof(dst), NULL);
    ASSERT_EQ_INT((int)dst[0], 0);
}

TEST(test_copy_detail_null_dst)
{
    fdir_internal_copy_detail(NULL, 8, "hello");
}

TEST(test_copy_detail_zero_len)
{
    char dst[4] = "abc";
    fdir_internal_copy_detail(dst, 0, "hello");
    ASSERT_STR_EQ(dst, "abc");
}

TEST(test_copy_detail_empty_src)
{
    char dst[8] = "garbage";
    fdir_internal_copy_detail(dst, sizeof(dst), "");
    ASSERT_EQ_INT((int)dst[0], 0);
}

TEST(test_config_roundtrip)
{
    fdir_config_t cfg;
    cfg.health_check_period_ms               = 1234;
    cfg.missed_heartbeat_tolerance           = 5;
    cfg.safe_mode_critical_failure_threshold = 3;

    fdir_internal_set_config(&cfg);

    const fdir_config_t *stored = fdir_internal_get_config();
    ASSERT(stored != NULL);
    ASSERT_EQ_INT(stored->health_check_period_ms, 1234);
    ASSERT_EQ_INT(stored->missed_heartbeat_tolerance, 5);
    ASSERT_EQ_INT(stored->safe_mode_critical_failure_threshold, 3);
}

TEST(test_config_null_uses_default)
{
    fdir_internal_set_config(NULL);

    const fdir_config_t *stored = fdir_internal_get_config();
    fdir_config_t def = fdir_config_default();
    ASSERT_EQ_INT(stored->health_check_period_ms, def.health_check_period_ms);
    ASSERT_EQ_INT(stored->missed_heartbeat_tolerance, def.missed_heartbeat_tolerance);
    ASSERT_EQ_INT(stored->safe_mode_critical_failure_threshold,
                  def.safe_mode_critical_failure_threshold);
}

TEST(test_event_metadata_on_failure)
{
    fdir_config_t cfg = fdir_config_default();
    fdir_entity_desc_t desc;
    fdir_entity_id_t id = 0;
    fdir_failure_report_t report;
    fdir_port_t port = internal_test_port();

    g_emit_count = 0;
    g_has_failure_event = 0;
    (void)fdir_init(&cfg, &port);

    desc.name = "sensor";
    desc.max_restarts = 0U;
    desc.max_watchdog_restarts = 0U;
    desc.on_exhausted = FDIR_ACTION_DEGRADE;
    desc.linked_subsystem = FDIR_SUBSYSTEM_NONE;
    desc.restart = NULL;
    desc.decide = NULL;
    desc.user = NULL;
    (void)fdir_entity_register(&desc, &id);

    memset(&report, 0, sizeof(report));
    report.entity = id;
    report.reason = FDIR_REASON_IO_ERROR;
    report.error_code = 99U;
    fdir_internal_copy_detail(report.detail, sizeof(report.detail), "disk error");
    fdir_handle_failure(&report);

    ASSERT_EQ_INT(g_emit_count, 2);
    ASSERT_EQ_INT(g_has_failure_event, 1);
    ASSERT_EQ_INT(g_failure_event.kind, FDIR_EVENT_FAILURE);
    ASSERT_EQ_INT(g_failure_event.entity, id);
    ASSERT_EQ_INT(g_failure_event.reason, FDIR_REASON_IO_ERROR);
    ASSERT_EQ_INT(g_failure_event.error_code, 99U);
    ASSERT_EQ_INT(g_failure_event.level, FDIR_LEVEL_ENTITY);
    ASSERT_EQ_INT(g_failure_event.phase, FDIR_ANOMALY_RAISED);
    ASSERT_EQ_INT(g_failure_event.anomaly_id, FDIR_ANOMALY_ID(id, FDIR_REASON_IO_ERROR));
    ASSERT_STR_EQ(g_failure_event.detail, "disk error");
    ASSERT_EQ_INT(g_last_event.kind, FDIR_EVENT_MODE_CHANGE);
}

TEST(test_event_metadata_on_safe_mode)
{
    init_port();
    g_emit_count = 0;
    fdir_enter_safe_mode();

    ASSERT_EQ_INT(g_emit_count, 1);
    ASSERT_EQ_INT(g_last_event.kind, FDIR_EVENT_MODE_CHANGE);
    ASSERT_EQ_INT(g_last_event.mode, FDIR_MODE_SAFE);
    ASSERT_EQ_INT(g_last_event.level, FDIR_LEVEL_SYSTEM);
    ASSERT_EQ_INT(g_last_event.severity, FDIR_SEVERITY_CRITICAL);
    ASSERT_STR_EQ(g_last_event.detail, "safe");
}

TEST(test_init_rejects_null_port)
{
    fdir_config_t cfg = fdir_config_default();
    ASSERT_EQ_INT(fdir_init(&cfg, NULL), FDIR_ERR_PORT);
}

TEST(test_init_rejects_incomplete_port)
{
    fdir_config_t cfg = fdir_config_default();
    fdir_port_t port = *test_port_default();
    port.emit_event = NULL;
    ASSERT_EQ_INT(fdir_init(&cfg, &port), FDIR_ERR_PORT);
}

static fdir_entity_id_t setup_entity_for_may_run(void)
{
    fdir_config_t cfg = fdir_config_default();
    fdir_entity_desc_t desc;
    fdir_entity_id_t id;

    (void)fdir_init(&cfg, test_port_default());

    desc.name = "worker";
    desc.max_restarts = 1U;
    desc.max_watchdog_restarts = 1U;
    desc.on_exhausted = FDIR_ACTION_DEGRADE;
    desc.linked_subsystem = FDIR_SUBSYSTEM_NONE;
    desc.restart = NULL;
    desc.decide = NULL;
    desc.user = NULL;
    (void)fdir_entity_register(&desc, &id);
    return id;
}

TEST(test_entity_may_run_when_healthy)
{
    fdir_entity_id_t id = setup_entity_for_may_run();

    ASSERT_EQ_INT(fdir_entity_may_run(id), 1);
}

TEST(test_entity_may_run_false_when_failed)
{
    fdir_entity_id_t id = setup_entity_for_may_run();

    fdir_health_set(id, FDIR_HEALTH_FAILED, 0U, "x");
    ASSERT_EQ_INT(fdir_entity_may_run(id), 0);
}

TEST(test_entity_may_run_false_in_safe_mode)
{
    fdir_entity_id_t id = setup_entity_for_may_run();

    fdir_enter_safe_mode();
    ASSERT_EQ_INT(fdir_entity_may_run(id), 0);
}

TEST(test_entity_may_run_false_for_invalid_entity)
{
    setup_entity_for_may_run();
    ASSERT_EQ_INT(fdir_entity_may_run(FDIR_ENTITY_NONE), 0);
}

static int g_lock_calls;
static int g_unlock_calls;

static void test_port_lock(void)
{
    g_lock_calls++;
}

static void test_port_unlock(void)
{
    g_unlock_calls++;
}

TEST(test_port_lock_used_during_report)
{
    fdir_config_t cfg = fdir_config_default();
    fdir_entity_desc_t desc;
    fdir_entity_id_t id;
    fdir_port_t port = *test_port_default();

    g_lock_calls = 0;
    g_unlock_calls = 0;
    port.lock = test_port_lock;
    port.unlock = test_port_unlock;
    (void)fdir_init(&cfg, &port);

    desc.name = "locked";
    desc.max_restarts = 1U;
    desc.max_watchdog_restarts = 1U;
    desc.on_exhausted = FDIR_ACTION_DEGRADE;
    desc.linked_subsystem = FDIR_SUBSYSTEM_NONE;
    desc.restart = NULL;
    desc.decide = NULL;
    desc.user = NULL;
    (void)fdir_entity_register(&desc, &id);

    (void)fdir_report_fault(id, FDIR_REASON_IO_ERROR, 1U, "lock");
    ASSERT(g_lock_calls > 0);
    ASSERT(g_unlock_calls > 0);
}

int main(void)
{
    RUN(test_copy_detail_normal);
    RUN(test_copy_detail_truncation);
    RUN(test_copy_detail_null_src);
    RUN(test_copy_detail_null_dst);
    RUN(test_copy_detail_zero_len);
    RUN(test_copy_detail_empty_src);
    RUN(test_config_roundtrip);
    RUN(test_config_null_uses_default);
    RUN(test_event_metadata_on_failure);
    RUN(test_event_metadata_on_safe_mode);
    RUN(test_init_rejects_null_port);
    RUN(test_init_rejects_incomplete_port);
    RUN(test_entity_may_run_when_healthy);
    RUN(test_entity_may_run_false_when_failed);
    RUN(test_entity_may_run_false_in_safe_mode);
    RUN(test_entity_may_run_false_for_invalid_entity);
    RUN(test_port_lock_used_during_report);
    return test_harness_summary();
}
