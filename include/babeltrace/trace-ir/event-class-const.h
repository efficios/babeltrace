#ifndef BABELTRACE_TRACE_IR_EVENT_CLASS_CONST_H
#define BABELTRACE_TRACE_IR_EVENT_CLASS_CONST_H

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

/* For enum bt_property_availability */
#include <babeltrace/property.h>

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_event_class;
struct bt_field_class;
struct bt_stream_class;

enum bt_event_class_log_level {
	BT_EVENT_CLASS_LOG_LEVEL_EMERGENCY,
	BT_EVENT_CLASS_LOG_LEVEL_ALERT,
	BT_EVENT_CLASS_LOG_LEVEL_CRITICAL,
	BT_EVENT_CLASS_LOG_LEVEL_ERROR,
	BT_EVENT_CLASS_LOG_LEVEL_WARNING,
	BT_EVENT_CLASS_LOG_LEVEL_NOTICE,
	BT_EVENT_CLASS_LOG_LEVEL_INFO,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG,
};

extern const struct bt_stream_class *bt_event_class_borrow_stream_class_const(
		const struct bt_event_class *event_class);

extern const char *bt_event_class_get_name(const struct bt_event_class *event_class);

extern uint64_t bt_event_class_get_id(const struct bt_event_class *event_class);

extern enum bt_property_availability bt_event_class_get_log_level(
		const struct bt_event_class *event_class,
		enum bt_event_class_log_level *log_level);

extern const char *bt_event_class_get_emf_uri(
		const struct bt_event_class *event_class);

extern const struct bt_field_class *
bt_event_class_borrow_specific_context_field_class(
		const struct bt_event_class *event_class);

extern const struct bt_field_class *bt_event_class_borrow_payload_field_class(
		const struct bt_event_class *event_class);

extern void bt_event_class_get_ref(const struct bt_event_class *event_class);

extern void bt_event_class_put_ref(const struct bt_event_class *event_class);

#define BT_EVENT_CLASS_PUT_REF_AND_RESET(_var)		\
	do {						\
		bt_event_class_put_ref(_var);		\
		(_var) = NULL;				\
	} while (0)

#define BT_EVENT_CLASS_MOVE_REF(_var_dst, _var_src)	\
	do {						\
		bt_event_class_put_ref(_var_dst);	\
		(_var_dst) = (_var_src);		\
		(_var_src) = NULL;			\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_EVENT_CLASS_CONST_H */
