#define _GNU_SOURCE /* for RTLD_NEXT.  */
#include "bookkeeper.h"
#include "segment_heap.h"
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static struct safe_heap_s safe_heap;
static struct stack_region_s safe_stack;

typedef void *(*_malloc_t)(size_t);
typedef void *(*_calloc_t)(size_t, size_t);
typedef void *(*_realloc_t)(void *_Nullable, size_t);
typedef void (*_free_t)(void *_Nullable);
static _malloc_t _malloc = NULL;
static _calloc_t _calloc = NULL;
static _realloc_t _realloc = NULL;
static _free_t _free = NULL;

/*
 * initialization flag to handle correct usage of hooked allocations during
 * init procedure. We also need to use a temporary heap. Both vars are defined
 * in hook_init. Since hook_init is a constructor we needn't worry about
 * concurrency.
 */
static bool INITIALIZING;
static mi_heap_t *tmp_heap;

#define ERR_MSG_LEN 1024
static __thread char err_msg[ERR_MSG_LEN];
static __thread bool in_safe_block = false;
static __thread bool exempt = false;

#define LOAD_SYMBOL_ONCE(hook, sym, type)                                      \
	do {                                                                   \
		if (hook) {                                                    \
			break;                                                 \
		}                                                              \
		hook = (type)dlsym(RTLD_NEXT, sym);                            \
		if (!hook) {                                                   \
			strcpy(err_msg, "ERROR: LOAD_SYMBOL_ONCE: ");          \
			strcat(err_msg, #sym);                                 \
			strcat(err_msg, dlerror());                            \
			strcat(err_msg, "\n");                                 \
			write(STDERR_FILENO, err_msg, strlen(err_msg));        \
			exit(EXIT_FAILURE);                                    \
		}                                                              \
	} while (0)

/*
 * We're the last shared object to be loaded, so it's safe to use functions from
 * dependencies.
 * Also, calling malloc under the hood here is UNSAFE, calls to malloc are not
 * possible (might have dealt with this, be careful...). Last, we initialize
 * our bookkeeping module. initialization status handled through flag and
 * tmp_heap.
 * */
__attribute__((constructor)) static void hook_init(void)
{
	INITIALIZING = true;
	tmp_heap = mi_heap_get_default();

	LOAD_SYMBOL_ONCE(_malloc, "malloc", _malloc_t);
	LOAD_SYMBOL_ONCE(_calloc, "calloc", _calloc_t);
	LOAD_SYMBOL_ONCE(_realloc, "realloc", _realloc_t);
	LOAD_SYMBOL_ONCE(_free, "free", _free_t);

	int ret;
	ret = create_safe_heap(&safe_heap);
	if (ret == EXIT_FAILURE) {
		fprintf(stderr, "ERROR: create_safe_heap failed, exiting...\n");
		exit(EXIT_FAILURE);
	}

	/* set safe context during init */
	pkey_set_perm(safe_heap.pkey, RDWR);

	ret = bookkeeper_init(safe_heap.heap, safe_heap.mmap_addr,
			      safe_heap.heap_size);
	if (ret == EXIT_FAILURE) {
		fprintf(stderr, "ERROR: bookkeeper_init failed, exiting...\n");
		exit(EXIT_FAILURE);
	}

	safe_stack.bottom = safe_stack.top = 0x0;

	tmp_heap = NULL;

	/* init done, exiting safe context */
	pkey_set_perm(safe_heap.pkey, NO_ACCESS);
	INITIALIZING = false;
}

/*
 * We're the first shared object to be unloaded, so it's safe to use functions
 * from dependencies.
 * However, calling malloc under the hood here is NOT safe, since we still have
 * not unloaded the hooks. BE CAREFUL
 * */
__attribute__((destructor)) static void hook_exit(void)
{
	/* since we're exiting, we enable perms to properly cleanup without
	 * issues. */
	pkey_set_perm(safe_heap.pkey, RDWR);

#ifdef _BOOKKEEPER_DEBUG
	bookkeeper_dump();
#endif
	int ret = bookkeeper_exit();
	if (ret == EXIT_FAILURE) {
		fprintf(stderr, "ERROR: bookkeeper_exit failed\n");
	}

	ret = destroy_safe_heap(&safe_heap);
	if (ret == EXIT_FAILURE) {
		fprintf(stderr, "ERROR: destroy_safe_heap failed\n");
	}
}

/* initially used asserts for sanity checks, but they use malloc under the
 * hood...
 */
static void safe_block_sanity_check(void)
{
	char *err_msg;
	enum PKEY_PERM perm = pkey_get_perm(safe_heap.pkey);
	if (perm != RDWR) {
		err_msg = "ERROR: IN SAFE BLOCK, YET WE DON'T HAVE RDWR PERM\n";
		write(STDERR_FILENO, err_msg, strlen(err_msg));
		exit(EXIT_FAILURE);
	}

	if (safe_stack.bottom == 0x0) {
		err_msg =
		    "ERROR: IN SAFE BLOCK, YET SAFE STACK BOTTOM IS NOT SET\n";
		write(STDERR_FILENO, err_msg, strlen(err_msg));
		exit(EXIT_FAILURE);
	}
}

static void unsafe_block_sanity_check(void)
{
	if (pkey_get_perm(safe_heap.pkey) != RDWR) {
		return;
	}
	char *err_msg = "ERROR: IN UNSAFE BLOCK, YET WE HAVE RDWR PERM\n";
	write(STDERR_FILENO, err_msg, strlen(err_msg));
	exit(EXIT_FAILURE);
}

/* need to be given frame address of caller,
 * using __builtin_frame_address(unsigned int level) with non-zero level
 * is undefined behaviour https://gcc.gnu.org/onlinedocs/gcc/Return-Address.html
 * */
void enter_safe_block(void *stack_bottom)
{
	/* prefer aligning up, feeling more conservative I guess...
	 * I could perhaps add `padding` to ensure more we're scanning all
	 * that we should.
	 */
	safe_stack.bottom = (uintptr_t *)PTR_ALIGN_UP(stack_bottom);
	in_safe_block = true;
	pkey_set_perm(safe_heap.pkey, RDWR);
}

void exit_safe_block(void)
{
	safe_stack.top = 0x0;
	safe_stack.bottom = 0x0;
	in_safe_block = false;
	pkey_set_perm(safe_heap.pkey, NO_ACCESS);
}

void set_exempt(void)
{
	if (!in_safe_block) {
		char *err_msg =
		    "ERROR: CALLING EXEMPT IN UNSAFE BLOCK NOT ALLOWED";
		write(STDERR_FILENO, err_msg, strlen(err_msg));
		exit(EXIT_FAILURE);
	}
	exempt = true;
}

void unset_exempt(void)
{
	if (!in_safe_block) {
		char *err_msg =
		    "ERROR: CALLING EXEMPT IN UNSAFE BLOCK NOT ALLOWED";
		write(STDERR_FILENO, err_msg, strlen(err_msg));
		exit(EXIT_FAILURE);
	}
	exempt = false;
}

void purge_safe_block(void)
{
	if (!in_safe_block) {
		char *err_msg =
		    "ERROR: CALLING PURGE IN UNSAFE BLOCK NOT ALLOWED";
		write(STDERR_FILENO, err_msg, strlen(err_msg));
		exit(EXIT_FAILURE);
	}
	bookkeeper_purge_all();
}

void *malloc(size_t size)
{
	if (INITIALIZING) {
		return mi_heap_malloc(tmp_heap, size);
	}

	if (in_safe_block) {
		safe_block_sanity_check();
		if (exempt) {
			/* user requsted for allocs to bypass safe heap */
			return _malloc(size);
		}
		void *addr = mi_heap_malloc(safe_heap.heap, size);
		if (bookkeeper_add(addr, size) == EXIT_FAILURE) {
			// should not continue...
			char *err_msg =
			    "ERROR: malloc: bookkeeper_add failed\n";
			write(STDERR_FILENO, err_msg, strlen(err_msg));
			exit(EXIT_FAILURE);
		}
		return addr;
	}
	unsafe_block_sanity_check();
	pkey_set_perm(safe_heap.pkey, RDWR);
	void *addr = _malloc(size);
	pkey_set_perm(safe_heap.pkey, NO_ACCESS);
	return addr;
}

void *calloc(size_t nmemb, size_t size)
{
	if (INITIALIZING) {
		return mi_heap_calloc(tmp_heap, nmemb, size);
	}

	if (in_safe_block) {
		safe_block_sanity_check();
		if (exempt) {
			/* user requsted for allocs to  bypass safe heap */
			return _calloc(nmemb, size);
		}
		void *addr = mi_heap_calloc(safe_heap.heap, nmemb, size);
		size_t bsize = nmemb * size; // size in bytes
		if (bookkeeper_add(addr, bsize) == EXIT_FAILURE) {
			// should not continue...
			char *err_msg =
			    "ERROR: calloc: bookkeeper_add failed\n";
			write(STDERR_FILENO, err_msg, strlen(err_msg));
			exit(EXIT_FAILURE);
		}
		return addr;
	}
	unsafe_block_sanity_check();
	pkey_set_perm(safe_heap.pkey, RDWR);
	void *addr = _calloc(nmemb, size);
	pkey_set_perm(safe_heap.pkey, NO_ACCESS);
	return addr;
}

void *realloc(void *_Nullable ptr, size_t size)
{
	if (INITIALIZING) {
		return mi_heap_realloc(tmp_heap, ptr, size);
	}

	if (in_safe_block) {
		safe_block_sanity_check();
		if (exempt) {
			/* user requsted for allocs to  bypass safe heap */
			return _realloc(ptr, size);
		}
		/* there is a bug here. The old address needs to be removed
		 * from the bookkeeper. */
		void *addr = mi_heap_realloc(safe_heap.heap, ptr, size);
		if (bookkeeper_add(addr, size) == EXIT_FAILURE) {
			// should not continue...
			char *err_msg =
			    "ERROR: realloc: bookkeeper_add failed\n";
			write(STDERR_FILENO, err_msg, strlen(err_msg));
			exit(EXIT_FAILURE);
		}
		return addr;
	}
	unsafe_block_sanity_check();
	pkey_set_perm(safe_heap.pkey, RDWR);
	void *addr = _realloc(ptr, size);
	pkey_set_perm(safe_heap.pkey, NO_ACCESS);
	return addr;
}

void free(void *_Nullable ptr)
{
	if (INITIALIZING) {
		mi_free(ptr);
		return;
	}

	if (in_safe_block) {
		safe_block_sanity_check();
		if (exempt) {
			/* user requsted for allocs to  bypass safe heap */
			_free(ptr);
			return;
		}
		/* to get stack addr GNU builtin can be used, however it's not
		 * defined on clang yet... github issue:
		 * https://github.com/llvm/llvm-project/issues/82632
		 * use inline asm as temporary solution
		 */
		void *stack_top;
		__asm__("mov %%rsp, %0" : "=r"(stack_top));
		safe_stack.top = (uintptr_t *)PTR_ALIGN_DOWN(stack_top);
		bookkeeper_request_free(ptr, &safe_stack);
		return;
	}

	unsafe_block_sanity_check();
	pkey_set_perm(safe_heap.pkey, RDWR);
	_free(ptr);
	pkey_set_perm(safe_heap.pkey, NO_ACCESS);
}
