#include "test_harness.h"

#include "fdir.h"

TEST(test_config)
{
    fdir_config_t config = fdir_config_default();

    ASSERT_EQ_INT(config.health_check_period_ms, 500U);
    ASSERT_EQ_INT(config.missed_heartbeat_tolerance, 3U);
    ASSERT_EQ_INT(config.safe_mode_critical_failure_threshold, 2U);
}

int main(void)
{
    RUN(test_config);
    return test_harness_summary();
}
