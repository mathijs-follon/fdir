#include "test_harness.h"

#include "fdir.h"
#include "internal.h"

#include <string.h>

/* Port stubs */
uint32_t fdir_get_now_ms(void) { return 0; }
int      fdir_submit_failure(const fdir_failure_report_t *r) { (void)r; return 0; }
void     fdir_isolate_current_worker(void) {}

static fdir_event_t g_last_event;
static int          g_emit_count;

void fdir_emit_event(const fdir_event_t *e)
{
    g_last_event = *e;
    g_emit_count++;
}

void fdir_request_reboot(const char *reason) { (void)reason; }

/* fdir_internal_copy_detail: normal copy */
TEST(test_copy_detail_normal)
{
    char dst[16];
    fdir_internal_copy_detail(dst, sizeof(dst), "hello");
    ASSERT_STR_EQ(dst, "hello");
}

/* fdir_internal_copy_detail: truncates at dst_len - 1 */
TEST(test_copy_detail_truncation)
{
    char dst[4];
    fdir_internal_copy_detail(dst, sizeof(dst), "toolong");
    ASSERT_STR_EQ(dst, "too");
    ASSERT_EQ_INT((int)dst[3], 0);
}

/* fdir_internal_copy_detail: NULL src produces empty string */
TEST(test_copy_detail_null_src)
{
    char dst[8] = "garbage";
    fdir_internal_copy_detail(dst, sizeof(dst), NULL);
    ASSERT_EQ_INT((int)dst[0], 0);
}

/* fdir_internal_copy_detail: NULL dst is a no-op (no crash) */
TEST(test_copy_detail_null_dst)
{
    fdir_internal_copy_detail(NULL, 8, "hello");
    /* no assertion needed: must not crash */
}

/* fdir_internal_copy_detail: zero dst_len is a no-op */
TEST(test_copy_detail_zero_len)
{
    char dst[4] = "abc";
    fdir_internal_copy_detail(dst, 0, "hello");
    ASSERT_STR_EQ(dst, "abc"); /* unchanged */
}

/* fdir_internal_copy_detail: empty src produces empty string */
TEST(test_copy_detail_empty_src)
{
    char dst[8] = "garbage";
    fdir_internal_copy_detail(dst, sizeof(dst), "");
    ASSERT_EQ_INT((int)dst[0], 0);
}

/* fdir_internal_set_config / fdir_internal_get_config round-trip */
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

/* fdir_internal_set_config(NULL) falls back to fdir_config_default() */
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

/* fdir_internal_emit produces an event with the correct fields */
TEST(test_emit_fields)
{
    g_emit_count = 0;
    fdir_internal_emit(FDIR_EVENT_FAILURE, 2, FDIR_REASON_IO_ERROR, 99, "disk error");

    ASSERT_EQ_INT(g_emit_count, 1);
    ASSERT_EQ_INT(g_last_event.kind, FDIR_EVENT_FAILURE);
    ASSERT_EQ_INT(g_last_event.entity, 2);
    ASSERT_EQ_INT(g_last_event.reason, FDIR_REASON_IO_ERROR);
    ASSERT_EQ_INT(g_last_event.error_code, 99);
    ASSERT_STR_EQ(g_last_event.detail, "disk error");
}

/* fdir_internal_emit_mode sets the mode field correctly */
TEST(test_emit_mode_field)
{
    g_emit_count = 0;
    fdir_internal_emit_mode(FDIR_EVENT_MODE_CHANGE, FDIR_MODE_SAFE,
                            FDIR_ENTITY_NONE, FDIR_REASON_USER, 0, "safe");

    ASSERT_EQ_INT(g_emit_count, 1);
    ASSERT_EQ_INT(g_last_event.kind, FDIR_EVENT_MODE_CHANGE);
    ASSERT_EQ_INT(g_last_event.mode, FDIR_MODE_SAFE);
    ASSERT_STR_EQ(g_last_event.detail, "safe");
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
    RUN(test_emit_fields);
    RUN(test_emit_mode_field);
    return test_harness_summary();
}
