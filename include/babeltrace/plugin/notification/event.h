#ifndef BABELTRACE_PLUGIN_NOTIFICATION_EVENT_H
#define BABELTRACE_PLUGIN_NOTIFICATION_EVENT_H

/*
 * BabelTrace - Plug-in Event Notification
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
struct bt_ctf_stream;
struct bt_ctf_event;

/**
 * Create an event notification.
 *
 * @param trace			The event's trace
 * @param stream		The event's stream
 * @param event			The event
 * @returns			An event notification instance
 *
 * @see #bt_notification_type
 */
extern struct bt_notification *bt_notification_event_create(
		struct bt_ctf_trace *trace, struct bt_ctf_stream *stream,
		struct bt_ctf_event *event);

/**
 * Get an event notification's associated trace.
 *
 * @param notification	Event notification instance
 * @returns		A trace instance
 *
 * @see #bt_ctf_trace
 */
extern struct bt_ctf_trace *bt_notification_event_get_trace(
		struct bt_notification *notification);

/**
 * Get an event notification's associated stream.
 *
 * @param notification	Event notification instance
 * @returns		A stream instance
 *
 * @see #bt_ctf_stream
 */
extern struct bt_ctf_stream *bt_notification_event_get_stream(
		struct bt_notification *notification);

/**
 * Get an event notification's event.
 *
 * @param notification	Event notification instance
 * @returns		An event instance
 *
 * @see #bt_ctf_event
 */
extern struct bt_ctf_event *bt_notification_event_get_event(
		struct bt_notification *notification);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PLUGIN_NOTIFICATION_EVENT_H */
