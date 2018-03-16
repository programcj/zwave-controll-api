/*
 * zw_networkmanagent.c
 *
 *  Created on: 2017年12月11日
 *      Author: cj
 */

#include "config.h"

#include "zw_networkmanagent.h"
#include "zw_node_managent.h"
#include "port.h"
#include "Serialapi.h"
#include "zw_s0_layer.h"

static volatile zw_netstatus _netstatus;

zw_netstatus zw_networkmanagent_status() {
	return _netstatus;
}

void zw_networkmanagent_setstatus(zw_netstatus status) {
	_netstatus = status;
}

int node_issuppercls(unsigned char *data, int dlen, int cls) {
	int i;
	for (i = 0; i < dlen; i++) {
		if (data[i] == cls)
			return 1;
	}
	return 0;
}

struct NodeInfoTmp {
	BYTE node;
	NODEINFO info;
	BYTE cls[90];
	BYTE isSlaveApi;
} tempNode;

static struct ctimer ctime_netmang;

//const static uint8_t set_no_more_info[] = { COMMAND_CLASS_WAKE_UP,  WAKE_UP_NO_MORE_INFORMATION_V2};

static void _wake_set_call(int status, void *context) {
	zw_tx_param_t nodep;
	nodep.node = (int) context;
	nodep.scheme = SCHEME_AUTO;
	//zw_senddata_first(&nodep, (uint8_t*) &set_no_more_info, sizeof(set_no_more_info), NULL, NULL);
}

void node_wake_up_set(int node, int seconds) {
	zw_tx_param_t nodep;
	ZW_WAKE_UP_INTERVAL_SET_V2_FRAME frame;
	frame.cmdClass = COMMAND_CLASS_WAKE_UP;
	frame.cmd = WAKE_UP_INTERVAL_SET;
	frame.seconds1 = (0x00FF0000 & seconds) >> 16;
	frame.seconds2 = (0x00FF00 & seconds) >> 8;
	frame.seconds3 = 0x00FF & seconds;
	frame.nodeid = ZWaveControllerInfo.this_SUCNodeID == 0 ? 1 : ZWaveControllerInfo.this_SUCNodeID;

	nodep.node = node;
	nodep.scheme = SCHEME_AUTO;
	zw_senddata_first(&nodep, (uint8_t*) &frame, sizeof(frame), _wake_set_call, (int*) (int) node);
}

static void _node_adapter_end(uint8_t node) {
	if (zw_node_managent_issupper(tempNode.node, COMMAND_CLASS_WAKE_UP)) {
		int seconds = 300;
		log_d("wake up set");
		node_wake_up_set(tempNode.node, seconds);
	}

	log_d("========================================入网查询完成==================");
	zw_node_managent_adapter_end(node);
}

//报告新设备
//读取Plus info
//具体制造商 MSB是Most Significant Bit的缩写，最高有效位;LSB（Least Significant Bit），意为最低有效位
//读取每个协议的版本 COMMAND_CLASS_VERSION_V2
//关联
//判断是否是睡眠设备
//	----Y----发送COMMAND_CLASS_WAKE_UP-WAKE_UP_INTERVAL_SET

const static uint8_t firmware_md_get[] = { COMMAND_CLASS_FIRMWARE_UPDATE_MD_V2, FIRMWARE_MD_GET_V2 };
const static uint8_t switch_bin_get[] = { COMMAND_CLASS_SWITCH_BINARY_V2, SWITCH_BINARY_GET_V2 };
const static uint8_t battery_get[] = { COMMAND_CLASS_BATTERY, BATTERY_GET };
const static uint8_t get_plus[] = { COMMAND_CLASS_ZWAVEPLUS_INFO_V2, ZWAVEPLUS_INFO_GET_V2 };
const static uint8_t get_manufacture[] = { COMMAND_CLASS_MANUFACTURER_SPECIFIC_V2,
MANUFACTURER_SPECIFIC_GET_V2 };
const static uint8_t get_cen_scensup[] = { COMMAND_CLASS_CENTRAL_SCENE_V2,
CENTRAL_SCENE_SUPPORTED_GET_V2 };

struct AdapterValueGet {
	const char *name;
	const uint8_t *getcmds;
	uint8_t getcmdssize;
	uint8_t reportcmd;
} adapterValueGets[] = {
#define VALUEGET_ITEM(vv, report) 	{ .name=#vv, .getcmds=vv, .getcmdssize=sizeof(vv), .reportcmd=report}
		VALUEGET_ITEM(get_plus, ZWAVEPLUS_INFO_REPORT_V2),
		VALUEGET_ITEM(get_manufacture, MANUFACTURER_SPECIFIC_REPORT_V2),
		VALUEGET_ITEM(firmware_md_get, FIRMWARE_MD_REPORT_V2),
		VALUEGET_ITEM(switch_bin_get, SWITCH_BINARY_REPORT),
		VALUEGET_ITEM(battery_get,BATTERY_REPORT),
		VALUEGET_ITEM(get_cen_scensup, CENTRAL_SCENE_SUPPORTED_REPORT_V2),
		VALUEGET_ITEM(NULL, 0) };

static void _node_adapter_value(int status, uint8_t node, uint8_t *data, uint8_t len, void *context);

int _node_adapter_super_next(int index) {
	zw_tx_param_t nodep;
	nodep.node = tempNode.node;
	nodep.scheme = SCHEME_AUTO;
	struct AdapterValueGet *v = &adapterValueGets[index];

	while (v->getcmds != NULL) {
		if (zw_node_managent_issupper(tempNode.node, v->getcmds[0])) {
			log_d("获取(node=%d). %s", tempNode.node, v->name);
			if (v->reportcmd == 0) {
				zw_senddata_first(&nodep, v->getcmds, v->getcmdssize, NULL,
				NULL);
			} else {
				zw_senddata_request(&nodep, v->getcmds, v->getcmdssize, v->getcmds[0], v->reportcmd,
						5 * 1000, _node_adapter_value, (int*) index);
				return 0;
			}
		}
		v++;
	}
	return -1;
}

static void _node_adapter_value(int status, uint8_t node, uint8_t *data, uint8_t len, void *context) {
	ZW_APPLICATION_TX_BUFFER *frame = (ZW_APPLICATION_TX_BUFFER*) data;
	int index = (int) context;

	if (status == TRANSMIT_COMPLETE_OK) {
		log_d("node=%d, Cls:%02X-%02X", node, frame->ZW_Common.cmdClass, frame->ZW_Common.cmd);

		if (frame->ZW_Common.cmdClass == COMMAND_CLASS_ZWAVEPLUS_INFO_V2
				&& frame->ZW_Common.cmd == ZWAVEPLUS_INFO_REPORT_V2) {
			dao_zdevices_setattrib(node, "PlusInfo", "%d,%d,%d,%d,%d,%d,%d",
					frame->ZW_ZwaveplusInfoReportV2Frame.zWaveVersion, /**/
					frame->ZW_ZwaveplusInfoReportV2Frame.roleType, /**/
					frame->ZW_ZwaveplusInfoReportV2Frame.nodeType, /**/
					frame->ZW_ZwaveplusInfoReportV2Frame.installerIconType1, /* MSB */
					frame->ZW_ZwaveplusInfoReportV2Frame.installerIconType2, /* LSB */
					frame->ZW_ZwaveplusInfoReportV2Frame.userIconType1, /* MSB */
					frame->ZW_ZwaveplusInfoReportV2Frame.userIconType2); /* LSB */
		}
		if (frame->ZW_Common.cmdClass == COMMAND_CLASS_MANUFACTURER_SPECIFIC_V2
				&& frame->ZW_Common.cmd == MANUFACTURER_SPECIFIC_REPORT_V2) {
			log_d("制造商:%04X-%04X-%04X",
					BYTE_2_SHORT(frame->ZW_ManufacturerSpecificReportV2Frame.manufacturerId1, frame->ZW_ManufacturerSpecificReportV2Frame.manufacturerId2),
					BYTE_2_SHORT(frame->ZW_ManufacturerSpecificReportV2Frame.productTypeId1, frame->ZW_ManufacturerSpecificReportV2Frame.productTypeId2),
					BYTE_2_SHORT(frame->ZW_ManufacturerSpecificReportV2Frame.productId1, frame->ZW_ManufacturerSpecificReportV2Frame.productId2));

			dao_zdevices_setattrib(node, "manufacturerinfo", "%d,%d,%d",
					BYTE_2_SHORT(frame->ZW_ManufacturerSpecificReportV2Frame.manufacturerId1,
							frame->ZW_ManufacturerSpecificReportV2Frame.manufacturerId2),
					BYTE_2_SHORT(frame->ZW_ManufacturerSpecificReportV2Frame.productTypeId1,
							frame->ZW_ManufacturerSpecificReportV2Frame.productTypeId2),
					BYTE_2_SHORT(frame->ZW_ManufacturerSpecificReportV2Frame.productId1,
							frame->ZW_ManufacturerSpecificReportV2Frame.productId2));
		}
		if (frame->ZW_Common.cmdClass == COMMAND_CLASS_FIRMWARE_UPDATE_MD_V2
				&& frame->ZW_Common.cmd == FIRMWARE_MD_REPORT_V2) {
			int mid = 0;
			int fid = 0;
			int ck = 0;
			cJSON *json_array = cJSON_CreateArray();
			char tmp[20];
			char *tmpbuf = NULL;

			mid = BYTE_2_SHORT(frame->ZW_FirmwareMdReportFrame.manufacturerId1,
					frame->ZW_FirmwareMdReportFrame.manufacturerId2);
			fid = BYTE_2_SHORT(frame->ZW_FirmwareMdReportFrame.firmwareId1,
					frame->ZW_FirmwareMdReportFrame.firmwareId2);
			ck = BYTE_2_SHORT(frame->ZW_FirmwareMdReportFrame.checksum1,
					frame->ZW_FirmwareMdReportFrame.checksum2);
			sprintf(tmp, "%04X%04X%04X", mid, fid, ck);
			cJSON_AddItemToArray(json_array, cJSON_CreateString(tmp));

			tmpbuf = cJSON_Print(json_array);
			dao_zdevices_setattrib(node, "firmware", tmpbuf); //目前只有一个固件
			free(tmpbuf);
			cJSON_Delete(json_array);
		}
		if (frame->ZW_Common.cmdClass == COMMAND_CLASS_BATTERY
				&& frame->ZW_Common.cmd == BATTERY_REPORT) {
			BYTE value =
					frame->ZW_BatteryReportFrame.batteryLevel == 255 ?
							0 : frame->ZW_BatteryReportFrame.batteryLevel;
			dao_zdevices_setattrib(node, "80", "%d", value);
		}

		if (frame->ZW_Common.cmdClass == COMMAND_CLASS_SWITCH_BINARY_V2
				&& frame->ZW_Common.cmd == SWITCH_BINARY_REPORT_V2) {
			if (frame->ZW_SwitchBinaryReportFrame.value == 0)
				dao_zdevices_setattrib(node, "25", "0");
			else
				dao_zdevices_setattrib(node, "25", "1");
		}

		if (frame->ZW_Common.cmdClass == COMMAND_CLASS_CENTRAL_SCENE_V2
				&& frame->ZW_Common.cmd == CENTRAL_SCENE_SUPPORTED_REPORT_V2) {
			dao_zdevices_setattrib(node, "5B.01", "%d",
					frame->ZW_CentralSceneSupportedReportFrame.supportedScenes);
		}
	}

	if (_node_adapter_super_next(index + 1) != 0)
		_node_adapter_end(tempNode.node);
}

void _node_adapter_callback(int status) {
	log_d("===================入网成功>%d, status=%d", tempNode.node, status);

	zw_tx_param_t nodep;
	nodep.node = tempNode.node;
	nodep.scheme = SCHEME_AUTO;

	{ //设置关联节点
		uint8_t frame[] = { COMMAND_CLASS_ASSOCIATION_V2, ASSOCIATION_SET, 0x01, 0x01 };
		log_d("%d 关联节点 ", tempNode.node);
		zw_senddata_first(&nodep, (uint8_t*) &frame, sizeof(frame), NULL, NULL);
	}

	if (_node_adapter_super_next(0) != 0)
		_node_adapter_end(tempNode.node);
}

void _addNodeTimeOut(void *args) {
	log_d("time out");
	_netstatus = ZW_NETSTATUS_NOT;
	ZW_AddNodeToNetwork(ADD_NODE_STOP, NULL);
	zw_hook_network_addnodestatus(ADD_NODE_STATUS_DONE, 0);
}

static void AddNodeStatusUpdate(LEARN_INFO* inf) {
	zw_hook_network_addnodestatus(inf->bStatus, inf->bSource);

	switch (inf->bStatus) {
	case ADD_NODE_STATUS_LEARN_READY: //1 Z-Wave协议的准备包括新的节点。-
		memset(&tempNode, 0, sizeof(tempNode));
		log_d("Z-Wave protocol is ready to include new node.");
		ctimer_set(&ctime_netmang, 60 * 1000, _addNodeTimeOut, NULL);
		break;
	case ADD_NODE_STATUS_NODE_FOUND: //2 Z-Wave协议检测结。-
		log_d("Z-Wave protocol detected node.");
		break;
	case ADD_NODE_STATUS_ADDING_SLAVE: //03 Z-Wave协议包括一个控制器节点类型//可得到新节点-
		log_d("Z-Wave protocol included a slave type node");
	case ADD_NODE_STATUS_ADDING_CONTROLLER: // //04 Z-Wave协议包括奴隶式节点-
		log_d("Z-Wave protocol included a controller type node");
		if (inf->bLen >= 4) {
			tempNode.node = inf->bSource;
			tempNode.info.nodeType.basic = inf->pCmd[0];
			tempNode.info.nodeType.generic = inf->pCmd[1];
			tempNode.info.nodeType.specific = inf->pCmd[2];
			//supported cmdclass
			memcpy(tempNode.cls, inf->pCmd + 3, inf->bLen - 3);
			if (inf->bStatus == ADD_NODE_STATUS_ADDING_SLAVE)
				tempNode.isSlaveApi = 1;
			ZW_GetNodeProtocolInfo(tempNode.node, &tempNode.info);
		}
		break;
	case ADD_NODE_STATUS_PROTOCOL_DONE: //05 Z-Wave协议完成业务相关的包含。如果新的节点类型是控制器，控制器应用程序可以调用应用程序复制。-
		log_d("Z-Wave protocol completed operations related to inclusion.");
		ZW_AddNodeToNetwork(ADD_NODE_STOP, AddNodeStatusUpdate);
		break;
	case ADD_NODE_STATUS_DONE: //06 所有操作完成。协议准备返回空闲状态。-
		log_d("All operations completed.");
		ctimer_stop(&ctime_netmang);
		ZW_AddNodeToNetwork(ADD_NODE_STOP, NULL);
		zw_networkmanagent_setstatus(ZW_NETSTATUS_NOT);

		if (tempNode.node != 0) {
			log_d("入网节点:%d", tempNode.node);
			//判断是否已经入网
			zw_node_managnet_add(tempNode.node, &tempNode.info, tempNode.isSlaveApi, tempNode.cls);
			//未入网
			ApplicationControllerUpdate(UPDATE_STATE_NEW_ID_ASSIGNED, tempNode.node, 0, 0);
			if (node_issuppercls(tempNode.cls, sizeof(tempNode.cls),
			COMMAND_CLASS_SECURITY)) {
				log_d("安全适配中");
				zw_s0_add_start(tempNode.node, tempNode.isSlaveApi, _node_adapter_callback);
				return;
			}
			_node_adapter_callback(0);
		}
		break;
	case ADD_NODE_STATUS_FAILED:
		//Z-Wave协议的报告，包含不成功。新节点没有准备好操作-
	case ADD_NODE_STATUS_NOT_PRIMARY:
		//Z-Wave协议报告所请求的操作不能执行，因为它需要的节点是主控制器状态。
		log_d("Inclusion was not successful.");
		ZW_AddNodeToNetwork(ADD_NODE_STOP, AddNodeStatusUpdate);
		break;
	}
}

void zw_networkmanagent_addnode(uint8_t mode) {
	zw_networkmanagent_setstatus(ZW_NETSTATUS_ADDNODEING);
	ZW_AddNodeToNetwork(ADD_NODE_ANY, AddNodeStatusUpdate);
}

void zw_networkmanagent_addnodestop() {
	ZW_AddNodeToNetwork(ADD_NODE_STOP, AddNodeStatusUpdate);
}

////=====================================================================
void _removeNodeTimeOut(void *args) {
	log_d("remove time out");
	zw_networkmanagent_setstatus(ZW_NETSTATUS_NOT);
	ZW_RemoveNodeFromNetwork(REMOVE_NODE_STOP, 0);
	zw_hook_network_removenodestatus(REMOVE_NODE_STATUS_DONE, 0);
}

static void RemoveNodeStatusUpdate(LEARN_INFO* inf) {
	//ZW_NODE_REMOVE_STATUS_FRAME* r = (ZW_NODE_REMOVE_STATUS_FRAME*) &nms.buf;
	log_d("status=%d node %d", inf->bStatus, inf->bSource);

	zw_hook_network_removenodestatus(inf->bStatus, tempNode.node);

	switch (inf->bStatus) {
	case REMOVE_NODE_STATUS_LEARN_READY:
		memset(&tempNode, 0, sizeof(tempNode));
		ctimer_set(&ctime_netmang, 30 * 1000, _removeNodeTimeOut, NULL);
		break;
	case REMOVE_NODE_STATUS_NODE_FOUND:
		break;
	case REMOVE_NODE_STATUS_REMOVING_SLAVE:
	case REMOVE_NODE_STATUS_REMOVING_CONTROLLER:
		tempNode.node = inf->bSource;
		break;
	case REMOVE_NODE_STATUS_DONE: {
		log_d("Node Removed %d", tempNode.node);
		zw_networkmanagent_setstatus(ZW_NETSTATUS_NOT);
		ApplicationControllerUpdate(UPDATE_STATE_DELETE_DONE, tempNode.node, 0, 0);
		if (tempNode.node != 0)
			zw_node_managent_delete(tempNode.node);
	}
	case REMOVE_NODE_STATUS_FAILED:
		ctimer_stop(&ctime_netmang);
		ZW_RemoveNodeFromNetwork(REMOVE_NODE_STOP, 0);
		break;
	}

}

void zw_networkmanagent_remove(uint8_t mode) {
	memset(&tempNode, 0, sizeof(tempNode));
	log_d("出网开始");
	zw_networkmanagent_setstatus(ZW_NETSTATUS_REMOVEING);
	ZW_RemoveNodeFromNetwork(REMOVE_NODE_ANY, RemoveNodeStatusUpdate);
}
void zw_networkmanagent_removestop() {
	LEARN_INFO inf;
	inf.bStatus = REMOVE_NODE_STATUS_DONE;
	log_d("出网退出");
	RemoveNodeStatusUpdate(&inf);
}
static void _zw_setdefault_callback() {
	log_d("set default");
	zw_networkmanagent_setstatus(ZW_NETSTATUS_NOT);
	//清空所有节点
	zw_hook_network_setdefault();
}

void zw_networkmanagent_setdefault() {
	zw_networkmanagent_setstatus(ZW_NETSTATUS_SETDEFAULT);
	ZW_SetDefault(_zw_setdefault_callback);
}
static uint8_t _failed_node = 0;

void remove_failed_callback(BYTE status) {
	//ZW_FAILED_NODE_REMOVED
	//ZW_FAILED_NODE_NOT_REMOVED
	if (status == ZW_FAILED_NODE_REMOVED)
		zw_node_managent_delete(_failed_node);
	zw_hook_network_removefailednode(status, _failed_node);
	_failed_node = 0;
}

int zw_networkmanagent_removefailednode(uint8_t node) {
	int ret = 0;
	if (_failed_node == 0) { //
		_failed_node = node;
		ret = ZW_RemoveFailedNode(node, remove_failed_callback);
		if (ret == ZW_FAILED_NODE_REMOVE_STARTED)
			return ret;
		return ret;
	}
	return -1;
}
