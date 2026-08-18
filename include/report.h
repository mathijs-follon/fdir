#ifndef FDIR_REPORT_H
#define FDIR_REPORT_H

#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif

void fdir_report_fault(fdir_entity_id_t entity, fdir_reason_t reason, uint16_t error_code, const char *detail);


/** Sticky latch: 1 if fdir_post_failure() failed. Cleared on read. */
fdir_bool_t fdir_read_and_clear_post_failure(void);

void fdir_pause_if_failed(fdir_entity_id_t entity);

#ifdef __cplusplus
}
#endif

#endif /* FDIR_REPORT_H */
