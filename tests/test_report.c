#include "test_harness.h"

#include "fdir.h"

#include <stdint.h>

static uint32_t g_now_ms;
static int g_post_result;
static int g_post_calls;
static int g_isolate_calls;
static fdir_failure_report_t g_last_posted;

uint32_t fdir_get_now_ms(void)
{
    return g_now_ms;
}

int fdir_submit_failure(const fdir_failure_report_t *report)
{
    g_post_calls++;
    g_last_posted = *report;
    return g_post_result;
}

void fdir_isolate_current_worker(void)
{
    g_isolate_calls++;
}

void fdir_emit_event(const fdir_event_t *event)
{
    (void)event;
}

void fdir_request_reboot(const char *reason)
{
    (void)reason;
}

static fdir_entity_id_t setup_entity(void)
{
    fdir_config_t cfg = fdir_config_default();
    fdir_entity_desc_t desc;
    fdir_entity_id_t id;

    g_now_ms = 0U;
    g_post_result = 0;
    g_post_calls = 0;
    g_isolate_calls = 0;

    (void)fdir_init(&cfg);

    desc.name = "sensor";
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

TEST(test_report_fault_posts_expected_report)
{
    fdir_entity_id_t id = setup_entity();

    g_now_ms = 1234U;
    fdir_report_fault(id, FDIR_REASON_IO_ERROR, 55U, "disk");

    ASSERT_EQ_INT(g_post_calls, 1);
    ASSERT_EQ_INT(g_last_posted.entity, id);
    ASSERT_EQ_INT(g_last_posted.reason, FDIR_REASON_IO_ERROR);
    ASSERT_EQ_INT(g_last_posted.error_code, 55U);
    ASSERT_EQ_INT(g_last_posted.timestamp_ms, 1234U);
    ASSERT_STR_EQ(g_last_posted.detail, "disk");
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

TEST(test_report_fault_isolates_current_worker)
{
    fdir_entity_id_t id = setup_entity();

    fdir_report_fault(id, FDIR_REASON_USER, 0U, "stop");

    ASSERT_EQ_INT(g_isolate_calls, 1);
}

TEST(test_pause_if_failed_isolates_failed_entity)
{
    fdir_entity_id_t id = setup_entity();

    fdir_health_set(id, FDIR_HEALTH_FAILED, 0U, "x");
    fdir_pause_if_failed(id);

    ASSERT_EQ_INT(g_isolate_calls, 1);
}

TEST(test_pause_if_failed_does_not_isolate_healthy_entity)
{
    fdir_entity_id_t id = setup_entity();

    fdir_health_set(id, FDIR_HEALTH_OK, 0U, "");
    fdir_pause_if_failed(id);

    ASSERT_EQ_INT(g_isolate_calls, 0);
}

TEST(test_pause_if_failed_ignores_invalid_entity)
{
    setup_entity();
    fdir_pause_if_failed(FDIR_ENTITY_NONE);
    ASSERT_EQ_INT(g_isolate_calls, 0);
}

int main(void)
{
    RUN(test_report_fault_posts_expected_report);
    RUN(test_report_fault_marks_health_failed);
    RUN(test_report_fault_isolates_current_worker);
    RUN(test_pause_if_failed_isolates_failed_entity);
    RUN(test_pause_if_failed_does_not_isolate_healthy_entity);
    RUN(test_pause_if_failed_ignores_invalid_entity);
    return test_harness_summary();
}
