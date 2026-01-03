#include "safe_blocks.h"
#include <stdio.h>
#include <stdlib.h>

int main()
{
	ENTER_SAFE_BLOCK;
	size_t size = 10000;
	int *safe_ptr = malloc(size * sizeof(int));
	if (safe_ptr == NULL) {
		perror("safe_array alloc");
		return EXIT_FAILURE;
	}
	EXIT_SAFE_BLOCK;

	int ptrs = 50;
	int **unsafe_ptrs = malloc(ptrs * sizeof(int *));
	if (unsafe_ptrs == NULL) {
		perror("unsafe_array alloc");
		return EXIT_FAILURE;
	}
	for (size_t i = 0; i < ptrs; i++) {
		unsafe_ptrs[i] = malloc(size * sizeof(int));
		if (unsafe_ptrs + i == NULL) {
			perror("unsafe_array alloc");
			return EXIT_FAILURE;
		}
	}

	ENTER_SAFE_BLOCK;
	EXEMPT(printf("safe ptr:   %p\n", safe_ptr));
	printf("unsafe ptr: %p\n", unsafe_ptrs);
        fflush(stdout);
	EXIT_SAFE_BLOCK;

	/*printf("about to crash...\n");*/
	/*safe_ptr[26] = 15;*/

	ENTER_SAFE_BLOCK;
	printf("safe ptr:   %d\n", safe_ptr[26]);
	printf("unsafe ptr:   %d\n", unsafe_ptrs[3][8]);
        fflush(stdout);
	safe_ptr[26] = 10;
	printf("safe ptr:   %d\n", safe_ptr[26]);
        fflush(stdout);
	EXIT_SAFE_BLOCK;

	printf("HELLO\n");
	return EXIT_SUCCESS;
}
