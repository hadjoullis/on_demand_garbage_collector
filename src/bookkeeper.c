#include "bookkeeper.h"
#define INIT_LENGTH 1024
#define MSG_LEN 64

static struct alloc_data_s *book;
static uint32_t book_cnt = 0;
static uint32_t book_len = INIT_LENGTH;

static mi_heap_t *heap; /* heap used for explicit allocations */

static uintptr_t heap_addr; /* used for simple ptrs bounds checking */
static size_t heap_size;

static uint64_t free_requests_cnt = 0;
static uint64_t actual_frees_cnt = 0;

int bookkeeper_init(mi_heap_t *_heap, void *_heap_addr, size_t _heap_size)
{
	heap_addr = (uintptr_t)_heap_addr;
	heap_size = _heap_size;
	heap = _heap;
	size_t size = sizeof(struct alloc_data_s) * INIT_LENGTH;
	book = mi_heap_malloc(heap, size);
	if (book == NULL) {
		perror("bookkeeper_init: mi_heap_malloc");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int bookkeeper_exit(void)
{
	mi_free(book);
	book = NULL;

	return EXIT_SUCCESS;
}

int bookkeeper_add(void *addr, size_t size)
{
	if (book_cnt == book_len) {
		// look for empty slot
		for (size_t i = 0; i < book_len; i++) {
			if (book[i].addr != 0) {
				continue;
			}
			/* book_cnt should NOT be incremented since we're not
			 * extending the cnt of the book */
			book[i].addr = (uintptr_t)addr;
			book[i].size = size;
			book[i].requested_free = false;
			book[i].unreachable_cnt = 0;
			return EXIT_SUCCESS;
		}
		// realloc ...
		size_t elem_size = sizeof(struct alloc_data_s);
		size_t newsize = elem_size * book_len * 2;
		void *tmp = mi_heap_realloc(heap, book, newsize);
		if (tmp == NULL) {
			perror("bookkeeper_add: mi_heap_realloc");
			return EXIT_FAILURE;
		}
		book = tmp;
		book_len *= 2;
	}
	size_t idx = book_cnt++;
	book[idx].addr = (uintptr_t)addr;
	book[idx].size = size;
	book[idx].requested_free = false;
	book[idx].unreachable_cnt = 0;
	return EXIT_SUCCESS;
}

int book_del_entry(void *alloc_addr)
{
	for (size_t i = 0; i < book_len; i++) {
		if (UNTAG(book[i].addr) != (uintptr_t)alloc_addr) {
			continue;
		}
		book[i].addr = 0;
		return EXIT_SUCCESS;
	}

	// addr not found
	// addr always has 16 bytes size + 2 (for 0x) = 18 bytes
	static char err_msg[MSG_LEN];
	snprintf(err_msg, MSG_LEN, "book_del_entry: %p addr not found\n",
		 alloc_addr);
	write(STDERR_FILENO, err_msg, strlen(err_msg));
	return EXIT_FAILURE;
}

static int mark_from_region(struct stack_s *worklist, uintptr_t *start,
			    uintptr_t *end)
{
	DBG_PRNT("REGION: %p - %p\n", start, end);
	for (uintptr_t *cur = start; cur < end; cur++) {
		/* mark iff addr belongs to a heap object */
		uintptr_t addr = *cur;
		if (addr < heap_addr || addr > heap_addr + heap_size) {
			continue;
		}

		for (size_t i = 0; i < book_cnt; i++) {
			if (book[i].addr == 0) {
				continue;
			}

			uintptr_t obj_addr = UNTAG(book[i].addr);
			/* capture off by one ptrs */
			if (addr < obj_addr || addr > obj_addr + book[i].size) {
				continue;
			}
			if (IS_MARKED(book[i].addr)) {
				DBG_PRNT("FOUND: %p -> %p\n", (void *)cur,
					 (void *)obj_addr);
				continue;
			}

			/* mark */
			book[i].addr |= 0x1;
			int ret = stack_push(heap, worklist, &book[i]);
			if (ret) {
				return EXIT_FAILURE;
			}
		}
	}
	return EXIT_SUCCESS;
}

static int mark(struct stack_s *worklist)
{
	while (!stack_is_empty(worklist)) {
		struct alloc_data_s *book_entry;
		stack_pop(worklist, (void **)&book_entry);

		/* convert obj addr and size into region... */
		uintptr_t obj_addr = UNTAG(book_entry->addr);
		uintptr_t *start = (uintptr_t *)PTR_ALIGN_UP(obj_addr);
		uintptr_t *end =
		    (uintptr_t *)PTR_ALIGN_DOWN(obj_addr + book_entry->size);

		mark_from_region(worklist, start, end);
	}

	return EXIT_SUCCESS;
}

static int dynlibs_data_cb(struct dl_phdr_info *info, size_t size, void *data)
{
	/* ensures compatibility across systems by making sure
	 * that dl_iterate_phdr is large enough to hold
	 * necessary data */
	if (size < offsetof(struct dl_phdr_info, dlpi_phnum) +
		sizeof(info->dlpi_phnum)) {
		return 1;
	}

	struct mem_regions_s *data_segments = (struct mem_regions_s *)data;
	/* skip hook data. should NOT go through my metadata
	 * (maybe check with etext, end addr to avoid strstr...)
	 */
	if (strstr(info->dlpi_name, "libruntime.so")) {
		return 0;
	}
	DBG_PRNT("dynlib: %s\n", info->dlpi_name);

	for (int i = 0; i < info->dlpi_phnum; i++) {
		const ElfW(Phdr) *phdr = &info->dlpi_phdr[i];
		/*
		 * further optimization: find sections in
		 * GNU_RELRO and remove them from scanning since
		 * they're going to be made READ ONLY
		 */

		// data could only be found in a loadable and
		// writable segment
		if (phdr->p_type != PT_LOAD || (phdr->p_flags & PF_W) == 0) {
			continue;
		}

		uint32_t cnt = data_segments->cnt;
		if (cnt >= data_segments->len) {
			char *err_msg = "dynlibs_data_cb: "
					"dynlibs cnt >= MAX\n";
			write(STDERR_FILENO, err_msg, strlen(err_msg));
			return 1;
		}

		/* segment most likely containing .data and/or .bss */
		void *data_start = (void *)(info->dlpi_addr + phdr->p_vaddr);
		void *data_end = (void *)(data_start + phdr->p_memsz);
		DBG_PRNT("LOADING REGION: %p - %p\n", data_start, data_end);

		data_segments->start[cnt] = PTR_ALIGN_UP(data_start);
		data_segments->end[cnt] = PTR_ALIGN_DOWN(data_end);
		data_segments->cnt = ++cnt;
	}
	return 0;
}

static int trace_roots(struct stack_region_s *safe_stack)
{
	/* GLOBAL DATA SECTION */
	static struct mem_regions_s data_segments = {.len = MAX_SEGMENTS};
	data_segments.cnt = 0;
	int ret;
	ret = dl_iterate_phdr(dynlibs_data_cb, (void *)&data_segments);
	if (ret) {
		return EXIT_FAILURE;
	}
	struct stack_s worklist;
	ret = stack_init(heap, &worklist);
	if (ret) {
		return EXIT_FAILURE;
	}

	DBG_PRNT("GLOBAL DATA SECTION:\n");
	for (size_t i = 0; i < data_segments.cnt; i++) {
		/*DBG_PRNT("start: %p\tend: %p\n",
		 * roots_data.start[i],*/
		/*	roots_data.end[i]);*/
		ret = mark_from_region(&worklist, data_segments.start[i],
				       data_segments.end[i]);
		if (ret) {
			return EXIT_FAILURE;
		}
	}

	/* STACK SECTION */
	DBG_PRNT("SAFE STACK SECTION: %p - %p\n", safe_stack->top,
		 safe_stack->bottom);
	ret = mark_from_region(&worklist, safe_stack->top, safe_stack->bottom);
	if (ret) {
		return EXIT_FAILURE;
	}

	ret = mark(&worklist);
	if (ret) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int sweep(void)
{
	static const uint8_t UNREACHABLE_THRESHOLD = 1;
	for (size_t i = 0; i < book_cnt; i++) {
		if (book[i].addr == 0) {
			continue;
		}
		if (IS_MARKED(book[i].addr)) {
			/* unmark */
			book[i].addr = UNTAG(book[i].addr);
			continue;
		}
		if (!book[i].requested_free ||
		    book[i].unreachable_cnt < UNREACHABLE_THRESHOLD) {
			book[i].unreachable_cnt++;
			continue;
		}

		/* current object is garbage, and was requested
		 * to be freed  */
		mi_free((void *)book[i].addr);
		book_del_entry((void *)book[i].addr);
		actual_frees_cnt++;
	}
	return EXIT_SUCCESS;
}

int bookkeeper_request_free(void *ptr, struct stack_region_s *safe_stack)
{
	/* for compatibility with actual free, see `man 3 free` */
	if (ptr == NULL) {
		return EXIT_SUCCESS;
	}
	/*
	 * EACH FREE SHOULD GO TO A REQUEST LIST, WHEN THE LIST
	 * FILLS UP, SCAN MEMORY TO CHECK WHICH OF THOSE OBJECTS
	 * ARE SAFE TO FREE. But for now simply add flag on each
	 * object...
	 */
	free_requests_cnt++;
	static size_t requests = 0;
	const int threshold = 1;
	DBG_PRNT("stack_top: %p\n", safe_stack->top);
	DBG_PRNT("stack_bottom: %p\n", safe_stack->bottom);

	bool found_object = false;
	for (size_t i = 0; i < book_cnt; i++) {
		if (book[i].addr == 0 || book[i].addr != (uintptr_t)ptr) {
			continue;
		}
		book[i].requested_free = true;
		found_object = true;
		break;
	}

	if (!found_object) {
		/* is this success or fail? */
		DBG_PRNT("no object stored at addr: %p\n", ptr);
	}

	if (requests++ % threshold != 0) {
		return EXIT_SUCCESS;
	}

	trace_roots(safe_stack);
	sweep();

	return EXIT_SUCCESS;
}

void bookkeeper_purge_all(void)
{
	for (size_t i = 0; i < book_cnt; i++) {
		if (book[i].addr == 0) {
			continue;
		}
		book[i].addr = UNTAG(book[i].addr);
		mi_free((void *)book[i].addr);
		book_del_entry((void *)book[i].addr);
	}
}

/*
 * I've read that fprintf PROBABLY won't call malloc under the
 * hood...
 */
void bookkeeper_dump(void)
{
	/* these are NOT DBG_PRNT since calling this function means
	 * you want these values... */
	fprintf(stderr, "bookkeeper_dump:\n");
	fprintf(stderr, "free requests count: %lu\n", free_requests_cnt);
	fprintf(stderr, "actual frees count: %lu\n", actual_frees_cnt);
	fprintf(stderr, "addr:\t\tsize:\n");
	for (size_t i = 0; i < book_cnt; i++) {
		if (book[i].addr == 0) {
			// empty slot
			continue;
		}
		void *addr = (void *)book[i].addr;
		char *is_tagged = "NOT_TAGGED";
		if (IS_MARKED(addr)) {
			is_tagged = "TAGGED";
		}
		fprintf(stderr, "%p\t%u", (void *)UNTAG(addr), book[i].size);
		fprintf(stderr, "\t%s\n", is_tagged);
	}
}
