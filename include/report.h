#ifndef FDIR_REPORT_H
#define FDIR_REPORT_H

#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif

void fdir_report_fault(fdir_entity_id_t entity, fdir_reason_t reason, uint16_t error_code, const char *detail);

void fdir_pause_if_failed(fdir_entity_id_t entity);

#ifdef __cplusplus
}
#endif

#endif /* FDIR_REPORT_H */
