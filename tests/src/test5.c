#include "safe_blocks.h"
#include <stdlib.h>

int main()
{
        ENTER_SAFE_BLOCK;
        char *arr;
        EXEMPT(arr = malloc(0x100));

        EXEMPT(free(arr));
        EXIT_SAFE_BLOCK;
}
