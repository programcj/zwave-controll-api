/*
 * cJSON_cj.c
 *
 *  Created on: 2017年6月15日
 *      Author: cj
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "cJSON_cj.h"

const char *cJSON_GetObjectItemValueString(cJSON *obj, const char *name,
		const char *defvalue) {
	const char *v = NULL;
	cJSON *item = NULL;

	v = defvalue;
	if (obj == NULL) {
		return v;
	}

	item = cJSON_GetObjectItem(obj, name);
	if (item && item->type == cJSON_String) {
		v = item->valuestring;
	}
	return v;
}

int cJSON_GetObjectItemValueInt(cJSON *obj, const char *name, int defvalue) {
	cJSON *item = NULL;
	if (obj) {
		item = cJSON_GetObjectItem(obj, name);
		if (item && item->type == cJSON_Number)
			return item->valueint;
		if (item && item->type == cJSON_String) {
			return atoi(item->valuestring);
		}
	}
	return defvalue;
}

long long cJSON_GetObjectItemValueLong(cJSON *obj, const char *name,
		long defvalue) {
	cJSON *item = NULL;
	long long v = defvalue;
	if (obj) {
		item = cJSON_GetObjectItem(obj, name);
		if (item && item->type == cJSON_Number)
			v = item->valuedouble;
	}
	return v;
}

double cJSON_GetObjectItemValueDouble(cJSON *obj, const char *name,
		double defvalue) {
	cJSON *item = NULL;
	if (obj) {
		item = cJSON_GetObjectItem(obj, name);
		if (item && item->type == cJSON_Number)
			return item->valuedouble;
	}
	return defvalue;
}
