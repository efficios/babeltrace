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

/*
 * bt_ctf_clock_get_name: get a clock's name.
 *
 * Get the clock's name.
 *
 * @param clock Clock instance.
 *
 * Returns the clock's name, NULL on error.
 */
BT_HIDDEN
const char *bt_ctf_clock_get_name(struct bt_ctf_clock *clock);

/*
 * bt_ctf_clock_get_description: get a clock's description.
 *
 * Get the clock's description.
 *
 * @param clock Clock instance.
 *
 * Returns the clock's description, NULL if unset.
 */
BT_HIDDEN
const char *bt_ctf_clock_get_description(struct bt_ctf_clock *clock);

/*
 * bt_ctf_clock_get_frequency: get a clock's frequency.
 *
 * Get the clock's frequency (Hz).
 *
 * @param clock Clock instance.
 *
 * Returns the clock's frequency, -1ULL on error.
 */
BT_HIDDEN
uint64_t bt_ctf_clock_get_frequency(struct bt_ctf_clock *clock);

/*
 * bt_ctf_clock_get_precision: get a clock's precision.
 *
 * Get the clock's precision (in clock ticks).
 *
 * @param clock Clock instance.
 *
 * Returns the clock's precision, -1ULL on error.
 */
BT_HIDDEN
uint64_t bt_ctf_clock_get_precision(struct bt_ctf_clock *clock);

/*
 * bt_ctf_clock_get_offset_s: get a clock's offset in seconds.
 *
 * Get the clock's offset in seconds from POSIX.1 Epoch, 1970-01-01.
 *
 * @param clock Clock instance.
 * @param offset_s Pointer to clock offset in seconds (output).
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_clock_get_offset_s(struct bt_ctf_clock *clock,
		int64_t *offset_s);

/*
 * bt_ctf_clock_get_offset: get a clock's offset in ticks.
 *
 * Get the clock's offset in ticks from Epoch + offset_t.
 *
 * @param clock Clock instance.
 * @param offset Clock offset in ticks from Epoch + offset_s (output).
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_clock_get_offset(struct bt_ctf_clock *clock,
		int64_t *offset);

/*
 * bt_ctf_clock_get_is_absolute: get a clock's absolute attribute.
 *
 * Get the clock's absolute attribute. A clock is absolute if the clock is a
 * global reference across the trace's other clocks.
 *
 * @param clock Clock instance.
 *
 * Returns the clock's absolute attribute, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_clock_get_is_absolute(struct bt_ctf_clock *clock);

/*
 * bt_ctf_clock_get_uuid: get a clock's UUID.
 *
 * Get the clock's UUID.
 *
 * @param clock Clock instance.
 *
 * Returns a pointer to the clock's UUID (16 byte array) on success,
 * NULL on error.
 */
BT_HIDDEN
const unsigned char *bt_ctf_clock_get_uuid(struct bt_ctf_clock *clock);

/*
 * bt_ctf_clock_set_uuid: set a clock's UUID.
 *
 * Set a clock's UUID.
 *
 * @param clock Clock instance.
 * @param uuid A 16-byte array containing a UUID.
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_clock_set_uuid(struct bt_ctf_clock *clock,
		const unsigned char *uuid);

BT_HIDDEN
int64_t bt_ctf_clock_ns_from_value(struct bt_ctf_clock *clock,
		uint64_t value);

BT_HIDDEN
uint64_t bt_ctf_clock_get_value(struct bt_ctf_clock *clock);

BT_HIDDEN
int bt_ctf_clock_set_value(struct bt_ctf_clock *clock, uint64_t value);

BT_HIDDEN
int bt_ctf_clock_get_time(struct bt_ctf_clock *clock, int64_t *time);

#endif /* BABELTRACE_CTF_IR_CLOCK_INTERNAL_H */
