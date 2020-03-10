/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2012 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 */

#ifndef _BABELTRACE_INTERNAL_H
#define _BABELTRACE_INTERNAL_H

#define bt_max_t(type, a, b)	\
	((type) (a) > (type) (b) ? (type) (a) : (type) (b))

/*
 * BT_HIDDEN: set the hidden attribute for internal functions
 * On Windows, symbols are local unless explicitly exported,
 * see https://gcc.gnu.org/wiki/Visibility
 */
#if defined(_WIN32) || defined(__CYGWIN__)
#define BT_HIDDEN
#else
#define BT_HIDDEN __attribute__((visibility("hidden")))
#endif

/*
 * Yield `ref`'s value while setting `ref` to NULL.
 *
 * This can be used to give a strong reference to a callee:
 *
 *   add_foo_to_list(list, BT_MOVE_REF(foo));
 *
 * or in a simple assignment:
 *
 *   my_struct->foo = BT_MOVE_REF(foo);
 *
 * When moving a reference in a function call, the reference is given to the
 * callee even if that function call fails, so make sure the called function
 * is written accordingly.
 */

#define BT_MOVE_REF(ref) 		\
	({				\
		typeof(ref) _ref = ref;	\
		ref = NULL;		\
		_ref;			\
	})

#endif
