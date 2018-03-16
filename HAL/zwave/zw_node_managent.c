/*
 * zw_node_managent.c
 *
 *  Created on: 2017年12月14日
 *      Author: cj
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"

#include "zw_node_managent.h"
#include "zw_send.h"

#ifdef ZW_FILE_EPROM

static int fd;

void file_read(unsigned long addr, void *data, int size) {
	lseek(fd, addr, SEEK_SET);
	read(fd, data, size);
}

void file_write(unsigned long addr, void *data, int size) {
	ssize_t wlen = 0;
	lseek(fd, addr, SEEK_SET); //如果执行失败(比如offset超过文件自身大小)，则不改变stream指向的位置。
	wlen = write(fd, data, size);
	if (wlen != size) {
		log_e("err:%d", wlen);
	}
}

//232 * sizeof(zw_node_t)=29k
void zw_node_managnet_init() {
	fd = open("zwave_eeprom.data", O_RDWR | O_CREAT, 0644);
	if (fd > 0) {
		lseek(fd, 0, SEEK_SET);
	}
}

static void _epnode_read(uint8_t node, zw_node_t *value) {
	file_read(node * sizeof(zw_node_t), value, sizeof(zw_node_t));
}

static void _epnode_write(uint8_t node, zw_node_t *value) {
	file_write(node * sizeof(zw_node_t), value, sizeof(zw_node_t));
}

int zw_node_exists(uint8_t node) {
	zw_node_t nodeinfo;
	memset(&nodeinfo, 0, sizeof(zw_node_t));
	_epnode_read(node, &nodeinfo);
	return nodeinfo.node != 0;
}

void zw_node_managnet_add(uint8_t node, NODEINFO *info, uint8_t isSlaveApi,
		uint8_t *cls) {
	zw_node_t nodeinfo;
	int i = 0;
	memset(&nodeinfo, 0, sizeof(zw_node_t));
	nodeinfo.node = node;
	memcpy(&nodeinfo.info, info, sizeof(NODEINFO));
	nodeinfo.isSlaveApi = isSlaveApi;
	for (i = 0; i < 50; i++) {
		if (cls[i] == 0)
		break;
		nodeinfo.cls[i] = cls[i];
	}
	_epnode_write(node, &nodeinfo);
}

void zw_node_managnet_setS0(uint8_t node, uint8_t scheme) {
	zw_node_t nodeinfo;
	memset(&nodeinfo, 0, sizeof(nodeinfo));
	_epnode_read(node, &nodeinfo);
	nodeinfo.scheme = ZW_NODE_SUPER_S0;
	_epnode_write(node, &nodeinfo);
}

int zw_node_managent_getScheme(uint8_t node, int cls) {
	zw_node_t nodeinfo;
	int i = 0;
	memset(&nodeinfo, 0, sizeof(nodeinfo));
	_epnode_read(node, &nodeinfo);
	if (nodeinfo.scheme == ZW_NODE_SUPER_S0) {
		for (i = 0; i < sizeof(nodeinfo.clss0); i++) {
			if (nodeinfo.clss0[i] == 0)
			return SCHEME_NO;
			if (nodeinfo.clss0[i] == cls)
			return SCHEME_SECURITY_0;
		}
	}
	return SCHEME_NO;
}

void zw_node_managent_setSuperS0(uint8_t node, uint8_t *cls, int clslen) {
	zw_node_t nodeinfo;
	int i = 0;
	memset(&nodeinfo, 0, sizeof(nodeinfo));
	_epnode_read(node, &nodeinfo);
	nodeinfo.scheme = ZW_NODE_SUPER_S0;
	for (i = 0; i < clslen; i++) {
		nodeinfo.clss0[i] = cls[i];
	}
	_epnode_write(node, &nodeinfo);
}

void zw_node_managent_delete(uint8_t node) {
	zw_node_t nodeinfo;
	memset(&nodeinfo, 0, sizeof(nodeinfo));
	_epnode_write(node, &nodeinfo);
}

void zw_node_managent_sync(uint8_t node, NODEINFO *info) {
	zw_node_t nodeinfo;
	memset(&nodeinfo, 0, sizeof(nodeinfo));
	_epnode_read(node, &nodeinfo);
	nodeinfo.node = node;
	memcpy(&nodeinfo.info, info, sizeof(NODEINFO));
	_epnode_write(node, &nodeinfo);
}

void zw_node_managent_update_lasttime(uint8_t node) {
	zw_node_t nodeinfo;
	memset(&nodeinfo, 0, sizeof(nodeinfo));
	_epnode_read(node, &nodeinfo);
	nodeinfo.lastUpdate = time(NULL);
	_epnode_write(node, &nodeinfo);
}

void zw_node_managent_update_failing(uint8_t node, int isfailing) {
	zw_node_t nodeinfo;
	memset(&nodeinfo, 0, sizeof(nodeinfo));
	_epnode_read(node, &nodeinfo);
	nodeinfo.lastAwake = time(NULL);
	nodeinfo.state = isfailing;
	_epnode_write(node, &nodeinfo);
	log_d("failing->(%d)  %d", node, isfailing);
}

int zw_node_managent_issupper(uint8_t node, int cls) {
	zw_node_t nodeinfo;
	int i=0;
	memset(&nodeinfo, 0, sizeof(nodeinfo));
	_epnode_read(node, &nodeinfo);
	for(i=0;i<sizeof(nodeinfo.cls);i++) {
		if(nodeinfo.cls[i]==0)
		break;
		if(nodeinfo.cls[i]==cls)
		return 1;
	}
	return 0;
}

void zw_node_managnet_debug() {
	zw_node_t nodeinfo;
	int i = 0, j;

	printf("====================================%ld(sum:%d)==\n",
			sizeof(zw_node_t), 232 * sizeof(zw_node_t));
	for (i = 1; i < 232; i++) {
		memset(&nodeinfo, 0, sizeof(nodeinfo));
		_epnode_read(i, &nodeinfo);
		if (nodeinfo.node == 0)
		continue;

		printf("-->>>> node=%d  ", nodeinfo.node);
		printf("\t cap=%02X, res=%02X, sec=%02X, basic=%02X  G=%02X S=%02X\n",
				nodeinfo.info.capability, nodeinfo.info.reserved,
				nodeinfo.info.security, nodeinfo.info.nodeType.basic,
				nodeinfo.info.nodeType.generic,
				nodeinfo.info.nodeType.specific);
		if (nodeinfo.info.capability & NODEINFO_LISTENING_SUPPORT) {
			printf("listening \n");
		} else {
			printf("no listening \n");
		}
		printf("\tcls:");
		for (j = 0; j < 50; j++) {
			if (nodeinfo.cls[j] == 0)
			break;
			printf("%02X ", nodeinfo.cls[j]);
		}
		printf("\n");

		printf("\tscheme = %02X, lastUpdate=%d state=%d\n", nodeinfo.scheme,
				nodeinfo.lastUpdate, nodeinfo.state);
		if (nodeinfo.scheme == ZW_NODE_SUPER_S0) {
			printf("\tcls-s0:");
			for (j = 0; j < 50; j++) {
				if (nodeinfo.clss0[j] == 0)
				break;
				printf("%02X ", nodeinfo.clss0[j]);
			}
			printf("\n");
		}
		fflush(stdout);
	}
	printf("======================================\n");
}
#else

zwave_node_t ZWaveAllNodes[0xFF];

void zw_node_managnet_init() {
	memset(ZWaveAllNodes, 0, sizeof(ZWaveAllNodes));
}

int zw_node_exists(uint8_t node) {
	return ZWaveAllNodes[node].node == 0 ? 0 : 1;
}

void zw_node_managnet_add(uint8_t node, NODEINFO *info, uint8_t isSlaveApi,
		uint8_t *cls) {
	int i = 0;
	memset(&ZWaveAllNodes[node], 0, sizeof(zwave_node_t));

	ZWaveAllNodes[node].node = node;
	ZWaveAllNodes[node].nodeInfo.capability = info->capability;
	ZWaveAllNodes[node].nodeInfo.reserved = info->reserved;
	ZWaveAllNodes[node].nodeInfo.security = info->security;
	ZWaveAllNodes[node].nodeInfo.nodeType.basic = info->nodeType.basic;
	ZWaveAllNodes[node].nodeInfo.nodeType.generic = info->nodeType.generic;
	ZWaveAllNodes[node].nodeInfo.nodeType.specific = info->nodeType.specific;
	ZWaveAllNodes[node].isSlaveApi = isSlaveApi;

	for (i = 0; i < 50; i++) {
		if (cls[i] == 0)
			break;
		ZWaveAllNodes[node].cmdclass[i] = cls[i];
	}
}

void zw_node_managnet_setS0(uint8_t node, uint8_t scheme) {
	ZWaveAllNodes[node].securitychannel = ZW_NODE_SUPER_S0;
}

int zw_node_managent_getScheme(uint8_t node, int cls) {
	int i = 0;
	if (ZWaveAllNodes[node].securitychannel == ZW_NODE_SUPER_S0) {
		for (i = 0; i < sizeof(ZWaveAllNodes[node].clsSuperSecurity); i++) {
			if (ZWaveAllNodes[node].clsSuperSecurity[i] == 0)
				return SCHEME_NO;
			if (ZWaveAllNodes[node].clsSuperSecurity[i] == cls)
				return SCHEME_SECURITY_0;
		}
	}
	return SCHEME_NO;
}

void zw_node_managent_setSuperS0(uint8_t node, uint8_t *cls, int clslen) {
	int i = 0;
	ZWaveAllNodes[node].securitychannel = ZW_NODE_SUPER_S0;
	for (i = 0; i < clslen; i++) {
		ZWaveAllNodes[node].clsSuperSecurity[i] = cls[i];
	}
}

extern void hal_sync_zwave_node();

void zw_node_managent_delete(uint8_t node) {
	ZWaveAllNodes[node].node = 0;
	hal_sync_zwave_node();
}

void zw_node_managent_sync(uint8_t node, int exists, NODEINFO *info) {
	if (!exists) {
		ZWaveAllNodes[node].node = 0;
		return;
	}
	ZWaveAllNodes[node].node = node;
	ZWaveAllNodes[node].nodeInfo.capability = info->capability;
	ZWaveAllNodes[node].nodeInfo.reserved = info->reserved;
	ZWaveAllNodes[node].nodeInfo.security = info->security;
	ZWaveAllNodes[node].nodeInfo.nodeType.basic = info->nodeType.basic;
	ZWaveAllNodes[node].nodeInfo.nodeType.generic = info->nodeType.generic;
	ZWaveAllNodes[node].nodeInfo.nodeType.specific = info->nodeType.specific;
}

void zw_node_managent_update_lasttime(uint8_t node) {
}

void zw_node_managent_update_failing(uint8_t node, int isfailing) {
	ZWaveAllNodes[node].failed = isfailing;
}

int zw_node_managent_issupper(uint8_t node, int cls) {
	int i = 0;
	for (i = 0; i < sizeof(ZWaveAllNodes[node].cmdclass); i++) {
		if (ZWaveAllNodes[node].cmdclass[i] == 0)
			break;
		if (ZWaveAllNodes[node].cmdclass[i] == cls)
			return 1;
	}
	return 0;
}

void zw_node_managent_adapter_end(uint8_t node) {
	hal_sync_zwave_node();
}

void zw_node_managnet_debug() {

}

#endif
