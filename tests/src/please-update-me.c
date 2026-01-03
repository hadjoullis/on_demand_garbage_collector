#include "safe_blocks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

struct auth {
	char name[32];
	int auth;
} auth;

struct auth *authVar;
char *service;

int main(int argc, char **argv)
{
	enter_safe_block();
	char line[128];

	while (1) {
		printf("[ auth = %p, service = %p ]\n", authVar, service);

		if (fgets(line, sizeof(line), stdin) == NULL)
			break;

		if (strncmp(line, "auth ", 5) == 0) {
			authVar = malloc(sizeof(auth));
			memset(authVar, 0, sizeof(auth));
			if (strlen(line + 5) < 31) {
				strcpy(authVar->name, line + 5);
			}
		}
		if (strncmp(line, "reset", 5) == 0) {
			free(authVar);
		}
		if (strncmp(line, "service", 6) == 0) {
			/*service = strdup(line + 7);*/
			/*int i = 0;*/
			/*while ((void *)service != (void *)authVar) {*/
			/*	service = strdup(line + 7);*/
			/*	i++;*/
			/*}*/
			while ((void *)service != (void *)authVar) {
				service = malloc(sizeof(auth) - 1);
			}
                        strcpy(service, line + 7);
		}
		if (strncmp(line, "login", 5) == 0) {
			if (authVar->auth) {
				fprintf(stderr, "REPORT_UAF_OCCURED_REPORT\n");
			} else {
				printf("please enter your password\n");
			}
		}
	}
	exit_safe_block();
}
