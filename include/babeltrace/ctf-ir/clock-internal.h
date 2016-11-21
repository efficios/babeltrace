#ifndef BABELTRACE_CTF_IR_CLOCK_INTERNAL_H
#define BABELTRACE_CTF_IR_CLOCK_INTERNAL_H

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

#include <babeltrace/ctf-writer/clock.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <glib.h>
#include <babeltrace/compat/uuid.h>

struct bt_ctf_clock {
	struct bt_object base;
	GString *name;
	GString *description;
	uint64_t frequency;
	uint64_t precision;
	int64_t offset_s;	/* Offset in seconds */
	int64_t offset;		/* Offset in ticks */
	uint64_t value;		/* Current clock value */
	uuid_t uuid;
	int uuid_set;
	int absolute;

	/*
	 * This field is set once a clock is added to a trace. If the
	 * trace was created by a CTF writer, then the clock's value
	 * can be set and returned. Otherwise both functions fail
	 * because, in non-writer mode, clocks do not have global
	 * values: values are per-stream.
	 */
	int has_value;

	/*
	 * A clock's properties can't be modified once it is added to a stream
	 * class.
	 */
	int frozen;
};

struct bt_ctf_clock_value {
	struct bt_object base;
	struct bt_ctf_clock *clock_class;
	uint64_t value;
};

BT_HIDDEN
void bt_ctf_clock_freeze(struct bt_ctf_clock *clock);

BT_HIDDEN
void bt_ctf_clock_serialize(struct bt_ctf_clock *clock,
		struct metadata_context *context);

BT_HIDDEN
bool bt_ctf_clock_is_valid(struct bt_ctf_clock *clock);

BT_HIDDEN
int bt_ctf_clock_get_value(struct bt_ctf_clock *clock, uint64_t *value);

#endif /* BABELTRACE_CTF_IR_CLOCK_INTERNAL_H */
