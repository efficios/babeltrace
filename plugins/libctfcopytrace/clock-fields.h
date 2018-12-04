#ifndef BABELTRACE_CLOCK_FIELDS_H
#define BABELTRACE_CLOCK_FIELDS_H

/*
 * BabelTrace - Update clock fields to write uint64 values
 *
 * Copyright 2017 Julien Desfossez <jdesfossez@efficios.com>
 *
 * Author: Julien Desfossez <jdesfossez@efficios.com>
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

#include <stdbool.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/babeltrace.h>

#ifdef __cplusplus
extern "C" {
#endif

BT_HIDDEN
struct bt_field_type *override_header_type(FILE *err,
		struct bt_field_type *type,
		const struct bt_trace *writer_trace);

BT_HIDDEN
int copy_override_field(FILE *err, const struct bt_event *event,
		const struct bt_event *writer_event, const struct bt_field *field,
		const struct bt_field *copy_field);

BT_HIDDEN
const struct bt_clock_class *stream_class_get_clock_class(FILE *err,
		const struct bt_stream_class *stream_class);

BT_HIDDEN
const struct bt_clock_class *event_get_clock_class(FILE *err,
		const struct bt_event *event);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CLOCK_FIELDS_H */
