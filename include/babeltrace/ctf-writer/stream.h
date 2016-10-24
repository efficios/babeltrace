#ifndef BABELTRACE_CTF_WRITER_STREAM_H
#define BABELTRACE_CTF_WRITER_STREAM_H

/*
 * BabelTrace - CTF Writer: Stream
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

#include <babeltrace/ctf-writer/stream-class.h>

struct bt_ctf_stream;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * bt_ctf_stream_append_discarded_events: increment discarded events count.
 *
 * Increase the current packet's discarded event count. Has no effect if the
 * stream class' packet context has no "events_discarded" field.
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
 * The stream event context will be sampled for every appended event if
 * a stream event context was defined.
 *
 * @param stream Stream instance.
 * @param event Event instance to append to the stream's current packet.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_stream_append_event(struct bt_ctf_stream *stream,
		struct bt_ctf_event *event);

/*
 * bt_ctf_stream_get_packet_context: get a stream's packet context.
 *
 * @param stream Stream instance.
 *
 * Returns a field instance on success, NULL on error.
 */
extern struct bt_ctf_field *bt_ctf_stream_get_packet_context(
		struct bt_ctf_stream *stream);

/*
 * bt_ctf_stream_flush: flush a stream.
 *
 * The stream's current packet's events will be flushed, thus closing the
 * current packet. Events subsequently appended to the stream will be
 * added to a new packet.
 *
 * Flushing will also set the packet context's default attributes if
 * they remained unset while populating the current packet. These default
 * attributes, along with their expected types, are detailed in stream-class.h.
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
 * You may also use bt_ctf_get() and bt_ctf_put() with stream objects.
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
