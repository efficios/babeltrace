#ifndef BABELTRACE_CTF_WRITER_EVENT_H
#define BABELTRACE_CTF_WRITER_EVENT_H

/*
 * BabelTrace - CTF Writer: Event
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#include <babeltrace/object.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_event;
struct bt_ctf_event_class;
struct bt_ctf_stream;
struct bt_ctf_field;
struct bt_ctf_field_type;

enum bt_ctf_event_class_log_level {
	/// Unknown, used for errors.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_UNKNOWN		= -1,

	/// Unspecified log level.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED	= 255,

	/// System is unusable.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_EMERGENCY		= 0,

	/// Action must be taken immediately.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_ALERT		= 1,

	/// Critical conditions.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_CRITICAL		= 2,

	/// Error conditions.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_ERROR		= 3,

	/// Warning conditions.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_WARNING		= 4,

	/// Normal, but significant, condition.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_NOTICE		= 5,

	/// Informational message.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_INFO		= 6,

	/// Debug information with system-level scope (set of programs).
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM	= 7,

	/// Debug information with program-level scope (set of processes).
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM	= 8,

	/// Debug information with process-level scope (set of modules).
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS	= 9,

	/// Debug information with module (executable/library) scope (set of units).
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE	= 10,

	/// Debug information with compilation unit scope (set of functions).
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT		= 11,

	/// Debug information with function-level scope.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION	= 12,

	/// Debug information with line-level scope (default log level).
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE		= 13,

	/// Debug-level message.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG		= 14,
};

extern struct bt_ctf_event *bt_ctf_event_create(
		struct bt_ctf_event_class *event_class);

extern struct bt_ctf_field *bt_ctf_event_get_payload(struct bt_ctf_event *event,
		const char *name);

extern int bt_ctf_event_set_payload(struct bt_ctf_event *event,
		const char *name, struct bt_ctf_field *field);

extern struct bt_ctf_field *bt_ctf_event_get_payload_field(
		struct bt_ctf_event *event);

extern int bt_ctf_event_set_payload_field(struct bt_ctf_event *event,
		struct bt_ctf_field *field);

extern int bt_ctf_event_set_context(struct bt_ctf_event *event,
		struct bt_ctf_field *field);

extern struct bt_ctf_field *bt_ctf_event_get_context(
		struct bt_ctf_event *event);

extern int bt_ctf_event_set_stream_event_context(struct bt_ctf_event *event,
		struct bt_ctf_field *field);

extern struct bt_ctf_field *bt_ctf_event_get_stream_event_context(
		struct bt_ctf_event *event);

extern int bt_ctf_event_set_header(struct bt_ctf_event *event,
		struct bt_ctf_field *field);

extern struct bt_ctf_field *bt_ctf_event_get_header(
		struct bt_ctf_event *event);

extern struct bt_ctf_stream *bt_ctf_event_get_stream(
		struct bt_ctf_event *event);

extern struct bt_ctf_event_class *bt_ctf_event_get_class(
		struct bt_ctf_event *event);

/* Pre-2.0 CTF writer compatibility */
static inline
void bt_ctf_event_get(struct bt_ctf_event *event)
{
	bt_object_get_ref(event);
}

/* Pre-2.0 CTF writer compatibility */
static inline
void bt_ctf_event_put(struct bt_ctf_event *event)
{
	bt_object_put_ref(event);
}

extern struct bt_ctf_event_class *bt_ctf_event_class_create(const char *name);

extern struct bt_ctf_stream_class *bt_ctf_event_class_get_stream_class(
		struct bt_ctf_event_class *event_class);

extern const char *bt_ctf_event_class_get_name(
		struct bt_ctf_event_class *event_class);

extern int64_t bt_ctf_event_class_get_id(
		struct bt_ctf_event_class *event_class);

extern int bt_ctf_event_class_set_id(
		struct bt_ctf_event_class *event_class, uint64_t id);

extern enum bt_ctf_event_class_log_level bt_ctf_event_class_get_log_level(
		struct bt_ctf_event_class *event_class);

extern int bt_ctf_event_class_set_log_level(
		struct bt_ctf_event_class *event_class,
		enum bt_ctf_event_class_log_level log_level);

extern const char *bt_ctf_event_class_get_emf_uri(
		struct bt_ctf_event_class *event_class);

extern int bt_ctf_event_class_set_emf_uri(
		struct bt_ctf_event_class *event_class,
		const char *emf_uri);

extern struct bt_ctf_field_type *bt_ctf_event_class_get_context_field_type(
		struct bt_ctf_event_class *event_class);

extern int bt_ctf_event_class_set_context_field_type(
		struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *context_type);

extern struct bt_ctf_field_type *bt_ctf_event_class_get_payload_field_type(
		struct bt_ctf_event_class *event_class);

extern int bt_ctf_event_class_set_payload_field_type(
		struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *payload_type);

extern int bt_ctf_event_class_add_field(struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *field_type,
		const char *name);

extern struct bt_ctf_field_type *bt_ctf_event_class_get_field_by_name(
		struct bt_ctf_event_class *event_class, const char *name);

/* Pre-2.0 CTF writer compatibility */
static inline
void bt_ctf_event_class_get(struct bt_ctf_event_class *event_class)
{
	bt_object_get_ref(event_class);
}

/* Pre-2.0 CTF writer compatibility */
static inline
void bt_ctf_event_class_put(struct bt_ctf_event_class *event_class)
{
	bt_object_put_ref(event_class);
}

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_WRITER_EVENT_H */
