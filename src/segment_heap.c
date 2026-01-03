#include "segment_heap.h"
#include <stdio.h>
#include <sys/mman.h>

int create_safe_heap(struct safe_heap_s *safe_heap)
{
	assert(safe_heap != NULL && "create_safe_heap: given NULL safe_heap");
	int pkey = pkey_alloc(0, 0);
	if (pkey == -1) {
		perror("create_safe_heap: pkey_alloc");
		return EXIT_FAILURE;
	}

	void *hint = (void *)0x300000000000;
	void *addr = mmap(hint, SAFE_HEAP_SIZE, PROT_WRITE | PROT_READ,
			  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		perror("create_safe_heap: mmap");
		goto cleanup_pkey;
	}

	if (pkey_mprotect(addr, SAFE_HEAP_SIZE, PROT_WRITE | PROT_READ, pkey) ==
	    -1) {
		perror("create_safe_heap: pkey_mprotect");
		goto cleanup;
	}

	// found through github that they need 4MiB alignment, perhaps look
	// again later though from my expirements it's 32MB...
	mi_arena_id_t mi_id;
	if (!mi_manage_os_memory_ex(addr, SAFE_HEAP_SIZE, false, false, true,
				    -1, true, &mi_id)) {
		fprintf(stderr, "create_safe_heap: mi_manage_os_memory_ex\n");
		goto cleanup;
	}

	mi_heap_t *heap = mi_heap_new_in_arena(mi_id);
	if (heap == NULL) {
		fprintf(stderr, "create_safe_heap: mi_heap_new_in_arena");
		goto cleanup;
	}
	safe_heap->heap = heap;
	safe_heap->heap_size = SAFE_HEAP_SIZE;
	safe_heap->pkey = pkey;
	safe_heap->mmap_addr = addr;
	return EXIT_SUCCESS;

cleanup:
	if (munmap(addr, SAFE_HEAP_SIZE) == -1) {
		perror("create_safe_heap: munmap");
	}
cleanup_pkey:
	if (pkey_free(pkey) == -1) {
		perror("create_safe_heap: pkey_free");
	}
	return EXIT_FAILURE;
}

int destroy_safe_heap(struct safe_heap_s *safe_heap)
{
	assert(safe_heap != NULL && "destroy_safe_heap: given NULL safe_heap");
	assert(safe_heap->mmap_addr != NULL &&
	       "destroy_safe_heap: tried to free NULL mmap_addr");
	assert(safe_heap->heap != NULL &&
	       "destroy_safe_heap: tried to free NULL safe_heap");
	mi_heap_delete(safe_heap->heap);
	if (munmap(safe_heap->mmap_addr, SAFE_HEAP_SIZE) != 0) {
		perror("destroy_safe_heap: munmap");
		return EXIT_FAILURE;
	}
	if (pkey_free(safe_heap->pkey) != 0) {
		perror("destroy_safe_heap: pkey_free");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

enum PKEY_PERM_OLD read_pkey_perm_old(int pkey)
{
	uint32_t pkru = _rdpkru_u32();
	int mask = 0b11; // we need the two LSBits
	// we shift by two since each pkey is assigned two bits
	int ret_val = (pkru >> (2 * pkey)) & mask;
	return ret_val;
}

int write_pkey_perm_old(int pkey, enum PKEY_PERM_OLD perm)
{
	uint32_t pkru = _rdpkru_u32();
	uint32_t mask = 0b11; // we need to clear two bits
	pkru &= ~(mask << (2 * pkey));
	// we shift by two since each pkey is assigned two bits
	_wrpkru((perm << (2 * pkey) | pkru));
	if (read_pkey_perm_old(pkey) != perm) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

enum PKEY_PERM pkey_get_perm(int pkey)
{
        enum PKEY_PERM retval = pkey_get(pkey);
        return retval;
}

/*
 * include wrapper here to handle errors cleaner.
 */
void pkey_set_perm(int pkey, enum PKEY_PERM perm)
{
	int retval = pkey_set(pkey, perm);
	if (retval == 0) {
		return;
	}

	fprintf(
	    stderr,
	    "ERROR: pkey_write_perm: unable to write pkey value, exiting...\n");
	exit(EXIT_FAILURE);
}
