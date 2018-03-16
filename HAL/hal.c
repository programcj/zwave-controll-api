/*
 * hal.c
 *
 *  Created on: 2017年6月15日
 *      Author: cj
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "hal.h"
#include "zwave/zwave_api.h"

struct id_name {
	int id;
	char *name;
};

#define ID_TO_NAME(_id)		{ .id=_id, .name=#_id }

static struct id_name zwave_res_base[] = {
ID_TO_NAME(BASIC_TYPE_CONTROLLER),
ID_TO_NAME(BASIC_TYPE_ROUTING_SLAVE),
ID_TO_NAME(BASIC_TYPE_SLAVE),
ID_TO_NAME(BASIC_TYPE_STATIC_CONTROLLER), { .id = 0, .name = NULL } };

static struct id_name zwave_res_generic[] = {
ID_TO_NAME(GENERIC_TYPE_AV_CONTROL_POINT),
ID_TO_NAME(GENERIC_TYPE_DISPLAY),
ID_TO_NAME(GENERIC_TYPE_ENTRY_CONTROL),
ID_TO_NAME(GENERIC_TYPE_GENERIC_CONTROLLER),
ID_TO_NAME(GENERIC_TYPE_METER),
ID_TO_NAME(GENERIC_TYPE_METER_PULSE),
ID_TO_NAME(GENERIC_TYPE_NON_INTEROPERABLE),
ID_TO_NAME(GENERIC_TYPE_REPEATER_SLAVE),
ID_TO_NAME(GENERIC_TYPE_SECURITY_PANEL),
ID_TO_NAME(GENERIC_TYPE_SEMI_INTEROPERABLE),
ID_TO_NAME(GENERIC_TYPE_SENSOR_ALARM),
ID_TO_NAME(GENERIC_TYPE_SENSOR_BINARY),
ID_TO_NAME(GENERIC_TYPE_SENSOR_MULTILEVEL),
ID_TO_NAME(GENERIC_TYPE_STATIC_CONTROLLER),
ID_TO_NAME(GENERIC_TYPE_SWITCH_BINARY),
ID_TO_NAME(GENERIC_TYPE_SWITCH_MULTILEVEL),
ID_TO_NAME(GENERIC_TYPE_SWITCH_REMOTE),
ID_TO_NAME(GENERIC_TYPE_SWITCH_TOGGLE),
ID_TO_NAME(GENERIC_TYPE_THERMOSTAT),
ID_TO_NAME(GENERIC_TYPE_VENTILATION),
ID_TO_NAME(GENERIC_TYPE_WINDOW_COVERING),
ID_TO_NAME(GENERIC_TYPE_ZIP_NODE),
ID_TO_NAME(GENERIC_TYPE_WALL_CONTROLLER),
ID_TO_NAME(GENERIC_TYPE_NETWORK_EXTENDER),
ID_TO_NAME(GENERIC_TYPE_APPLIANCE),
ID_TO_NAME(GENERIC_TYPE_SENSOR_NOTIFICATION), { .id = 0, .name = NULL } };

static struct id_id_name {
	int generic;
	int specific;
	char *name;
} zwave_res_specific[] =
		{
				{ GENERIC_TYPE_SENSOR_ALARM, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_SENSOR_ALARM,
				SPECIFIC_TYPE_ADV_ZENSOR_NET_ALARM_SENSOR, "Alarm Sensor" }, ////////////////////////
				{ GENERIC_TYPE_SENSOR_ALARM,
				SPECIFIC_TYPE_ADV_ZENSOR_NET_SMOKE_SENSOR, "Smoke Sensor" }, ////////////////////////
				{ GENERIC_TYPE_SENSOR_ALARM,
				SPECIFIC_TYPE_BASIC_ROUTING_ALARM_SENSOR, "Alarm Sensor" }, ////////////////////////
				{ GENERIC_TYPE_SENSOR_ALARM,
				SPECIFIC_TYPE_BASIC_ROUTING_SMOKE_SENSOR, "Smoke Sensor" }, ////////////////////////
				{ GENERIC_TYPE_SENSOR_ALARM,
				SPECIFIC_TYPE_BASIC_ZENSOR_NET_ALARM_SENSOR, "Alarm Sensor" }, ////////////////////////
				{ GENERIC_TYPE_SENSOR_ALARM,
				SPECIFIC_TYPE_BASIC_ZENSOR_NET_SMOKE_SENSOR, "Smoke Sensor" }, ////////////////////////
				{ GENERIC_TYPE_SENSOR_ALARM, SPECIFIC_TYPE_ROUTING_ALARM_SENSOR,
						"Alarm Sensor" }, ////////////////////////
				{ GENERIC_TYPE_SENSOR_ALARM, SPECIFIC_TYPE_ROUTING_SMOKE_SENSOR,
						"Smoke Sensor" }, ////////////////////////
				{ GENERIC_TYPE_SENSOR_ALARM,
				SPECIFIC_TYPE_ZENSOR_NET_ALARM_SENSOR, "Alarm Sensor" }, ////////////////////////
				{ GENERIC_TYPE_SENSOR_ALARM,
				SPECIFIC_TYPE_ZENSOR_NET_SMOKE_SENSOR, "Smoke Sensor" }, ////////////////////////
				{ GENERIC_TYPE_SENSOR_ALARM, SPECIFIC_TYPE_ALARM_SENSOR,
						"Alarm Sensor" }, ////////////////////////
				{ GENERIC_TYPE_SENSOR_MULTILEVEL, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_SENSOR_MULTILEVEL,
				SPECIFIC_TYPE_ROUTING_SENSOR_MULTILEVEL, "Multilevel Sensor" }, ////////////////////////
				{ GENERIC_TYPE_SENSOR_MULTILEVEL, SPECIFIC_TYPE_CHIMNEY_FAN,
						"Chimney Fan" }, ////////////////////////
				{ GENERIC_TYPE_AV_CONTROL_POINT, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_AV_CONTROL_POINT, SPECIFIC_TYPE_DOORBELL,
						"Doorbell" }, ////////////////////////
				{ GENERIC_TYPE_AV_CONTROL_POINT,
				SPECIFIC_TYPE_SATELLITE_RECEIVER, "Satellite Receiver" }, ////////////////////////
				{ GENERIC_TYPE_AV_CONTROL_POINT,
				SPECIFIC_TYPE_SATELLITE_RECEIVER_V2, "Satellite Receiver V2" }, ////////////////////////
				{ GENERIC_TYPE_DISPLAY, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_DISPLAY, SPECIFIC_TYPE_SIMPLE_DISPLAY,
						"Simple Display" }, ////////////////////////
				{ GENERIC_TYPE_GENERIC_CONTROLLER, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_GENERIC_CONTROLLER,
				SPECIFIC_TYPE_PORTABLE_REMOTE_CONTROLLER, " Remote Controller" }, ////////////////////////
				{ GENERIC_TYPE_GENERIC_CONTROLLER,
				SPECIFIC_TYPE_PORTABLE_SCENE_CONTROLLER, " Scene Controller" }, ////////////////////////
				{ GENERIC_TYPE_GENERIC_CONTROLLER,
				SPECIFIC_TYPE_PORTABLE_INSTALLER_TOOL, "Installer Tool" }, ////////////////////////
				{ GENERIC_TYPE_GENERIC_CONTROLLER,
				SPECIFIC_TYPE_REMOTE_CONTROL_AV, "Remote AV Control" }, ////////////////////////
				{ GENERIC_TYPE_GENERIC_CONTROLLER,
				SPECIFIC_TYPE_REMOTE_CONTROL_SIMPLE, "Remote Control" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_REMOTE, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_REMOTE,
				SPECIFIC_TYPE_SWITCH_REMOTE_BINARY, "Remote Switch" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_REMOTE,
				SPECIFIC_TYPE_SWITCH_REMOTE_MULTILEVEL,
						"Remote Multilevel Switch" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_REMOTE,
				SPECIFIC_TYPE_SWITCH_REMOTE_TOGGLE_BINARY,
						"Remote Toggle Switch" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_REMOTE,
				SPECIFIC_TYPE_SWITCH_REMOTE_TOGGLE_MULTILEVEL,
						"Remote Multilevel Toggle Switch" }, ////////////////////////
				{ GENERIC_TYPE_NETWORK_EXTENDER, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_NETWORK_EXTENDER, SPECIFIC_TYPE_SECURE_EXTENDER,
						"Secure Extender" }, ////////////////////////
				{ GENERIC_TYPE_METER_PULSE, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_VENTILATION, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_VENTILATION, SPECIFIC_TYPE_RESIDENTIAL_HRV,
						"Residential Hrv" }, ////////////////////////
				{ GENERIC_TYPE_APPLIANCE, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_APPLIANCE, SPECIFIC_TYPE_GENERAL_APPLIANCE,
						"Appliance" }, ////////////////////////
				{ GENERIC_TYPE_APPLIANCE, SPECIFIC_TYPE_KITCHEN_APPLIANCE,
						"Kitchen Appliance" }, ////////////////////////
				{ GENERIC_TYPE_APPLIANCE, SPECIFIC_TYPE_LAUNDRY_APPLIANCE,
						"Laundry Appliance" }, ////////////////////////
				{ GENERIC_TYPE_METER, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_METER, SPECIFIC_TYPE_SIMPLE_METER, "Simple Meter" }, ////////////////////////
				{ GENERIC_TYPE_METER, SPECIFIC_TYPE_ADV_ENERGY_CONTROL,
						"Energy Control" }, ////////////////////////
				{ GENERIC_TYPE_METER, SPECIFIC_TYPE_WHOLE_HOME_METER_SIMPLE,
						"Whole Home Meter" }, ////////////////////////
				{ GENERIC_TYPE_SECURITY_PANEL, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_SECURITY_PANEL,
				SPECIFIC_TYPE_ZONED_SECURITY_PANEL, "Zoned Security Panel" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_MULTILEVEL, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_MULTILEVEL,
				SPECIFIC_TYPE_CLASS_A_MOTOR_CONTROL, "Window Covering" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_MULTILEVEL,
				SPECIFIC_TYPE_CLASS_B_MOTOR_CONTROL, "Window Covering" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_MULTILEVEL,
				SPECIFIC_TYPE_CLASS_C_MOTOR_CONTROL, "Window Covering" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_MULTILEVEL,
				SPECIFIC_TYPE_MOTOR_MULTIPOSITION, "Multiposition Motor" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_MULTILEVEL,
				SPECIFIC_TYPE_POWER_SWITCH_MULTILEVEL, "Dimmer" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_MULTILEVEL,
				SPECIFIC_TYPE_SCENE_SWITCH_MULTILEVEL, "Multilevel Scene Switch" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_MULTILEVEL, SPECIFIC_TYPE_FAN_SWITCH,
						"Fan Switch" }, ////////////////////////场景通知服务
				{ GENERIC_TYPE_SWITCH_MULTILEVEL,
				SPECIFIC_TYPE_COLOR_TUNABLE_MULTILEVEL,
						"Color Tunable Multilevel" }, ////////////////////////
				{ GENERIC_TYPE_WALL_CONTROLLER, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_WALL_CONTROLLER,
				SPECIFIC_TYPE_BASIC_WALL_CONTROLLER, "Wall Controller" }, ////////////////////////
				{ GENERIC_TYPE_STATIC_CONTROLLER, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_STATIC_CONTROLLER, SPECIFIC_TYPE_PC_CONTROLLER,
						"Central Controller" }, ////////////////////////
				{ GENERIC_TYPE_STATIC_CONTROLLER, //场景通知服务
						SPECIFIC_TYPE_SCENE_CONTROLLER, "Scene Controller" }, ////////////////////////
				{ GENERIC_TYPE_STATIC_CONTROLLER,
				SPECIFIC_TYPE_STATIC_INSTALLER_TOOL, "Static Installer Tool" }, ////////////////////////
				{ GENERIC_TYPE_STATIC_CONTROLLER, SPECIFIC_TYPE_SET_TOP_BOX,
						"Set Top Box" }, ////////////////////////
				{ GENERIC_TYPE_STATIC_CONTROLLER,
				SPECIFIC_TYPE_SUB_SYSTEM_CONTROLLER, "Sub System Controller" }, ////////////////////////
				{ GENERIC_TYPE_STATIC_CONTROLLER, SPECIFIC_TYPE_TV, "TV" }, ////////////////////////
				{ GENERIC_TYPE_STATIC_CONTROLLER, SPECIFIC_TYPE_GATEWAY,
						"Gateway" }, ////////////////////////
				{ GENERIC_TYPE_ENTRY_CONTROL, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_ENTRY_CONTROL, SPECIFIC_TYPE_DOOR_LOCK,
						"Door Lock" }, ////////////////////////
				{ GENERIC_TYPE_ENTRY_CONTROL, SPECIFIC_TYPE_ADVANCED_DOOR_LOCK,
						"Advanced Door Lock" }, ////////////////////////
				{ GENERIC_TYPE_ENTRY_CONTROL,
				SPECIFIC_TYPE_SECURE_KEYPAD_DOOR_LOCK, "Secure Keypad Door Lock" }, ////////////////////////
				{ GENERIC_TYPE_ENTRY_CONTROL,
				SPECIFIC_TYPE_SECURE_KEYPAD_DOOR_LOCK_DEADBOLT, "Door Lock" }, ////////////////////////
				{ GENERIC_TYPE_ENTRY_CONTROL, SPECIFIC_TYPE_SECURE_DOOR,
						"Barrier Door" }, ////////////////////////
				{ GENERIC_TYPE_ENTRY_CONTROL, SPECIFIC_TYPE_SECURE_GATE,
						"Barrier Gate" }, ////////////////////////
				{ GENERIC_TYPE_ENTRY_CONTROL,
				SPECIFIC_TYPE_SECURE_BARRIER_ADDON, "Barrier Addon" }, ////////////////////////
				{ GENERIC_TYPE_ENTRY_CONTROL,
				SPECIFIC_TYPE_SECURE_BARRIER_OPEN_ONLY, "Barrier Open" }, ////////////////////////
				{ GENERIC_TYPE_ENTRY_CONTROL,
				SPECIFIC_TYPE_SECURE_BARRIER_CLOSE_ONLY, "Barrier Close" }, ////////////////////////
				{ GENERIC_TYPE_ENTRY_CONTROL, SPECIFIC_TYPE_SECURE_LOCKBOX,
						"SDS12724" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_BINARY, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_BINARY, SPECIFIC_TYPE_POWER_SWITCH_BINARY,
						"Switch" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_BINARY, SPECIFIC_TYPE_SCENE_SWITCH_BINARY,
						"Scene Switch" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_BINARY, SPECIFIC_TYPE_POWER_STRIP,
						"Power Strip" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_BINARY, SPECIFIC_TYPE_SIREN, "Siren" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_BINARY, SPECIFIC_TYPE_VALVE_OPEN_CLOSE,
						"Valve" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_BINARY,
				SPECIFIC_TYPE_COLOR_TUNABLE_BINARY, "Color Tunable" }, ////////////////////////
				{ GENERIC_TYPE_WINDOW_COVERING, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_WINDOW_COVERING,
				SPECIFIC_TYPE_SIMPLE_WINDOW_COVERING, "Window Covering" }, ////////////////////////
				{ GENERIC_TYPE_SENSOR_BINARY, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_SENSOR_BINARY,
				SPECIFIC_TYPE_ROUTING_SENSOR_BINARY, "Binary Sensor" }, ////////////////////////
				{ GENERIC_TYPE_REPEATER_SLAVE, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_REPEATER_SLAVE, SPECIFIC_TYPE_REPEATER_SLAVE,
						"Repeater" }, ////////////////////////
				{ GENERIC_TYPE_REPEATER_SLAVE, SPECIFIC_TYPE_VIRTUAL_NODE,
						"Virtual Node" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_TOGGLE, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_TOGGLE,
				SPECIFIC_TYPE_SWITCH_TOGGLE_BINARY, "Toggle Switch" }, ////////////////////////
				{ GENERIC_TYPE_SWITCH_TOGGLE,
				SPECIFIC_TYPE_SWITCH_TOGGLE_MULTILEVEL,
						"Multilevel Toggle Switch" }, ////////////////////////
				{ GENERIC_TYPE_SEMI_INTEROPERABLE, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_SEMI_INTEROPERABLE,
				SPECIFIC_TYPE_ENERGY_PRODUCTION, "Energy Production" }, ////////////////////////
				{ GENERIC_TYPE_SENSOR_NOTIFICATION, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class not used" }, ////////////////////////
				{ GENERIC_TYPE_SENSOR_NOTIFICATION,
				SPECIFIC_TYPE_NOTIFICATION_SENSOR, "Notification Sensor" }, ////////////////////////
				{ GENERIC_TYPE_THERMOSTAT, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_THERMOSTAT,
				SPECIFIC_TYPE_SETBACK_SCHEDULE_THERMOSTAT,
						"Setback Schedule Thermostat" }, ////////////////////////
				{ GENERIC_TYPE_THERMOSTAT, SPECIFIC_TYPE_SETBACK_THERMOSTAT,
						"Thermostat " }, ////////////////////////
				{ GENERIC_TYPE_THERMOSTAT, SPECIFIC_TYPE_SETPOINT_THERMOSTAT,
						"Thermostat" }, ////////////////////////
				{ GENERIC_TYPE_THERMOSTAT, SPECIFIC_TYPE_THERMOSTAT_GENERAL,
						"Thermostat" }, ////////////////////////
				{ GENERIC_TYPE_THERMOSTAT, SPECIFIC_TYPE_THERMOSTAT_GENERAL_V2,
						"Thermostat" }, ////////////////////////
				{ GENERIC_TYPE_THERMOSTAT, SPECIFIC_TYPE_THERMOSTAT_HEATING,
						"Thermostat" }, ////////////////////////
				{ GENERIC_TYPE_NON_INTEROPERABLE, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_ZIP_NODE, SPECIFIC_TYPE_NOT_USED,
						"Specific Device Class Not Used" }, ////////////////////////
				{ GENERIC_TYPE_ZIP_NODE, SPECIFIC_TYPE_ZIP_ADV_NODE,
						"Zip adv node" }, ////////////////////////
				{ GENERIC_TYPE_ZIP_NODE, SPECIFIC_TYPE_ZIP_TUN_NODE,
						"Zip tun node" }, ////////////////////////

				{ 0, 0,
				NULL } ////////////////////////
		};

const char *id_namge_get(struct id_name *idinfo, int id) {
	int i = 0;
	while (idinfo[i].name != NULL) {
		if (idinfo[i].id == id)
			return idinfo[i].name;
		i++;
	}
	return "";
}

const char *id_id_name_get(struct id_id_name *idinfo, int id1, int id2) {
	int i = 0;
	for (i = 0; idinfo[i].name != NULL; i++) {
		if (idinfo[i].generic == id1 && idinfo[i].specific == id2) {
			return idinfo[i].name;
		}
	}
	return NULL;
}

void *_t_zwaveloop(void *args) {
	pthread_detach(pthread_self());
	zwave_app_loop((char*)args);
	return NULL;
}

void hal_init(const char *path) {
	pthread_t pt;
	pthread_create(&pt, NULL, _t_zwaveloop, path);
}

void zwave_debug_info() {
	int i = 0;
	for (i = 0; i < sizeof(ZWaveAllNodes) / sizeof(ZWaveAllNodes[0]); i++) {
		if (ZWaveAllNodes[i].node == 0)
			continue;
		log_d("zwave have node:%d-%d", i, ZWaveAllNodes[i].node);
	}
	for (i = 0; i < sizeof(ZWaveAllNodes) / sizeof(ZWaveAllNodes[0]); i++) {
		if (ZWaveAllNodes[i].node == 0)
			continue;

		log_d("-------------------zwave node:%d -------------------------", i);
		log_hex(&ZWaveAllNodes[i].nodeInfo, sizeof(ZNODEINFO), "ZNODEINFO:");
		log_d("capability: %02X, %s", ZWaveAllNodes[i].nodeInfo.capability,
				((ZWaveAllNodes[i].nodeInfo.capability & 0x70) == 0x70) ?
						"----> listening" : "-->not listening");

		log_d("base:%02X  %s", ZWaveAllNodes[i].nodeInfo.nodeType.basic,
				id_namge_get(zwave_res_base,
						ZWaveAllNodes[i].nodeInfo.nodeType.basic));

		log_d("generic:%02X   %s", ZWaveAllNodes[i].nodeInfo.nodeType.generic,
				id_namge_get(zwave_res_generic,
						ZWaveAllNodes[i].nodeInfo.nodeType.generic));

		log_d("specific:%02X   %s", ZWaveAllNodes[i].nodeInfo.nodeType.specific,
				id_id_name_get(zwave_res_specific,
						ZWaveAllNodes[i].nodeInfo.nodeType.generic,
						ZWaveAllNodes[i].nodeInfo.nodeType.specific));

		log_hex(&ZWaveAllNodes[i].cmdclass, 50, "class:");
		log_hex(&ZWaveAllNodes[i].version, 50, "clver:");
		log_hex(&ZWaveAllNodes[i].clsSuperSecurity, 50, "security:");
	}
}

void zw_hook_init_end() {
	log_d("zwave init success");
	hal_sync_zwave_node();
}

void hal_sync_zwave_node() {
//	unsigned char i = 0;
//	int existsFlag = 0;
//	/**
//	 * 同步数据库
//	 */
//	for (i = 1; i < 233; i++) {
//		//ZWaveAllNodes[i].node=i; //cj-debug.code:
//		existsFlag = dao_zdevices_exists_node(i);
//
//		if (ZWaveAllNodes[i].node == 0 && existsFlag == 0) {
//			//log_d("不存在的节点：%d", i);
//			continue;
//		}
//
//		if (ZWaveAllNodes[i].node == 0 && existsFlag == 1) { //数据库里面存在，而实际不存在
//		//delete node need.....
//			log_d("移除节点:%d", i);
//			service_dev_managent_delete(i);
//			continue;
//		}
//		if (existsFlag == 0) { //不存在数据库里面
//			//need add the node
//			log_d("同步到数据库->%d", ZWaveAllNodes[i].node);
//			service_dev_managent_add(ZWaveAllNodes[i].node, &ZWaveAllNodes[i]);
//			continue;
//		}
//		////////////////////////////////////////////
//		log_d("sync the node info ----%d", i);
//		////////////////////////////////////////////
//		dao_zdevices_get(ZWaveAllNodes[i].node, &ZWaveAllNodes[i]);
//		ZWaveAllNodes[i].node = i;
//	}
//
//	zwave_debug_info();
}

typedef struct zwcls {
	uint8_t node;
	void *data;
	int dlen;
	sem_t sem;
	int status;
} zwcls_t;

static pthread_mutex_t _lock_zwsend = PTHREAD_MUTEX_INITIALIZER;
static zwcls_t *_zcls = NULL;
static pthread_mutex_t _lock_zcls = PTHREAD_MUTEX_INITIALIZER;

static void _zls_send_callback(int status, void *context) {
	zwcls_t *cls = (zwcls_t*) context;
	pthread_mutex_lock(&_lock_zcls);
	if (_zcls != NULL && _zcls == cls) {
		_zcls->status = status;
		sem_post(&_zcls->sem);
	}
	pthread_mutex_unlock(&_lock_zcls);
}

static void _zcls_send(void *context) {
	zwcls_t *cls = (zwcls_t*) context;
	zw_tx_param_t nodep;
	zw_tx_param_init(&nodep, cls->node, 0);
	zw_senddata_first(&nodep, cls->data, cls->dlen, _zls_send_callback, cls);
}

int zwclass_send(uint8_t node, void *data, int dlen, int txoptions) {
	zwcls_t *cls = (zwcls_t*) malloc(sizeof(zwcls_t));
	int status = 2;
	if (cls) {
		cls->node = node;
		cls->data = data;
		cls->dlen = dlen;
		cls->status = 2;
		sem_init(&cls->sem, 0, 0);

		pthread_mutex_lock(&_lock_zwsend);

		pthread_mutex_lock(&_lock_zcls);
		_zcls = cls;
		pthread_mutex_unlock(&_lock_zcls);

		zw_dispatch_async(_zcls_send, cls);
		sem_wait_ms(&cls->sem, 1000 * 8);

		pthread_mutex_lock(&_lock_zcls);
		_zcls = NULL;
		pthread_mutex_unlock(&_lock_zcls);

		pthread_mutex_unlock(&_lock_zwsend);
		sem_destroy(&cls->sem);
		status = cls->status;
		free(cls);
	}
	status = cls->status;
	return status;
}
