#include "config.h"
#include "types.h"

fdir_config_t fdir_config_default(void)
{
    fdir_config_t cfg;

    cfg.health_check_period_ms = 500U;
    cfg.missed_heartbeat_tolerance = 3U;
    cfg.safe_mode_critical_failure_threshold = 2U;
    return cfg;
}
