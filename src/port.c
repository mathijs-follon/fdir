#include "port.h"
#include <stdlib.h>

__attribute((weak)) uint32_t fdir_get_now_ms(void) {
    abort();
}

__attribute((weak)) int fidr_post_failure(const fdir_failure_report_t *report) {
    (void)report;
    abort();
}

__attribute((weak)) void fdir_isolate_current_worker(void) {
    abort();
}

__attribute((weak)) void fdir_emit_event(const fdir_event_t *event) {
    (void)event;
    // abort(); Optional if you dont want to listen for events
}

__attribute((weak)) void fdir_request_reboot(const char *reason) {
    (void)reason;
    // abort(); Optional if you can't reboot the system
}
