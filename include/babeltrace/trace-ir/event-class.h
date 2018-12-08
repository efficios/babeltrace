#ifndef BABELTRACE_TRACE_IR_EVENT_CLASS_H
#define BABELTRACE_TRACE_IR_EVENT_CLASS_H

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

/* For enum bt_event_class_status, enum bt_event_class_log_level */
#include <babeltrace/trace-ir/event-class-const.h>

/* For bt_event_class, bt_stream_class */
#include <babeltrace/types.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bt_event_class *bt_event_class_create(
		bt_stream_class *stream_class);

extern bt_event_class *bt_event_class_create_with_id(
		bt_stream_class *stream_class, uint64_t id);

extern bt_stream_class *bt_event_class_borrow_stream_class(
		bt_event_class *event_class);

extern enum bt_event_class_status bt_event_class_set_name(
		bt_event_class *event_class, const char *name);

extern void bt_event_class_set_log_level(
		bt_event_class *event_class,
		enum bt_event_class_log_level log_level);

extern enum bt_event_class_status bt_event_class_set_emf_uri(
		bt_event_class *event_class,
		const char *emf_uri);

extern enum bt_event_class_status
bt_event_class_set_specific_context_field_class(bt_event_class *event_class,
		bt_field_class *field_class);

extern enum bt_event_class_status bt_event_class_set_payload_field_class(
		bt_event_class *event_class,
		bt_field_class *field_class);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_EVENT_CLASS_H */
