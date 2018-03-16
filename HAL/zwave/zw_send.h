/*
 * zw_send.h
 *
 *  Created on: 2017年12月15日
 *      Author: cj
 */

#ifndef HAL_ZWAVE_ZW_SEND_H_
#define HAL_ZWAVE_ZW_SEND_H_

#include <stddef.h>
#include <stdint.h>

typedef enum SECURITY_SCHEME {
	SCHEME_NO = 0xFF, SCHEME_AUTO = 0xFE, SCHEME_NET = 0xFC, //Highest network shceme
	SCHEME_USE_CRC16 = 0xFD, //Force use of CRC16, not really a security scheme..
	SCHEME_SECURITY_0 = 7,
	SCHEME_SECURITY_2_UNAUTHENTICATED = 1,
	SCHEME_SECURITY_2_AUTHENTICATED = 2,
	SCHEME_SECURITY_2_ACCESS = 3,
	SCHEME_SECURITY_UDP = 4,
} security_scheme_t;

typedef struct zw_tx_param {
	unsigned char node;
	security_scheme_t scheme;
} zw_tx_param_t;

#define ZW_SENDDATA_STATUS_OK 				0
#define ZW_SENDDATA_STATUS_NO_ACK		1
#define ZW_SENDDATA_STATUS_FAIL 				2
#define ZW_SENDDATA_STATUS_ERROR			0xFF
#define ZW_SENDDATA_STATUS_TIMEOUT   -1

typedef void (*zw_senddata_callback_t)(int status, void *context);
typedef void (*zw_senddata_request_callback_t)(int status, uint8_t node,
		uint8_t *data, uint8_t len, void *context);

void zw_send_init();

void zw_tx_param_init(zw_tx_param_t *param, uint8_t node, int scheme);

extern void zw_senddata_first(zw_tx_param_t *node, const uint8_t *data,
		uint8_t dlen, zw_senddata_callback_t callback, void *context);

extern void zw_senddata_request(zw_tx_param_t *node, void *data, int dlen,
		uint8_t _cls, uint8_t _cmd, int timeout,
		zw_senddata_request_callback_t callback, void *context);

int zw_request_handle(uint8_t node, void *data, int dlen);

void zw_senddata_first_handle();
void zw_senddata_end_handle();

#endif /* HAL_ZWAVE_ZW_SEND_H_ */
