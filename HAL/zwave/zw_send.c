/*
 * zw_send.c
 *
 *  Created on: 2017年12月15日
 *      Author: cj
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "zw_send.h"
#include "zwave_app.h"
#include "zw_node_managent.h"
#include "config.h"
#include "Serialapi.h"

#include "mem.h"

#define free	mem_free
#define malloc mem_malloc

static struct list_queue _queue_senddata_first;
static struct list_queue _queue_senddata_end;
static struct list_queue _queue_sendrequest;

volatile int _flag_senddata_end = 0;

void zw_tx_param_init(zw_tx_param_t *param, uint8_t node, int scheme) {
	param->node = node;
	param->scheme = scheme == 0 ? SCHEME_AUTO : scheme;
}

void zw_send_init() {
	list_queue_init(&_queue_senddata_first);
	list_queue_init(&_queue_senddata_end);
	list_queue_init(&_queue_sendrequest);
}

typedef struct z_senddatarequest {
	struct list_head list;
	uint8_t node;
	int timeout;
	struct ctimer tim;
	int wcls;
	int wcmd;
	zw_senddata_request_callback_t callback;
	void *context;
} z_senddatarequest_t;

struct z_senddata_t {
	struct list_head list;
	zw_tx_param_t node;
	uint8_t *data;
	uint8_t dlen;
	zw_senddata_callback_t callback;
	void *context;
};

typedef struct z_senddata_t z_senddata_first_t;
typedef struct z_senddata_t z_senddata_end_t;

static struct z_senddata_t * _queue_pop(struct list_queue *queue) {
	struct list_head *p = NULL;
	struct z_senddata_t *item = NULL;
	p = list_queue_pop(queue);
	if (!p)
		return NULL;
	item = list_entry(p, struct z_senddata_t, list);
	return item;
}

struct z_senddata_t *z_senddata_t_new() {
	struct z_senddata_t *item = (struct z_senddata_t*) malloc(
			sizeof(struct z_senddata_t));
	if (item)
		memset(item, 0, sizeof(struct z_senddata_t));
	return item;
}

void z_senddata_t_init(struct z_senddata_t *item, zw_tx_param_t * node,
		const void *data, int dlen, zw_senddata_callback_t callback,
		void *context) {
	memcpy(&item->node, node, sizeof(zw_tx_param_t));
	item->data = (uint8_t *) malloc(dlen);
	memcpy(item->data, data, dlen);
	item->dlen = dlen;
	item->callback = callback;
	item->context = context;
}

int zw_request_handle(uint8_t node, void *data, int dlen) {
	struct list_head *p = NULL, *n;
	z_senddatarequest_t *req = NULL;
	uint8_t *zdata = (uint8_t*) data;

	list_queue_lock(&_queue_sendrequest);
	list_for_each_safe(p,n,&_queue_sendrequest.list)
	{
		req = list_entry(p, z_senddatarequest_t, list);
		if (req->node == node && req->wcls == zdata[0]
				&& req->wcmd == zdata[1]) {
			list_del(p);
			break;
		}
		req = NULL;
	}
	list_queue_unlock(&_queue_sendrequest);

	if (!req)
		return 0;
	//log_d("response->%d:  %02X-%02X", node, zdata[0], zdata[1]);
	ctimer_stop(&req->tim);
	req->callback(0, node, zdata, dlen, req->context);
	free(req);
	return 1;
}

void zw_senddata_request_timeout(void *context) {
	z_senddatarequest_t *req = (z_senddatarequest_t*) context;
	//log_d("response timeout: %d-  (%02X-%02X)",  req->node, req->wcls, req->wcmd);
	list_queue_list_delete(&_queue_sendrequest, &req->list);
	req->callback(ZW_SENDDATA_STATUS_TIMEOUT, req->node, NULL, 0, req->context);
	free(req);
}

//等待响应开始
void zw_senddata_request_callback(int status, void *context) {
	z_senddatarequest_t *req = (z_senddatarequest_t*) context;
	//log_d("status=%d requestWait: %d-  (%02X-%02X)", status, req->node, req->wcls, req->wcmd);
	if (status == TRANSMIT_COMPLETE_OK) {
		list_queue_append_tail2(&_queue_sendrequest, &req->list, 0);
		ctimer_set(&req->tim, req->timeout, zw_senddata_request_timeout, req);
	} else {
		req->callback(status, req->node, NULL, 0, req->context);
		free(req);
	}
}

void zw_senddata_request(zw_tx_param_t *node, void *data, int dlen,
		uint8_t _cls, uint8_t _cmd, int timeout,
		zw_senddata_request_callback_t callback, void *context) {
	z_senddatarequest_t *req = (z_senddatarequest_t*) malloc(
			sizeof(z_senddatarequest_t));
	req->node = node->node;
	req->wcls = _cls;
	req->wcmd = _cmd;
	req->timeout = timeout;
	req->callback = callback;
	req->context = context;
	zw_senddata_first(node, data, dlen, zw_senddata_request_callback, req);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void zw_senddata_first(zw_tx_param_t *node, const uint8_t *data, uint8_t dlen,
		zw_senddata_callback_t callback, void *context) {
	z_senddata_first_t *item = z_senddata_t_new();
	z_senddata_t_init(item, node, data, dlen, callback, context);

	list_queue_append_tail2(&_queue_senddata_first, &item->list, 0);
}

void zw_senddata_end_callback(int status, void *context) {
	z_senddata_first_t *item = (z_senddata_first_t *) context;
	if (item->callback)
		item->callback(status, item->context);
	if (item->data)
		free(item->data);
	free(item);
}

extern void zw_senddata_end(zw_tx_param_t *node, uint8_t *data,
			uint8_t dlen, zw_senddata_callback_t callback, void *context);

void zw_senddata_first_handle() {
	z_senddata_first_t *item = _queue_pop(&_queue_senddata_first);
	if (!item)
		return;

	int scheme = item->node.scheme;
	if (item->node.scheme == SCHEME_AUTO) {
		scheme = zw_node_managent_getScheme(item->node.node, item->data[0]);
	}
	switch (scheme) {
	case SCHEME_AUTO:
		break;
	case SCHEME_NO:
		zw_senddata_end(&item->node, item->data, item->dlen,
				zw_senddata_end_callback, item);
		item->data = NULL;
		break;
	case SCHEME_NET: //Highest network shceme
	case SCHEME_USE_CRC16: //Force use of CRC16, not really a security scheme..
		break;
	case SCHEME_SECURITY_0:
		zw_s0_layer_senddata(item->node.node, item->data, item->dlen,
				zw_senddata_end_callback, item);
		break;
	case SCHEME_SECURITY_2_UNAUTHENTICATED:
	case SCHEME_SECURITY_2_AUTHENTICATED:
	case SCHEME_SECURITY_2_ACCESS:
	case SCHEME_SECURITY_UDP:
	default:
		break;
	}
	item->data = NULL;
}
/////////////////////////////////////////////////////////////////////////////////////
void zw_senddata_end(zw_tx_param_t *node, uint8_t *data, uint8_t dlen,
		zw_senddata_callback_t callback, void *context) {
	z_senddata_end_t *item = z_senddata_t_new();
	memcpy(&item->node, node, sizeof(zw_tx_param_t));
	item->data = data;
	item->dlen = dlen;
	item->callback = callback;
	item->context = context;
	list_queue_append_tail2(&_queue_senddata_end, &item->list, 0);
}

void ZW_SendData_callback(BYTE status, TX_STATUS_TYPE* type) {
	z_senddata_end_t *item = _queue_pop(&_queue_senddata_end);
	_flag_senddata_end = 0; //可以发送下次功能

	if (!item)
		return;

	if (item->data) {
		free(item->data);
	}
	item->callback(status, item->context); //callback必须存在
	if (status != TRANSMIT_COMPLETE_OK) {
		zw_node_managent_update_failing(item->node.node, TRUE);
	}
	free(item);
}

void zw_senddata_end_handle() {
	struct list_head *p = NULL;
	z_senddata_end_t *item;
	BYTE ret = 0;

	if (!_flag_senddata_end) { //物理传输
		p = list_queue_first(&_queue_senddata_end);
		if (p) {
			item = list_entry(p, struct z_senddata_t, list);
			ret = ZW_SendData(item->node.node, item->data, item->dlen, 0x25,
					ZW_SendData_callback);
			_flag_senddata_end = 1;
			if (!ret) {
				ZW_SendData_callback(TRANSMIT_COMPLETE_FAIL, NULL);
			}
		}
	}
}
