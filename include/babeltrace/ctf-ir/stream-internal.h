#ifndef BABELTRACE_CTF_WRITER_STREAM_INTERNAL_H
#define BABELTRACE_CTF_WRITER_STREAM_INTERNAL_H

/*
 * BabelTrace - CTF Writer: Stream internal
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

#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/ctf-writer/clock.h>
#include <babeltrace/ctf-writer/event-fields.h>
#include <babeltrace/ctf-writer/event-types.h>
#include <babeltrace/ctf-writer/serialize-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>

struct bt_port;
struct bt_component;

typedef void (*bt_stream_destroy_listener_func)(
		struct bt_stream *stream, void *data);

struct bt_stream_destroy_listener {
	bt_stream_destroy_listener_func func;
	void *data;
};

struct bt_stream {
	struct bt_object base;
	int64_t id;
	struct bt_stream_class *stream_class;
	GString *name;
	struct bt_field *packet_header;
	struct bt_field *packet_context;

	/*
	 * When a notification which contains a reference to a stream
	 * object (event notification, for example) is returned by the
	 * "next" method of a sink or filter component's notification
	 * iterator, it must NOT be returned by the "next" method of a
	 * notification iterator which iterates on the notifications of
	 * another output port of the same component.
	 *
	 * To ensure this, the stream object keeps a hash table which
	 * indicates which port, for a given component, is currently
	 * allowed to emit notifications which contain a reference to
	 * this stream.
	 *
	 * This is a `struct bt_component *` to `struct bt_port *` hash
	 * table. Both pointers are weak references because there's no
	 * need to keep one or the other alive as far as this stream is
	 * concerned.
	 */
	GHashTable *comp_cur_port;

	/* Writer-specific members. */
	/* Array of pointers to bt_event for the current packet */
	GPtrArray *events;
	struct bt_stream_pos pos;
	unsigned int flushed_packet_count;
	uint64_t discarded_events;
	uint64_t size;
	uint64_t last_ts_end;

	/* Array of struct bt_stream_destroy_listener */
	GArray *destroy_listeners;
};

BT_HIDDEN
int bt_stream_set_fd(struct bt_stream *stream, int fd);

BT_HIDDEN
void bt_stream_map_component_to_port(struct bt_stream *stream,
		struct bt_component *comp,
		struct bt_port *port);

BT_HIDDEN
struct bt_port *bt_stream_port_for_component(struct bt_stream *stream,
		struct bt_component *comp);

BT_HIDDEN
void bt_stream_add_destroy_listener(struct bt_stream *stream,
		bt_stream_destroy_listener_func func, void *data);

BT_HIDDEN
void bt_stream_remove_destroy_listener(struct bt_stream *stream,
		bt_stream_destroy_listener_func func, void *data);

static inline
struct bt_stream_class *bt_stream_borrow_stream_class(
		struct bt_stream *stream)
{
	BT_ASSERT(stream);
	return stream->stream_class;
}

#endif /* BABELTRACE_CTF_WRITER_STREAM_INTERNAL_H */
