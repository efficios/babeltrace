#ifndef BABELTRACE_COMPONENT_NOTIFICATION_SCHEMA_H
#define BABELTRACE_COMPONENT_NOTIFICATION_SCHEMA_H

/*
 * BabelTrace - Plug-in Schema Change Notification
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#ifdef __cplusplus
extern "C" {
#endif

struct bt_notification;
struct bt_ctf_trace;
struct bt_ctf_stream_class;
struct bt_ctf_event_class;

/* BT_NOTIFICATION_TYPE_NEW_TRACE */
/**
 * Create a new trace notification.
 *
 * @param trace			The new trace
 * @returns			A new trace notification instance
 *
 * @see #bt_notification_type
 */
extern struct bt_notification *bt_notification_new_trace_create(
		struct bt_ctf_trace *trace);

/**
 * Get a new trace notification's associated trace.
 *
 * @param notification	New trace notification instance
 * @returns		A trace instance
 *
 * @see #bt_ctf_trace
 */
extern struct bt_ctf_trace *bt_notification_new_trace_get_trace(
		struct bt_notification *notification);


/* BT_NOTIFICATION_TYPE_NEW_STREAM_CLASS */
/**
 * Create a new stream class notification.
 *
 * @param trace			The event's trace
 * @returns			A new stream class notification instance
 *
 * @see #bt_notification_type
 */
extern struct bt_notification *bt_notification_new_stream_class_create(
		struct bt_ctf_stream_class *stream_class);

/**
 * Get a new stream class notification's associated stream class.
 *
 * @param notification	New stream class notification instance
 * @returns		A stream class instance
 *
 * @see #bt_ctf_stream_class
 */
extern struct bt_ctf_trace *bt_notification_new_stream_class_get_stream_class(
		struct bt_notification *notification);


/* BT_NOTIFICATION_TYPE_EVENT_CLASS */
/**
 * Create a new trace notification.
 *
 * @param trace			The event's trace
 * @returns			An event notification instance
 *
 * @see #bt_notification_type
 */
extern struct bt_notification *bt_notification_new_trace_create(
		struct bt_ctf_trace *trace);

/**
 * Get a new trace notification's associated trace.
 *
 * @param notification	New trace notification instance
 * @returns		A trace instance
 *
 * @see #bt_ctf_trace
 */
extern struct bt_ctf_trace *bt_notification_new_trace_get_trace(
		struct bt_notification *notification);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_COMPONENT_NOTIFICATION_SCHEMA_H */
