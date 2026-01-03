#include "safe_blocks.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct unicorn_counter {
	int num;
};

int main()
{
        ENTER_SAFE_BLOCK;

	struct unicorn_counter *p_unicorn_counter;
	int *run_calc = malloc(sizeof(int));
	printf("p to p: %p -> %p\n", &run_calc, run_calc);
        fflush(stdout);
	*run_calc = 0;
	free(run_calc);
	/*run_calc = NULL;*/

	/* force second allocation on the dangling pointer
         * tested and found that it takes 512 for UAF to occur
         * */
	int i = 0;
	while ((void *)p_unicorn_counter != (void *)run_calc && i < 2048) {
		/*free(p_unicorn_counter);*/
		p_unicorn_counter = malloc(sizeof(struct unicorn_counter));
		p_unicorn_counter->num = 42;
		i++;
	}
	printf("tried %d times to force UAF\n", i);
	printf("new p: %p\n", p_unicorn_counter);
        fflush(stdout);
	if (run_calc && *run_calc) {
		/*execl("/bin/sh", "/bin/sh", NULL);*/
		fprintf(stderr, "REPORT_UAF_OCCURED_REPORT\n");
	}

        EXIT_SAFE_BLOCK;
}
