#ifndef BOOKKEEPER_H
#define BOOKKEEPER_H

/*
 * BOOKKEEPING FOR RUNTIME. WILL BE CALLED FROM MALLOC HOOKS, WHICH MEANS
 * CALLING MALLOC IS NOT ALLOWED. (mi_heap_malloc AND FRIENDS ARE ALLOWED)
 */

#define _GNU_SOURCE
#include "stack.h"
#include <link.h>
#include <mimalloc.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef _BOOKKEEPER_DEBUG
#define DBG_PRNT(fmt, args...)                                                 \
	fprintf(stderr, "DEBUG: %s: " fmt, __func__, ##args)
#else
#define DBG_PRNT(fmt, args...)
#endif

/* assuming sizeof(uintptr_t) == 8 */
#define UNTAG(p) ((uintptr_t)(p) & 0xfffffffffffffffe)
#define IS_MARKED(p) ((uintptr_t)(p) & 0x1)

#define PTR_ALIGN_UP(p) __builtin_align_up((p), alignof(void *))
#define PTR_ALIGN_DOWN(p) __builtin_align_down((p), alignof(void *))

#define MAX_SEGMENTS 64 /* max num of data segments */

struct alloc_data_s {
	uintptr_t addr; /* easier to work with uintptr_t */
	uint32_t size;
	bool requested_free;
	uint8_t unreachable_cnt;
};

struct mem_regions_s {
	uintptr_t *start[MAX_SEGMENTS];
	uintptr_t *end[MAX_SEGMENTS];
	uint32_t cnt;
	uint32_t len;
};

struct stack_region_s {
	uintptr_t *top;
	uintptr_t *bottom;
};

int bookkeeper_init(mi_heap_t *heap, void *heap_addr, size_t heap_size);
int bookkeeper_add(void *addr, size_t size);
int bookkeeper_exit(void);
int bookkeeper_request_free(void *ptr, struct stack_region_s *safe_stack);
void bookkeeper_purge_all(void);
void bookkeeper_dump(void);
#endif
