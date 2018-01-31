#ifndef BABELTRACE_CTF_IR_EVENT_CLASS_INTERNAL_H
#define BABELTRACE_CTF_IR_EVENT_CLASS_INTERNAL_H

/*
 * Babeltrace - CTF IR: Event class internal
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

#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/values.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/event-class.h>
#include <babeltrace/object-internal.h>
#include <glib.h>

struct bt_event_class {
	struct bt_object base;
	/* Structure type containing the event's context */
	struct bt_field_type *context;
	/* Structure type containing the event's fields */
	struct bt_field_type *fields;
	int frozen;

	/*
	 * This flag indicates if the event class is valid. A valid
	 * event class is _always_ frozen. However, an event class
	 * may be frozen, but not valid yet. This is okay, as long as
	 * no events are created out of this event class.
	 */
	int valid;

	/* Attributes */
	GString *name;
	int64_t id;
	enum bt_event_class_log_level log_level;
	GString *emf_uri;
};

BT_HIDDEN
void bt_event_class_freeze(struct bt_event_class *event_class);

BT_HIDDEN
int bt_event_class_serialize(struct bt_event_class *event_class,
		struct metadata_context *context);

BT_HIDDEN
void bt_event_class_set_native_byte_order(
		struct bt_event_class *event_class,
		int byte_order);

static inline
struct bt_stream_class *bt_event_class_borrow_stream_class(
		struct bt_event_class *event_class)
{
	assert(event_class);
	return (void *) bt_object_borrow_parent(event_class);
}

static inline
const char *bt_event_class_log_level_string(
		enum bt_event_class_log_level level)
{
	switch (level) {
	case BT_EVENT_CLASS_LOG_LEVEL_UNKNOWN:
		return "BT_EVENT_CLASS_LOG_LEVEL_UNKNOWN";
	case BT_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED:
		return "BT_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED";
	case BT_EVENT_CLASS_LOG_LEVEL_EMERGENCY:
		return "BT_EVENT_CLASS_LOG_LEVEL_EMERGENCY";
	case BT_EVENT_CLASS_LOG_LEVEL_ALERT:
		return "BT_EVENT_CLASS_LOG_LEVEL_ALERT";
	case BT_EVENT_CLASS_LOG_LEVEL_CRITICAL:
		return "BT_EVENT_CLASS_LOG_LEVEL_CRITICAL";
	case BT_EVENT_CLASS_LOG_LEVEL_ERROR:
		return "BT_EVENT_CLASS_LOG_LEVEL_ERROR";
	case BT_EVENT_CLASS_LOG_LEVEL_WARNING:
		return "BT_EVENT_CLASS_LOG_LEVEL_WARNING";
	case BT_EVENT_CLASS_LOG_LEVEL_NOTICE:
		return "BT_EVENT_CLASS_LOG_LEVEL_NOTICE";
	case BT_EVENT_CLASS_LOG_LEVEL_INFO:
		return "BT_EVENT_CLASS_LOG_LEVEL_INFO";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM:
		return "BT_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM:
		return "BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS:
		return "BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE:
		return "BT_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT:
		return "BT_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION:
		return "BT_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE:
		return "BT_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG:
		return "BT_EVENT_CLASS_LOG_LEVEL_DEBUG";
	default:
		return "(unknown)";
	}
};

BT_HIDDEN
int bt_event_class_validate_single_clock_class(
		struct bt_event_class *event_class,
		struct bt_clock_class **expected_clock_class);

#endif /* BABELTRACE_CTF_IR_EVENT_CLASS_INTERNAL_H */
