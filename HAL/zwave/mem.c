/*
 * zw_mem.c
 *
 *  Created on: 2017年12月14日
 *      Author: cj
 */

#include "config.h"
#include "mem.h"

struct list_head _list_mem;

typedef struct mem {
	struct list_head list;
	void *ptr;
	const char *file;
	const char *fun;
	int line;
} mem_t;

void mem_init() {
	INIT_LIST_HEAD(&_list_mem);
}

void mem_debug() {
	struct list_head *p, *n;
	mem_t *mem;
	list_for_each_safe(p,n, &_list_mem)
	{
		mem = list_entry(p, mem_t, list);
		log_d("%04X(%s, %s,%d)", mem->ptr, mem->file, mem->fun, mem->line);
	}
}

void zw_mem_append(void *ptr, const char *file, const char *function, int line) {
	mem_t *mem = (mem_t*) malloc(sizeof(mem_t));
	if (mem) {
		memset(mem, 0, sizeof(mem_t));
		mem->ptr = ptr;
		mem->line = line;
		mem->fun = function;
		mem->file = file;
		list_add_tail(&mem->list, &_list_mem);
	}
}

void zw_mem_remove(void *ptr) {
	struct list_head *p, *n;
	mem_t *mem;
	list_for_each_safe(p,n, &_list_mem)
	{
		mem = list_entry(p, mem_t, list);
		if (mem->ptr == ptr) {
			list_del(&mem->list);
			free(mem);
		}
	}
}

void *_mem_malloc(size_t size, const char *file, const char *function, int line) {
	void *ptr = malloc(size);
	if (ptr) {
		zw_mem_append(ptr, file, function, line);
	}
	return ptr;
}

void *mem_realloc(void *__ptr, size_t __size, const char *file,
		const char *function, int line) {
	void *ptr = realloc(__ptr, __size);
	if (__ptr) {
		zw_mem_append(ptr, file, function, line);
	}
	return ptr;
}

void mem_free(void *ptr) {
	if (ptr) {
		free(ptr);
		zw_mem_remove(ptr);
	}
}
