#ifndef BABELTRACE_CTF_WRITER_CLOCK_CLASS_INTERNAL_H
#define BABELTRACE_CTF_WRITER_CLOCK_CLASS_INTERNAL_H

/*
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

#include <babeltrace/ctf-writer/object-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/object-pool-internal.h>
#include <babeltrace/compat/uuid-internal.h>
#include <babeltrace/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <glib.h>

struct bt_ctf_clock_class {
	struct bt_ctf_object base;
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
void bt_ctf_clock_class_freeze(struct bt_ctf_clock_class *clock_class);

BT_HIDDEN
bt_bool bt_ctf_clock_class_is_valid(struct bt_ctf_clock_class *clock_class);

BT_HIDDEN
int bt_ctf_clock_class_compare(struct bt_ctf_clock_class *clock_class_a,
		struct bt_ctf_clock_class *clock_class_b);

BT_HIDDEN
struct bt_ctf_clock_class *bt_ctf_clock_class_create(const char *name,
		uint64_t freq);
BT_HIDDEN
const char *bt_ctf_clock_class_get_name(
		struct bt_ctf_clock_class *clock_class);
BT_HIDDEN
int bt_ctf_clock_class_set_name(struct bt_ctf_clock_class *clock_class,
		const char *name);
BT_HIDDEN
const char *bt_ctf_clock_class_get_description(
		struct bt_ctf_clock_class *clock_class);
BT_HIDDEN
int bt_ctf_clock_class_set_description(
		struct bt_ctf_clock_class *clock_class,
		const char *desc);
BT_HIDDEN
uint64_t bt_ctf_clock_class_get_frequency(
		struct bt_ctf_clock_class *clock_class);
BT_HIDDEN
int bt_ctf_clock_class_set_frequency(
		struct bt_ctf_clock_class *clock_class, uint64_t freq);
BT_HIDDEN
uint64_t bt_ctf_clock_class_get_precision(
		struct bt_ctf_clock_class *clock_class);
BT_HIDDEN
int bt_ctf_clock_class_set_precision(
		struct bt_ctf_clock_class *clock_class, uint64_t precision);
BT_HIDDEN
int bt_ctf_clock_class_get_offset_s(
		struct bt_ctf_clock_class *clock_class, int64_t *seconds);
BT_HIDDEN
int bt_ctf_clock_class_set_offset_s(
		struct bt_ctf_clock_class *clock_class, int64_t seconds);
BT_HIDDEN
int bt_ctf_clock_class_get_offset_cycles(
		struct bt_ctf_clock_class *clock_class, int64_t *cycles);
BT_HIDDEN
int bt_ctf_clock_class_set_offset_cycles(
		struct bt_ctf_clock_class *clock_class, int64_t cycles);
BT_HIDDEN
bt_bool bt_ctf_clock_class_is_absolute(
		struct bt_ctf_clock_class *clock_class);
BT_HIDDEN
int bt_ctf_clock_class_set_is_absolute(
		struct bt_ctf_clock_class *clock_class, bt_bool is_absolute);
BT_HIDDEN
const unsigned char *bt_ctf_clock_class_get_uuid(
		struct bt_ctf_clock_class *clock_class);
BT_HIDDEN
int bt_ctf_clock_class_set_uuid(struct bt_ctf_clock_class *clock_class,
		const unsigned char *uuid);

#endif /* BABELTRACE_CTF_WRITER_CLOCK_CLASS_INTERNAL_H */
