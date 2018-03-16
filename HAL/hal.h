/*
 * hal.h
 *
 *  Created on: 2017年6月15日
 *      Author: cj
 */

#ifndef FILE_SRC_HAL_HAL_H_
#define FILE_SRC_HAL_HAL_H_

#include "zwave/config.h"

extern void hal_init();

extern void hal_sync_zwave_node();

extern int zwclass_send(uint8_t node ,void *data, int dlen, int txoptions);

#endif /* FILE_SRC_HAL_HAL_H_ */
