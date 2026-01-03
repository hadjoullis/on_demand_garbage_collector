#ifndef SAFE_BLOCKS_H
#define SAFE_BLOCKS_H

#include <assert.h>
#include <stdint.h>

/* all symbols are weak so that compiler doesn't NEED to resolve their
 * definition during link time. This makes it very easy to include in other
 * projects since the header file can simply be included and call the functions
 * without requiring special configuration during compilation of said project.
 * In addition, since they're weak, we need to check before calling... */

__attribute__((weak)) void enter_safe_block(void *safe_stack_bottom);
__attribute__((weak)) void exit_safe_block(void);
__attribute__((weak)) void purge_safe_block(void);
__attribute__((weak)) void set_exempt(void);
__attribute__((weak)) void unset_exempt(void);

#define ENTER_SAFE_BLOCK                                                       \
	do {                                                                   \
		void *safe_stack_bottom;                                       \
		safe_stack_bottom = __builtin_frame_address(0);                \
		if (enter_safe_block) {                                        \
			enter_safe_block(safe_stack_bottom);                   \
		}                                                              \
	} while (0)

#define EXIT_SAFE_BLOCK                                                        \
	do {                                                                   \
		if (exit_safe_block) {                                         \
			exit_safe_block();                                     \
		}                                                              \
	} while (0)

#define PURGE_BLOCK                                                            \
	do {                                                                   \
		if (purge_safe_block) {                                        \
			purge_safe_block();                                    \
		}                                                              \
	} while (0)

#define EXEMPT(foo)                                                            \
	do {                                                                   \
		if (set_exempt) {                                              \
			set_exempt();                                          \
		}                                                              \
		foo;                                                           \
		if (unset_exempt) {                                            \
			unset_exempt();                                        \
		}                                                              \
	} while (0)

#endif
