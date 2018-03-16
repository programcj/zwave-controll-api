/*
 * zw_node_managent.h
 *
 *  Created on: 2017年12月14日
 *      Author: cj
 */

#ifndef HAL_ZW_NODE_MANAGENT_H_
#define HAL_ZW_NODE_MANAGENT_H_

#include <stddef.h>
#include "port.h"
#include "Serialapi.h"

#define ZW_NODE_SUPER_S0		1

#ifdef ZW_FILE_EPROM

typedef struct zw_node {
	uint8_t node;
	NODEINFO info;
	uint8_t isSlaveApi;
	uint8_t scheme; //是否走安全0 , 1=s0, 2=s2

	int state;

	uint32_t wakeUp_interval;
	uint32_t lastAwake;
	uint32_t lastUpdate;

	uint16_t manufacturerID;
	uint16_t productType;
	uint16_t productID;

	uint8_t cls[50];
	uint8_t clss0[50];
	unsigned char _temp[100];
} zw_node_t;

void zw_node_managnet_init();

int zw_node_exists(uint8_t node);

void zw_node_managnet_add(uint8_t node, NODEINFO *info, uint8_t isSlaveApi,
		uint8_t *cls);

void zw_node_managnet_setS0(uint8_t node, uint8_t scheme);

int zw_node_managent_getScheme(uint8_t node, int cls);

void zw_node_managent_setSuperS0(uint8_t node, uint8_t *cls, int clslen);

void zw_node_managent_delete(uint8_t node);

void zw_node_managent_sync(uint8_t node, NODEINFO *info);

void zw_node_managent_update_lasttime(uint8_t node);

void zw_node_managent_update_failing(uint8_t node, int isfailing);

int zw_node_managent_issupper(uint8_t node, int cls);

void zw_node_managnet_debug();

#else


#define ZWAVE_NODE_MAX	232
#define MY_NODE_CMD_CLASS_SIZE 50

typedef struct  {
	uint8_t basic; /* Basic Device Type, is it a Controller, Controller_Static, */
	/* Slave or a Slave_Routing Device Type */
	uint8_t generic; /* Generic Device Type */
	uint8_t specific; /* Specific Device Type */
} ZNODE_TYPE_t;

/* Node info stored within the non-volatile memory */
/* This are the first (protocol part) payload uint8_ts from the Node Infomation frame */
typedef struct _ZNODEINFO_ {
	uint8_t capability; /* Network capabilities */
	uint8_t security; /* Network security */
	uint8_t reserved;
	ZNODE_TYPE_t nodeType; /* Basic, Generic and Specific Device types - Basic is generated... */
} ZNODEINFO;

typedef struct zwave_node {
	unsigned char node;
	ZNODEINFO nodeInfo;
	uint8_t cmdclass[MY_NODE_CMD_CLASS_SIZE]; //max 32   super/control
	uint8_t version[MY_NODE_CMD_CLASS_SIZE];    //cmdclass version,每个命令类的版本
	uint8_t clsSuperSecurity[MY_NODE_CMD_CLASS_SIZE];  //安全

	uint8_t isSlaveApi;
	uint8_t securitychannel; //是否走安全通道0 1 2(安全适配出错)
	uint8_t failed; //是否失效了
	int time_last; //最后通信时间
} zwave_node_t;

extern zwave_node_t ZWaveAllNodes[0xFF]; //所有的节点,下标即节点 id

void zw_node_managnet_init();

int zw_node_exists(uint8_t node);

void zw_node_managnet_add(uint8_t node, NODEINFO *info, uint8_t isSlaveApi,
		uint8_t *cls);

void zw_node_managnet_setS0(uint8_t node, uint8_t scheme);

int zw_node_managent_getScheme(uint8_t node, int cls);

void zw_node_managent_setSuperS0(uint8_t node, uint8_t *cls, int clslen);

void zw_node_managent_delete(uint8_t node);

void zw_node_managent_sync(uint8_t node,int exists, NODEINFO *info);

void zw_node_managent_update_lasttime(uint8_t node);

void zw_node_managent_update_failing(uint8_t node, int isfailing);

int zw_node_managent_issupper(uint8_t node, int cls);

void zw_node_managent_adapter_end(uint8_t node);

void zw_node_managnet_debug();

#endif

#endif /* HAL_ZW_NODE_MANAGENT_H_ */
