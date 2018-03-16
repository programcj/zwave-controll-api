/*
 * zw_s0_layer.h
 *
 *  Created on: 2017年12月13日
 *      Author: cj
 */

#ifndef HAL_ZW_S0_LAYER_H_
#define HAL_ZW_S0_LAYER_H_

#include "port.h"
#include "Serialapi.h"
#include "zwave_app.h"
#include "zw_send.h"

void zw_s0_init();

void zw_s0_layer_handle();

void zw_s0_layer_CommandHandler(BYTE rxStatus, BYTE destNode,
		BYTE sourceNode, ZW_APPLICATION_TX_BUFFER *pCmd, BYTE cmdLength);

void zw_s0_add_start(BYTE node, int isSlaveApi, void (*callback)(int status));

int zw_s0_layer_senddata(BYTE node,void *data, BYTE dlen, zw_senddata_callback_t callback, void *user);

#endif /* HAL_ZW_S0_LAYER_H_ */
