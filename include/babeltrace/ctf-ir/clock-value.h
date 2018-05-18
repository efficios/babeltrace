#ifndef BABELTRACE_CTF_IR_CLOCK_VALUE_H
#define BABELTRACE_CTF_IR_CLOCK_VALUE_H

/*
 * BabelTrace - CTF IR: Clock class
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

/* For bt_get() */
#include <babeltrace/ref.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_clock_class;
struct bt_clock_value;

extern struct bt_clock_class *bt_clock_value_borrow_class(
		struct bt_clock_value *clock_value);

static inline
struct bt_clock_class *bt_clock_value_get_class(
		struct bt_clock_value *clock_value)
{
	return bt_get(bt_clock_value_borrow_class(clock_value));
}

extern int bt_clock_value_set_value(
		struct bt_clock_value *clock_value, uint64_t raw_value);
extern int bt_clock_value_get_value(
		struct bt_clock_value *clock_value, uint64_t *raw_value);
extern int bt_clock_value_get_value_ns_from_epoch(
		struct bt_clock_value *clock_value, int64_t *value_ns);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_CLOCK_VALUE_H */
