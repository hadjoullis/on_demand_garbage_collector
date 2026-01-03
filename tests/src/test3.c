#include "safe_blocks.h"
#include <stdio.h>

int main()
{
        ENTER_SAFE_BLOCK;
        EXEMPT(printf("I will allocate a buffer\n"));
        /*(puts("HELLO"));*/
        /*EXEMPT(puts("HELLO"));*/
        fflush(stdout);

        EXIT_SAFE_BLOCK;
        printf("I will reuse that buffer, and thus cause a segfault...\n");
        /*puts("HELLO AGAIN");*/
        fflush(stdout);
}
