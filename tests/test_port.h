#ifndef TEST_PORT_H
#define TEST_PORT_H

#include "fdir.h"

const fdir_port_t *test_port_default(void);
fdir_status_t test_fdir_init(const fdir_config_t *cfg);
uint32_t test_port_set_now_ms(uint32_t now_ms);

#endif /* TEST_PORT_H */
