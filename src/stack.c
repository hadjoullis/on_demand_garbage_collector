#include "stack.h"

int stack_init(mi_heap_t *heap, struct stack_s *s)
{
	s->stack = mi_heap_malloc(heap, INIT_LEN * sizeof(void *));
	if (s == NULL) {
		perror("stack_init: mi_heap_malloc");
		return EXIT_FAILURE;
	}
	s->size = INIT_LEN;
	s->top = -1;
	return EXIT_SUCCESS;
}

void stack_fini(struct stack_s *s)
{
	mi_free(s->stack);
	s->stack = NULL;
}

int stack_push(mi_heap_t *heap, struct stack_s *s, void *val)
{
	if (s->top + 1 == s->size) {
		size_t newsize = 2 * s->size * sizeof(void *);
		void **tmp = mi_heap_realloc(heap, s->stack, newsize);
		if (tmp == NULL) {
			perror("stack_push: mi_heap_realloc");
			return EXIT_FAILURE;
		}
		s->stack = tmp;
		s->size *= 2;
	}
	s->stack[++s->top] = val;
	return EXIT_SUCCESS;
}

bool stack_is_empty(struct stack_s *s)
{
	return s->top <= -1;
}

void stack_pop(struct stack_s *s, void **retval)
{
	/* will not check if stack is not empty */
	*retval = s->stack[s->top--];
}
