#ifndef BABELTRACE_TRACE_IR_CLOCK_VALUE_CONST_H
#define BABELTRACE_TRACE_IR_CLOCK_VALUE_CONST_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_clock_class;
struct bt_clock_value;

enum bt_clock_value_status {
	BT_CLOCK_VALUE_STATUS_KNOWN,
	BT_CLOCK_VALUE_STATUS_UNKNOWN,
};

extern const struct bt_clock_class *bt_clock_value_borrow_clock_class_const(
		const struct bt_clock_value *clock_value);

extern uint64_t bt_clock_value_get_value(
		const struct bt_clock_value *clock_value);

extern int bt_clock_value_get_ns_from_origin(
		const struct bt_clock_value *clock_value,
		int64_t *ns_from_origin);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_CLOCK_VALUE_CONST_H */
