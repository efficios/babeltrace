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

#define BT_LOG_TAG "PACKET"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/ctf-ir/packet-internal.h>
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/stream-internal.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/assert-internal.h>
#include <inttypes.h>

struct bt_stream *bt_packet_borrow_stream(struct bt_packet *packet)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	return packet->stream;
}

struct bt_field *bt_packet_borrow_header(
		struct bt_packet *packet)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	return packet->header;
}

BT_ASSERT_PRE_FUNC
static inline bool validate_field_to_set(struct bt_field *field,
		struct bt_field_type_common *expected_ft)
{
	bool ret = true;

	if (!field) {
		if (expected_ft) {
			BT_ASSERT_PRE_MSG("Setting no field, but expected "
				"field type is not NULL: "
				"%![field-]+f, %![expected-ft-]+F",
				field, expected_ft);
			ret = false;
			goto end;
		}

		goto end;
	}

	if (bt_field_type_compare(bt_field_borrow_type(field),
			BT_FROM_COMMON(expected_ft)) != 0) {
		BT_ASSERT_PRE_MSG("Field type is different from expected "
			" field type: %![field-ft-]+F, %![expected-ft-]+F",
			bt_field_borrow_type(field), expected_ft);
		ret = false;
		goto end;
	}

end:
	return ret;
}

int bt_packet_set_header(struct bt_packet *packet,
		struct bt_field *header)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_HOT(packet, "Packet", ": +%!+a", packet);
	BT_ASSERT_PRE(validate_field_to_set(header,
		bt_stream_class_borrow_trace(
			BT_FROM_COMMON(packet->stream->common.stream_class))->common.packet_header_field_type),
		"Invalid packet header field: "
		"%![packet-]+a, %![field-]+f", packet, header);
	bt_put(packet->header);
	packet->header = bt_get(header);
	BT_LOGV("Set packet's header field: packet-addr=%p, packet-header-addr=%p",
		packet, header);
	return 0;
}

struct bt_field *bt_packet_borrow_context(struct bt_packet *packet)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	return packet->context;
}

int bt_packet_set_context(struct bt_packet *packet,
		struct bt_field *context)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_HOT(packet, "Packet", ": +%!+a", packet);
	BT_ASSERT_PRE(validate_field_to_set(context,
		BT_FROM_COMMON(packet->stream->common.stream_class->packet_context_field_type)),
		"Invalid packet context field: "
		"%![packet-]+a, %![field-]+f", packet, context);
	bt_put(packet->context);
	packet->context = bt_get(context);
	BT_LOGV("Set packet's context field: packet-addr=%p, packet-context-addr=%p",
		packet, context);
	return 0;
}

BT_HIDDEN
void _bt_packet_freeze(struct bt_packet *packet)
{
	if (!packet || packet->frozen) {
		return;
	}

	BT_LOGD("Freezing packet: addr=%p", packet);
	BT_LOGD_STR("Freezing packet's header field.");
	bt_field_freeze_recursive(packet->header);
	BT_LOGD_STR("Freezing packet's context field.");
	bt_field_freeze_recursive(packet->context);
	packet->frozen = 1;
}

static
void bt_packet_destroy(struct bt_object *obj)
{
	struct bt_packet *packet = (void *) obj;

	BT_LOGD("Destroying packet: addr=%p", packet);
	BT_LOGD_STR("Putting packet's header field.");
	bt_put(packet->header);
	BT_LOGD_STR("Putting packet's context field.");
	bt_put(packet->context);
	BT_LOGD_STR("Putting packet's stream.");
	bt_put(packet->stream);
	g_free(packet);
}

struct bt_packet *bt_packet_create(
		struct bt_stream *stream)
{
	struct bt_packet *packet = NULL;
	struct bt_stream_class *stream_class = NULL;
	struct bt_trace *trace = NULL;

	BT_ASSERT_PRE_NON_NULL(stream, "Stream");
	BT_LOGD("Creating packet object: stream-addr=%p, "
		"stream-name=\"%s\", stream-class-addr=%p, "
		"stream-class-name=\"%s\", stream-class-id=%" PRId64,
		stream, bt_stream_get_name(stream),
		stream->common.stream_class,
		bt_stream_class_common_get_name(stream->common.stream_class),
		bt_stream_class_common_get_id(stream->common.stream_class));
	stream_class = bt_stream_borrow_class(stream);
	BT_ASSERT(stream_class);
	trace = bt_stream_class_borrow_trace(stream_class);
	BT_ASSERT(trace);
	packet = g_new0(struct bt_packet, 1);
	if (!packet) {
		BT_LOGE_STR("Failed to allocate one packet object.");
		goto end;
	}

	bt_object_init(packet, bt_packet_destroy);
	packet->stream = bt_get(stream);

	if (trace->common.packet_header_field_type) {
		BT_LOGD("Creating initial packet header field: ft-addr=%p",
			trace->common.packet_header_field_type);
		packet->header = bt_field_create(
			BT_FROM_COMMON(trace->common.packet_header_field_type));
		if (!packet->header) {
			BT_LOGE_STR("Cannot create initial packet header field object.");
			BT_PUT(packet);
			goto end;
		}
	}

	if (stream->common.stream_class->packet_context_field_type) {
		BT_LOGD("Creating initial packet context field: ft-addr=%p",
			stream->common.stream_class->packet_context_field_type);
		packet->context = bt_field_create(
			BT_FROM_COMMON(stream->common.stream_class->packet_context_field_type));
		if (!packet->context) {
			BT_LOGE_STR("Cannot create initial packet header field object.");
			BT_PUT(packet);
			goto end;
		}
	}

	BT_LOGD("Created packet object: addr=%p", packet);

end:
	return packet;
}
