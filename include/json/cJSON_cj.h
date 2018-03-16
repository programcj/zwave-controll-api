/*
 * cJSON_cj.h
 *
 *  Created on: 2017年6月15日
 *      Author: cj
 */

#ifndef FILE_SRC_INCLUDE_JSON_CJSON_CJ_H_
#define FILE_SRC_INCLUDE_JSON_CJSON_CJ_H_

#include "cJSON.h"

const char *cJSON_GetObjectItemValueString(cJSON *obj, const char *name,
		const char *defvalue);

int cJSON_GetObjectItemValueInt(cJSON *obj, const char *name, int defvalue);

long long cJSON_GetObjectItemValueLong(cJSON *obj, const char *name,
		long defvalue);

double cJSON_GetObjectItemValueDouble(cJSON *obj, const char *name,
		double defvalue);

#endif /* FILE_SRC_INCLUDE_JSON_CJSON_CJ_H_ */
