#include "recovery.h"
#include <string.h>

fdir_config_t fdir_config_default(void)
{
    fdir_config_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.health_check_period_ms = 500U;
    cfg.missed_heartbeat_tolerance = 3U;
    cfg.safe_mode_critical_failure_threshold = 2U;
    return cfg;
}
