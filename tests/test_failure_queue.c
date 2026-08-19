#include "test_harness.h"

#include "failure_queue.h"

#include <string.h>

TEST(test_failure_queue_put_null)
{
    fdir_failure_queue_init();
    ASSERT_EQ_INT(fdir_failure_queue_put(NULL), -1);
}

TEST(test_failure_queue_get_null)
{
    fdir_failure_queue_init();
    ASSERT_EQ_INT(fdir_failure_queue_get(NULL), -1);
}

TEST(test_failure_queue_get_empty)
{
    fdir_failure_report_t out;

    fdir_failure_queue_init();
    ASSERT_EQ_INT(fdir_failure_queue_get(&out), -1);
}

TEST(test_failure_queue_put_get_roundtrip)
{
    fdir_failure_report_t in;
    fdir_failure_report_t out;

    fdir_failure_queue_init();
    memset(&in, 0, sizeof(in));
    in.entity = 2U;
    in.reason = FDIR_REASON_TIMEOUT;
    in.error_code = 9U;
    in.detail[0] = 'x';
    in.detail[1] = '\0';

    ASSERT_EQ_INT(fdir_failure_queue_put(&in), 0);
    ASSERT_EQ_INT(fdir_failure_queue_get(&out), 0);
    ASSERT_EQ_INT(out.entity, 2U);
    ASSERT_EQ_INT(out.reason, FDIR_REASON_TIMEOUT);
    ASSERT_EQ_INT(out.error_code, 9U);
    ASSERT_EQ_INT(fdir_failure_queue_get(&out), -1);
}

int main(void)
{
    RUN(test_failure_queue_put_null);
    RUN(test_failure_queue_get_null);
    RUN(test_failure_queue_get_empty);
    RUN(test_failure_queue_put_get_roundtrip);
    return test_harness_summary();
}
