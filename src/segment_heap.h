#ifndef SEGMENT_HEAP_H
#define SEGMENT_HEAP_H
#define _GNU_SOURCE
#include <assert.h>
#include <immintrin.h>
#include <mimalloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define SAFE_HEAP_SIZE (1024UL * 1024 * 1024) * 4 // 1GiB * 4

struct safe_heap_s {
	void *mmap_addr;
        size_t heap_size;
	int pkey;
	mi_heap_t *heap;
};

// each pkey_perm has two bits, (WD, AD)
// WD: write disable, AD: access disable
enum PKEY_PERM_OLD {
	RDWR_OLD = 0b00,
	RD_ONLY_OLD = 0b10,
	NO_ACCESS_OLD = 0b11,
};

enum PKEY_PERM {
	RDWR = 0,
	RD_ONLY = PKEY_DISABLE_WRITE,
	NO_ACCESS = PKEY_DISABLE_ACCESS,
};

int create_safe_heap(struct safe_heap_s *safe_heap);
int destroy_safe_heap(struct safe_heap_s *safe_heap);
enum PKEY_PERM pkey_get_perm(int pkey);
void pkey_set_perm(int pkey, enum PKEY_PERM perm);

/* old functions, should not be used */
enum PKEY_PERM_OLD read_pkey_perm_old(int pkey);
int write_pkey_perm_old(int pkey, enum PKEY_PERM_OLD perm);
#endif
