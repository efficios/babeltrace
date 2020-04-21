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

/*
 * Copied from:
 * <https://stackoverflow.com/questions/37411809/how-to-elegantly-fix-this-unused-variable-warning/37412551#37412551>:
 *
 * * sizeof() ensures that the expression is not evaluated at all, so
 *   its side-effects don't happen. That is to be consistent with the
 *   usual behaviour of debug-only constructs, such as assert().
 *
 * * `((_expr), 0)` uses the comma operator to swallow the actual type
 *   of `(_expr)`. This is to prevent VLAs from triggering evaluation.
 *
 * * `(void)` explicitly ignores the result of `(_expr)` and sizeof() so
 *   no "unused value" warning appears.
 */

#define BT_USE_EXPR(_expr)		((void) sizeof((void) (_expr), 0))
#define BT_USE_EXPR2(_expr1, _expr2)					\
	((void) sizeof((void) (_expr1), (void) (_expr2), 0))
#define BT_USE_EXPR3(_expr1, _expr2, _expr3)				\
	((void) sizeof((void) (_expr1), (void) (_expr2), (void) (_expr3), 0))
#define BT_USE_EXPR4(_expr1, _expr2, _expr3, _expr4)			\
	((void) sizeof((void) (_expr1), (void) (_expr2),		\
		(void) (_expr3), (void) (_expr4), 0))
#define BT_USE_EXPR5(_expr1, _expr2, _expr3, _expr4, _expr5)		\
	((void) sizeof((void) (_expr1), (void) (_expr2),		\
		(void) (_expr3), (void) (_expr4), (void) (_expr5), 0))

#endif
