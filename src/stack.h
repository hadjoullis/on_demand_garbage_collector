#ifndef STACK_H
#define STACK_H

#include <mimalloc.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

/* ONLY ALLOCATE MEMORY FOR THE ARRAY, STACK IS NOT RESPONSIBLE FOR VALUE 
 * ALLOCATION */

#define INIT_LEN 1024

struct stack_s {
        void **stack;
        int32_t size;
        int32_t top;
};

int stack_init(mi_heap_t *heap, struct stack_s *s);
int stack_push(mi_heap_t *heap, struct stack_s *s, void *val);
bool stack_is_empty(struct stack_s *s);
void stack_pop(struct stack_s *s, void **retval);
void stack_fini(struct stack_s *s);

#endif
