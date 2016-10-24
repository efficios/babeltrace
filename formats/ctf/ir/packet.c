/*
 * packet.c
 *
 * Babeltrace CTF IR - Stream packet
 *
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/ctf-ir/packet-internal.h>
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/stream-internal.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/ref.h>

BT_HIDDEN
struct bt_ctf_stream *bt_ctf_packet_get_stream(struct bt_ctf_packet *packet)
{
	return packet ? bt_get(packet->stream) : NULL;
}

BT_HIDDEN
struct bt_ctf_field *bt_ctf_packet_get_header(
		struct bt_ctf_packet *packet)
{
	return packet ? bt_get(packet->header) : NULL;
}

BT_HIDDEN
int bt_ctf_packet_set_header(struct bt_ctf_packet *packet,
		struct bt_ctf_field *header)
{
	int ret = 0;
	struct bt_ctf_trace *trace = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_field_type *header_field_type = NULL;
	struct bt_ctf_field_type *expected_header_field_type = NULL;

	if (!packet || !header || packet->frozen) {
		ret = -1;
		goto end;
	}

	stream_class = bt_ctf_stream_get_class(packet->stream);
	assert(stream_class);
	trace = bt_ctf_stream_class_get_trace(stream_class);
	assert(trace);
	header_field_type = bt_ctf_field_get_type(header);
	assert(header_field_type);
	expected_header_field_type = bt_ctf_trace_get_packet_header_type(trace);

	if (bt_ctf_field_type_compare(header_field_type,
			expected_header_field_type)) {
		ret = -1;
		goto end;
	}

	bt_put(packet->header);
	packet->header = bt_get(header);

end:
	BT_PUT(trace);
	BT_PUT(stream_class);
	BT_PUT(header_field_type);
	BT_PUT(expected_header_field_type);

	return ret;
}

BT_HIDDEN
struct bt_ctf_field *bt_ctf_packet_get_context(
		struct bt_ctf_packet *packet)
{
	return packet ? bt_get(packet->context) : NULL;
}

BT_HIDDEN
int bt_ctf_packet_set_context(struct bt_ctf_packet *packet,
		struct bt_ctf_field *context)
{
	int ret = 0;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_field_type *context_field_type = NULL;
	struct bt_ctf_field_type *expected_context_field_type = NULL;

	if (!packet || !context || packet->frozen) {
		ret = -1;
		goto end;
	}

	stream_class = bt_ctf_stream_get_class(packet->stream);
	assert(stream_class);
	context_field_type = bt_ctf_field_get_type(context);
	assert(context_field_type);
	expected_context_field_type =
		bt_ctf_stream_class_get_packet_context_type(stream_class);

	if (bt_ctf_field_type_compare(context_field_type,
			expected_context_field_type)) {
		ret = -1;
		goto end;
	}

	bt_put(packet->context);
	packet->context = bt_get(context);

end:
	BT_PUT(stream_class);
	BT_PUT(context_field_type);
	BT_PUT(expected_context_field_type);

	return ret;
}

BT_HIDDEN
void bt_ctf_packet_freeze(struct bt_ctf_packet *packet)
{
	if (!packet) {
		return;
	}

	bt_ctf_field_freeze(packet->header);
	bt_ctf_field_freeze(packet->context);
	packet->frozen = 1;
}

static
void bt_ctf_packet_destroy(struct bt_object *obj)
{
	struct bt_ctf_packet *packet;

	packet = container_of(obj, struct bt_ctf_packet, base);
	bt_put(packet->header);
	bt_put(packet->context);
	bt_put(packet->stream);
	g_free(packet);
}

BT_HIDDEN
struct bt_ctf_packet *bt_ctf_packet_create(
		struct bt_ctf_stream *stream)
{
	struct bt_ctf_packet *packet = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_trace *trace = NULL;

	if (!stream || stream->pos.fd >= 0) {
		goto end;
	}

	stream_class = bt_ctf_stream_get_class(stream);
	assert(stream_class);
	trace = bt_ctf_stream_class_get_trace(stream_class);
	assert(trace);
	packet = g_new0(struct bt_ctf_packet, 1);
	if (!packet) {
		goto end;
	}

	bt_object_init(packet, bt_ctf_packet_destroy);
	packet->stream = bt_get(stream);
	packet->header = bt_ctf_field_create(trace->packet_header_type);
	if (!packet->header) {
		BT_PUT(packet);
		goto end;
	}

	packet->context = bt_ctf_field_create(
		stream->stream_class->packet_context_type);
	if (!packet->context) {
		BT_PUT(packet);
		goto end;
	}

end:
	BT_PUT(trace);
	BT_PUT(stream_class);

	return packet;
}
