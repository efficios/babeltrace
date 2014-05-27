#ifndef BABELTRACE_CTF_WRITER_STREAM_H
#define BABELTRACE_CTF_WRITER_STREAM_H

/*
 * BabelTrace - CTF Writer: Stream
 *
 * Copyright 2013 EfficiOS Inc.
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

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_event;
struct bt_ctf_event_class;
struct bt_ctf_stream_class;
struct bt_ctf_stream;
struct bt_ctf_clock;

/*
 * bt_ctf_stream_class_create: create a stream class.
 *
 * Allocate a new stream class of the given name. The creation of an event class
 * sets its reference count to 1.
 *
 * @param name Stream name.
 *
 * Returns an allocated stream class on success, NULL on error.
 */
extern struct bt_ctf_stream_class *bt_ctf_stream_class_create(const char *name);

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
 * bt_ctf_stream_class_set_clock: assign a clock to a stream class.
 *
 * Add an event class to a stream class. New events can be added even after a
 * stream has beem instanciated and events have been appended. However, a stream
 * will not accept events of a class that has not been registered beforehand.
 * The stream class will share the ownership of "event_class" by incrementing
 * its reference count.
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
 * bt_ctf_stream_class_get and bt_ctf_stream_class_put: increment and
 * decrement the stream class' reference count.
 *
 * These functions ensure that the stream class won't be destroyed while it
 * is in use. The same number of get and put (plus one extra put to
 * release the initial reference done at creation) have to be done to
 * destroy a stream class.
 *
 * When the stream class' reference count is decremented to 0 by a
 * bt_ctf_stream_class_put, the stream class is freed.
 *
 * @param stream_class Stream class.
 */
extern void bt_ctf_stream_class_get(struct bt_ctf_stream_class *stream_class);
extern void bt_ctf_stream_class_put(struct bt_ctf_stream_class *stream_class);

/*
 * bt_ctf_stream_append_discarded_events: increment discarded events count.
 *
 * Increase the current packet's discarded event count.
 *
 * @param stream Stream instance.
 * @param event_count Number of discarded events to add to the stream's current
 *	packet.
 */
extern void bt_ctf_stream_append_discarded_events(struct bt_ctf_stream *stream,
		uint64_t event_count);

/*
 * bt_ctf_stream_append_event: append an event to the stream.
 *
 * Append "event" to the stream's current packet. The stream's associated clock
 * will be sampled during this call. The event shall not be modified after
 * being appended to a stream. The stream will share the event's ownership by
 * incrementing its reference count. The current packet is not flushed to disk
 * until the next call to bt_ctf_stream_flush.
 *
 * @param stream Stream instance.
 * @param event Event instance to append to the stream's current packet.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_stream_append_event(struct bt_ctf_stream *stream,
		struct bt_ctf_event *event);

/*
 * bt_ctf_stream_flush: flush a stream.
 *
 * The stream's current packet's events will be flushed to disk. Events
 * subsequently appended to the stream will be added to a new packet.
 *
 * @param stream Stream instance.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_stream_flush(struct bt_ctf_stream *stream);

/*
 * bt_ctf_stream_get and bt_ctf_stream_put: increment and decrement the
 * stream's reference count.
 *
 * These functions ensure that the stream won't be destroyed while it
 * is in use. The same number of get and put (plus one extra put to
 * release the initial reference done at creation) have to be done to
 * destroy a stream.
 *
 * When the stream's reference count is decremented to 0 by a
 * bt_ctf_stream_put, the stream is freed.
 *
 * @param stream Stream instance.
 */
extern void bt_ctf_stream_get(struct bt_ctf_stream *stream);
extern void bt_ctf_stream_put(struct bt_ctf_stream *stream);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_WRITER_STREAM_H */
