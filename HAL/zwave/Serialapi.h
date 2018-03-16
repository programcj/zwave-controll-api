

#ifndef SERIAL_API_H_
#define SERIAL_API_H_

#include "TYPES.H"

#define ZW_CONTROLLER
#define CC_REENTRANT_ARG
//#define ZW_CONTROLLER_BRIDGE

#include "include/ZW_SerialAPI.h"
#include "include/ZW_basis_api.h"
#include "include/ZW_controller_api.h"
#include "include/ZW_transport_api.h"
#include "include/ZW_timer_api.h"
#include "include/ZW_classcmd_ex.h"

extern const char* zw_lib_names[];


struct SerialAPI_Callbacks {
	void (*ApplicationCommandHandler)(BYTE rxStatus, BYTE destNode,
			BYTE sourceNode, ZW_APPLICATION_TX_BUFFER *pCmd, BYTE cmdLength);
//节点信息回调
	void (*ApplicationNodeInformation)(BYTE *deviceOptionsMask,
			APPL_NODE_TYPE *nodeType, BYTE **nodeParm, BYTE *parmLength);
	void (*ApplicationControllerUpdate)(BYTE bStatus, BYTE bNodeID, BYTE* pCmd,
			BYTE bLen);
	BYTE (*ApplicationInitHW)(BYTE bWakeupReason); //初始化回调 串口打开调用 1
	BYTE (*ApplicationInitSW)(void);	//初始化回调 串口打开调用 2
	void (*ApplicationPoll)(void);
	void (*ApplicationTestPoll)(void);
	//桥的回调
	void (*ApplicationCommandHandler_Bridge)(BYTE rxStatus, BYTE destNode,
			BYTE sourceNode, ZW_APPLICATION_TX_BUFFER *pCmd, BYTE cmdLength);
};

BOOL SerialAPI_Init(const char* serial_port,
	const struct SerialAPI_Callbacks* callbacks);
void SerialAPI_Destroy();

u8_t SerialAPI_Poll();
void QueueHandler();

#define TRANSMIT_COMPLETE_ERROR 0xFF

BYTE SerialAPI_GetInitData( BYTE *ver, BYTE *capabilities, BYTE *len,
	BYTE *nodesList, BYTE* chip_type, BYTE* chip_version);

BOOL SerialAPI_GetRandom(BYTE count, BYTE* randomBytes);


BYTE ZW_LTimerStart(void (*func)(), unsigned long timerTicks, BYTE repeats);
BYTE ZW_LTimerRestart(BYTE handle);
BYTE ZW_LTimerCancel(BYTE handle);

BOOL ZW_EnableSUC( BYTE state, BYTE capabilities);

void SerialAPI_ApplicationSlaveNodeInformation(BYTE dstNode, BYTE listening,APPL_NODE_TYPE nodeType, BYTE *nodeParm, BYTE parmLength);

void Get_SerialAPI_AppVersion(uint8_t *major, uint8_t *minor);

void Get_SerialAPI_Manufacture(BYTE *mid1, BYTE *mid2, BYTE *pt1, BYTE *pt2,BYTE *pid1, BYTE *pid2);

BYTE ZW_SendData_NOTFUNID(BYTE nodeID, BYTE *pData, BYTE dataLength,BYTE txOptions);

void ZW_AutoProgrammingEnable(void);

void AddUnsocDestCCsToGW(BYTE *ccList, BYTE ccCount);

void ZW_GetRoutingInfo_old( BYTE bNodeID, BYTE *buf, BYTE bRemoveBad,BYTE bRemoveNonReps);

#endif /* SERIAL_API_H_ */

