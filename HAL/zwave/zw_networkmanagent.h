/*
 * zw_networkmanagent.h
 *
 *  Created on: 2017年12月11日
 *      Author: cj
 */

#ifndef HAL_ZW_NETWORKMANAGENT_H_
#define HAL_ZW_NETWORKMANAGENT_H_

#include <stdint.h>

typedef enum {
	ZW_NETSTATUS_NOT,
	ZW_NETSTATUS_ADDNODEING,
	ZW_NETSTATUS_REMOVEING,
	ZW_NETSTATUS_SETDEFAULT,
} zw_netstatus;



void zw_hook_network_addnodestatus(int status, uint8_t node); //入网状态反馈
void zw_hook_network_removenodestatus(int status, uint8_t node); //状态反馈

void zw_hook_network_removefailednode(int status, uint8_t node);
void zw_hook_network_setdefault();

zw_netstatus zw_networkmanagent_status();

void zw_networkmanagent_addnode(uint8_t mode);
void zw_networkmanagent_addnodestop();

void zw_networkmanagent_remove(uint8_t mode);
void zw_networkmanagent_removestop();

void zw_networkmanagent_setdefault();

int zw_networkmanagent_removefailednode(uint8_t node);

#endif /* HAL_ZW_NETWORKMANAGENT_H_ */
