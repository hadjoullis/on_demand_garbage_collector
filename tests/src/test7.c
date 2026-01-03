#include "safe_blocks.h"
#include <stdio.h>
#include <stdlib.h>

FILE *fp;
void close_stdout()
{
	/*fclose(stdout);*/
	fclose(fp);
}

int main()
{
	/*atexit(close_stdout);*/
	FILE *fp;
	fp = fopen("/tmp/temp", "w");
	if (fp == NULL) {
		perror("file");
		return 1;
	}

	ENTER_SAFE_BLOCK;
	EXEMPT(fprintf(fp, "I will allocate a buffer, but will not flush it"));
	/*fflush(stdout);*/

	EXIT_SAFE_BLOCK;
	fprintf(stderr, "%p\n", fp->_IO_buf_base);
	fclose(fp);
	/*fflush(stdout);*/
}
