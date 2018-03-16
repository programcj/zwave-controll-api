/*
 * z_ctimer.c
 *
 *  Created on: 2017年12月15日
 *      Author: cj
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "z_ctimer.h"
#include "port.h"

static struct list_head _list_ctime;

void ctimer_init() {
	INIT_LIST_HEAD(&_list_ctime);
}

void ctimer_set(struct ctimer *t, int usecs, void (*callback)(void*),
		void *context) {
	timer_set(&t->tim, usecs);
	t->context = context;
	t->callback = callback;
	list_add_tail(&t->list, &_list_ctime);
}

void ctimer_stop(struct ctimer *t) {
	if (t->list.next != LIST_POISON1 && t->list.prev != LIST_POISON2) {
		list_del(&t->list);
	} else {
		log_d("ctimer err.....");
	}
}

void ctimer_handle() {
	struct list_head *p, *n;
	struct ctimer *item = NULL;
	list_for_each_safe(p, n, &_list_ctime)
	{
		item = list_entry(p, struct ctimer, list);
		if (timer_expired(&item->tim)) {
			list_del(p);
			if (item->callback)
				item->callback(item->context);
		}
	}
}
