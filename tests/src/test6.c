#include "safe_blocks.h"
#include <stdio.h>

int main()
{
        ENTER_SAFE_BLOCK;
        printf("I will allocate a buffer, but will not flush it");
        printf("I will allocate a buffer, but will not flush it");
        /*fflush(stdout);*/

        /*fflush(stdout);*/
        fclose(stdout);
        EXIT_SAFE_BLOCK;
}
