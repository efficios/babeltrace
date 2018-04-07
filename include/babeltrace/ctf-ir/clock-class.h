#ifndef BABELTRACE_CTF_IR_CLOCK_CLASS_H
#define BABELTRACE_CTF_IR_CLOCK_CLASS_H

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

#include <stdint.h>

/* For bt_bool */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_clock_class;
struct bt_clock_value;

extern struct bt_clock_class *bt_clock_class_create(const char *name,
		uint64_t freq);
extern const char *bt_clock_class_get_name(
		struct bt_clock_class *clock_class);
extern int bt_clock_class_set_name(struct bt_clock_class *clock_class,
		const char *name);
extern const char *bt_clock_class_get_description(
		struct bt_clock_class *clock_class);
extern int bt_clock_class_set_description(
		struct bt_clock_class *clock_class,
		const char *desc);
extern uint64_t bt_clock_class_get_frequency(
		struct bt_clock_class *clock_class);
extern int bt_clock_class_set_frequency(
		struct bt_clock_class *clock_class, uint64_t freq);
extern uint64_t bt_clock_class_get_precision(
		struct bt_clock_class *clock_class);
extern int bt_clock_class_set_precision(
		struct bt_clock_class *clock_class, uint64_t precision);
extern int bt_clock_class_get_offset_s(
		struct bt_clock_class *clock_class, int64_t *seconds);
extern int bt_clock_class_set_offset_s(
		struct bt_clock_class *clock_class, int64_t seconds);
extern int bt_clock_class_get_offset_cycles(
		struct bt_clock_class *clock_class, int64_t *cycles);
extern int bt_clock_class_set_offset_cycles(
		struct bt_clock_class *clock_class, int64_t cycles);
extern bt_bool bt_clock_class_is_absolute(
		struct bt_clock_class *clock_class);
extern int bt_clock_class_set_is_absolute(
		struct bt_clock_class *clock_class, bt_bool is_absolute);
extern const unsigned char *bt_clock_class_get_uuid(
		struct bt_clock_class *clock_class);
extern int bt_clock_class_set_uuid(struct bt_clock_class *clock_class,
		const unsigned char *uuid);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_CLOCK_CLASS_H */
