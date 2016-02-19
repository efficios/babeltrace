#ifndef BABELTRACE_CTF_IR_STREAM_CLASS_H
#define BABELTRACE_CTF_IR_STREAM_CLASS_H

/*
 * BabelTrace - CTF IR: Stream Class
 *
 * Copyright 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_event_class;
struct bt_ctf_stream_class;
struct bt_ctf_clock;

/*
 * bt_ctf_stream_class_create: create a stream class.
 *
 * Allocate a new stream class of the given name. The creation of an event class
 * sets its reference count to 1.
 *
 * A stream class' packet context is a structure initialized with the following
 * fields:
 *	- uint64_t timestamp_begin
 *	- uint64_t timestamp_end
 *	- uint64_t content_size
 *	- uint64_t packet_size
 *	- uint64_t events_discarded
 *
 * A stream class's event header is a structure initialized the following
 * fields:
 *  - uint32_t id
 *  - uint64_t timestamp
 *
 * @param name Stream name, NULL to create an unnamed stream class.
 *
 * Returns an allocated stream class on success, NULL on error.
 */
extern struct bt_ctf_stream_class *bt_ctf_stream_class_create(const char *name);

/*
 * bt_ctf_stream_class_get_trace: Get a stream class' associated trace.
 *
 * @param stream_class Stream class.
 *
 * Returns the stream class' associated trace, NULL on error.
 */
extern struct bt_ctf_trace *bt_ctf_stream_class_get_trace(
		struct bt_ctf_stream_class *stream_class);

/*
 * bt_ctf_stream_class_get_name: Get a stream class' name.
 *
 * @param stream_class Stream class.
 *
 * Returns the stream class' name, NULL on error.
 */
extern const char *bt_ctf_stream_class_get_name(
		struct bt_ctf_stream_class *stream_class);

/*
 * bt_ctf_stream_class_set_name: Set a stream class' name.
 *
 * @param stream_class Stream class.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_stream_class_set_name(
		struct bt_ctf_stream_class *stream_class, const char *name);

/*
 * bt_ctf_stream_class_get_clock: get the clock associated with a stream class.
 *
 * @param stream_class Stream class.
 *
 * Returns a clock instance, NULL on error.
 */
extern struct bt_ctf_clock *bt_ctf_stream_class_get_clock(
		struct bt_ctf_stream_class *stream_class);

/*
 * bt_ctf_stream_class_set_clock: assign a clock to a stream class.
 *
 * Assign a clock to a stream class. This clock will be sampled each time an
 * event is appended to an instance of this stream class.
 *
 * @param stream_class Stream class.
 * @param clock Clock to assign to the provided stream class.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_stream_class_set_clock(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_clock *clock);

/*
 * bt_ctf_stream_class_get_id: Get a stream class' id.
 *
 * @param stream_class Stream class.
 *
 * Returns the stream class' id, a negative value on error.
 */
extern int64_t bt_ctf_stream_class_get_id(
		struct bt_ctf_stream_class *stream_class);

/*
 * bt_ctf_stream_class_set_id: Set a stream class' id.
 *
 * Set a stream class' id. Must be unique trace-wise.
 * Note that stream classes are assigned a unique id when a stream instance
 * is created for the first time from a trace or writer.
 *
 * @param stream_class Stream class.
 * @param id Stream class id.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_stream_class_set_id(
		struct bt_ctf_stream_class *stream_class, uint32_t id);

/*
 * bt_ctf_stream_class_set_clock: assign a clock to a stream class.
 *
 * Add an event class to a stream class. New events can be added even after a
 * stream has beem instanciated and events have been appended. However, a stream
 * will not accept events of a class that has not been registered beforehand.
 * The stream class will share the ownership of "event_class" by incrementing
 * its reference count.
 *
 * Note that an event class may only be added to one stream class. It
 * also becomes immutable.
 *
 * @param stream_class Stream class.
 * @param event_class Event class to add to the provided stream class.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_stream_class_add_event_class(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_event_class *event_class);

/*
 * bt_ctf_stream_class_get_event_class_count: Get a stream class' event class
 * count.
 *
 * @param stream_class Stream class.
 *
 * Returns the stream class' event count, a negative value on error.
 */
extern int bt_ctf_stream_class_get_event_class_count(
		struct bt_ctf_stream_class *stream_class);

/*
 * bt_ctf_stream_class_get_event_class: Get stream class event class by index.
 *
 * @param stream_class Stream class.
 * @param index Index of field.
 *
 * Returns event class, NULL on error.
 */
extern struct bt_ctf_event_class *bt_ctf_stream_class_get_event_class(
		struct bt_ctf_stream_class *stream_class, int index);

/*
 * bt_ctf_stream_class_get_event_class_by_name: Get stream class event class by
 * name.
 *
 * @param stream_class Stream class.
 * @param name Event name.
 *
 * Returns event class, NULL on error.
 */
extern struct bt_ctf_event_class *bt_ctf_stream_class_get_event_class_by_name(
		struct bt_ctf_stream_class *stream_class, const char *name);

/*
 * bt_ctf_stream_class_get_event_class_by_name: Get stream class event class by
 * ID.
 *
 * @param stream_class Stream class.
 * @param id Event class ID.
 *
 * Returns event class, NULL on error.
 */
extern struct bt_ctf_event_class *bt_ctf_stream_class_get_event_class_by_id(
		struct bt_ctf_stream_class *stream_class, uint32_t id);

/*
 * bt_ctf_stream_class_get_packet_context_type: get the stream class' packet
 * context type.
 *
 * @param stream_class Stream class.
 *
 * Returns the packet context's type (a structure), NULL on error.
 */
extern struct bt_ctf_field_type *bt_ctf_stream_class_get_packet_context_type(
		struct bt_ctf_stream_class *stream_class);

/*
 * bt_ctf_stream_class_set_packet_context_type: set the stream class' packet
 * context type.
 *
 * @param stream_class Stream class.
 * @param packet_context_type Packet context type (must be a structure).
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_stream_class_set_packet_context_type(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *packet_context_type);

/*
 * bt_ctf_stream_class_get_event_header_type: get the stream class'
 * event header type.
 *
 * @param stream_class Stream class.
 *
 * Returns the stream event header's type (a structure), NULL on error.
 */
extern struct bt_ctf_field_type *
bt_ctf_stream_class_get_event_header_type(
		struct bt_ctf_stream_class *stream_class);

/*
 * bt_ctf_stream_class_set_event_header_type: set the stream class'
 * event header type.
 *
 * @param stream_class Stream class.
 * @param event_header_type Event header type (must be a structure).
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_stream_class_set_event_header_type(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *event_header_type);

/*
 * bt_ctf_stream_class_get_event_context_type: get the stream class'
 * event context type.
 *
 * @param stream_class Stream class.
 *
 * Returns the stream event context's type (a structure), NULL on error.
 */
extern struct bt_ctf_field_type *
bt_ctf_stream_class_get_event_context_type(
		struct bt_ctf_stream_class *stream_class);

/*
 * bt_ctf_stream_class_set_event_context_type: set the stream class'
 * event context type.
 *
 * @param stream_class Stream class.
 * @param event_context_type Event context type (must be a structure).
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_stream_class_set_event_context_type(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *event_context_type);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_STREAM_CLASS_H */
