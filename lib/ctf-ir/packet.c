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
#include <babeltrace/ctf-ir/field-wrapper-internal.h>
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

struct bt_field *bt_packet_borrow_header(struct bt_packet *packet)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	return packet->header ? (void *) packet->header->field : NULL;
}

struct bt_field *bt_packet_borrow_context(struct bt_packet *packet)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	return packet->context ? (void *) packet->context->field : NULL;
}

BT_HIDDEN
void _bt_packet_set_is_frozen(struct bt_packet *packet, bool is_frozen)
{
	if (!packet) {
		return;
	}

	BT_LOGD("Setting packet's frozen state: addr=%p, frozen=%d",
		packet, is_frozen);

	if (packet->header) {
		BT_LOGD("Setting packet's header field's frozen state: "
			"frozen=%d", is_frozen);
		bt_field_set_is_frozen_recursive((void *) packet->header->field,
			is_frozen);
	}

	if (packet->context) {
		BT_LOGD("Setting packet's context field's frozen state: "
			"frozen=%d", is_frozen);
		bt_field_set_is_frozen_recursive((void *) packet->context->field,
			is_frozen);
	}

	packet->frozen = is_frozen;
}

static inline
void bt_packet_reset(struct bt_packet *packet)
{
	BT_ASSERT(packet);
	bt_packet_set_is_frozen(packet, false);

	if (packet->header) {
		bt_field_set_is_frozen_recursive(
			(void *) packet->header->field, false);
		bt_field_reset_recursive((void *) packet->header->field);
	}

	if (packet->context) {
		bt_field_set_is_frozen_recursive(
			(void *) packet->context->field, false);
		bt_field_reset_recursive((void *) packet->context->field);
	}

	bt_clock_value_set_reset(&packet->begin_cv_set);
	bt_clock_value_set_reset(&packet->end_cv_set);
}

static
void bt_packet_header_field_recycle(struct bt_field_wrapper *header_field,
		struct bt_trace *trace)
{
	BT_ASSERT(header_field);
	BT_LIB_LOGD("Recycling packet header field: "
		"addr=%p, %![trace-]+t, %![field-]+f", header_field,
		trace, header_field->field);
	bt_object_pool_recycle_object(&trace->packet_header_field_pool,
		header_field);
}

static
void bt_packet_context_field_recycle(struct bt_field_wrapper *context_field,
		struct bt_stream_class *stream_class)
{
	BT_ASSERT(context_field);
	BT_LIB_LOGD("Recycling packet context field: "
		"addr=%p, %![sc-]+S, %![field-]+f", context_field,
		stream_class, context_field->field);
	bt_object_pool_recycle_object(&stream_class->packet_context_field_pool,
		context_field);
}

BT_HIDDEN
void bt_packet_recycle(struct bt_packet *packet)
{
	struct bt_stream *stream;

	BT_ASSERT(packet);
	BT_LIB_LOGD("Recycling packet: %!+a", packet);

	/*
	 * Those are the important ordered steps:
	 *
	 * 1. Reset the packet object (put any permanent reference it
	 *    has, unfreeze it and its fields in developer mode, etc.),
	 *    but do NOT put its stream's reference. This stream
	 *    contains the pool to which we're about to recycle this
	 *    packet object, so we must guarantee its existence thanks
	 *    to this existing reference.
	 *
	 * 2. Move the stream reference to our `stream`
	 *    variable so that we can set the packet's stream member
	 *    to NULL before recycling it. We CANNOT do this after
	 *    we put the stream reference because this bt_put()
	 *    could destroy the stream, also destroying its
	 *    packet pool, thus also destroying our packet object (this
	 *    would result in an invalid write access).
	 *
	 * 3. Recycle the packet object.
	 *
	 * 4. Put our stream reference.
	 */
	bt_packet_reset(packet);
	stream = packet->stream;
	BT_ASSERT(stream);
	packet->stream = NULL;
	bt_object_pool_recycle_object(&stream->packet_pool, packet);
	bt_object_put_no_null_check(&stream->common.base);
}

BT_HIDDEN
void bt_packet_destroy(struct bt_packet *packet)
{
	BT_LOGD("Destroying packet: addr=%p", packet);
	BT_LOGD_STR("Destroying packet's header field.");

	if (packet->header) {
		if (packet->stream) {
			BT_LOGD_STR("Recycling packet's header field.");
			bt_packet_header_field_recycle(packet->header,
				bt_stream_class_borrow_trace(
					bt_stream_borrow_class(packet->stream)));
		} else {
			bt_field_wrapper_destroy(packet->header);
		}
	}

	if (packet->context) {
		if (packet->stream) {
			BT_LOGD_STR("Recycling packet's context field.");
			bt_packet_context_field_recycle(packet->context,
				bt_stream_borrow_class(packet->stream));
		} else {
			bt_field_wrapper_destroy(packet->context);
		}
	}

	bt_clock_value_set_finalize(&packet->begin_cv_set);
	bt_clock_value_set_finalize(&packet->end_cv_set);
	BT_LOGD_STR("Putting packet's stream.");
	bt_put(packet->stream);
	g_free(packet);
}

BT_HIDDEN
struct bt_packet *bt_packet_new(struct bt_stream *stream)
{
	struct bt_packet *packet = NULL;
	struct bt_stream_class *stream_class = NULL;
	struct bt_trace *trace = NULL;

	BT_ASSERT(stream);
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

	bt_object_init_shared(&packet->base,
		(bt_object_release_func) bt_packet_recycle);
	packet->stream = bt_get(stream);

	if (trace->common.packet_header_field_type) {
		BT_LOGD("Creating initial packet header field: ft-addr=%p",
			trace->common.packet_header_field_type);
		packet->header = bt_field_wrapper_create(
			&trace->packet_header_field_pool,
			(void *) trace->common.packet_header_field_type);
		if (!packet->header) {
			BT_LOGE("Cannot create packet header field wrapper.");
			BT_PUT(packet);
			goto end;
		}
	}

	if (stream->common.stream_class->packet_context_field_type) {
		BT_LOGD("Creating initial packet context field: ft-addr=%p",
			stream->common.stream_class->packet_context_field_type);
		packet->context = bt_field_wrapper_create(
			&stream_class->packet_context_field_pool,
			(void *) stream->common.stream_class->packet_context_field_type);
		if (!packet->context) {
			BT_LOGE("Cannot create packet context field wrapper.");
			BT_PUT(packet);
			goto end;
		}
	}

	if (bt_clock_value_set_initialize(&packet->begin_cv_set)) {
		BT_PUT(packet);
		goto end;
	}

	if (bt_clock_value_set_initialize(&packet->end_cv_set)) {
		BT_PUT(packet);
		goto end;
	}

	BT_LOGD("Created packet object: addr=%p", packet);

end:
	return packet;
}

struct bt_packet *bt_packet_create(struct bt_stream *stream)
{
	struct bt_packet *packet = NULL;

	BT_ASSERT_PRE_NON_NULL(stream, "Stream");
	packet = bt_object_pool_create_object(&stream->packet_pool);
	if (unlikely(!packet)) {
		BT_LIB_LOGE("Cannot allocate one packet from stream's packet pool: "
			"%![stream-]+s", stream);
		goto end;
	}

	if (unlikely(!packet->stream)) {
		packet->stream = stream;
		bt_object_get_no_null_check_no_parent_check(
			&packet->stream->common.base);
	}

	goto end;

end:
	return packet;
}

int bt_packet_move_header(struct bt_packet *packet,
		struct bt_packet_header_field *header_field)
{
	struct bt_trace *trace;
	struct bt_field_wrapper *field_wrapper = (void *) header_field;

	BT_ASSERT_PRE_NON_NULL(packet, "Event");
	BT_ASSERT_PRE_NON_NULL(field_wrapper, "Header field");
	BT_ASSERT_PRE_HOT(packet, "Packet", ": %!+a", packet);
	trace = bt_stream_class_borrow_trace(
		bt_stream_borrow_class(packet->stream));
	BT_ASSERT_PRE(trace->common.packet_header_field_type,
		"Trace has no packet header field type: %!+t",
		trace);

	/* TODO: compare field types (precondition) */

	/* Recycle current header field: always exists */
	BT_ASSERT(packet->header);
	bt_packet_header_field_recycle(packet->header, trace);

	/* Move new field */
	packet->header = field_wrapper;
	return 0;
}

int bt_packet_move_context(struct bt_packet *packet,
		struct bt_packet_context_field *context_field)
{
	struct bt_stream_class *stream_class;
	struct bt_field_wrapper *field_wrapper = (void *) context_field;

	BT_ASSERT_PRE_NON_NULL(packet, "Event");
	BT_ASSERT_PRE_NON_NULL(field_wrapper, "Context field");
	BT_ASSERT_PRE_HOT(packet, "Packet", ": %!+a", packet);
	stream_class = bt_stream_borrow_class(packet->stream);
	BT_ASSERT_PRE(stream_class->common.packet_context_field_type,
		"Stream class has no packet context field type: %!+S",
		stream_class);

	/* TODO: compare field types (precondition) */

	/* Recycle current context field: always exists */
	BT_ASSERT(packet->context);
	bt_packet_context_field_recycle(packet->context, stream_class);

	/* Move new field */
	packet->context = field_wrapper;
	return 0;
}

int bt_packet_set_beginning_clock_value(struct bt_packet *packet,
		struct bt_clock_class *clock_class, uint64_t raw_value,
		bt_bool is_default)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_HOT(packet, "Packet", ": %!+a", packet);
	BT_ASSERT_PRE(is_default,
		"You can only set a default clock value as of this version.");
	return bt_clock_value_set_set_clock_value(&packet->begin_cv_set,
		clock_class, raw_value, is_default);
}

struct bt_clock_value *bt_packet_borrow_default_begin_clock_value(
		struct bt_packet *packet)
{
	struct bt_clock_value *clock_value = NULL;

	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	clock_value = packet->begin_cv_set.default_cv;
	if (!clock_value) {
		BT_LIB_LOGV("No default clock value: %![packet-]+a", packet);
	}

	return clock_value;
}

int bt_packet_set_end_clock_value(struct bt_packet *packet,
		struct bt_clock_class *clock_class, uint64_t raw_value,
		bt_bool is_default)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_HOT(packet, "Packet", ": %!+a", packet);
	BT_ASSERT_PRE(is_default,
		"You can only set a default clock value as of this version.");
	return bt_clock_value_set_set_clock_value(&packet->end_cv_set,
		clock_class, raw_value, is_default);
}

struct bt_clock_value *bt_packet_borrow_default_end_clock_value(
		struct bt_packet *packet)
{
	struct bt_clock_value *clock_value = NULL;

	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	clock_value = packet->end_cv_set.default_cv;
	if (!clock_value) {
		BT_LIB_LOGV("No default clock value: %![packet-]+a", packet);
	}

	return clock_value;
}
