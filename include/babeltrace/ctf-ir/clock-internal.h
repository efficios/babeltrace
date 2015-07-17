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
#include <babeltrace/ctf-ir/common-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <glib.h>
#include <babeltrace/compat/uuid.h>

struct bt_ctf_clock {
	struct bt_ctf_base base;
	GString *name;
	GString *description;
	uint64_t frequency;
	uint64_t precision;
	uint64_t offset_s;	/* Offset in seconds */
	uint64_t offset;	/* Offset in ticks */
	uint64_t time;		/* Current clock value */
	uuid_t uuid;
	int uuid_set;
	int absolute;
	/*
	 * A clock's properties can't be modified once it is added to a stream
	 * class.
	 */
	int frozen;
};

/*
 * This is not part of the public API to prevent users from creating clocks
 * in an invalid state (being nameless, in this case).
 *
 * The only legitimate use-case for this function is to allocate a clock
 * while the TSDL metadata is being parsed.
 */
BT_HIDDEN
struct bt_ctf_clock *_bt_ctf_clock_create(void);

/*
 * Not exposed as part of the public API since the only usecase
 * for this is when we are creating clocks from the TSDL metadata.
 */
BT_HIDDEN
int bt_ctf_clock_set_name(struct bt_ctf_clock *clock,
		const char *name);

BT_HIDDEN
void bt_ctf_clock_freeze(struct bt_ctf_clock *clock);

BT_HIDDEN
void bt_ctf_clock_serialize(struct bt_ctf_clock *clock,
		struct metadata_context *context);

#endif /* BABELTRACE_CTF_IR_CLOCK_INTERNAL_H */
