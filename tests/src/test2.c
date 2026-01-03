#include "safe_blocks.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef void (*fp)();

void func1()
{
	printf("[*] --------> This is func 1\n");
}

void func_uaf()
{
	fprintf(stderr, "REPORT_UAF_OCCURED_REPORT\n");
}

int main()
{
        ENTER_SAFE_BLOCK;

	fp *pointer1 = malloc(sizeof(fp));
	printf("p to p: %p -> %p\n", &pointer1, pointer1);
        fflush(stdout);
	*pointer1 = func1;
	(*pointer1)();

	printf("[1] pointer1 points to %p\n", pointer1);
	printf("[2] Freeing pointer1\n");
        fflush(stdout);
	free(pointer1);
	fp *pointer2;
	int i = 0;
	/* force second allocation on the dangling pointer 
         * tested and found that it takes 512 for UAF to occur
         * */
	while (pointer1 != pointer2 && i < 2048) {
		pointer2 = malloc(sizeof(fp));
		i++;
	}
	printf("tried %d times to force UAF\n", i);
        fflush(stdout);
	*pointer2 = func_uaf;
	/*(*pointer2)();*/

	printf("[3] pointer2 points to %p\n", pointer2);
	printf("[4] Using pointer1 after free\n");
        fflush(stdout);

	(*pointer1)();

        EXIT_SAFE_BLOCK;

        fflush(NULL);
	return 0;
}
