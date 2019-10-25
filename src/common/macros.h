#ifndef _BABELTRACE_INTERNAL_H
#define _BABELTRACE_INTERNAL_H

/*
 * Copyright 2012 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


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
