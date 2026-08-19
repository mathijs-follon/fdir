#include "worker.h"
#include "health.h"
#include "recovery.h"

fdir_bool_t fdir_worker_may_run(fdir_entity_id_t id)
{
    const fdir_mode_t mode = fdir_system_mode();

    if (mode >= FDIR_MODE_SAFE) {
        fdir_health_heartbeat_notify(id);
        return 0U;
    }

    return fdir_entity_may_run(id);
}
