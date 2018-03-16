/*
 * zwave_app.c
 *
 *  Created on: 2017年5月24日
 *      Author: cj
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "config.h"

#include "zwave_app.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <errno.h>

#include "port.h"
#include "Serialapi.h"
#include "mem.h"
#include "zw_send.h"
#include "zw_s0_layer.h"
#include "zw_node_managent.h"
#include "zw_networkmanagent.h"
#include "z_ctimer.h"

#define free	mem_free
#define malloc mem_malloc

//===========================================================================
static void _ApplicationNodeInformation(BYTE *deviceOptionsMask, /*OUT Bitmask with application options    */
APPL_NODE_TYPE *nodeType, /*OUT  Device type Generic and Specific   */
BYTE **nodeParm, /*OUT  Device parameter buffer pointer    */
BYTE *parmLength /*OUT  Number of Device parameter bytes   */
) {
	/**
	 * 这里是我支持的所有的命令类的说明信息NodeInfo,还包括安全回应部分
	 */
	static BYTE MyClasses[] = {
	COMMAND_CLASS_ZWAVEPLUS_INFO_V2, //Z-Wave Plus
			COMMAND_CLASS_VERSION_V2,		 //version
			COMMAND_CLASS_MANUFACTURER_SPECIFIC_V2,  //manufacturer
			COMMAND_CLASS_DEVICE_RESET_LOCALLY, //Device Reset Locally
			COMMAND_CLASS_ASSOCIATION_GRP_INFO, //Association Group Information
			COMMAND_CLASS_ASSOCIATION_V2, //Association (version 2 or new
			COMMAND_CLASS_APPLICATION_STATUS, //Application Status
			COMMAND_CLASS_SECURITY, //security
			COMMAND_CLASS_CRC_16_ENCAP, //crc16
			COMMAND_CLASS_POWERLEVEL,     //Power Level
			//COMMAND_CLASS_TRANSPORT_SERVICE  //
			COMMAND_CLASS_MARK,//////////////////
			COMMAND_CLASS_ASSOCIATION_V2,
			COMMAND_CLASS_BASIC,
			COMMAND_CLASS_ASSOCIATION_GRP_INFO,
			COMMAND_CLASS_CRC_16_ENCAP,
			COMMAND_CLASS_SWITCH_BINARY, //v1
			COMMAND_CLASS_SWITCH_COLOR_V2,
			COMMAND_CLASS_SWITCH_MULTILEVEL_V3,
			COMMAND_CLASS_SENSOR_BINARY_V2,
			COMMAND_CLASS_SENSOR_MULTILEVEL_V5,  //v1-v7
			COMMAND_CLASS_SECURITY,
			//COMMAND_CLASS_MULTI_CHANNEL_V4,
			COMMAND_CLASS_MULTI_CHANNEL_ASSOCIATION_V2,
			COMMAND_CLASS_METER_V3, //v1-v4
			COMMAND_CLASS_BATTERY, //v1
			COMMAND_CLASS_WAKE_UP_V2,
			COMMAND_CLASS_NOTIFICATION_V3, //v3 v4
			COMMAND_CLASS_CENTRAL_SCENE //v1
			};

	/* this is a listening node and it supports optional CommandClasses */
	*deviceOptionsMask = APPLICATION_NODEINFO_LISTENING
			| APPLICATION_NODEINFO_OPTIONAL_FUNCTIONALITY;

	nodeType->generic = GENERIC_TYPE_STATIC_CONTROLLER; /* Generic device type */
	nodeType->specific = SPECIFIC_TYPE_GATEWAY; /* Specific class */

	*nodeParm = MyClasses; /* Send list of known command classes. */
	*parmLength = sizeof(MyClasses); /* Set length*/
}

extern void zwave_hook_apphandler(unsigned char node, const void *data, int dlen);

const static uint8_t set_no_more_info[] = { COMMAND_CLASS_WAKE_UP, WAKE_UP_NO_MORE_INFORMATION_V2 };

void ApplicationCommandHandler(BYTE rxStatus, BYTE destNode,
BYTE sourceNode, ZW_APPLICATION_TX_BUFFER *pCmd, BYTE cmdLength) {
	log_hex(pCmd, cmdLength, "sourceNode=%d:", sourceNode);

	if (0 == zw_request_handle(sourceNode, pCmd, cmdLength))

		if (pCmd->ZW_Common.cmdClass == COMMAND_CLASS_WAKE_UP_V2
				&& pCmd->ZW_Common.cmd == WAKE_UP_NOTIFICATION_V2) {
			zw_tx_param_t nodep;
			nodep.node = sourceNode;
			nodep.scheme = SCHEME_AUTO;

			zw_senddata_first(&nodep, (uint8_t*) &set_no_more_info, sizeof(set_no_more_info), NULL,
					NULL);
			return;
		}
	zwave_hook_apphandler(sourceNode, pCmd, cmdLength);
}

static void _ApplicationCommandHandlerSerial(BYTE rxStatus, BYTE destNode,
BYTE sourceNode, ZW_APPLICATION_TX_BUFFER *pCmd, BYTE cmdLength) {
	zw_node_managent_update_lasttime(sourceNode);
	if (pCmd->ZW_Common.cmdClass == COMMAND_CLASS_SECURITY) {
		zw_s0_layer_CommandHandler(rxStatus, destNode, sourceNode, pCmd, cmdLength);
		return;
	}
	if (pCmd->ZW_Common.cmdClass == COMMAND_CLASS_SECURITY_2) {
		return;
	}
	ApplicationCommandHandler(rxStatus, destNode, sourceNode, pCmd, cmdLength);
}

void ApplicationControllerUpdate(BYTE bStatus, /*IN  Status event */
BYTE bNodeID, /*IN  Node id of the node that send node info */
BYTE* pCmd, /*IN  Pointer to Application Node information */
BYTE bLen /*IN  Node info length                        */
) {
	log_d("status=%d, NodeId=%d", bStatus, bNodeID);
}

const struct SerialAPI_Callbacks serial_api_callbacks = {
		_ApplicationCommandHandlerSerial, ///////////////////////////////
		_ApplicationNodeInformation, //ApplicationNodeInformation, /////////////////////////////////////////
		ApplicationControllerUpdate, //////////////////////////////////////
		0, //////////////////////////////////////////////////////////////////////////////////
		0, /////////////////////////////////////////
		0, //////////////////////////////////////////////////////////////////////////////////
		0, //////////////////////////////////////////////////////////////////////////////////
		0, };

struct list_queue _queue_frame;

struct zw_request_frame {
	struct list_head list;
	BYTE data[0xFF];
	int dlen;
};

void ZWHOOK_frame_push(void *data, int datalen) {
	struct zw_request_frame *frame = (struct zw_request_frame*) malloc(
			sizeof(struct zw_request_frame));
	frame->dlen = datalen;
	memcpy(frame->data, data, datalen);
	list_queue_append_tail2(&_queue_frame, &frame->list, 0);
}
//=======================================================================================
typedef struct _zwsync {
	struct list_head list;
	void (*callback)(void *context);
	void *context;
} zwsync_t;

struct list_queue _queue_async;

void zw_dispatch_async(void (*callback)(void *context), void *context) {
	zwsync_t *item = mem_malloc(sizeof(zwsync_t));
	item->callback = callback;
	item->context = context;
	list_queue_append_tail2(&_queue_async, &item->list, 0);
}

static void zwave_async_handle() {
	zwsync_t *item = NULL;
	struct list_head *p = list_queue_pop(&_queue_async);
	if (!p)
		return;
	item = list_entry(p, zwsync_t, list);
	item->callback(item->context);
	free(item);
}

struct zwave_controller_info ZWaveControllerInfo;

static struct ctimer  _tim_debugmem;

void __tim_debugmem_back(void *context){
	mem_debug();
	ctimer_set(&_tim_debugmem, 3*1000,__tim_debugmem_back , NULL);
}

void zwave_app_loop(const char *serialpath) {
	struct list_head *p = NULL;
	struct zw_request_frame *frame = NULL;

	mem_init();
	ctimer_init();
	list_queue_init(&_queue_frame);
	list_queue_init(&_queue_async);
	zw_send_init();
	zw_s0_init();
	//zwave_server_init();
	zw_node_managnet_init();

	SerialAPI_Init(serialpath, &serial_api_callbacks);
	{
		BYTE ver, len, capabilities, chip_type, chip_version;
		BYTE nodelist[32]; //nodelist
		BYTE controller_capability = 0;
		DWORD homeId;
		BYTE NodeID;
		int i = 0, nodeCount = 0, nodeId = 0, exists = 0;
		NODEINFO nodeInfo;
		char version[14];
		BYTE v_type = 0;

		memset(&ZWaveControllerInfo, 0, sizeof(ZWaveControllerInfo));

		v_type = ZW_Version((BYTE*) version); //ZWaveProtocolVersion.ZWaveProtocolSubVersion\0Library
		if (v_type < 7) {
			log_d("Z-Wave Version:%s,Type:%s", version, zw_lib_names[v_type]);
		}

		Get_SerialAPI_AppVersion(&ZWaveControllerInfo.appVer,
				&ZWaveControllerInfo.appSubVer);

		ZWaveControllerInfo.library = v_type;
		ZWaveControllerInfo.protoVer = atoi(version + 7);
		ZWaveControllerInfo.protoSubVer = atoi(version + 9);

		Get_SerialAPI_Manufacture(&ZWaveControllerInfo.manufactor_id1,
				&ZWaveControllerInfo.manufactor_id2,
				&ZWaveControllerInfo.product_type1,
				&ZWaveControllerInfo.product_type2,
				&ZWaveControllerInfo.product_id1,
				&ZWaveControllerInfo.product_id2);

		SerialAPI_GetInitData(&ver, &capabilities, &len, nodelist, &chip_type,
				&chip_version);
		controller_capability = ZW_GetControllerCapabilities();

		if (capabilities & GET_INIT_DATA_FLAG_SECONDARY_CTRL)
			log_d("I'am a Secondary controller");
		if (capabilities & GET_INIT_DATA_FLAG_IS_SUC)
			log_d("I'am SUC");
		if (capabilities & GET_INIT_DATA_FLAG_SLAVE_API)
			log_d("I'am slave");
		//ZW_AssignReturnRoute()
		MemoryGetID((BYTE*) &homeId, &NodeID);
		sprintf(ZWaveControllerInfo.deviceChip, "ZW%02X%02X", chip_type,
				chip_version);
		if (v_type < 7) {
			strcpy(ZWaveControllerInfo.libName, zw_lib_names[v_type]);
		}
		if (v_type == ZW_LIB_CONTROLLER_BRIDGE) {
			strcpy(ZWaveControllerInfo.libName, "ZW_LIB_CONTROLLER_BRIDGE");
		}
		memcpy(ZWaveControllerInfo.version, version, 14);
		//
		ZWaveControllerInfo.capabilities = controller_capability;
		ZWaveControllerInfo.this_HomeID = homeId;
		ZWaveControllerInfo.this_NodeID = NodeID == 0 ? 1 : NodeID;
		ZWaveControllerInfo.this_SUCNodeID = ZW_GetSUCNodeID();

		log_d(
				"GetInitData:ver=%02x; capabilities=%02x:C.%02x;chip_type=%02x; chip_version=%02x; len=%d,homeId=%08X,nodeId=%02X,"
						"sucnode=%d", ver, capabilities, controller_capability,
				chip_type, chip_version, len, homeId, NodeID,
				ZWaveControllerInfo.this_SUCNodeID);

		ZW_AddNodeToNetwork(ADD_NODE_STOP, 0);
		ZW_RemoveNodeFromNetwork(REMOVE_NODE_STOP, 0);
		ZW_SetLearnMode(ZW_SET_LEARN_MODE_DISABLE, 0);
		ZW_ControllerChange(CONTROLLER_CHANGE_STOP, NULL);

#define BIT8_TST(bit,array) ((array[(bit)>>3] >> ((bit) & 7)) & 0x1)

		for (i = 0, nodeCount = 0; i < ZW_MAX_NODES; i++) {
			nodeId = i + 1;

			if (BIT8_TST(i, nodelist)) {
				nodeCount++;
				ZW_GetNodeProtocolInfo(nodeId, &nodeInfo);
				exists = 1;
			} else {
				exists = 0;
			}
			zw_node_managent_sync(nodeId, exists, &nodeInfo);
		}
		if (nodeCount == 0) {
			//ZW_SetSUCNodeID(0x01, 0x01, 0, 0x01, NULL); //SIS
			if (ZW_IsPrimaryCtrl() == FALSE) {
				ZW_EnableSUC(TRUE, ZW_SUC_FUNC_NODEID_SERVER);
			}
		}

//	fd_set fds;
//	int n;
//	struct timeval tv;
//	int delay = 10;

		zw_hook_init_end();
	}

	ctimer_set(&_tim_debugmem, 3*1000, __tim_debugmem_back, NULL);
	while (1) {
		SerialAPI_Poll();
		p = list_queue_pop(&_queue_frame);
		if (p) {
			extern void Dispatch( BYTE *pData, int pDataLen);
			frame = list_entry(p, struct zw_request_frame, list);
			Dispatch(frame->data, frame->dlen);
			free(frame);
		}
		/////////////
		zw_senddata_first_handle();
		zw_senddata_end_handle();
		zw_s0_layer_handle();

		//zwave_server_handle();

		zwave_async_handle();
		ctimer_handle();

		usleep(100);

//		{
//			FD_ZERO(&fds);
//			FD_SET(STDIN_FILENO, &fds);
//
//			tv.tv_sec = delay / 1000;
//			tv.tv_usec = (delay % 1000) * 1000;
//			n = STDIN_FILENO;
//
//			if (select(n + 1, &fds, NULL, NULL, &tv) > 0) {
//				if (FD_ISSET(STDIN_FILENO, &fds)) {
//					char cmds[100];
//					memset(cmds, 0, sizeof(cmds));
//					scanf("%s", cmds);
//					//fgets(cmds, 100, STDIN_FILENO);
//					if (strcmp(cmds, "add") == 0) {
//						log_d("入网");
//						zw_networkmanagent_addnode();
//					}
//					if (strcmp(cmds, "remove") == 0) {
//						log_d("出网");
//						zw_networkmanagent_remove();
//					}
//					if (strcmp(cmds, "mem") == 0) {
//						mem_debug();
//					}
//					if (strcmp(cmds, "setdefault") == 0) {
//						zw_networkmanagent_setdefault();
//					}
//					if (strcmp(cmds, "node") == 0) {
//						zw_node_managnet_debug();
//					}
//					if (strcmp(cmds, "test") == 0) {
//						zw_tx_param_t node;
//						uint8_t data[] = { COMMAND_CLASS_WAKE_UP,
//						WAKE_UP_INTERVAL_GET_V2 };
//						node.node = 43;
//						node.scheme = SCHEME_AUTO;
//						zw_senddata_request(&node, data, 2,
//						COMMAND_CLASS_WAKE_UP,
//						WAKE_UP_INTERVAL_REPORT_V2, 2 * 1000, _test_callback,
//						NULL);
//					}
//					if (strcmp(cmds, "test1") == 0) {
//						zw_tx_param_t node;
//						uint8_t data[] = { COMMAND_CLASS_ASSOCIATION_V2,
//						ASSOCIATION_GET_V2 };
//						node.node = 43;
//						node.scheme = SCHEME_AUTO;
//						zw_senddata_request(&node, data, 2,
//						COMMAND_CLASS_ASSOCIATION_V2,
//						ASSOCIATION_REPORT_V2, 2 * 1000, _test_callback,
//						NULL);
//					}
//				}
//			}
//		}
	}
}
