#ifndef BABELTRACE2_TRACE_IR_CLOCK_CLASS_H
#define BABELTRACE2_TRACE_IR_CLOCK_CLASS_H

/*
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
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

/* For bt_bool, bt_uuid, bt_clock_class, bt_trace_class */
#include <babeltrace2/types.h>

/* For __BT_FUNC_STATUS_* */
#define __BT_FUNC_STATUS_ENABLE
#include <babeltrace2/func-status.h>
#undef __BT_FUNC_STATUS_ENABLE

#ifdef __cplusplus
extern "C" {
#endif

extern bt_clock_class *bt_clock_class_create(bt_self_component *self_comp);

typedef enum bt_clock_class_set_name_status {
	BT_CLOCK_CLASS_SET_NAME_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_CLOCK_CLASS_SET_NAME_STATUS_OK		= __BT_FUNC_STATUS_OK,
} bt_clock_class_set_name_status;

extern bt_clock_class_set_name_status bt_clock_class_set_name(
		bt_clock_class *clock_class, const char *name);

typedef enum bt_clock_class_set_description_status {
	BT_CLOCK_CLASS_SET_DESCRIPTION_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_CLOCK_CLASS_SET_DESCRIPTION_STATUS_OK		= __BT_FUNC_STATUS_OK,
} bt_clock_class_set_description_status;

extern bt_clock_class_set_description_status bt_clock_class_set_description(
		bt_clock_class *clock_class, const char *description);

extern void bt_clock_class_set_frequency(bt_clock_class *clock_class,
		uint64_t freq);

extern void bt_clock_class_set_precision(bt_clock_class *clock_class,
		uint64_t precision);

extern void bt_clock_class_set_offset(bt_clock_class *clock_class,
		int64_t seconds, uint64_t cycles);

extern void bt_clock_class_set_origin_is_unix_epoch(bt_clock_class *clock_class,
		bt_bool origin_is_unix_epoch);

extern void bt_clock_class_set_uuid(bt_clock_class *clock_class,
		bt_uuid uuid);

#ifdef __cplusplus
}
#endif

#include <babeltrace2/undef-func-status.h>

#endif /* BABELTRACE2_TRACE_IR_CLOCK_CLASS_H */
