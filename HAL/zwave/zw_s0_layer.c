/*
 * zw_s0_layer.c
 *
 *  Created on: 2017年12月13日
 *      Author: cj
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <openssl/aes.h>

#include "config.h"

#include "mem.h"
#include "zw_s0_layer.h"
#include "zwave_app.h"
#include "zw_node_managent.h"

#define log_hex(...)

static struct list_head _list_adapter_node;
static struct list_head _list_none_get;
static struct list_head _list_senddata;

//==============================================================================================
static void LoadKeys();
static void AES_ECB(BYTE key[16], BYTE input[16], BYTE output[16]);
//static void AES_ECB_Decrypt(BYTE key[16], BYTE input[16], BYTE output[16]);

static struct SecurityNonceReport {
	BYTE iv[0xFF][8];
} NodeReport[255]; //520200byte=508kb

static BYTE K1[16];
static BYTE K2[16];
static BYTE H0[16];
static BYTE H1[16];
static BYTE H2[16];
static BYTE innerState[16];

void ZWaveRPNG() {
	memset(K1, 0, 16);
	memset(K2, 0, 16);
	memset(H0, 0, 16);
	memset(H1, 0, 16);
	memset(H2, 0, 16);
	memset(innerState, 0, 16);
}

void RPNGUpdate() {
	int i;
	BYTE hwData[32];
	BYTE S[16];
	BYTE ISGen[16];

	memcpy(K1, hwData, 16);
	memcpy(K2, hwData + 16, 16);
	memset(H0, 0xA5, 16);
	AES_ECB(K1, H0, H1);
	for (i = 0; i < 16; i++)
		H1[i] ^= H0[i];
	AES_ECB(K2, H1, H2);
	for (i = 0; i < 16; i++)
		H2[i] ^= H1[i];
	for (i = 0; i < 16; i++)
		S[i] = innerState[i] ^ H2[i];
	memset(ISGen, 0x36, 16);
	AES_ECB(S, ISGen, innerState);
}

void RPNGOutput(BYTE output[8]) {
	BYTE tempOutput[16];
	BYTE tempKey[16];
	BYTE ISGen[16];

	memset(H0, 0x5C, 16);
	AES_ECB(innerState, H0, tempOutput);
	memset(ISGen, 0x36, 16);
	memcpy(tempKey, innerState, 16);
	AES_ECB(tempKey, ISGen, innerState);
	memcpy(output, tempOutput, 8);
}

void zw_s0_init() {
	LoadKeys();
	INIT_LIST_HEAD(&_list_adapter_node);
	INIT_LIST_HEAD(&_list_none_get);
	INIT_LIST_HEAD(&_list_senddata);
}

/**
 * 依据ID找到节点node响应的nonce report.
 */
int ZWUtil_SecureNonceFind(BYTE node, BYTE id, BYTE out[8]) {
	memcpy(out, NodeReport[node].iv[id], 8);
	return TRUE;
}

/**
 * 生产自己的
 */
void ZWUtil_SecureGenerateNonceReport8(BYTE node, BYTE ret[8]) {
	RPNGOutput(ret);
}

//网络秘钥
static BYTE NetworkKey[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 2, 3, 4, 5, 6, 7 };
static BYTE AuthKey[16]; //128bit
static BYTE EncKey[16]; //128bit
static pthread_mutex_t _lock = PTHREAD_MUTEX_INITIALIZER;

static void LoadKeys() {
	BYTE pattern[16];
	FILE *fp = NULL;
	pthread_mutex_lock(&_lock);
	fp = fopen("z-wave.key", "rb");
	if (fp) {
		if (tools_file_get_size("z-wave.key") > 0)
			fread(NetworkKey, 1, 16, fp);
		fclose(fp);
	}
	pthread_mutex_unlock(&_lock);
	log_d("----------------------------------:");
	memset(pattern, 0x55, 16);
	AES_ECB(NetworkKey, pattern, AuthKey); // K_A = AES(K_N,pattern)
	memset(pattern, 0xAA, 16);
	AES_ECB(NetworkKey, pattern, EncKey); // K_E = AES(K_N,pattern)
}

void ZW_SecureKeyClear() {
	FILE *fp = NULL;
	pthread_mutex_lock(&_lock);
	memset(NetworkKey, 0, sizeof(NetworkKey));
	fp = fopen("z-wave.key", "wb");
	if (fp) {
		fwrite(NetworkKey, 1, 16, fp);
		fclose(fp);
	}
	pthread_mutex_unlock(&_lock);
	LoadKeys();
}

int ZW_SecureKeyIsEmpty() {
	int i = 0;
	for (i = 0; i < sizeof(NetworkKey); i++) {
		if (NetworkKey[i] != 0)
			return FALSE;
	}
	return TRUE;
}

void ZW_SecureKeySet(BYTE key[16]) {
	FILE *fp = NULL;
	pthread_mutex_lock(&_lock);
	memcpy(NetworkKey, key, sizeof(NetworkKey));
	fp = fopen("z-wave.key", "wb");
	if (fp) {
		fwrite(NetworkKey, 1, 16, fp);
		fclose(fp);
	}
	pthread_mutex_unlock(&_lock);
	LoadKeys();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AES_ECB(BYTE key[16], BYTE input[16], BYTE output[16]) {
	AES_KEY aes;
	AES_set_encrypt_key(key, 128, &aes);
	AES_ecb_encrypt(input, output, &aes, AES_ENCRYPT);
}

//static void AES_ECB_Decrypt(BYTE key[16], BYTE input[16], BYTE output[16]) {
//	AES_KEY aes;
//	AES_set_decrypt_key(key, 128, &aes);
//	AES_ecb_encrypt(input, output, &aes, AES_DECRYPT);
//}

static void AES_OFB(BYTE key[16], BYTE IV[16], BYTE *data, int dataLen,
BYTE modifyIV) {
	AES_KEY aes;
	BYTE ive[16];
	int num = 0;
	memcpy(ive, IV, 16);
	AES_set_encrypt_key(key, 128, &aes);
	AES_ofb128_encrypt(data, data, dataLen, &aes, ive, &num);
}

static void AES_CBCMAC(BYTE key[16], BYTE header[20], BYTE *data, int datalen,
BYTE MAC[16]) {
	BYTE input16ByteChunk[16];
	BYTE input[255];
	int inputLength;
	int i = 0;
//input header size +datalen
//BYTE input = new byte[header.Length + data.Length];
//header.CopyTo(input, 0);
//data.CopyTo(input, header.Length);
	memcpy(input, header, 20);
	memcpy(input + 20, data, datalen);
	inputLength = datalen + 20;

// Perform initial hashing
	memset(input16ByteChunk, 0, sizeof(input16ByteChunk));

// Build initial input data, pad with 0 if lenght shorter than 16
	for (i = 0; i < 16; i++) {
		if (i >= inputLength) {
			input16ByteChunk[i] = 0;
		} else {
			input16ByteChunk[i] = input[i];
		}

	}
	AES_ECB(key, input16ByteChunk, MAC);
//input16ByteChunk = newbyte[16];
	memset(input16ByteChunk, 0, sizeof(input16ByteChunk));

// XOR tempMAC with any left over data and encrypt

	int cipherIndex;
	int blockIndex = 0;

	for (cipherIndex = 16; cipherIndex < inputLength; cipherIndex++) {
		input16ByteChunk[blockIndex] = input[cipherIndex];
		blockIndex++;
		if (blockIndex == 16) {
			for (i = 0; i <= 15; i++) {
				MAC[i] = (BYTE) (input16ByteChunk[i] ^ MAC[i]);
			}
			//input16ByteChunk = new byte[16];
			memset(input16ByteChunk, 0, sizeof(input16ByteChunk));
			blockIndex = 0;

			AES_ECB(key, MAC, MAC);
		}
	}

	if (blockIndex != 0) {
		for (i = 0; i < 16; i++) {
			MAC[i] = (BYTE) (input16ByteChunk[i] ^ MAC[i]);
		}
		AES_ECB(key, MAC, MAC);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * 解密
 */
BOOL ZWUtil_Decrypt(BYTE destNode, BYTE sourceNode, BYTE cmd, BYTE *msg,
		int msglen) {
	BYTE nonceReport[8]; //我发送出去的nonce数据
	BYTE nonceReportId = 0;
	BYTE IV[16];
	BYTE header[20];
	BYTE payload_length = 0;
	BYTE mac[16]; //只比较前8byte
	BYTE *payload = NULL;

	nonceReportId = msg[msglen - 8 - 1]; //
	payload = msg + 8;
	payload_length = msglen - 8 - 8 - 1; //IV,id,auth

	if (ZWUtil_SecureNonceFind(sourceNode, nonceReportId, nonceReport) == FALSE) { //无法找到发出去的
		return FALSE;
	}log_hex((BYTE*) nonceReport, 8, "解密-report:");

	memcpy(IV, msg, 8);
	memcpy(IV + 8, nonceReport, 8);

//check authTag
	memcpy(header, IV, 16);
	header[16] = cmd;
	header[17] = sourceNode;
	header[18] = destNode;
	header[19] = payload_length;

	AES_CBCMAC(AuthKey, header, payload, payload_length, mac); //用autkey加密

	log_hex((BYTE*) mac, 8, "解密-aut:");

	if (memcmp(mac, msg + msglen - 8, 8) != 0) {
		log_d("authcode cmp error....");
		return FALSE;
	}

	AES_OFB(EncKey, IV, payload, payload_length, FALSE); //用enckey加密
	return TRUE;
}

//iv0:发送者内部临时初始化向量
//externalNonce:NONCE_REPORT的初始化向量
/**
 * 加密
 */
void ZWUtil_Encrypt(const BYTE *input, BYTE inputLenght, BYTE *out,
BYTE *outLen,
BYTE srcNode, BYTE desNode, BYTE iv0[8], BYTE externalNonce[8],
BYTE properties,
BYTE encKey[16], BYTE authKey[16]) {
	BYTE encapId = SECURITY_MESSAGE_ENCAPSULATION;
	BYTE IV[16];
	BYTE payload[255];
	BYTE header[20];
	BYTE *ret = out;
	BYTE payload_Length = (inputLenght + 1);

	*ret = COMMAND_CLASS_SECURITY;
	ret++;
	if ((properties & 0x30) == 0x10) //Second Frame,第二帧
		encapId = SECURITY_MESSAGE_ENCAPSULATION_NONCE_GET;
	*ret = (encapId);
	ret++;
	memcpy(ret, iv0, 8);
	ret += 8;

	payload[0] = properties;
	memcpy(&payload[1], input, inputLenght);

	memcpy(IV, iv0, 8);
	memcpy(IV + 8, externalNonce, 8);

	AES_OFB(encKey, IV, payload, payload_Length, FALSE); //用enckey加密

	memcpy(ret, payload, payload_Length);
	ret += payload_Length;
	*ret = externalNonce[0];
	ret++;

	memcpy(header, IV, 16);
	header[16] = encapId;
	header[17] = srcNode;
	header[18] = desNode;
	header[19] = inputLenght + 1;

	BYTE mac[16];
	memset(mac, 0, 16);
	AES_CBCMAC(authKey, header, payload, payload_Length, mac); //用autkey加密
	memcpy(ret, mac, 8);
	ret += 8;
	*outLen = ret - out;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//加载临时keys
void LoadTemporaryKeys(BYTE authKey[16], BYTE encKey[16]) {
	BYTE pattern[16];
	BYTE networkKey[16];
	memset(networkKey, 0, 16);
	log_d("LoadTemporaryKeys:");
	memset(pattern, 0x55, 16);
	AES_ECB(networkKey, pattern, authKey); // K_A = AES(K_N,pattern)
	memset(pattern, 0xAA, 16);
	AES_ECB(networkKey, pattern, encKey); // K_E = AES(K_N,pattern)
}

//==============================================================================================
//发送消息
//-----------------------------------------------------------
// NONCE_GET  -------->>
//                          <<--------NONCE REPORT
//EncMsg           ------>>
//-----------------------------------------------------------
// NONCE_GET  -------->>
//                          <<--------NONCE REPORT
//EncMsg_NonceGet ------>>
//                          <<-------- Nonce_REPORT
//EncMsg_NonceGet ------>>
//-----------------------------------------------------------
//KEY set
//SCHEME_GET ---->
//                           <----Scheme Report
//Nonce_Get ------->
//                           <---- Nonce Report
//EncMsg(keySet) ----->
//                          <---- Nonce Get
//Nonce Report ----->
//                         <-----(KeyVerify)EncMsg
//-----------------------------------------------------------
void zw_s0_send_none_report(BYTE node) {
	ZW_SECURITY_NONCE_REPORT_FRAME frame;
	frame.cmdClass = COMMAND_CLASS_SECURITY;
	frame.cmd = SECURITY_NONCE_REPORT;
	RPNGOutput(((BYTE*) &frame) + 2);
	memcpy(NodeReport[node].iv[((BYTE*) &frame)[2]], ((BYTE*) &frame) + 2, 8);

	zw_tx_param_t nodep;
	nodep.node = node;
	nodep.scheme = SCHEME_NO;
	zw_senddata_first(&nodep, (uint8_t*) &frame, 10, NULL, NULL);
}

extern struct zwave_controller_info ZWaveControllerInfo;

void handler_MESSAGE_ENCAPSULATION(BYTE rxStatus, BYTE destNode,
BYTE sourceNode, BYTE cmd,
BYTE *msg, BYTE msgLen) {
	BYTE *tmp = NULL;
	BYTE decLen = 0;
	BYTE sequenced = 0;
	BYTE secondFrame = 0;
	BYTE reserved = 0;
	BYTE sequenceCounter = 0;

	log_d(">>>解密数据开始,%d, s:%d -> d:%d", msgLen, sourceNode,
			ZWaveControllerInfo.this_NodeID);

	if (TRUE
			== ZWUtil_Decrypt(ZWaveControllerInfo.this_NodeID, sourceNode, cmd,
					msg, msgLen)) {
		tmp = ((BYTE*) msg) + 8; //得到解密的数据部分
		//长度为:cmdLength-2-8-8-1;
		decLen = msgLen - 8 - 8 - 1;
		if (decLen > 2) {
			sequenced = tmp[0] >> 4 & 0x01; //测序,多帧标志
			secondFrame = tmp[0] >> 5 & 0x01; //
			reserved = tmp[0] >> 6 & 0x03; //保留
			sequenceCounter = tmp[0] & 0x0F;
			if (sequenced == 0) { //
				tmp++;
				log_hex(tmp, decLen - 1, "解密数据:");
				///
				ApplicationCommandHandler(rxStatus, destNode, sourceNode,
						(ZW_APPLICATION_TX_BUFFER*) tmp, decLen - 1);
			}
		}
	}
}

void zw_s0_nonce_report(BYTE node, uint8_t *externalNonce);

void zw_s0_layer_CommandHandler(BYTE rxStatus, BYTE destNode,
BYTE sourceNode, ZW_APPLICATION_TX_BUFFER *pCmd, BYTE cmdLength) {
	log_hex((BYTE*) pCmd, cmdLength, "安全数据包:");

	switch (pCmd->ZW_Common.cmd) {
	case SECURITY_SCHEME_INHERIT:
		break;
	case SECURITY_SCHEME_REPORT: //0x05
		if (pCmd->ZW_SecuritySchemeReportFrame.supportedSecuritySchemes == 0
				|| pCmd->ZW_SecuritySchemeReportFrame.supportedSecuritySchemes
						== 1) {
			scheme_report(sourceNode, 1);
		} else {
			scheme_report(sourceNode, 0);
		}
		break;
	case SECURITY_NONCE_GET:
		zw_s0_send_none_report(sourceNode);
		break;
	case SECURITY_NONCE_REPORT: //0x80
		zw_s0_nonce_report(sourceNode,
				(uint8_t*) (&pCmd->ZW_SecurityNonceReportFrame.nonceByte1));
		break;
	case SECURITY_MESSAGE_ENCAPSULATION:
	case SECURITY_MESSAGE_ENCAPSULATION_NONCE_GET: {
		handler_MESSAGE_ENCAPSULATION(rxStatus, destNode, sourceNode,
				pCmd->ZW_Common.cmd, ((BYTE*) pCmd) + 2, cmdLength - 2);

		if (pCmd->ZW_Common.cmd == SECURITY_MESSAGE_ENCAPSULATION_NONCE_GET) {
			zw_s0_send_none_report(sourceNode);
		}
	}
		break;

	}
}
///
typedef enum {
	ADAPTER_START,
	ADAPTER_SCHEME_GET,
	ADAPTER_SCHEME_REPORT_WAIT,
	ADAPTER_SCHEME_REPORT,
	ADAPTER_KEY_SET,
	ADAPTER_NONCE_GET,
	ADAPTER_NONCE_REPORT_WAIT,
	ADAPTER_NONCE_REPORT
} adapter_stat_t;

typedef struct adapter_node {
	struct list_head list;
	BYTE node;
	int isSlaveApi;
	struct ctimer tim;
	void (*callback)(int status);
	adapter_stat_t stat;
} adapter_node_t;

adapter_node_t *list_adapter_node_first(uint8_t node);

typedef struct s0_session {
	struct list_head list;
	BYTE node;
	unsigned char *data;
	BYTE dlen;
	struct ctimer tim;
	uint8_t cryptmsg[64];
	zw_senddata_callback_t callback;
	adapter_stat_t stat;
	void *context;
} s0_session_t;

void zw_s0_nonce_report_send_callback(int status, void *context) {
	s0_session_t *item = (s0_session_t*) context;
	item->callback(status, item->context);
	if (item->data)
		mem_free(item->data);
	mem_free(item);
}

void zw_s0_nonce_report(BYTE node, uint8_t *externalNonce) {
	struct list_head *p, *n;
	s0_session_t *item = NULL;
	BYTE internalNonce[8];
	BYTE properties = 0;
	BYTE _authKey[16];
	BYTE _encKey[16];
	zw_tx_param_t nodep;

	list_for_each_safe(p,n, &_list_none_get)
	{
		item = list_entry(p, s0_session_t, list);
		if (item->node == node) {
			//encode send
			list_del(p); //应该只有一次才成
			ctimer_stop(&item->tim);
			item->stat = ADAPTER_NONCE_REPORT;
			ZWUtil_SecureGenerateNonceReport8(node, internalNonce); //我的Initialization Vector

			if (list_adapter_node_first(node) != NULL) {
				LoadTemporaryKeys(_authKey, _encKey); //

				ZWUtil_Encrypt(item->data, item->dlen, item->cryptmsg,
						&item->dlen, ZWaveControllerInfo.this_NodeID, node,
						internalNonce, externalNonce, properties, _encKey,
						_authKey);
			} else {
				ZWUtil_Encrypt(item->data, item->dlen, item->cryptmsg,
						&item->dlen, ZWaveControllerInfo.this_NodeID, node,
						internalNonce, externalNonce, properties, EncKey,
						AuthKey);
			}
			nodep.node = node;
			nodep.scheme = SCHEME_NO;
			zw_senddata_first(&nodep, item->cryptmsg, item->dlen,
					zw_s0_nonce_report_send_callback, item);
		}
	}
}

void _nonce_get_timeout(void *context) {
	s0_session_t *item = (s0_session_t*) context;
	list_del(&item->list);
	log_d("nonce time out...");
	item->callback(ZW_SENDDATA_STATUS_TIMEOUT, item->context);
	if (item->data)
		mem_free(item->data);
	mem_free(item);
}

static void nonce_get_callback(int status, void *context) {
	s0_session_t *s0s = (s0_session_t*) context;

	if (status == TRANSMIT_COMPLETE_OK) {
		if (s0s->stat == ADAPTER_NONCE_GET) {
			s0s->stat = ADAPTER_NONCE_REPORT_WAIT;
			list_add_tail(&s0s->list, &_list_none_get);
			ctimer_set(&s0s->tim, 3000, _nonce_get_timeout, s0s);
			return;
		}
		log_d("status err != ADAPTER_NONCE_GET(%x)", s0s);
		status = ZW_SENDDATA_STATUS_ERROR;
	}
	s0s->callback(status, s0s->context);
	if (s0s->data)
		mem_free(s0s->data);
	mem_free(s0s);
}

typedef struct z_s0_senddata {
	struct list_head list;
	uint8_t node;
	uint8_t *data;
	uint8_t dlen;
	int status;
	zw_senddata_callback_t callback;
	void *context;
} z_s0_senddata_t;

static void _zw_s0_senddata_back(int status, void *context) {
	z_s0_senddata_t *item = (z_s0_senddata_t *) context;
	list_del(&item->list);  //remove in  _list_senddata
	if (item->callback)
		item->callback(status, item->context);
	free(item);
}

static void _zw_s0_senddata(z_s0_senddata_t *item) {
	static const uint8_t nonce_get[] = { COMMAND_CLASS_SECURITY,
	SECURITY_NONCE_GET };  //98 40

	s0_session_t *s0s = (s0_session_t*) mem_malloc(sizeof(s0_session_t));

	memset(s0s, 0, sizeof(s0_session_t));
	s0s->node = item->node;
	s0s->data = item->data;
	s0s->dlen = item->dlen;
	s0s->callback = _zw_s0_senddata_back;
	s0s->context = item;
	s0s->stat = ADAPTER_NONCE_GET;

	zw_tx_param_t nodeparam;
	nodeparam.node = item->node;
	nodeparam.scheme = SCHEME_NO;
	zw_senddata_first(&nodeparam, nonce_get, 2, nonce_get_callback, s0s);
	item->data = NULL;
}

/******************************************************
 *
 ******************************************************/
void zw_s0_layer_handle() {
	struct list_head *p, *n;
	z_s0_senddata_t *item = NULL;

	list_for_each_safe(p,n, &_list_senddata)
	{
		item = list_entry(p, z_s0_senddata_t, list);
		break;
	}
	if (!item)
		return;
	if (item->status != 0)
		return;
	item->status++;
	_zw_s0_senddata(item);
}

/**
 * 必须一个一个节点获取才行
 */
int zw_s0_layer_senddata(BYTE node, void *data, BYTE dlen,
		zw_senddata_callback_t callback, void *context) {

	z_s0_senddata_t *item = (z_s0_senddata_t*) malloc(sizeof(z_s0_senddata_t));
	memset(item, 0, sizeof(z_s0_senddata_t));
	item->node = node;
	item->data = data;
	item->dlen = dlen;
	item->callback = callback;
	item->context = context;

	list_add_tail(&item->list, &_list_senddata);
	return 0;
}
/////////////////////////////////////////////////

adapter_node_t *list_adapter_node_first(uint8_t node) {
	struct list_head *p, *n;
	adapter_node_t *item = NULL;
	list_for_each_safe(p,n, &_list_adapter_node)
	{
		item = list_entry(p, adapter_node_t, list);
		if (item->node == node)
			return item;
	}
	return NULL;
}

void _scheme_inherit_callback(int status, void *context) {
	adapter_node_t *item = (adapter_node_t *) context;
	item->callback(status);
	mem_free(item);
}

void _s0_supported_callback(int status, uint8_t node, uint8_t *data,
		uint8_t len, void *context) {
	adapter_node_t *item = (adapter_node_t *) context;
	log_hex(data, len, "%d 安全支持(len=%d):", status, len);
	if (status == ZW_SENDDATA_STATUS_OK) {
		zw_node_managent_setSuperS0(node, data + 3, len - 3);
	}
	item->callback(status);
	mem_free(item);
}

void zw_s0_keyset_callback(int status, uint8_t node, uint8_t *data, uint8_t len,
		void *context) {
	adapter_node_t *item = (adapter_node_t *) context;
	zw_tx_param_t nodep;

	uint8_t scheme_inherit[] = { COMMAND_CLASS_SECURITY,
	SECURITY_SCHEME_INHERIT, 0 };
	uint8_t sec_super_get[] = { COMMAND_CLASS_SECURITY,
	SECURITY_COMMANDS_SUPPORTED_GET }; //wait SECURITY_COMMANDS_SUPPORTED_REPORT

	log_d("key set send success:%d", status);
	list_del(&item->list);

	if (status == TRANSMIT_COMPLETE_OK) {
		zw_node_managnet_setS0(node, ZW_NODE_SUPER_S0);
		nodep.node = node;
		nodep.scheme = SCHEME_SECURITY_0;

		if (!item->isSlaveApi) {
			log_d("scheme_inherit");
			zw_senddata_first(&nodep, scheme_inherit, sizeof(scheme_inherit),
					_scheme_inherit_callback, item);
		} else {
			log_d("获取安全支持");
			zw_senddata_request(&nodep, sec_super_get, 2,
			COMMAND_CLASS_SECURITY,
			SECURITY_COMMANDS_SUPPORTED_REPORT, 3 * 1000,
					_s0_supported_callback, item);
		}
	} else {
		item->callback(status);
		mem_free(item);
	}
}

void zw_s0_keyset(BYTE node, adapter_node_t *item) {
	BYTE authKey[16];
	BYTE encKey[16];
	BYTE buff[2 + 16];
	zw_tx_param_t nodep;
	LoadTemporaryKeys(authKey, encKey); //

	buff[0] = COMMAND_CLASS_SECURITY;
	buff[1] = NETWORK_KEY_SET; //06
	memcpy(&buff[2], NetworkKey, 16);

	nodep.node = node;
	nodep.scheme = SCHEME_SECURITY_0;

	item->stat = ADAPTER_KEY_SET;
	log_d("安全适配 key set");
	zw_senddata_request(&nodep, buff, 2 + 16, COMMAND_CLASS_SECURITY,
	NETWORK_KEY_VERIFY, 3 * 1000, zw_s0_keyset_callback, item);
}

////////////////////////////////////////////////////////////
void scheme_report(BYTE node, int status) {
	struct list_head *p, *n;
	adapter_node_t *item = NULL;
	list_for_each_safe(p,n, &_list_adapter_node)
	{
		item = list_entry(p, adapter_node_t, list);
		if (item->node == node) {
			if (item->stat == ADAPTER_SCHEME_REPORT_WAIT) { //有状态判断
				ctimer_stop(&item->tim);
				item->stat = ADAPTER_SCHEME_REPORT;
				zw_s0_keyset(node, item);
			}
		}
	}
}

static void scheme_get_adapter_timeout(void *context) {
	adapter_node_t *adapter = (adapter_node_t *) context;
	list_del(&adapter->list);
	log_d("安全适配 scheme report timeout");
	adapter->callback(ZW_SENDDATA_STATUS_TIMEOUT);
	mem_free(adapter);
}

static void s0_send_scheme_get_callback(int status, void*context) {
	adapter_node_t *adapter = (adapter_node_t *) context;

	if (status == TRANSMIT_COMPLETE_OK) {
		log_d("s0_scheme_get wait");
		adapter->stat = ADAPTER_SCHEME_REPORT_WAIT;
		list_add_tail(&adapter->list, &_list_adapter_node);
		ctimer_set(&adapter->tim, 3000, scheme_get_adapter_timeout, adapter);
	} else {
		log_d("安全适配 err s0_scheme_ge");
		adapter->callback(status);
		mem_free(adapter);
	}
}

void s0_send_scheme_get(BYTE node, adapter_node_t *context) {
	ZW_SECURITY_SCHEME_GET_FRAME scheme_get_frame;
	scheme_get_frame.cmdClass = COMMAND_CLASS_SECURITY;
	scheme_get_frame.cmd = SECURITY_SCHEME_GET;
	scheme_get_frame.supportedSecuritySchemes = 0;
	context->stat = ADAPTER_SCHEME_GET;

	zw_tx_param_t nodeparam;
	nodeparam.node = node;
	nodeparam.scheme = SCHEME_NO;

	zw_senddata_first(&nodeparam, (uint8_t*) &scheme_get_frame,
			sizeof(scheme_get_frame), s0_send_scheme_get_callback, context);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *添加到安全网络开始
 */
void zw_s0_add_start(BYTE node, int isSlaveApi, void (*callback)(int status)) {
	log_d("安全适配开始..%d", node);
	adapter_node_t *adapter = (adapter_node_t*) mem_malloc(
			sizeof(adapter_node_t));
	memset(adapter, 0, sizeof(adapter_node_t));
	adapter->callback = callback;
	adapter->node = node;
	adapter->isSlaveApi = isSlaveApi;
	adapter->stat = ADAPTER_START;
	s0_send_scheme_get(node, adapter);
}

