#include "Serialapi.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "conhandle.h"
#include "include/ZW_basis_api.h"
#include "include/ZW_controller_api.h"
#include "include/ZW_controller_static_api.h"
#include "include/ZW_mem_api.h"
#include "include/ZW_SerialAPI.h"
#include "port.h"
#include "TYPES.H"      // Standard types

#define NEW_NODEINFO

#define MAX_RXQUEUE_LEN 10
#define INVALID_TIMER_HANDLE 255
#define BUF_SIZE  (74+64)

#define SER_PRINTF log_e
#define ERR_PRINTF log_e


typedef struct SerialAPICpabilities {
	BYTE appl_version;
	BYTE appl_revision;
	BYTE manufactor_id1;
	BYTE manufactor_id2;
	BYTE product_type1;
	BYTE product_type2;
	BYTE product_id1;
	BYTE product_id2;
	BYTE supported_bitmask[29];
	BYTE nodelist[29];
	BYTE chipType;
	BYTE chipVersion;
} SerialAPICpabilities_t;

SerialAPICpabilities_t capabilities;

#undef ASSERT
#ifdef SERIALAPI_DEBUG
#define ASSERT(a) {if(!(a)) __asm("int $3");}
#else
#define ASSERT(a) {if(!(a)) SER_PRINTF("Assertion failed at %s:%i\n", __FILE__,__LINE__ );}
#endif

#define SupportsCommand(cmd) (cmd==FUNC_ID_SERIAL_API_GET_CAPABILITIES || capabilities.supported_bitmask[((cmd-1)>>3)]  & (1<<((cmd-1) & 0x7)))

void SerialAPI_ApplicationNodeInformation( BYTE listening,
		APPL_NODE_TYPE nodeType, BYTE *nodeParm, BYTE parmLength);
void Dispatch( BYTE *pData, int pDataLen);
static int SendFrameWithResponse(BYTE cmd, BYTE *Buf, BYTE len, BYTE *reply,
BYTE *replyLen);

LEARN_INFO learnNodeInfo;

BYTE buffer[BUF_SIZE];
BYTE pCmd[BUF_SIZE];

static BYTE idx;
static BYTE byLen;
static BYTE byCompletedFunc;
static struct SerialAPI_Callbacks* callbacks;

#define MAX_SERIAL_RETRY 3
#define TIMEOUT_TIME 1600


#define STATUS_RXTIMEOUT 1
#define STATUS_FRAMESENT 2
#define STATUS_FRAMEERROR 3
#define STATUS_FRAMERECEIVED 4


#define IDX_CMD   2
#define IDX_DATA  3
#define data __data

static VOID_CALLBACKFUNC(cbFuncZWSendData) (BYTE, TX_STATUS_TYPE*);
static VOID_CALLBACKFUNC(cbFuncZWSendTestFrame) (BYTE);
static VOID_CALLBACKFUNC(cbFuncZWSendDataBridge) (BYTE, TX_STATUS_TYPE*);
static void (*cbFuncZWSendDataMulti)( BYTE txStatus);
static void (*cbFuncZWSendNodeInformation)( BYTE txStatus);
static void (*cbFuncMemoryPutBuffer)(void);
static void (*cbFuncZWSetDefault)(void);
static void (*cbFuncZWNewController)(LEARN_INFO*);
static void (*cbFuncRemoveNodeFromNetwork)(LEARN_INFO*);
static void (*cbFuncAddNodeToNetwork)(LEARN_INFO*);
static void (*cbFuncZWControllerChange)(LEARN_INFO*);

static void (*cbFuncZWReplicationSendData)( BYTE txStatus);
static void (*cbFuncZWAssignReturnRoute)( BYTE bStatus);
static void (*cbFuncZWAssignSUCReturnRoute)( BYTE bStatus);
static void (*cbFuncZWDeleteSUCReturnRoute)( BYTE bStatus);
static void (*cbFuncZWDeleteReturnRoute)( BYTE bStatus);
static void (*cbFuncZWSetLearnMode)(LEARN_INFO*);
static void (*cbFuncZWSetSlaveLearnMode)(BYTE bStatus, BYTE orgID, BYTE newID);
static void (*cbFuncZWRequestNodeNodeNeighborUpdate)( BYTE bStatus);
static void (*cbFuncZWSetSUCNodeID)( BYTE bSstatus);
static void (*cbFuncZWRequestNetworkUpdate)( BYTE txStatus);
static void (*cbFuncZWRemoveFailedNode)( BYTE txStatus);
static void (*cbFuncZWReplaceFailedNode)( BYTE txStatus);
/** @} */

const char* zw_lib_names[] = { "Unknown", "Static controller",
		"Bridge controller library", "Portable controller library",
		"Enhanced slave library", "Slave library", "Installer library", };

BYTE ZW_Random() {
	return PORT_RANDOM() & 0xFF;
}

BOOL SerialAPI_Init(const char* serial_port,
		const struct SerialAPI_Callbacks* _callbacks) {
	BYTE buf[14];
	BYTE listening;
	APPL_NODE_TYPE nodeType;
	BYTE *nodeParm;
	BYTE parmLength;
	BYTE type;
	//INIT_LIST_HEAD(&_ListRx);
	if (!ConInit(serial_port))
		return FALSE;

	cbFuncZWSendData = NULL;
	cbFuncZWSendTestFrame = NULL;
	cbFuncZWSendDataBridge = NULL;
	cbFuncZWSendDataMulti = NULL;
	cbFuncZWSendNodeInformation = NULL;
	cbFuncMemoryPutBuffer = NULL;
	cbFuncZWSetDefault = NULL;
	cbFuncZWNewController = NULL;
	cbFuncAddNodeToNetwork = NULL;
	cbFuncRemoveNodeFromNetwork = NULL;
	cbFuncZWControllerChange = NULL;
	cbFuncZWReplicationSendData = NULL;
	cbFuncZWAssignReturnRoute = NULL;
	cbFuncZWAssignSUCReturnRoute = NULL;
	cbFuncZWDeleteSUCReturnRoute = NULL;
	cbFuncZWDeleteReturnRoute = NULL;
	cbFuncZWSetLearnMode = NULL;
	cbFuncZWRequestNodeNodeNeighborUpdate = NULL;
	cbFuncZWSetSUCNodeID = NULL;
	cbFuncZWRequestNetworkUpdate = NULL;

	callbacks = (struct SerialAPI_Callbacks*) _callbacks;

	/*Get Capabilities of serial API*/
	byLen = 0;
	if (!SendFrameWithResponse(FUNC_ID_SERIAL_API_GET_CAPABILITIES, 0, 0,
			buffer, &byLen)) {
		SER_PRINTF("SendFrameWithResponse(FUNC_ID_SERIAL_API_GET_CAPABILITIES,...) failed in SerialAPI_Init()\n");
		ASSERT(0);
		return FALSE;
	}
	memcpy(&capabilities, &(buffer[IDX_DATA]), byLen - IDX_DATA);
	log_d(
			"app version:%d.%d,manufactor:%02X %02X,Product Type:%02X %02X,pid:%02X %02X",
			capabilities.appl_version, capabilities.appl_revision,
			capabilities.manufactor_id1, capabilities.manufactor_id2,
			capabilities.product_type1, capabilities.product_type2,
			capabilities.product_id1, capabilities.product_id2);

	type = ZW_Version(buf);
	if (type < sizeof(zw_lib_names) / sizeof(char*)) {
		log_d("Z-Wave library version : %s", &buf[7]);
		log_d("Z-Wave library type    : %s", zw_lib_names[type]);
	}

	if (callbacks->ApplicationInitHW)
		callbacks->ApplicationInitHW(0);
	if (callbacks->ApplicationInitSW)
		callbacks->ApplicationInitSW();

	if (callbacks->ApplicationNodeInformation) {
		callbacks->ApplicationNodeInformation(&listening, &nodeType, &nodeParm,
				&parmLength);
		SerialAPI_ApplicationNodeInformation(listening, nodeType, nodeParm,
				parmLength);
	}

	return TRUE;
}

void Get_SerialAPI_AppVersion(uint8_t *major, uint8_t *minor) {
	*major = capabilities.appl_version;
	*minor = capabilities.appl_revision;
	return;
}
void Get_SerialAPI_Manufacture(BYTE *mid1, BYTE *mid2, BYTE *pt1, BYTE *pt2,
		BYTE *pid1, BYTE *pid2) {
	*mid1 = capabilities.manufactor_id1;
	*mid2 = capabilities.manufactor_id2;
	*pt1 = capabilities.product_type1;
	*pt2 = capabilities.product_type2;
	*pid1 = capabilities.product_id1;
	*pid2 = capabilities.product_id2;
}

void SerialAPI_Destroy() {
	ConDestroy();
}


extern void ZWHOOK_frame_push(void *data, int datalen);


static void QueueFrame() {
	ZWHOOK_frame_push(serBuf, serBufLen);
}

int WaitResponse() {
	int ret;
	PORT_TIMER t;
	PORT_TIMER_INIT(t, TIMEOUT_TIME);

	while (1) {
		ret = ConUpdate(TRUE);
		if (ret != conIdle) {
			break;
		}

		if (PORT_TIMER_EXPIRED(t)) {
			SER_PRINTF("WaitResponse Timeout\n");
			ret = conRxTimeout;
			break;
		}
	}
	return ret;
}

static void DrainRX() {
	while (ConUpdate(TRUE) == conFrameReceived) {
		if (serBuf[1] == REQUEST) {
			QueueFrame();
		}
	}
}

static int SendFrame(
BYTE cmd, /* IN Command */
BYTE *Buf, /* IN pointer to BYTE buffer containing DATA to send */
BYTE len) /* IN the length of DATA to transmit */
{
	int i;
	enum T_CON_TYPE ret;
	PORT_TIMER t;
	if (!SupportsCommand(cmd)) {
		SER_PRINTF("Command: 0x%x is not supported by this SerialAPI\n",(unsigned)cmd);
		ASSERT(0);
		return conTxErr;
	}
	/*First check for incoming.*/
	DrainRX();
	ConTxFrame(cmd, REQUEST, Buf, len);
	for (i = 0; i < 20; i++) {
		ret = WaitResponse();
		switch (ret) {
		case conFrameSent:
			return ret;
		case conFrameReceived:
			if (serBuf[1] == REQUEST) {
				QueueFrame();
			}
			SER_PRINTF("Got RESPONSE frame while sending.... cmd=0x%x type=%d\n",cmd, serBuf[1]);
//      dump_serbuf();
			//Don't queue frame here because WaitResponse does it if its a request
			//QueueFrame();
			/* If we received a frame here then we were both sending. The embedded target will have
			 * queued a CAN at this point, since we have been sending a frame to the uart buffer.
			 * before ACK'ing the received frame.
			 */
			continue; /*Go back, there should be more*/
		case conTxWait:
			break;
		default:
			break;
		}
		//SER_PRINTF("Retransmission %i of %2x %s\n",i,cmd, ConTypeToStr(ret));

		SER_PRINTF("Retransmission %d of cmd=0x%02x\n",i,cmd);

#ifdef __ASIX_C51__
		//When SSL handshake is in progress, don't retry sending out.
		if((gIsSslHshakeInProgress == 1) && (i >= 3))
		{
			SER_PRINTF("SSL in progress. No serial retries!!!!\r\n" );
			break;
		}
#endif

		/* It seems that the serial port sometimes stalls on osx, this seems to help */
		if ((i & 7) == 7) {
			SER_PRINTF("restart Open serial:%s\n", "");
			SerialClose();
			//SerialInit(");
		}
		SerialFlush();
		/*TODO consider to use an exponential backoff, and do not backoff until our own framehandler is idle. Also
		 * the magnitude of the backoff seem very large... this is to be analyzed. */
		PORT_TIMER_INIT(t, 10+i*100);
		while (!PORT_TIMER_EXPIRED(t)) {
			DrainRX();
		}

		ConTxFrame(cmd, REQUEST, Buf, len);
	}
	SER_PRINTF("Unable to send frame!!!!!!\n");
	return ret;
}

static int SendFrameWithResponse(
BYTE cmd, /* IN Command */
BYTE *Buf, /* IN pointer to BYTE buffer containing DATA to send */
BYTE len,
BYTE *reply,
BYTE *replyLen) /* IN the length of DATA to transmit */
{
	int ret;
	int i;

	ret = SendFrame(cmd, Buf, len);
	if (ret == conFrameSent) {
		for (i = 0; i < 3; i++) {
			ret = WaitResponse();
			if (ret == conFrameReceived) {
				if (serBuf[1] == REQUEST) {
					QueueFrame();
				} else {
					if (serBuf[ IDX_CMD] == cmd) {
						memcpy(reply, serBuf, serFrameLen);
						if (replyLen)
							*replyLen = serFrameLen;
						return ret;
					} else {
						/*This if for the case where we get a callback from another function instead of a response */
						//Dispatch(serBuf);
						SER_PRINTF("Got new frame while sending cmd:%x-%d\n",cmd, i);
					}
				}
			} else {
				SER_PRINTF("Unexpected receive state! %s\n",ConTypeToStr(ret));
			}
		}
	}
	SER_PRINTF("SendFrameWithResponse() returning failure\n");
	return 0;
}

//extern BYTE transportServiceState;
//extern BYTE bCompleteTimerHandle;
//extern BYTE bRecoverTimer;

/**
 * \ingroup SerialAPI
 * Serial port polling loop.
 * This should be called continuously from a main loop. All callbacks are
 * called as a decendant of this loop.
 */
//void QueueHandler() {
//	int i = 0;
//	struct list* l;
//	i = rxQueue_Len(); //杩欓噷灏嗕細鏈夊緢澶氬懡浠�
//	while (i--) {
//		l = rxQueue;
//		rxQueue = rxQueue->next;
//		memcpy(serBuf, l->data, l->len);
//		serBufLen = l->len;
//		Dispatch(l->data, l->len);
//		free(l->data);
//		free(l);
//	}
//	struct list_head *pos = NULL;
//	struct list_head *n = NULL;
//	struct RxData *l = NULL;
//	list_for_each_safe(pos,n,&_ListRx)
//	{
//		l = list_entry(pos, struct RxData, list);
//		list_del(pos);  //鍒犻櫎鑺傜偣锛屽垹闄よ妭鐐瑰繀椤诲湪鍒犻櫎鑺傜偣鍐呭瓨涔嬪墠
//		memcpy(serBuf, l->data, l->len);
//		serBufLen = l->len;
//		Dispatch(l->data, l->len);
//		free(l->data);
//		free(l);
//	}
//}
u8_t SerialAPI_Poll(void) {
//	if (rxQueue) {
//		l = rxQueue;
//		rxQueue = rxQueue->next;
//		serBufLen = l->len;
//		memcpy(serBuf, l->data, l->len);
//		Dispatch(l->data);
//#ifdef __ASIX_C51__
//		zfree(l->data);
//		zfree(l);
//#else
//		free(l->data);
//		free(l);
//#endif
//	}
//
//	DrainRX();

	DrainRX();
	//QueueHandler();

	if (callbacks && callbacks->ApplicationPoll)
		callbacks->ApplicationPoll();

	return 0;
}

/* Check if the received frame will overflow pCmd buffer */
char /* RET TRUE or FALSE */
DetectBufferOverflow(unsigned char len) /* IN  length of frame */
{
	if (len > sizeof(pCmd)) {
		SER_PRINTF("Buffer overflow, dropping SerialAPI frame\n");
		return TRUE;
	}
	return FALSE;
}

/*************************************************************************************************/

/**
 * \ingroup SerialAPI
 * ZWave鎺ユ敹鍒扮殑鏁版嵁鐨勫鐞嗚繃绋嬩笌鍏朵腑鐨勫洖璋�
 * Execute a callback based on the received frame.
 * NOTE: Must only be called locally in the Serial API.
 * \param[in] pData    Pointer to data frame (without SOF)
 * \param[in] byLen    Length of data frame
 */
void Dispatch( BYTE *pData, int pDataLen) {
	int i;

	void (*funcLearnInfo)(LEARN_INFO*)=0;
	VOID_CALLBACKFUNC(f) (BYTE, TX_STATUS_TYPE*);
	VOID_CALLBACKFUNC(f2) (BYTE);

	i = 0;
	if (pData[1] == 1)
		return;

	switch (pData[ IDX_CMD]) {

	case FUNC_ID_ZW_APPLICATION_CONTROLLER_UPDATE: {
		//if (pData[ IDX_DATA + 2 ]!=0x0)
		{
			if (DetectBufferOverflow(IDX_DATA + 2))
				break;

			for (i = 0; i < pData[ IDX_DATA + 2]; i++)
				pCmd[i] = pData[ IDX_DATA + 3 + i];

			if (callbacks->ApplicationControllerUpdate) {
				learnNodeInfo.bStatus = pData[ IDX_DATA];
				learnNodeInfo.bSource = pData[ IDX_DATA + 1];
				learnNodeInfo.pCmd = pCmd;
				learnNodeInfo.bLen = pData[ IDX_DATA + 2];
				callbacks->ApplicationControllerUpdate(learnNodeInfo.bStatus,
						learnNodeInfo.bSource, learnNodeInfo.pCmd,
						learnNodeInfo.bLen);
			}

		}

	}
		break;

	case FUNC_ID_APPLICATION_COMMAND_HANDLER: { //0x04
		if (callbacks->ApplicationCommandHandler) {
			if (DetectBufferOverflow(IDX_DATA + 2))
				break;

			for (i = 0; i < pData[ IDX_DATA + 2]; i++)
				pCmd[i] = pData[ IDX_DATA + 3 + i];
			callbacks->ApplicationCommandHandler(pData[ IDX_DATA], 0,
					pData[ IDX_DATA + 1], (ZW_APPLICATION_TX_BUFFER*) pCmd,
					pData[ IDX_DATA + 2]);
		}
	}
		break;

	case FUNC_ID_PROMISCUOUS_APPLICATION_COMMAND_HANDLER: { //0xD1
		if (callbacks->ApplicationCommandHandler) {
			if (DetectBufferOverflow(IDX_DATA + 2))
				break;

			for (i = 0; i < pData[ IDX_DATA + 2]; i++)
				pCmd[i] = pData[ IDX_DATA + 3 + i];
			callbacks->ApplicationCommandHandler(pData[ IDX_DATA],
					pData[byLen - 1], pData[ IDX_DATA + 1],
					(ZW_APPLICATION_TX_BUFFER*) pCmd, pData[ IDX_DATA + 2]);
		}
	}
		break;

	case FUNC_ID_APPLICATION_COMMAND_HANDLER_BRIDGE: {
		if (callbacks->ApplicationCommandHandler_Bridge) {
			if (DetectBufferOverflow(IDX_DATA + 3))
				break;

			for (i = 0; i < pData[ IDX_DATA + 3]; i++)
				pCmd[i] = pData[ IDX_DATA + 4 + i];
			callbacks->ApplicationCommandHandler_Bridge(pData[ IDX_DATA],
					pData[IDX_DATA + 1], pData[IDX_DATA + 2],
					(ZW_APPLICATION_TX_BUFFER*) pCmd, pData[IDX_DATA + 3]);
		}
	}
		break;

#if 0  /* ZIP Router bridge does not support 300 series */
		case FUNC_ID_APPLICATION_SLAVE_COMMAND_HANDLER: /* pre 6.0x only */
		{
			if(callbacks->ApplicationSlaveCommandHandler)
			{
				for ( i = 0;i < pData[ IDX_DATA + 3 ];i++ )
				pCmd[ i ] = pData[ IDX_DATA + 4 + i ];
				callbacks->ApplicationSlaveCommandHandler( pData[ IDX_DATA ],pData[IDX_DATA + 1],
						pData[IDX_DATA + 2], (ZW_APPLICATION_TX_BUFFER*)pCmd, pData[IDX_DATA + 3] );
			}
		}
		break;
#endif

		// The rest are callback functions

	case FUNC_ID_ZW_SEND_NODE_INFORMATION:
		if (cbFuncZWSendNodeInformation != NULL) {
			cbFuncZWSendNodeInformation(pData[ IDX_DATA + 1]);
		}
		break;

	case FUNC_ID_ZW_SEND_DATA: {
		//SER_PRINTF("SendDataCallBack >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
		if (cbFuncZWSendData != NULL) {
			f = cbFuncZWSendData;
			cbFuncZWSendData = 0;
			f(pData[ IDX_DATA + 1], 0);      //TODO add repeaters
		}
	}
		break;

	case FUNC_ID_ZW_SEND_TEST_FRAME: {
		if (cbFuncZWSendTestFrame != NULL) {
			f2 = cbFuncZWSendTestFrame;
			cbFuncZWSendTestFrame = 0;
			f2(pData[ IDX_DATA + 1]);
		}
	}
		break;

	case FUNC_ID_ZW_SEND_DATA_BRIDGE: {
		//SER_PRINTF("SendDataCallBack\n");
		if (cbFuncZWSendDataBridge != NULL) {
			f = cbFuncZWSendDataBridge;
			cbFuncZWSendDataBridge = 0;
			f(pData[ IDX_DATA + 1], 0);      //TODO add repeaters
		}
	}
		break;

	case FUNC_ID_ZW_SEND_DATA_MULTI:
		if (cbFuncZWSendDataMulti != NULL) {
			cbFuncZWSendDataMulti(pData[ IDX_DATA + 1]);
		}
		break;

	case FUNC_ID_MEMORY_PUT_BUFFER:
		if (cbFuncMemoryPutBuffer != NULL) {
			cbFuncMemoryPutBuffer();
		}
		break;

	case FUNC_ID_ZW_SET_DEFAULT:
		if (cbFuncZWSetDefault != NULL) {
			cbFuncZWSetDefault();
		}
		break;

	case FUNC_ID_ZW_CONTROLLER_CHANGE:
	case FUNC_ID_ZW_CREATE_NEW_PRIMARY:
	case FUNC_ID_ZW_REMOVE_NODE_FROM_NETWORK:
	case FUNC_ID_ZW_ADD_NODE_TO_NETWORK:
		switch (pData[ IDX_CMD]) {
		case FUNC_ID_ZW_CONTROLLER_CHANGE:
			funcLearnInfo = cbFuncZWControllerChange;
			break;
		case FUNC_ID_ZW_CREATE_NEW_PRIMARY:
			funcLearnInfo = cbFuncZWNewController;
			break;
		case FUNC_ID_ZW_REMOVE_NODE_FROM_NETWORK:
			funcLearnInfo = cbFuncRemoveNodeFromNetwork;
			break;
		case FUNC_ID_ZW_ADD_NODE_TO_NETWORK:
			funcLearnInfo = cbFuncAddNodeToNetwork;
			break;
		}

		if (funcLearnInfo != NULL) {
			memset(&learnNodeInfo, 0, sizeof(learnNodeInfo));
			learnNodeInfo.bStatus = pData[ IDX_DATA + 1];

			if ((learnNodeInfo.bStatus == ADD_NODE_STATUS_ADDING_SLAVE)
					|| (learnNodeInfo.bStatus
							== ADD_NODE_STATUS_ADDING_CONTROLLER)) {
				learnNodeInfo.bSource = pData[ IDX_DATA + 2];
				learnNodeInfo.bLen = pData[ IDX_DATA + 3];
				learnNodeInfo.pCmd = pCmd;
				if (DetectBufferOverflow(learnNodeInfo.bLen))
					break;
				memcpy(pCmd, &pData[ IDX_DATA + 4], learnNodeInfo.bLen);
			}
			/*
			 if (learnNodeInfo.bStatus==0x6)
			 cbFuncZWNewController( &learnNodeInfo );
			 */

			funcLearnInfo(&learnNodeInfo);
		}

		break;

	case FUNC_ID_ZW_REPLICATION_SEND_DATA:
		if (cbFuncZWReplicationSendData != NULL) {
			cbFuncZWReplicationSendData(pData[ IDX_DATA + 1]);
		}
		break;

	case FUNC_ID_ZW_ASSIGN_RETURN_ROUTE:
		if (cbFuncZWAssignReturnRoute != NULL) {
			cbFuncZWAssignReturnRoute(pData[ IDX_DATA + 1]);
		}
		break;

	case FUNC_ID_ZW_DELETE_RETURN_ROUTE:
		if (cbFuncZWDeleteReturnRoute != NULL) {
			cbFuncZWDeleteReturnRoute(pData[ IDX_DATA + 1]);
		}
		break;

	case FUNC_ID_ZW_ASSIGN_SUC_RETURN_ROUTE:
		if (cbFuncZWAssignSUCReturnRoute != NULL) {
			cbFuncZWAssignSUCReturnRoute(pData[ IDX_DATA + 1]);
		}
		break;

	case FUNC_ID_ZW_DELETE_SUC_RETURN_ROUTE:
		if (cbFuncZWDeleteSUCReturnRoute != NULL) {
			cbFuncZWDeleteSUCReturnRoute(pData[ IDX_DATA + 1]);
		}
		break;

	case FUNC_ID_ZW_SET_LEARN_MODE:
		if (cbFuncZWSetLearnMode != NULL) {
			learnNodeInfo.bStatus = pData[ IDX_DATA + 1];
			learnNodeInfo.bSource = pData[ IDX_DATA + 2];

			if (DetectBufferOverflow(IDX_DATA + 3))
				break;
			for (i = 0; i < pData[ IDX_DATA + 3]; i++) {
				pCmd[i] = pData[ IDX_DATA + 4 + i];
			}
			learnNodeInfo.pCmd = pCmd;
			learnNodeInfo.bLen = pData[ IDX_DATA + 3];

			cbFuncZWSetLearnMode(
					&learnNodeInfo /*learnNodeInfo.bStatus, learnNodeInfo.bSource */);
		}
		break;

	case FUNC_ID_ZW_SET_SLAVE_LEARN_MODE:
		if (cbFuncZWSetSlaveLearnMode != NULL) {
			cbFuncZWSetSlaveLearnMode(pData[ IDX_DATA + 1],
					pData[ IDX_DATA + 2], pData[ IDX_DATA + 3]);
		}
		break;

	case FUNC_ID_ZW_SET_SUC_NODE_ID:
		if (cbFuncZWSetSUCNodeID != NULL) {
			cbFuncZWSetSUCNodeID(pData[ IDX_DATA + 1]);
		}
		break;

	case FUNC_ID_ZW_REQUEST_NODE_NEIGHBOR_UPDATE:
		if (cbFuncZWRequestNodeNodeNeighborUpdate != NULL) {
			cbFuncZWRequestNodeNodeNeighborUpdate(pData[ IDX_DATA + 1]);
		}
		break;

	case FUNC_ID_ZW_REQUEST_NETWORK_UPDATE:
		if (cbFuncZWRequestNetworkUpdate != NULL) {
			cbFuncZWRequestNetworkUpdate(pData[ IDX_DATA + 1]);
		}
		break;
	case FUNC_ID_ZW_REMOVE_FAILED_NODE_ID:
		if (cbFuncZWRemoveFailedNode != NULL) {
			cbFuncZWRemoveFailedNode(pData[ IDX_DATA + 1]);
		}
		break;
	case FUNC_ID_ZW_REPLACE_FAILED_NODE:
		if (cbFuncZWReplaceFailedNode != NULL) {
			cbFuncZWReplaceFailedNode(pData[ IDX_DATA + 1]);
		}
		break;

	default:
		SER_PRINTF("Unknown SerialAPI FUNC_ID: 0x%02x\n",pData[ IDX_CMD ]);
	}

}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  浠ヤ笅鏄悇绉岮PI
//  SendFrameWithResponse
//  SendFrame...
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * \ingroup SerialAPI
 * \defgroup ZWCMD Z-Wave Commands
 */

/**
 * \ingroup ZWCMD
 * Initialize the Z-Wave RF chip.
 * \param[in] mode
 * = TRUE : Set the RF chip in receive mode and starts the data sampling. \n
 * = FALSE : Set the RF chip in power down mode. \n
 */
BYTE ZW_SetRFReceiveMode( BYTE mode) {
	byLen = 0;
	idx = 0;
	buffer[idx++] = mode;
	SendFrameWithResponse(FUNC_ID_ZW_SET_RF_RECEIVE_MODE, buffer, idx, buffer,
			&byLen);

	return buffer[ IDX_DATA];
}

/*==========================   ZW_RFPowerLevelSet  ==========================
 **    Set the powerlevel used in RF transmitting.
 **    Valid powerlevel values are :
 **
 **       normalPower : Max power possible
 **       minus2dBm    - normalPower - 2dBm
 **       minus4dBm    - normalPower - 4dBm
 **       minus6dBm    - normalPower - 6dBm
 **       minus8dBm    - normalPower - 8dBm
 **       minus10dBm   - normalPower - 10dBm
 **       minus12dBm   - normalPower - 12dBm
 **       minus14dBm   - normalPower - 14dBm
 **       minus16dBm   - normalPower - 16dBm
 **       minus18dBm   - normalPower - 18dBm
 **
 **--------------------------------------------------------------------------*/
BYTE /*RET The powerlevel set */
ZW_RFPowerLevelSet(
BYTE powerLevel) /* IN Powerlevel to set */
{
	idx = 0;
	byLen = 0;
	buffer[idx++] = powerLevel;
	SendFrameWithResponse(FUNC_ID_ZW_RF_POWER_LEVEL_SET, buffer, idx, buffer,
			&byLen);
	return buffer[ IDX_DATA];
}

/**
 *    Create and transmit a node information broadcast frame
 *    \retval FALSE if transmitter queue overflow
 *    \retval FALSE if transmitter queue overflow
 */
BYTE /*RET */
ZW_SendNodeInformation(
BYTE destNode, /*IN  Destination Node ID  */
BYTE txOptions, /*IN  Transmit option flags         */
VOID_CALLBACKFUNC(completedFunc) (/*uto*/BYTE)) /*IN  Transmit completed call back function  */
{
	idx = 0;
	byCompletedFunc = (completedFunc == NULL ? 0 : 0x03);
	buffer[idx++] = destNode;
	buffer[idx++] = txOptions;
	buffer[idx++] = byCompletedFunc;      // Func id for CompletedFunc
	cbFuncZWSendNodeInformation = completedFunc;
	SendFrame(FUNC_ID_ZW_SEND_NODE_INFORMATION,buffer, idx);
	return 0;
}

/**===============================   ZW_SendData   ===========================
 **    Transmit data buffer to a single ZW-node or all ZW-nodes (broadcast).
 **

 ** @param[in] nodeID        Destination node ID (0xFF == broadcast)
 ** @param[in] pData         Data buffer pointer
 ** @param[in] dataLength    Data buffer length
 ** @param[in] txOptions     Transmit option flags
 **          TRANSMIT_OPTION_LOW_POWER     transmit at low output power level
 **                                        (1/3 of normal RF range).
 **          TRANSMIT_OPTION_ACK           the multicast frame will be followed
 **                                        by a  singlecast frame to each of
 **                                        the destination nodes
 **                                        and request acknowledge from each
 **                                        destination node.
 **          TRANSMIT_OPTION_AUTO_ROUTE    request retransmission via repeater
 **                                        nodes at normal output power level).
 ** @param[in] completedFunc Transmit completed call back function
 ** @param[in] txStatus      IN Transmit status
 **
 ** @retval FALSE if transmitter queue overflow
 **--------------------------------------------------------------------------*/
BYTE /*RET  FALSE if transmitter busy      */
ZW_SendData(
BYTE nodeID, /*IN  Destination node ID (0xFF == broadcast) */
BYTE *pData, /*IN  Data buffer pointer           */
BYTE dataLength, /*IN  Data buffer length            */
BYTE txOptions, /*IN  Transmit option flags         */
VOID_CALLBACKFUNC(completedFunc) (BYTE, TX_STATUS_TYPE*)
) /*IN  Transmit completed call back function  */
{
	static int txnr =0;
	uint8_t i;

	if((uint8_t)(dataLength+2) >sizeof(buffer)) {
		SER_PRINTF("ZW_SendData: Frame is too long\n");
		ASSERT(0);
		return FALSE;
	}

	//ASSERT(cbFuncZWSendData==0);
	idx = 0;
	byLen = 0;
	byCompletedFunc = (completedFunc == NULL ? 0 : (1 +(txnr & 0xf7)));

	buffer[idx++] = nodeID;
	buffer[idx++] = dataLength;
	for (i = 0; i < dataLength; i++)
	{
		buffer[idx++] = pData[i];
	}
	buffer[idx++] = txOptions;
	buffer[idx++] = byCompletedFunc;      // Func id for CompletedFunc
	cbFuncZWSendData = completedFunc;

	if(SendFrameWithResponse(FUNC_ID_ZW_SEND_DATA,buffer, idx , buffer, &byLen) != conFrameReceived) {
		buffer[IDX_DATA] = FALSE;
		SER_PRINTF("Fail\n");
	} else {
		txnr++;
	}

	if(buffer[IDX_DATA] != TRUE) {
		SER_PRINTF("SendData fail\n");
		cbFuncZWSendData = 0;
	}

	return buffer[IDX_DATA];
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BYTE ZW_SendData_NOTFUNID(BYTE nodeID, BYTE *pData, BYTE dataLength,
		BYTE txOptions) {
	uint8_t i;

	if ((uint8_t) (dataLength + 2) > sizeof(buffer)) {
		SER_PRINTF("ZW_SendData: Frame is too long\n");
		ASSERT(0);
		return FALSE;
	}
	//ASSERT(cbFuncZWSendData==0);
	idx = 0;
	byLen = 0;
	buffer[idx++] = nodeID;
	buffer[idx++] = dataLength;
	for (i = 0; i < dataLength; i++) {
		buffer[idx++] = pData[i];
	}
	buffer[idx++] = txOptions;
	buffer[idx++] = 0;      // Func id for CompletedFunc
	if (SendFrameWithResponse(FUNC_ID_ZW_SEND_DATA, buffer, idx, buffer, &byLen)
			!= conFrameReceived) {
		buffer[IDX_DATA] = FALSE;
		SER_PRINTF("Fail\n");
	}

	if (buffer[IDX_DATA] != TRUE) {
		SER_PRINTF("SendData fail\n");
		cbFuncZWSendData = 0;
	}
	return buffer[IDX_DATA];
}

BYTE /*RET FALSE if transmitter busy else TRUE */
ZW_SendTestFrame(
BYTE nodeID, /* IN nodeID to transmit to */
BYTE powerLevel, /* IN powerlevel index */
VOID_CALLBACKFUNC(func) (BYTE txStatus)) /* Call back function called when done */
{
	byCompletedFunc = (func == NULL ? 0 : 4);
	cbFuncZWSendTestFrame = func;
	idx = 0;
	byLen = 0;
	buffer[idx++] = nodeID;
	buffer[idx++] = powerLevel;
	buffer[idx++] = byCompletedFunc;
	SendFrameWithResponse(FUNC_ID_ZW_SEND_TEST_FRAME,buffer, idx, buffer, &byLen);
	return buffer[ IDX_DATA ];
}

/**===============================   ZW_SendData_Bridge   ========================
 **    Transmit data buffer to a single ZW-node or all ZW-nodes (broadcast)
 **    from a virtual node.
 **

 ** @param[in] srcNodeID        Destination node ID (0xFF == broadcast)
 ** @param[in] dstNodeID        Vurtual source node ID
 ** @param[in] pData         Data buffer pointer
 ** @param[in] dataLength    Data buffer length
 ** @param[in] txOptions     Transmit option flags
 **          TRANSMIT_OPTION_LOW_POWER     transmit at low output power level
 **                                        (1/3 of normal RF range).
 **          TRANSMIT_OPTION_ACK           the multicast frame will be followed
 **                                        by a  singlecast frame to each of
 **                                        the destination nodes
 **                                        and request acknowledge from each
 **                                        destination node.
 **          TRANSMIT_OPTION_AUTO_ROUTE    request retransmission via repeater
 **                                        nodes at normal output power level).
 ** @param[in] completedFunc Transmit completed call back function
 ** @param[in] txStatus      IN Transmit status
 **
 **    @param txOptions:
 **
 ** @retval FALSE if transmitter queue overflow
 **--------------------------------------------------------------------------*/
BYTE /*RET  FALSE if transmitter busy      */
ZW_SendData_Bridge(
BYTE srcNodeID, /*IN  Virtual source node ID*/
BYTE destNodeID, /*IN  Destination node ID (0xFF == broadcast) */
BYTE *pData, /*IN  Data buffer pointer           */
BYTE dataLength, /*IN  Data buffer length            */
BYTE txOptions, /*IN  Transmit option flags         */
VOID_CALLBACKFUNC(completedFunc) (BYTE, TX_STATUS_TYPE*)) /*IN  Transmit completed call back function  */
{
	static int txnr =0;
	int i;

	if((uint8_t)(dataLength+2)>sizeof(buffer)) {
		SER_PRINTF("ZW_SendData: Frame is too long\n");
		ASSERT(0);
		return FALSE;
	}

	idx = 0;
	byLen = 0;
	byCompletedFunc = (completedFunc == NULL ? 0 : (1 +(txnr & 0xf7)));

	buffer[idx++] = srcNodeID;
	buffer[idx++] = destNodeID;
	buffer[idx++] = dataLength;
	for (i = 0; i < dataLength; i++)
	{
		buffer[idx++] = pData[i];
	}
	buffer[idx++] = txOptions;
	buffer[idx++] = 0;
	buffer[idx++] = 0;
	buffer[idx++] = 0;
	buffer[idx++] = 0;
	buffer[idx++] = byCompletedFunc;      // Func id for CompletedFunc
	cbFuncZWSendDataBridge = completedFunc;
	if(SendFrameWithResponse(FUNC_ID_ZW_SEND_DATA_BRIDGE,buffer, idx , buffer, &byLen) != conFrameReceived) {
		buffer[IDX_DATA] = FALSE;
		SER_PRINTF("Fail\n");
	} else {
		txnr++;
	}

	if(buffer[IDX_DATA] != TRUE) {
		SER_PRINTF("SendData fail\n");
		cbFuncZWSendDataBridge = 0;
	}

	return buffer[IDX_DATA];
}

/**
 * \ingroup ZWCMD
 * Abort the ongoing transmit started with ZW_SendData() or ZW_SendDataMulti(). If an ongoing
 * transmission is aborted, the callback function from the send call will return with the status
 * TRANSMIT_COMPLETE_NO_ACK.
 */
void ZW_SendDataAbort(void) {
	byLen = 0;
	SendFrame( FUNC_ID_ZW_SEND_DATA_ABORT, 0, 0);
}
#if 0
/**
 * \ingroup ZWCMD
 * Transmit data buffer to a list of Z-Wave Nodes (multicast frame).
 * \param[in] pNodeIDList    List of destination node ID's
 * \param[in] numberNodes    Number of Nodes
 * \param[in] pData          Data buffer pointer
 * \param[in] dataLength     Data buffer length
 * \param[in] txOptions      Transmit option flags
 * \param[in] completedFunc  Transmit completed call back function
 * \return FALSE if transmitter queue overflow
 */
BYTE ZW_SendDataMulti( BYTE *pNodeIDList, BYTE numberNodes, BYTE *pData, BYTE dataLength, BYTE txOptions, void ( *completedFunc ) ( BYTE txStatus ) )
{
	int i;
	idx = 0;
	BYTE byCompletedFunc = ( completedFunc == NULL ? 0 : 0x03 );

	numberNodes = ( numberNodes <= MAX_NODES_IN_MULTICAST ) ? numberNodes : MAX_NODES_IN_MULTICAST;
	buffer[ idx++ ] = numberNodes;

	for ( i = 0; i < numberNodes; i++ )
	{
		buffer[ idx++ ] = pNodeIDList[ i ];
	}

	buffer[ idx++ ] = dataLength;

	for ( i = 0; i < dataLength; i++ )
	{
		buffer[ idx++ ] = pData[ i ];
	}

	buffer[ idx++ ] = txOptions;
	buffer[ idx++ ] = byCompletedFunc;	// Func id for CompletedFunc
	byLen = 0;
	cbFuncZWSendDataMulti = completedFunc;
	SendFrameWithResponse( buffer, idx, FUNC_ID_ZW_SEND_DATA_MULTI, buffer, &byLen );

	return buffer[ IDX_DATA ];
}
#endif

/**
 * \ingroup ZWCMD
 * Copy the Home-ID and Node-ID to the specified RAM addresses.
 * \param[out] homeID  Home-ID pointer
 * \param[out] nodeID  Node-ID pointer
 */
void MemoryGetID( BYTE *pHomeID, BYTE *pNodeID) {
	idx = 0;
	byLen = 0;
	SendFrameWithResponse( FUNC_ID_MEMORY_GET_ID, 0, 0, buffer, &byLen);

	pHomeID[0] = buffer[ IDX_DATA];
	pHomeID[1] = buffer[ IDX_DATA + 1];
	pHomeID[2] = buffer[ IDX_DATA + 2];
	pHomeID[3] = buffer[ IDX_DATA + 3];
	pNodeID[0] = buffer[ IDX_DATA + 4];
	if (*pNodeID < 1 || *pNodeID > ZW_MAX_NODES) {
		ERR_PRINTF("Module returns a bad node ID! Resetting module (%d)",
				*pNodeID);
		ZW_SetDefault(0);
		*pNodeID = 1;
	}
}

/**
 * \ingroup ZWCMD
 *  Read one byte from the EEPROM
 *  @param[in] offset Application area offset
 *  @retval Data
 */
BYTE MemoryGetByte(WORD offset) {
	idx = 0;
	buffer[idx++] = (offset) >> 8;
	buffer[idx++] = (offset) & 0xFF;
	byLen = 0;
	SendFrameWithResponse(FUNC_ID_MEMORY_GET_BYTE, buffer, idx, buffer, &byLen);
	return buffer[IDX_DATA];
}

/**
 *  Add one byte to the EEPROM write queue
 *  @param[in] Application area offset
 *  @param[in] Data to store
 *  @retval FALSE if write buffer full
 */
BYTE /*RET    */
MemoryPutByte(WORD offset,
BYTE bData) {
	idx = 0;
	buffer[idx++] = (offset) >> 8;
	buffer[idx++] = (offset) & 0xFF;
	buffer[idx++] = bData;
	byLen = 0;
	SendFrameWithResponse(FUNC_ID_MEMORY_PUT_BYTE, buffer, idx, buffer, &byLen);
	return buffer[IDX_DATA];
}

/**
 * \ingroup ZWCMD
 * Read number of bytes from the EEPROM to a RAM buffer.
 * \param[in] offset   Application area offset
 * \param[in] buffer   Buffer pointer
 * \param[in] length   Number of bytes to read
 */
void MemoryGetBuffer(WORD offset, BYTE *buf, BYTE length) {
	int i;

	if (SupportsCommand(FUNC_ID_MEMORY_GET_BUFFER)) {
		idx = 0;
		buffer[idx++] = (offset) >> 8;
		buffer[idx++] = (offset) & 0xFF;
		buffer[idx++] = (BYTE) length;		// Number of bytes to read

		byLen = 0;
		SendFrameWithResponse(FUNC_ID_MEMORY_GET_BUFFER, buffer, idx, buffer,
				&byLen);

		for (i = 0; i < length; i++) {
			buf[i] = buffer[ IDX_DATA + i];
		}
	} else {
		for (i = 0; i < length; i++) {
			buf[i] = MemoryGetByte(offset + i);
		}
	}
}

/**
 * \ingroup ZWCMD
 * Copy number of bytes from a RAM buffer to the EEPROM.
 * \param[in] offset   Application area offset
 * \param[in] buffer   Buffer pointer
 * \param[in] length   Number of bytes to write
 * \param[in] func     Write completed function pointer
 * \return FALSE if the buffer put queue is full
 */
BYTE MemoryPutBuffer(WORD offset, BYTE *buf, WORD length, void (*func)(void)) {
	int i;

	if (SupportsCommand(FUNC_ID_MEMORY_PUT_BUFFER)) {
		idx = 0;
		byCompletedFunc = (func == NULL ? 0 : 0x03);
		buffer[idx++] = (offset) >> 8;
		buffer[idx++] = (offset) & 0xFF;

		if (length > BUF_SIZE - 8)
			length = BUF_SIZE - 8;

		buffer[idx++] = (length) >> 8;

		buffer[idx++] = (length) & 0xFF;

		for (i = 0; i < length; i++) {
			buffer[idx++] = buf[i];
		}

		buffer[idx++] = byCompletedFunc;
		cbFuncMemoryPutBuffer = func;
		byLen = 0;
		SendFrameWithResponse(FUNC_ID_MEMORY_PUT_BUFFER, buffer, idx, buffer,
				&byLen);
		return buffer[ IDX_DATA];
	} else {
		for (i = 0; i < length; i++) {
			MemoryPutByte(offset + i, buf[i]);
			if (func)
				func();
		}
		return TRUE;
	}
}

/**
 * \ingroup ZWCMD
 * Copy number of bytes from a RAM buffer to the EEPROM.
 * \param[in] offset   Application area offset
 * \param[in] buffer   Buffer pointer
 * \param[in] length   Number of bytes to write
 * \param[in] func     Write completed function pointer
 * \return FALSE if the buffer put queue is full
 */
BYTE ZW_MemoryPutBuffer(WORD offset, BYTE *buf, WORD length) {
	int i;

	if (SupportsCommand(FUNC_ID_MEMORY_PUT_BUFFER)) {
		idx = 0;

		buffer[idx++] = (offset) >> 8;
		buffer[idx++] = (offset) & 0xFF;

		if (length > BUF_SIZE - 8)
			length = BUF_SIZE - 8;

		buffer[idx++] = (length) >> 8;

		buffer[idx++] = (length) & 0xFF;

		for (i = 0; i < length; i++) {
			buffer[idx++] = buf[i];
		}
		buffer[idx++] = byCompletedFunc;

		byLen = 0;
		SendFrameWithResponse( FUNC_ID_MEMORY_PUT_BUFFER, buffer, idx, buffer,
				&byLen);

		return buffer[ IDX_DATA];
	} else {
		for (i = 0; i < length; i++) {
			MemoryPutByte(offset + i, buf[i]);
		}
		return TRUE;
	}
}

/**
 * \ingroup ZWCMD
 * Lock a response route for a specific node.
 * \param[in] bNodeID
 */
void ZW_LockRoute( BYTE bNodeID) {
	idx = 0;
	buffer[idx++] = bNodeID;
	SendFrame(FUNC_ID_LOCK_ROUTE_RESPONSE, buffer, idx);
}

/**
 * \ingroup ZWCMD
 * Read out neighbor information.
 * \param[in] bNodeID        Node ID on node whom routing info is needed on
 * \param[out] pMask            Pointer where routing info should be putd
 * \param[in]  bOptions     If TRUE remove bad repeaters
 * \param[in]  Upper nibble is bit flag options, lower nibble is speed
 *     Combine exactly one speed with any number of options
 *     Bit flags options for upper nibble:
 *       ZW_GET_ROUTING_INFO_REMOVE_BAD      - Remove bad link from routing info
 *       ZW_GET_ROUTING_INFO_REMOVE_NON_REPS  - Remove non-repeaters from the routing info
 *     Speed values for lower nibble:
 *       ZW_GET_ROUTING_INFO_ANY  - Return all nodes regardless of speed
 *       ZW_GET_ROUTING_INFO_9600 - Return nodes supporting 9.6k
 *       ZW_GET_ROUTING_INFO_40K  - Return nodes supporting 40k
 *       ZW_GET_ROUTING_INFO_100K - Return nodes supporting 100k
 */

void ZW_GetRoutingInfo_old( BYTE bNodeID, BYTE *buf, BYTE bRemoveBad,
BYTE bRemoveNonReps) {
	int i;
	idx = 0;
	/*  bLine | bRemoveBad | bRemoveNonReps | funcID */
	buffer[idx++] = bNodeID;
	buffer[idx++] = bRemoveBad;
	buffer[idx++] = bRemoveNonReps;
	byLen = 0;

	SendFrameWithResponse( FUNC_ID_GET_ROUTING_TABLE_LINE, buffer, idx, buffer,
			&byLen);

	for (i = 0; i < 29; i++) {
		buf[i] = buffer[ IDX_DATA + i];
	}
}

/**
 * \ingroup ZWCMD
 * Reset TX Counter.
 */
void ZW_ResetTXCounter(void) {
	idx = 0;
	SendFrame(FUNC_ID_RESET_TX_COUNTER, buffer, idx);
}

/**
 * \ingroup ZWCMD
 * Get TX Counter.
 * \return The number of frames transmitted since the TX Counter last was reset
 */
BYTE ZW_GetTXCounter(void) {
	idx = 0;
	byLen = 0;
	SendFrameWithResponse( FUNC_ID_GET_TX_COUNTER, buffer, idx, buffer, &byLen);
	return buffer[ IDX_DATA];
}

/*=====================   ZW_RequestNodeNeighborUpdate  ======================
 **
 **    Start neighbor discovery for bNodeID, if any other nodes present.
 **
 **    Side effects:
 **
 **--------------------------------------------------------------------------*/
BYTE /*RET TRUE neighbor discovery started */
ZW_RequestNodeNeighborUpdate(
BYTE bNodeID, /* IN Node id */
VOID_CALLBACKFUNC(completedFunc) ( /* IN Function to be called when the done */
auto BYTE))
{
	idx = 0;
	byCompletedFunc = ( completedFunc == NULL ? 0 : 0x03 );
	buffer[ idx++ ] = bNodeID;
	buffer[ idx++ ] = byCompletedFunc;
	cbFuncZWRequestNodeNodeNeighborUpdate = completedFunc;
	SendFrame(FUNC_ID_ZW_REQUEST_NODE_NEIGHBOR_UPDATE, buffer, idx );
	return 0;
}

/**
 * \ingroup ZWCMD
 * Copy the Node's current protocol information from the non-volatile memory.
 * \param[in] nodeID      Node ID
 * \param[out] nodeInfo   Node info buffer
 */

void ZW_GetNodeProtocolInfo( BYTE bNodeID, NODEINFO *nodeInfo) {
	idx = 0;
	buffer[idx++] = bNodeID;
	byLen = 0;
	SendFrameWithResponse( FUNC_ID_ZW_GET_NODE_PROTOCOL_INFO, buffer, idx,
			buffer, &byLen);

	nodeInfo->capability = buffer[ IDX_DATA];
	nodeInfo->security = buffer[ IDX_DATA + 1];
	nodeInfo->reserved = buffer[ IDX_DATA + 2];
#ifdef NEW_NODEINFO

	nodeInfo->nodeType.basic = buffer[ IDX_DATA + 3];
	nodeInfo->nodeType.generic = buffer[ IDX_DATA + 4];
	nodeInfo->nodeType.specific = buffer[ IDX_DATA + 5];
#else

	nodeInfo->nodeType = buffer[ IDX_DATA + 3 ];
#endif
}

void ZW_GetVirtualNodes(char *pNodeMask) {
	idx = 0;
	byLen = 0;
	SendFrameWithResponse( FUNC_ID_ZW_GET_VIRTUAL_NODES, buffer, idx, buffer,
			&byLen);
	memcpy(pNodeMask, buffer + IDX_DATA, MAX_NODEMASK_LENGTH);
}

/*===========================   ZW_SetDefault   =============================
 **    Remove all Nodes and timers from the EEPROM memory.
 **
 **--------------------------------------------------------------------------*/
void /*RET Nothing */
ZW_SetDefault(
VOID_CALLBACKFUNC(completedFunc) ( /* IN Command completed call back function */
void))
{
	idx = 0;
	byCompletedFunc = ( completedFunc == NULL ? 0 : 0x03 );
	buffer[ idx++ ] = byCompletedFunc;
	cbFuncZWSetDefault = completedFunc;
	SendFrame(FUNC_ID_ZW_SET_DEFAULT, buffer, idx );
}

/*========================   ZW_ControllerChange   ======================
 **
 **    Transfer the role as primary controller to another controller
 **
 **    The modes are:
 **
 **    CONTROLLER_CHANGE_START          Start the creation of a new primary
 **    CONTROLLER_CHANGE_STOP           Stop the creation of a new primary
 **    CONTROLLER_CHANGE_STOP_FAILED    Report that the replication failed
 **
 **    ADD_NODE_OPTION_HIGH_POWER       Set this flag in bMode for High Power exchange.
 **
 **    Side effects:
 **
 **--------------------------------------------------------------------------*/
void ZW_ControllerChange(BYTE bMode,
VOID_CALLBACKFUNC(completedFunc) (auto LEARN_INFO*))
{
	idx = 0;
	byCompletedFunc = ( completedFunc == NULL ? 0 : 0x03 );
	buffer[ idx++ ] = bMode;
	buffer[ idx++ ] = byCompletedFunc;
	cbFuncZWControllerChange = completedFunc;
	SendFrame(FUNC_ID_ZW_CONTROLLER_CHANGE, buffer, idx );
}

/*========================   ZW_CreateNewPrimaryCtrl   ======================
 **
 **    Create a new primary controller
 **
 **    The modes are:
 **
 **    CREATE_PRIMARY_START          Start the creation of a new primary
 **    CREATE_PRIMARY_STOP           Stop the creation of a new primary
 **    CREATE_PRIMARY_STOP_FAILED    Report that the replication failed
 **
 **    ADD_NODE_OPTION_HIGH_POWER    Set this flag in bMode for High Power inclusion.
 **
 **    Side effects:
 **
 **--------------------------------------------------------------------------*/
void ZW_CreateNewPrimaryCtrl(BYTE bMode,
VOID_CALLBACKFUNC(completedFunc) (LEARN_INFO*)) {
	idx = 0;
	byCompletedFunc = ( completedFunc == NULL ? 0 : 0x03 );
	buffer[ idx++ ] = bMode;
	buffer[ idx++ ] = byCompletedFunc;
	cbFuncZWNewController= completedFunc;
	SendFrame(FUNC_ID_ZW_NEW_CONTROLLER, buffer, idx );
}

/*==========================   ZW_AddNodeToNetwork   ========================
 **
 **    Add any type of node to the network
 **
 **    The modes are:
 **
 **    ADD_NODE_ANY            Add any node to the network
 **    ADD_NODE_CONTROLLER     Add a controller to the network
 **    ADD_NODE_SLAVE          Add a slaev node to the network
 **    ADD_NODE_STOP           Stop learn mode without reporting an error.
 **    ADD_NODE_STOP_FAILED    Stop learn mode and report an error to the
 **                            new controller.
 **
 **    ADD_NODE_OPTION_HIGH_POWER    Set this flag in bMode for High Power inclusion.
 **
 **    Side effects:
 **
 **--------------------------------------------------------------------------*/

void ZW_AddNodeToNetwork(BYTE bMode,
VOID_CALLBACKFUNC(completedFunc) (auto LEARN_INFO*))
{
	idx = 0;
	byCompletedFunc = ( completedFunc == NULL ? 0 : 0x03 );
	buffer[ idx++ ] = bMode;
	buffer[ idx++ ] = byCompletedFunc;
	cbFuncAddNodeToNetwork = completedFunc;
	SendFrame(FUNC_ID_ZW_ADD_NODE_TO_NETWORK, buffer, idx );
}

/*==========================   ZW_RemoveNodeFromNetwork   ========================
 **
 **    Remove any type of node from the network
 **
 **    The modes are:
 **
 **    REMOVE_NODE_ANY            Remove any node from the network
 **    REMOVE_NODE_CONTROLLER     Remove a controller from the network
 **    REMOVE_NODE_SLAVE          Remove a slaev node from the network
 **
 **    REMOVE_NODE_STOP           Stop learn mode without reporting an error.
 **
 **    ADD_NODE_OPTION_HIGH_POWER    Set this flag in bMode for High Power exclusion.
 **
 **    Side effects:
 **
 **--------------------------------------------------------------------------*/
void ZW_RemoveNodeFromNetwork(BYTE bMode,
VOID_CALLBACKFUNC(completedFunc) (auto LEARN_INFO*))
{
	idx = 0;
	byCompletedFunc = ( completedFunc == NULL ? 0 : 0x03 );
	buffer[ idx++ ] = bMode;
	buffer[ idx++ ] = byCompletedFunc;
	cbFuncRemoveNodeFromNetwork = completedFunc;
	SendFrame(FUNC_ID_ZW_REMOVE_NODE_FROM_NETWORK, buffer, idx );
}

/*==========================   ZW_RemoveFailedNode   ===============================
 **
 **    remove a node from the failed node list, if it already exist.
 **    A call back function should be provided otherwise the function will return
 **    without removing the node.
 **    If the removing process started successfully then the function will return
 **    ZW_FAILED_NODE_REMOVE_STARTED        The removing process started
 **
 **    If the removing process can not be started then the API function will return
 **    on or more of the following flags
 **    ZW_NOT_PRIMARY_CONTROLLER             The removing process was aborted because the controller is not the primaray one
 **    ZW_NO_CALLBACK_FUNCTION              The removing process was aborted because no call back function is used
 **    ZW_FAILED_NODE_NOT_FOUND             The removing process aborted because the node was node found
 **    ZW_FAILED_NODE_REMOVE_PROCESS_BUSY   The removing process is busy
 **
 **    The call back function parameter value is:
 **
 **    ZW_NODE_OK                     The node is working proppely (removed from the failed nodes list )
 **    ZW_FAILED_NODE_REMOVED         The failed node was removed from the failed nodes list
 **    ZW_FAILED_NODE_NOT_REMOVED     The failed node was not
 **    Side effects:
 **--------------------------------------------------------------------------*/
BYTE /*RET function return code */
ZW_RemoveFailedNode(
BYTE NodeID, /* IN the failed nodeID */
VOID_CALLBACKFUNC(completedFunc) ( /* IN callback function to be called */
BYTE)) { /*    when the remove process end. */

	idx = 0;
	byCompletedFunc = ( completedFunc == NULL ? 0 : 0x03 );
	buffer[ idx++ ] = NodeID;
	buffer[ idx++ ] = byCompletedFunc;
	cbFuncZWRemoveFailedNode = completedFunc;

	byLen = 0;
	SendFrameWithResponse(FUNC_ID_ZW_REMOVE_FAILED_NODE_ID, buffer, idx, buffer, &byLen );
	return buffer[ IDX_DATA ];
}
BYTE /*RET true if node in failed node table, else false */
ZW_isFailedNode(
BYTE NodeID) /* IN the failed node ID */
{
	idx = 0;
	buffer[idx++] = NodeID;
	byLen = 0;
	SendFrameWithResponse(FUNC_ID_ZW_IS_FAILED_NODE_ID, buffer, idx, buffer,
			&byLen);
	return buffer[ IDX_DATA];
}
BYTE /*RET function return code */
ZW_ReplaceFailedNode(BYTE NodeID,
BOOL bNormalPower,
VOID_CALLBACKFUNC(completedFunc) (BYTE txStatus))
{
	idx = 0;
	byCompletedFunc = ( completedFunc == NULL ? 0 : 0x03 );
	buffer[ idx++ ] = NodeID;
	buffer[ idx++ ] = byCompletedFunc;
	cbFuncZWReplaceFailedNode = completedFunc;

	byLen = 0;
	SendFrameWithResponse(FUNC_ID_ZW_REPLACE_FAILED_NODE, buffer, idx, buffer, &byLen );
	return buffer[ IDX_DATA ];
}

//BYTE /*RET function return code */
//ZW_IsFailedNode(BYTE NodeID) {
//	idx = 0;
//	buffer[idx++] = NodeID;
//	buffer[idx++] = 0;
//
//	byLen = 0;
//	SendFrameWithResponse(FUNC_ID_ZW_REPLACE_FAILED_NODE, buffer, idx, buffer,
//			&byLen);
//	return buffer[ IDX_DATA];
//}

/**
 * \ingroup ZWCMD
 * Sends command completed to master remote.
 * Called in replication mode when a command from the sender has been processed.
 */
/*
 void ZW_ReplicationCommandComplete( void )
 {
 idx = 0;
 buffer[ idx++ ] = 0;
 buffer[ idx++ ] = REQUEST;
 buffer[ idx++ ] = FUNC_ID_ZW_REPLICATION_COMMAND_COMPLETE;
 buffer[ 0 ] = idx;	// length
 SendData( buffer, idx );
 }
 */

/**
 * \ingroup ZWCMD
 * Used when the controller is replication mode.
 * It sends the payload and expects the receiver to respond with a command complete message.
 * \param[in] nodeID         Destination node ID
 * \param[in] pData          Data buffer pointer
 * \param[in] dataLength     Data buffer length
 * \param[in] txOptions      Transmit option flags
 * \param[in] completedFunc  Transmit completed call back function
 * \param[in] txStatus       Transmit status
 * \return FALSE if transmitter queue overflow
 */
/*
 BYTE ZW_ReplicationSendData( BYTE nodeID, BYTE *pData, BYTE dataLength, BYTE txOptions, void ( *completedFunc ) ( BYTE ) )
 {
 int i;
 idx = 0;
 BYTE byCompletedFunc = ( completedFunc == NULL ? 0 : 0x03 );
 buffer[ idx++ ] = nodeID;
 buffer[ idx++ ] = dataLength;

 for ( i = 0;i < dataLength;i++ )
 buffer[ idx++ ] = pData[ i ];

 buffer[ idx++ ] = txOptions;
 buffer[ idx++ ] = byCompletedFunc;	// Func id for CompletedFunc
 byLen = 0;
 cbFuncZWReplicationSendData = completedFunc;
 SendDataAndWaitForResponse( buffer, idx, FUNC_ID_ZW_REPLICATION_SEND_DATA, buffer, &byLen );

 return buffer[ IDX_DATA ];
 }
 */

/*========================   ZW_AssignReturnRoute   =========================
 **
 **    Assign static return routes within a Routing Slave node.
 **    Calculate the shortest transport routes from the Routing Slave node
 **    to the route destination node and
 **    transmit the return routes to the Routing Slave node.
 **
 **--------------------------------------------------------------------------*/
BOOL /*RET TRUE if assign was initiated. FALSE if not */
ZW_AssignReturnRoute(
BYTE bSrcNodeID, /* IN Routing Slave Node ID */
BYTE bDstNodeID, /* IN Route destination Node ID */
VOID_CALLBACKFUNC(completedFunc) ( /* IN Callback function called when done */
BYTE bStatus))
{
	idx = 0;
	byCompletedFunc = ( completedFunc == NULL ? 0 : 0x03 );
	buffer[ idx++ ] = bSrcNodeID;
	buffer[ idx++ ] = bDstNodeID;
	buffer[ idx++ ] = byCompletedFunc;	// Func id for CompletedFunc
	cbFuncZWAssignReturnRoute = completedFunc;

	byLen = 0;
	SendFrameWithResponse(FUNC_ID_ZW_ASSIGN_RETURN_ROUTE, buffer, idx, buffer, &byLen );
	return buffer[ IDX_DATA ];

///    SendData( buffer, idx );
///    return  0;
}

/**
 * \ingroup ZWCMD
 * Delete static return routes within a Routing Slave node.
 * Transmit "NULL" routes to the Routing Slave node.
 * \param[in] nodeID          Routing Slave
 * \param[in] completedFunc   Completion handler
 * \param[in] bStatus        Transmit complete status
 */

BOOL ZW_DeleteReturnRoute( BYTE nodeID, void (*completedFunc)( BYTE)) {
	BYTE byCompletedFunc;

	idx = 0;
	byCompletedFunc = (completedFunc == NULL ? 0 : 0x03);
	buffer[idx++] = nodeID;
	buffer[idx++] = byCompletedFunc;	// Func id for CompletedFunc

	cbFuncZWDeleteReturnRoute = completedFunc;

	SendFrameWithResponse(FUNC_ID_ZW_DELETE_RETURN_ROUTE, buffer, idx, buffer,
			&byLen);
	return buffer[ IDX_DATA];
}

/**
 * \ingroup ZWCMD
 * Assign static return routes within a Routing Slave node.
 * Calculate the shortest transport routes to a Routing Slave node
 * from the Static Update Controller (SUC) Node and
 * transmit the return routes to the Routing Slave node.
 * \param[in] bSrcNodeID     Routing Slave Node ID
 * \param[in] bDstNodeID     SUC node ID
 * \param[in] completedFunc  Completion handler
 * \param[in] bStatus        Transmit complete status
 */

BOOL ZW_AssignSUCReturnRoute( BYTE bSrcNodeID, void (*completedFunc)( BYTE)) {
	BYTE byCompletedFunc;

	idx = 0;
	byCompletedFunc = (completedFunc == NULL ? 0 : 0x03);
	buffer[idx++] = bSrcNodeID;
	buffer[idx++] = byCompletedFunc;	// Func id for CompletedFunc
	cbFuncZWAssignSUCReturnRoute = completedFunc;
	SendFrameWithResponse(FUNC_ID_ZW_ASSIGN_SUC_RETURN_ROUTE, buffer, idx,
			buffer, &byLen);
	return buffer[ IDX_DATA];
}

/**
 * \ingroup ZWCMD
 * Delete the ( Static Update Controller -SUC-) static return routes
 * within a Routing Slave node.
 * Transmit "NULL" routes to the Routing Slave node.
 * \param[in] nodeID         Routing Slave
 * \param[in] completedFunc  Completion handler
 * \param[in] bStatus        Transmit complete status
 */

BOOL ZW_DeleteSUCReturnRoute( BYTE nodeID, void (*completedFunc)( BYTE)) {
	BYTE byCompletedFunc;

	idx = 0;
	byCompletedFunc = (completedFunc == NULL ? 0 : 0x03);
	buffer[idx++] = nodeID;
	buffer[idx++] = byCompletedFunc;	// Func id for CompletedFunc
	cbFuncZWDeleteSUCReturnRoute = completedFunc;

	SendFrameWithResponse(FUNC_ID_ZW_DELETE_SUC_RETURN_ROUTE, buffer, idx,
			buffer, &byLen);
	return buffer[ IDX_DATA];
}

/**
 * \ingroup ZWCMD
 * Get the currently registered SUC node ID.
 * \return SUC node ID, ZERO if no SUC available
 */
BYTE ZW_GetSUCNodeID(void) {
	idx = 0;
	byLen = 0;
	SendFrameWithResponse(FUNC_ID_ZW_GET_SUC_NODE_ID, 0, 0, buffer, &byLen);
	return buffer[ IDX_DATA];
}

/*============================   ZW_SetSUCNodeID  ===========================
 **    Function description
 **    This function enable /disable a specified static controller
 **    of functioning as the Static Update Controller
 **
 **--------------------------------------------------------------------------*/
BYTE /*RET TRUE target is a static controller*/
/*    FALSE if the target is not a static controller,  */
/*    the source is not primary or the SUC functinality is not enabled.*/
ZW_SetSUCNodeID(
BYTE nodeID, /* IN the node ID of the static controller to be a SUC */
BYTE SUCState, /* IN TRUE enable SUC, FALSE disable */
BYTE bTxOption, /* IN TRUE if to use low poer transmition, FALSE for normal Tx power */
BYTE bCapabilities, /* The capabilities of the new SUC */
VOID_CALLBACKFUNC(completedFunc) (auto BYTE txStatus)) /* IN a call back function */
{
	idx = 0;
	byCompletedFunc = ( completedFunc == NULL ? 0 : 0x03 );
	buffer[ idx++ ] = nodeID;
	buffer[ idx++ ] = SUCState; /* Do we want to enable or disable?? */
	buffer[ idx++ ] = bTxOption;
	buffer[ idx++ ] = bCapabilities;
	buffer[ idx++ ] = byCompletedFunc;	// Func id for CompletedFunc

	cbFuncZWSetSUCNodeID = completedFunc;
	byLen = 0;
	SendFrameWithResponse(FUNC_ID_ZW_SET_SUC_NODE_ID, buffer, idx, buffer, &byLen );
	return buffer[ IDX_DATA ];
}

/*===========================   ZW_SetLearnMode   ===========================
 **    Enable/Disable home/node ID learn mode.
 **    When learn mode is enabled, received "Assign ID's Command" are handled:
 **    If the current stored ID's are zero, the received ID's will be stored.
 **    If the received ID's are zero the stored ID's will be set to zero.
 **
 **    The learnFunc is called when the received assign command has been handled.
 **
 **--------------------------------------------------------------------------*/
void /*RET  Nothing        */
ZW_SetLearnMode(
BYTE mode, /*IN  TRUE: Enable; FALSE: Disable   */
VOID_CALLBACKFUNC(completedFunc) (LEARN_INFO*)/*VOID_CALLBACKFUNC(learnFunc)( BYTE bStatus,  BYTE nodeID)*/) /*IN  Node learn call back function. */
{
	idx = 0;
	byCompletedFunc = (completedFunc == NULL ? 0 : 0x03);
	buffer[idx++] = mode;
	buffer[idx++] = byCompletedFunc;      // Func id for CompletedFunc
	cbFuncZWSetLearnMode = completedFunc;
	SendFrame(FUNC_ID_ZW_SET_LEARN_MODE,buffer, idx);
}

/**
 * \ingroup ZWCMD
 * Get the Z-Wave library basis version.
 * \param[out]   pBuf
 *   Pointer to buffer where version string will be
 *   copied to. Buffer must be at least 14 bytes long.
 */
BYTE ZW_Version( BYTE *pBuf) {
	BYTE retVal;

	idx = 0;
	byLen = 0;
	retVal = SendFrameWithResponse(FUNC_ID_ZW_GET_VERSION, 0, 0, buffer,
			&byLen);
	if (retVal == conFrameReceived) {
		memcpy(pBuf, (void *) &buffer[ IDX_DATA], 12);
		return buffer[ IDX_DATA + 12];
	}
	return 0;
}



BYTE ZW_SetSlaveLearnMode(BYTE node, BYTE mode, /*IN */
VOID_CALLBACKFUNC(completedFunc) (BYTE, BYTE, BYTE))
{
	BYTE retVal;

	idx = 0;
	byCompletedFunc = (completedFunc == NULL ? 0 : 0x03);
	buffer[idx++] = node;
	buffer[idx++] = mode;
	buffer[idx++] = byCompletedFunc;      // Func id for CompletedFunc
	cbFuncZWSetSlaveLearnMode = completedFunc;
	retVal = SendFrameWithResponse(FUNC_ID_ZW_SET_SLAVE_LEARN_MODE, buffer, idx, buffer, &byLen );
	if (retVal == conFrameReceived) {
		return buffer[ IDX_DATA ];
	}
	return 0;
}

/**
 * \ingroup ZWCMD
 * Set ApplicationNodeInformation data to be used in subsequent
 * calls to \ref ZW_SendNodeInformation.
 * \param[in] listening   = TRUE : if this node is always on air
 * \param[in] nodeType    Device type
 * \param[in] nodeParm    Device parameter buffer
 * \param[in] parmLength  Number of Device parameter bytes
 */
void SerialAPI_ApplicationNodeInformation( BYTE listening,
		APPL_NODE_TYPE nodeType, BYTE *nodeParm, BYTE parmLength) {
	int i;
	idx = 0;
	buffer[idx++] = listening;
	buffer[idx++] = nodeType.generic;
	buffer[idx++] = nodeType.specific;
	buffer[idx++] = parmLength;

	for (i = 0; i != parmLength; i++) {
		buffer[idx++] = nodeParm[i];
	}

	SendFrame(FUNC_ID_SERIAL_API_APPL_NODE_INFORMATION, buffer, idx);
}
/**
 * \ingroup ZWCMD
 * Set ApplicationNodeInformation data to be used in subsequent
 * calls to \ref ZW_SendSlaveNodeInformation.
 * This takes effect for all virtual nodes, regardless of dstNode value.
 * \param[in] dstNode     Virtual node id. This value is IGNORED.
 * \param[in] listening   = TRUE : if this node is always on air
 * \param[in] nodeType    Device type
 * \param[in] nodeParm    Device parameter buffer
 * \param[in] parmLength  Number of Device parameter bytes
 */
void SerialAPI_ApplicationSlaveNodeInformation(BYTE dstNode, BYTE listening,
		APPL_NODE_TYPE nodeType, BYTE *nodeParm, BYTE parmLength) {
	int i;
	idx = 0;
	buffer[idx++] = dstNode;
	buffer[idx++] = listening;
	buffer[idx++] = nodeType.generic;
	buffer[idx++] = nodeType.specific;
	buffer[idx++] = parmLength;

	for (i = 0; i != parmLength; i++) {
		buffer[idx++] = nodeParm[i];
	}
	SendFrame(FUNC_ID_SERIAL_API_APPL_SLAVE_NODE_INFORMATION, buffer, idx);
}

/**
 * \ingroup ZWCMD
 * Get Serial API initialization data from remote side (Enhanced Z-Wave module).
 * \param[out]   ver            Remote sides Serial API version
 * \param[out]   capabilities   Capabilities flag (GET_INIT_DATA_FLAG_xxx)
 *   Capabilities flag: \n
 *      Bit 0: 0 = Controller API; 1 = Slave API \n
 *      Bit 1: 0 = Timer functions not supported; 1 = Timer functions supported. \n
 *      Bit 2: 0 = Primary Controller; 1 = Secondary Controller \n
 *      Bit 3: 0 = Not SUC; 1 = This node is SUC (static controller only) \n
 *      Bit 3-7: Reserved \n
 *   Timer functions are: TimerStart, TimerRestart and TimerCancel.
 * \param[out]   len            Number of bytes in nodesList
 * \param[out]   nodesList      Bitmask list with nodes known by Z-Wave module 29B
 */
BYTE SerialAPI_GetInitData( BYTE *ver, BYTE *capabilities, BYTE *len,
BYTE *nodesList, BYTE* chip_type, BYTE* chip_version) {
	BYTE *p;
	int i;
	idx = 0;
	byLen = 0;
	*ver = 0;
	*capabilities = 0;
	SendFrameWithResponse(FUNC_ID_SERIAL_API_GET_INIT_DATA, 0, 0, buffer,
			&byLen);
	p = &buffer[ IDX_DATA];
	*ver = *p++;

	//controller api eller slave api
	*capabilities = *p++;
	*len = *p++;

	for (i = 0; i < 29; i++)
		nodesList[i] = *p++;
	*chip_type = *p++;
	*chip_version = *p++;

	// Bit 2 tells if it is Primary Controller (FALSE) or Secondary Controller (TRUE).
	if ((*capabilities) & GET_INIT_DATA_FLAG_SECONDARY_CTRL)
		return ( TRUE);
	else
		return ( FALSE);
}

/**
 * \ingroup ZWCMD
 * Enable the SUC functionality in a controller.
 * \param[in] state
 *   = TRUE : Enable SUC \n
 *   = FALSE : Disable SUC \n
 * \param[in] capabilities
 *   = ZW_SUC_FUNC_BASIC_SUC : Only enables the basic SUC functionality \n
 *   = ZW_SUC_FUNC_NODEID_SERVER : Enable the node ID server functionality to become a SIS. \n
 * \return
 *   = TRUE : The SUC functionality was enabled/disabled \n
 *   = FALSE : Attempting to disable a running SUC, not allowed \n
 */
BOOL ZW_EnableSUC( BYTE state, BYTE capabilities) {
	idx = 0;
	idx++;
	buffer[idx++] = state;
	buffer[idx++] = capabilities;
	byLen = 0;
	SendFrameWithResponse( FUNC_ID_ZW_ENABLE_SUC, buffer, idx, buffer, &byLen);

	return buffer[ IDX_DATA];
}

/*============================   ZW_RequestNodeInfo   ======================
 **    Function description.
 **     Request a node to send it's node information.
 **     Function return TRUE if the request is send, else it return FALSE.
 **     FUNC is a callback function, which is called with the status of the
 **     Request nodeinformation frame transmission.
 **     If a node sends its node info, ApplicationControllerUpdate will be called
 **     with UPDATE_STATE_NODE_INFO_RECEIVED as status together with the received
 **     nodeinformation.
 **
 **    Side effects:
 **		backfun Not
 **--------------------------------------------------------------------------*/
BOOL /*RET FALSE if transmitter busy */
ZW_RequestNodeInfo(
BYTE nodeID, /*IN: node id of the node to request node info from it.*/
VOID_CALLBACKFUNC(completedFunc) (auto BYTE)) /* IN Callback function not callback*/
{
	idx = 0;
	byLen = 0;
	buffer[ idx++ ] = nodeID;
	idx++;
	SendFrameWithResponse(FUNC_ID_ZW_REQUEST_NODE_INFO, buffer, idx, buffer, &byLen );

	return buffer[ IDX_DATA ];
}

/**
 * \ingroup ZWCMD
 * Used when the controller is replication mode.
 * It sends the payload and expects the receiver to respond with a command complete message.
 * \param[in] nodeID         Destination node ID
 * \param[in] pData          Data buffer pointer
 * \param[in] dataLength     Data buffer length
 * \param[in] txOptions      Transmit option flags
 * \param[in] completedFunc  Transmit completed call back function
 * \param[in] txStatus       Transmit status
 * \return FALSE if transmitter queue overflow
 */

BYTE ZW_ReplicationSend( BYTE nodeID, BYTE *pData, BYTE dataLength,
BYTE txOptions, VOID_CALLBACKFUNC(completedFunc ) (auto BYTE txStatus) )
{
	int i;
	byCompletedFunc = ( completedFunc == NULL ? 0 : 0x03 );
	idx = 0;
	buffer[ idx++ ] = nodeID;
	buffer[ idx++ ] = dataLength;

	for ( i = 0;i < dataLength;i++ )
	buffer[ idx++ ] = pData[ i ];

	buffer[ idx++ ] = txOptions;
	buffer[ idx++ ] = byCompletedFunc;	// Func id for CompletedFunc
	byLen = 0;
	cbFuncZWReplicationSendData = completedFunc;
	SendFrameWithResponse(FUNC_ID_ZW_REPLICATION_SEND_DATA, buffer, idx, buffer, &byLen );

	return buffer[ IDX_DATA ];
}

/**
 * \ingroup ZWCMD
 * Get capabilities of a controller.
 * \return
 *   = CONTROLLER_IS_SECONDARY :
 *      If bit is set then the controller is a secondary controller \n
 *   = CONTROLLER_ON_OTHER_NETWORK :
 *      If this bit is set then this controller is not using its build-in home ID \n
 *   = CONTROLLER_IS_SUC :
 *      If this bit is set then this controller is a SUC \n
 *   = CONTROLLER_NODEID_SERVER_PRESENT :
 *      If this bit is set then there is a SUC ID server (SIS)
 *      in the network and this controller can therefore
 *      include/exclude nodes in the network (inclusion controller). \n
 */
BYTE ZW_GetControllerCapabilities(void) {
	idx = 0;
	byLen = 0;
	idx++;
	SendFrameWithResponse(FUNC_ID_ZW_GET_CONTROLLER_CAPABILITIES, buffer, idx,
			buffer, &byLen);

	return buffer[ IDX_DATA];
}

/**
 \ingroup BASIS
 \macro{ ZW_REQUEST_NETWORK_UPDATE(FUNC) }
 Used to request network topology updates from the SUC/SIS node. The update is done on protocol level
 and any changes are notified to the application by calling the ApplicationControllerUpdate).
 Secondary controllers can only use this call when a SUC is present in the network. All controllers can
 use this call in case a SUC ID Server (SIS) is available.
 Routing Slaves can only use this call, when a SUC is present in the network. In case the Routing Slave
 has called ZW_RequestNewRouteDestinations prior to ZW_RequestNetWorkUpdate, then Return
 Routes for the destinations specified by the application in ZW_RequestNewRouteDestinations will be
 updated along with the SUC Return Route.
 \note
 The SUC can only handle one network update at a time, so care should be taken not to have all
 the controllers in the network ask for updates at the same time.
 \warning
 This API call will generate a lot of network activity that will use bandwidth and stress the
 SUC in the network. Therefore, network updates should be requested as seldom as possible and never
 more often that once every hour from a controller.

 @return TRUE If the updating process is started.
 FALSE If the requesting controller is the SUC node or the SUC node is unknown.
 @param completedFunc Transmit complete call back.
 @param txStatus

 ZW_SUC_UPDATE_DONE The update process succeeded.
 ZW_SUC_UPDATE_ABORT The update process aborted because of
 an error.
 ZW_SUC_UPDATE_WAIT The SUC node is busy.
 ZW_SUC_UPDATE_DISABLED The SUC functionality is disabled.
 ZW_SUC_UPDATE_OVERFLOW The controller requested an update after more than 64 changes have occurred in
 the network. The update information is then out of date in respect to that
 controller. In this situation the controller have to make a replication
 before trying to request any new network updates.
 <b>Timeout:</b> 35s

 <b>Exption recovery:</b> Resume normal operation, no recovery needed

 \serialapi{
 HOST->ZW: REQ | 0x53 | funcID
 ZW->HOST: RES | 0x53 | retVal
 ZW->HOST: REQ | 0x53 | funcID | txStatus
 }
 */
BOOL ZW_RequestNetWorkUpdate(void (*complFunc)( BYTE)) {
	idx = 0;
	byCompletedFunc = (complFunc == NULL ? 0 : 0x03);
	buffer[idx++] = byCompletedFunc;
	cbFuncZWRequestNetworkUpdate = complFunc;
	byLen = 0;
	SendFrameWithResponse(FUNC_ID_ZW_REQUEST_NETWORK_UPDATE, buffer, idx,
			buffer, &byLen);
	return buffer[ IDX_DATA];
}

/**
 * \ingroup BASIS
 * \macro{ZW_EXPLORE_REQUEST_INCLUSION()}
 * This function sends out an explorer frame requesting inclusion into a network. If the inclusion request is
 * accepted by a controller in network wide inclusion mode then the application on this node will get notified
 * through the callback from the ZW_SetLearnMode() function. Once a callback is received from
 * ZW_SetLearnMode() saying that the inclusion process has started the application should not make
 * further calls to this function.
 * @return
 *  TRUE Inclusion request queued for transmission
 * @return
 *  FALSE Node is not in learn mode
 *
 *  \note Recommend not to call this function more than once every 4 seconds.
 *  \serialapi{
 *  HOST->ZW: REQ|0x5E
 *  ZW->HOST: RES|0x5E|retVal
 *  }
 */
BYTE ZW_ExploreRequestInclusion() {
	idx = 0;
	byLen = 0;
	SendFrameWithResponse(FUNC_ID_ZW_EXPLORE_REQUEST_INCLUSION, buffer, idx,
			buffer, &byLen);
	return buffer[ IDX_DATA];
}

/**
 * \ingroup BASIS
 * \macro{ZW_GET_PROTOCOL_STATUS()}
 * Report the status of the protocol.
 * The function return a mask telling which protocol function is currently running
 * @return
 * Zero Protocol is idle.
 * @return
 * ZW_PROTOCOL_STATUS_ROUTING Protocol is analyzing the routing table.
 * @return
 * ZW_PROTOCOL_STATUS_SUC  SUC sends pending updates.
 * \serialapi{
 * HOST->ZW: REQ | 0xBF
 * ZW->HOST: RES | 0xBF | retVal
 * }
 */
BYTE ZW_GetProtocolStatus() {
	idx = 0;
	byLen = 0;
	SendFrameWithResponse(FUNC_ID_ZW_GET_PROTOCOL_STATUS, buffer, idx, buffer,
			&byLen);
	return buffer[ IDX_DATA];
}

/**
 * \ingroup ZWCMD
 * Get the Z Wave library type.
 * \return
 *   = ZW_LIB_CONTROLLER_STATIC	Static controller library
 *   = ZW_LIB_CONTROLLER_BRIDGE	Bridge controller library
 *   = ZW_LIB_CONTROLLER	        Portable controller library
 *   = ZW_LIB_SLAVE_ENHANCED	    Enhanced slave library
 *   = ZW_LIB_SLAVE_ROUTING	    Routing slave library
 *   = ZW_LIB_SLAVE	            Slave library
 *   = ZW_LIB_INSTALLER	        Installer library
 */
BYTE ZW_Type_Library(void) {
	idx = 0;
	byLen = 0;
	idx++;
	SendFrameWithResponse(FUNC_ID_ZW_TYPE_LIBRARY, 0, 0, buffer, &byLen);

	return buffer[ IDX_DATA];
}

/**
 * \ingroup ZWCMD
 * Lock a response route for a specific node.
 * \param[in] bNodeID
 */
void ZW_ReplicationReceiveComplete() {
	SendFrame(FUNC_ID_ZW_REPLICATION_COMMAND_COMPLETE, 0, 0);
}

/**
 * \ingroup ZWCMD
 * Lock a response route for a specific node.
 * \param[in] bNodeID
 */
void ZW_SoftReset() {
	SendFrame(FUNC_ID_SERIAL_API_SOFT_RESET, buffer, idx);
}

/**
 * Get the current power level used in RF transmitting.
 * \note
 * This function should only be used in an install/test link situation.
 * \return
 * The power level currently in effect during RF transmissions
 * \sa ZW_RFPowerLevelSet
 *
 * \serialapi{
 * HOST->ZW: REQ | 0xBA
 * ZW->HOST: RES | 0xBA | powerlevel
 * }

 */
BYTE /*RET The current powerlevel */
ZW_RFPowerLevelGet(void) /* IN Nothing */
{
	idx = 0;
	byLen = 0;
	SendFrameWithResponse(FUNC_ID_ZW_RF_POWER_LEVEL_GET, buffer, idx, buffer,
			&byLen);
	return buffer[ IDX_DATA];
}

/**
 * \ingroup ZWCMD
 * Transmit data buffer to a list of Z-Wave Nodes (multicast frame).
 * \param[in] pNodeIDList    List of destination node ID's
 * \param[in] numberNodes    Number of Nodes
 * \param[in] pData          Data buffer pointer
 * \param[in] dataLength     Data buffer length
 * \param[in] txOptions      Transmit option flags
 * \param[in] completedFunc  Transmit completed call back function
 * \return FALSE if transmitter queue overflow
 */
BYTE ZW_SendDataMulti(
BYTE *pNodeIDList,
BYTE numberNodes,
BYTE *pData,
BYTE dataLength,
BYTE txOptions,
VOID_CALLBACKFUNC ( completedFunc ) (auto BYTE txStatus ) )
{
	int i;
	BYTE byCompletedFunc;
	byCompletedFunc = ( completedFunc == NULL ? 0 : 0x03 );
	idx = 0;

	numberNodes = ( numberNodes <= ZW_MAX_NODES ) ? numberNodes : ZW_MAX_NODES;
	buffer[ idx++ ] = numberNodes;

	for ( i = 0; i < numberNodes; i++ )
	{
		buffer[ idx++ ] = pNodeIDList[ i ];
	}

	buffer[ idx++ ] = dataLength;

	for ( i = 0; i < dataLength; i++ )
	{
		buffer[ idx++ ] = pData[ i ];
	}

	buffer[ idx++ ] = txOptions;
	buffer[ idx++ ] = byCompletedFunc;	// Func id for CompletedFunc
	byLen = 0;
	cbFuncZWSendDataMulti = completedFunc;
	SendFrameWithResponse(FUNC_ID_ZW_SEND_DATA_MULTI,buffer,idx, buffer, &byLen);
	return buffer[ IDX_DATA ];
}

/**
 * \ingroup BASIS
 * The API call generates a random word using the ZW0201/ZW0301 builtin random number
 * generator (RNG). If RF needs to be in Receive then ZW_SetRFReceiveMode should be called afterwards.
 * \return
 * TRUE If possible to generate random number.
 * \return
 * FALSE  If not possible e.g. RF not powered down.
 * \param[in,out] randomWord Pointer to word variable, which should receive the random word.
 * \param[in] bResetRadio  If TRUE the RF radio is reinitialized after generating the random word.
 *
 * \note
 * The ZW0201/ZW0301 RNG is based on the RF transceiver, which must be in powerdown
 * state (see ZW_SetRFReceiveMode) to assure proper operation of the RNG. Remember
 * to call ZW_GetRandomWord with bResetRadio = TRUE when the last random word is to
 * be generated. This is needed for the RF to be reinitialized, so that it can be
 * used to transmit and receive again.
 * *
 * \macro{ZW_GET_RANDOM_WORD(randomWord\,bResetRadio)}
 * \serialapi{
 * HOST -> ZW: REQ | 0x1C | noRandomBytes
 * ZW -> HOST: RES | 0x1C | randomGenerationSuccess | noRandomBytesGenerated | randombytes[]
 * }
 * \note
 * The Serial API function 0x1C makes use of the ZW_GetRandomWord to generate a specified number of random bytes and takes care of the handling of the RF:
 * - Set the RF in powerdown prior to calling the ZW_GetRandomWord the first time, if not possible then return result to HOST.
 * - Call ZW_GetRandomWord until enough random bytes generated or ZW_GetRandomWord returns FALSE.
 * - Call ZW_GetRandomWord with bResetRadio = TRUE to reinitialize the radio.
 * - Call ZW_SetRFReceiveMode with TRUE if the serialAPI hardware is a listening device or with FALSE if it is a non-listening device.
 * - Return result to HOST.
 *
 * @param[in]  noRandomBytes to generate
 * @param[out] randomGenerationSuccess  TRUE if random bytes could be generated
 * @param[out] noRandomBytesGenerated   Number of random numbers generated
 * @param[out] randombytes[] Array of generated random bytes
 *
 */
BOOL ZW_GetRandomWord( BYTE *randomWord, BOOL bResetRadio) {
	int i;
	idx = 0;

	/*  bLine | bRemoveBad | bRemoveNonReps | funcID */
	buffer[idx++] = 0x8;
	byLen = 0;
	SendFrameWithResponse(FUNC_ID_ZW_GET_RANDOM, buffer, idx, buffer, &byLen);

	for (i = 0; i < 0x8; i++) {
		randomWord[i] = buffer[ IDX_DATA + i + 2];
	}
	return 0;
}

/**
 * The Serial API function 0x1C makes use of the ZW_GetRandomWord to generate a specified number
 * of random bytes and takes care of the handling of the RF:
 *  - Set the RF in powerdown prior to calling the ZW_GetRandomWord the first time, if not
 *    possible then return result to HOST.
 *  - Call ZW_GetRandomWord until enough random bytes generated or ZW_GetRandomWord
 *    returns FALSE.
 *  - Call ZW_GetRandomWord with bResetRadio = TRUE to reinitialize the radio.
 *  - Call ZW_SetRFReceiveMode with TRUE if the serialAPI hardware is a listening device or with
 *    FALSE if it is a non-listening device.
 *  - Return result to HOST.
 *
 *  @param count        Number of random bytes needed. Returned Range 1...32 random bytes are supported.
 *  @param randomBytes  Destination buffer.
 */
BOOL SerialAPI_GetRandom(BYTE count, BYTE* randomBytes) {
	idx = 0;
	buffer[idx++] = count;
	byLen = 0;

	ASSERT(count <= 32);

	SendFrameWithResponse(FUNC_ID_ZW_GET_RANDOM, buffer, idx, buffer, &byLen);

	if (!buffer[IDX_DATA])
		return FALSE;

	memcpy(randomBytes, &buffer[IDX_DATA + 2], buffer[ IDX_DATA + 1]);
	return TRUE;
}

void ZW_GetBasic( BYTE *pData) {
	int i;
	idx = 0;
	/*  bLine | bRemoveBad | bRemoveNonReps | funcID */
	byLen = 0;
	SendFrameWithResponse(BASIC_GET, 0, 0, buffer, &byLen);

	for (i = 0; i < 29; i++) {
		pData[i] = buffer[ IDX_DATA + i + 3];
	}
}

/**
 * This function is used to request whether the controller is a primary controller or a secondary controller in
 * the network.
 * Defined in: ZW_controller_api.h
 * \return TRUE when the controller is a
 * primary controller in the network. FALSE when the controller is a
 * secondary controller in the network.
 */
BOOL ZW_IsPrimaryCtrl(void) {
	return ((ZW_GetControllerCapabilities() & CONTROLLER_IS_SECONDARY) == 0);
}

#ifdef SECURITY_SUPPORT
#include "rijndael-alg-fst.h"
BOOL SerialAPI_AES128_Encrypt(const BYTE *ext_input, BYTE *ext_output, const BYTE *cipherKey) CC_REENTRANT_ARG {
	int Nr; /* key-length-dependent number of rounds */
	u32 rk[4*(MAXNR + 1)]; /* key schedule */
	/*if(SupportsCommand(FUNC_ID_ZW_AES_ECB)) {
	 memcpy(&buffer[0],cipherKey,16);
	 memcpy(&buffer[16],ext_input,16);
	 SendFrameWithResponse(FUNC_ID_ZW_AES_ECB, buffer, 32 , buffer, &byLen );
	 memcpy(ext_output,&buffer[IDX_DATA],16);
	 return 1;
	 } else*/{
		Nr = rijndaelKeySetupEnc(rk, cipherKey, 128);
		rijndaelEncrypt(rk, Nr, ext_input, ext_output);
		return 1;
	}
}
#endif

void ZW_WatchDogDisable() {
	SendFrame(FUNC_ID_ZW_WATCHDOG_DISABLE, 0, 0);
}

void ZW_WatchDogEnable() {
	SendFrame(FUNC_ID_ZW_WATCHDOG_ENABLE, 0, 0);
}

/**
 * Use this function to set the maximum number of source routing attempts before the explorer frame mechanism kicks
 * in. Default value with respect to maximum number of source routing attempts is five.
 * Remember to enable the explorer frame mechanism by setting the transmit option flag
 * TRANSMIT_OP TION_EXPLORE in the send data calls.
 * A ZDK 4.5 controller uses the routing algorithm from 5.02 to address nodes from
 * ZDK's not supporting explorer frame.
 * The routing algorithm from 5.02 ignores the transmit option *TRANSMIT_OPTION_EXPLORE flag and maximum number of source routing attempts value
 * (maxRouteTries)
 */
void ZW_SetRoutingMAX(BYTE maxRouteTries) {
	idx = 0;
	byLen = 0;
	buffer[idx++] = maxRouteTries;
	SendFrameWithResponse(FUNC_ID_ZW_SET_ROUTING_MAX, buffer, idx, buffer,
			&byLen);
	return;
}

/*
 *   Add CCs to PAN Node Info Frame. Useful for advertising capabilities
 *   of the unsolicited destination.
 *   Subsequent calls to this function will overwrite previously added CCs.
 */
void AddUnsocDestCCsToGW(BYTE *ccList, BYTE ccCount) CC_REENTRANT_ARG
/* Reentrant to conserve XDATA on ASIX C51 */
{
	BYTE listening;
	APPL_NODE_TYPE nodeType;
	BYTE *nodeParm;
	BYTE parmLength;
	BYTE ccBuf[20];
	BYTE roomLeft;

	if (callbacks->ApplicationNodeInformation) {
		callbacks->ApplicationNodeInformation(&listening, &nodeType, &nodeParm,
				&parmLength);
		memcpy(ccBuf, nodeParm, parmLength);
		roomLeft = sizeof(ccBuf) - parmLength;
		if (ccCount > roomLeft) {
			ccCount = roomLeft;
		}
		memcpy(ccBuf + parmLength, ccList, ccCount);
		SerialAPI_ApplicationNodeInformation(listening, nodeType, ccBuf,
				parmLength + ccCount);
	}
	/* Note: Not caching added CC's anywhere. They are only cleared on GW reset, where the
	 * added CCs should be cleared too. */
}

/* Enable Auto Programming over serial interface */
void ZW_AutoProgrammingEnable(void) {
	SendFrame(FUNC_ID_AUTO_PROGRAMMING, 0, 0);
}

/**
 *  Use this API call to get the Last Working Route (LWR) for a destination node if any exist.
 *  The LWR is the
 *  last successful route used between sender and destination node. The LWR is stored in NVM.
 * @return TRUE A LWR exists for bNodeID and the found route placed in the
 *      5-byte array pointed out by pLastWorkingRoute.
 *         FALSE No LWR found for bNodeID.
 * @param[in] bNodeID IN The Node ID (1...232) specifies the destination node whom the LWR is wanted from.
 * @param[in] pLastWorkingRoute Pointer to a 5-byte array where the wanted LWR will be written.
 The 5-byte array contains in the first 4 byte the max 4 repeaters (index 0 -3) and
 1 routespeed byte (index 4) used in the LWR. The LWR which pLastWorkingRoute
 points to is valid if function return value equals
 TRUE. The first repeater byte (starting from index 0) equaling zero indicates no more
 repeaters in route. If the repeater at index 0 is zero then the LWR is direct. The routespeed
 byte (index 4) can be either ZW_LAST_WORKING_ROUTE_SPEED_9600,ZW_LAST_WORKING_ROUTE_SPEED_40K
 or ZW_LAST_WORKING_ROUTE_SPEED_100K

 Serial API
 HOST->ZW: REQ | 0x92 | bNodeID
 ZW->HOST: RES | 0x92 | bNodeID | retVal | repeater0 | repeater1 | repeater2 | repeater3 | routespeed
 */
BOOL ZW_GetLastWorkingRoute(BYTE bNodeID, XBYTE *pLastWorkingRoute) {
	idx = 0;
	byLen = 0;

	buffer[idx++] = bNodeID;
	SendFrameWithResponse(FUNC_ID_ZW_GET_LAST_WORKING_ROUTE, buffer, idx,
			buffer, &byLen);
	ASSERT(bNodeID == buffer[IDX_DATA]);
	memcpy(pLastWorkingRoute, &buffer[IDX_DATA + 2], 5);
	return buffer[IDX_DATA + 1];
}

/**
 * Use this API call to set the Last Working Route (LWR) for a destination node. The LWR is the last
 * successful route used between sender and destination node. The LWR is stored in NVM.
 *
 * @return BOOL TRUE The LWR for bNodeID was successfully set to the specified5-byte
 * LWR pointed out by pLastWorkingRoute. FALSE The specified bNodeID was not valid and no LWR was set.
 *
 * @param[IN] bNodeID The Node ID (1...232) - specifies the destination node for whom the LWR is to be set.
 * @param[IN] pLastWorkingRoute Pointer for a 5-byte array containing the new LWR to be set.
 * The 5-byte array contains 4 repeater node bytes (index 0 - 3) and 1 routespeed byte (index 4).
 * The first repeater byte (starting from index 0) equaling zero indicates no more repeaters in route.
 * If the repeater at index 0 is zero then the LWR is direct. The routespeed byte (index 4) can be either
 * ZW_LAST_WORKING_ROUTE_SPEED_9600,
 * ZW_LAST_WORKING_ROUTE_SPEED_40K
 * or
 * ZW_LAST_WORKING_ROUTE_SPEED_100K
 * Serial API
 * HOST->ZW: REQ | 0x93 | bNodeID | repeater0 | repeater1 | repeater2 | repeater3 | routespeed
 * ZW->HOST: RES | 0x93 | bNodeID | retVa
 */
BOOL ZW_SetLastWorkingRoute(BYTE bNodeID, XBYTE *pLastWorkingRoute) {

	idx = 0;
	buffer[idx++] = bNodeID;
	buffer[idx++] = pLastWorkingRoute[0];
	buffer[idx++] = pLastWorkingRoute[1];
	buffer[idx++] = pLastWorkingRoute[2];
	buffer[idx++] = pLastWorkingRoute[3];
	buffer[idx++] = pLastWorkingRoute[4];
	SendFrameWithResponse(FUNC_ID_ZW_SET_LAST_WORKING_ROUTE, buffer, idx,
			buffer, &byLen);
	return buffer[IDX_DATA];
}
