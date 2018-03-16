/*
 * zwave_app.h
 *
 *  Created on: 2017年5月24日
 *      Author: cj
 */

#ifndef HAL_ZWAVE_APP_H_
#define HAL_ZWAVE_APP_H_

#include "config.h"

struct zwave_controller_info {
	int capabilities; //
	char version[14];
	char deviceChip[10];

	unsigned int this_HomeID;
	uint8_t this_NodeID; //自己的节点ID
	uint8_t this_SUCNodeID;
	char libName[50]; //Z-Wave library basis version.

	uint8_t library;
	uint8_t appVer;
	uint8_t appSubVer;
	uint8_t protoVer;
	uint8_t protoSubVer;

	uint8_t manufactor_id1;
	uint8_t manufactor_id2;
	uint8_t product_type1;
	uint8_t product_type2;
	uint8_t product_id1;
	uint8_t product_id2;
};

extern struct zwave_controller_info ZWaveControllerInfo;

void zw_hook_init_end();

void zw_dispatch_async(void (*callback)(void *context), void *context);

void zwave_app_loop(const char *serialpath);

#endif /* HAL_ZWAVE_APP_H_ */
