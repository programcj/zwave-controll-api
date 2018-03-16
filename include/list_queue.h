/*
 * list_queue.h
 *
 *  Created on: 2017年3月29日
 *      Author: cj
 */

#ifndef SRC_LIST_QUEUE_H_
#define SRC_LIST_QUEUE_H_

#include <pthread.h>
#include <semaphore.h>
#include "list.h"

struct list_queue {
	pthread_mutex_t lock;
	sem_t sem;
	struct list_head list;
	int length;
};

void list_queue_init(struct list_queue *queue);

void list_queue_append_head(struct list_queue *queue, struct list_head *newlist);

void list_queue_append_tail(struct list_queue *queue, struct list_head *newlist);

void list_queue_append_tail2(struct list_queue *queue,
		struct list_head *newlist, int postflag);

void list_queue_release(struct list_queue *queue);

int list_queue_length(struct list_queue *queue);

int list_queue_wait(struct list_queue *queue, int ms);

int list_queue_timedwait(struct list_queue *queue, int seconds);

struct list_head *list_queue_pop(struct list_queue *queue);

struct list_head *list_queue_first(struct list_queue *queue);

struct list_head *list_queue_next(struct list_queue *queue,
		struct list_head *mylist);

void list_queue_list_delete(struct list_queue *queue, struct list_head *mylist);

int list_queue_lock(struct list_queue *queue);
int list_queue_unlock(struct list_queue *queue);

int sem_wait_ms(sem_t *sem, int ms);

#endif /* SRC_LIST_QUEUE_H_ */
