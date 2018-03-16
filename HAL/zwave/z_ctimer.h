/*
 * z_ctimer.h
 *
 *  Created on: 2017年12月15日
 *      Author: cj
 */

#ifndef HAL_Z_CTIMER_H_
#define HAL_Z_CTIMER_H_

#include "config.h"

typedef unsigned long clock_time_t;

struct timer {
	clock_time_t start;
	clock_time_t interval;
};

struct ctimer {
	struct list_head list;
	struct timer tim;
	void (*callback)(void *);
	void *context;
};

void ctimer_init();

void ctimer_set(struct ctimer *t, int usecs, void (*callback)(void*),
		void *context);

void ctimer_stop(struct ctimer *t);

void ctimer_handle();

#endif /* HAL_Z_CTIMER_H_ */
