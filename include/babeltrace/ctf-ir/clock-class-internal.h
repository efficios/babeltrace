#ifndef BABELTRACE_CTF_IR_CLOCK_CLASS_INTERNAL_H
#define BABELTRACE_CTF_IR_CLOCK_CLASS_INTERNAL_H

/*
 * BabelTrace - CTF IR: Clock internal
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
 */

#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/compat/uuid-internal.h>
#include <babeltrace/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <glib.h>

struct bt_clock_class {
	struct bt_object base;
	GString *name;
	GString *description;
	uint64_t frequency;
	uint64_t precision;
	int64_t offset_s;	/* Offset in seconds */
	int64_t offset;		/* Offset in ticks */
	unsigned char uuid[BABELTRACE_UUID_LEN];
	int uuid_set;
	int absolute;

	/*
	 * A clock's properties can't be modified once it is added to a stream
	 * class.
	 */
	int frozen;
};

BT_HIDDEN
void bt_clock_class_freeze(struct bt_clock_class *clock_class);

BT_HIDDEN
void bt_clock_class_serialize(struct bt_clock_class *clock_class,
		struct metadata_context *context);

BT_HIDDEN
bt_bool bt_clock_class_is_valid(struct bt_clock_class *clock_class);

BT_HIDDEN
int bt_clock_class_compare(struct bt_clock_class *clock_class_a,
		struct bt_clock_class *clock_class_b);

#endif /* BABELTRACE_CTF_IR_CLOCK_CLASS_INTERNAL_H */
