#ifndef BABELTRACE_CTF_IR_UTILS_INTERNAL_H
#define BABELTRACE_CTF_IR_UTILS_INTERNAL_H

/*
 * Babeltrace - Internal CTF IR utilities
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

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/ctf-ir/field-types.h>
#include <stdint.h>

#define BT_TO_COMMON(_obj)	(&(_obj)->common)
#define BT_FROM_COMMON(_obj)	((void *) _obj)

struct search_query {
	gpointer value;
	int found;
};

BT_HIDDEN
const char *get_byte_order_string(enum bt_byte_order byte_order);

#endif /* BABELTRACE_CTF_IR_UTILS_INTERNAL_H */
