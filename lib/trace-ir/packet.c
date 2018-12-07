/*
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
#include <babeltrace/trace-ir/fields-internal.h>
#include <babeltrace/trace-ir/packet-const.h>
#include <babeltrace/trace-ir/packet.h>
#include <babeltrace/trace-ir/packet-internal.h>
#include <babeltrace/trace-ir/field-wrapper-internal.h>
#include <babeltrace/trace-ir/trace.h>
#include <babeltrace/trace-ir/stream-class-internal.h>
#include <babeltrace/trace-ir/stream-class.h>
#include <babeltrace/trace-ir/stream.h>
#include <babeltrace/trace-ir/stream-internal.h>
#include <babeltrace/trace-ir/clock-value-internal.h>
#include <babeltrace/trace-ir/trace-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/object.h>
#include <babeltrace/assert-internal.h>
#include <inttypes.h>

#define BT_ASSERT_PRE_PACKET_HOT(_packet) \
	BT_ASSERT_PRE_HOT((_packet), "Packet", ": %!+a", (_packet))

struct bt_stream *bt_packet_borrow_stream(struct bt_packet *packet)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	return packet->stream;
}

const struct bt_stream *bt_packet_borrow_stream_const(
		const struct bt_packet *packet)
{
	return bt_packet_borrow_stream((void *) packet);
}

struct bt_field *bt_packet_borrow_header_field(struct bt_packet *packet)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	return packet->header_field ? packet->header_field->field : NULL;
}

const struct bt_field *bt_packet_borrow_header_field_const(
		const struct bt_packet *packet)
{
	return bt_packet_borrow_header_field((void *) packet);
}

struct bt_field *bt_packet_borrow_context_field(struct bt_packet *packet)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	return packet->context_field ? packet->context_field->field : NULL;
}

const struct bt_field *bt_packet_borrow_context_field_const(
		const struct bt_packet *packet)
{
	return bt_packet_borrow_context_field((void *) packet);
}

BT_HIDDEN
void _bt_packet_set_is_frozen(const struct bt_packet *packet, bool is_frozen)
{
	if (!packet) {
		return;
	}

	BT_LIB_LOGD("Setting packet's frozen state: %![packet-]+a, "
		"is-frozen=%d", packet, is_frozen);

	if (packet->header_field) {
		BT_LOGD_STR("Setting packet's header field's frozen state.");
		bt_field_set_is_frozen(packet->header_field->field,
			is_frozen);
	}

	if (packet->context_field) {
		BT_LOGD_STR("Setting packet's context field's frozen state.");
		bt_field_set_is_frozen(packet->context_field->field,
			is_frozen);
	}

	((struct bt_packet *) packet)->frozen = is_frozen;
}

static inline
void reset_counter_snapshots(struct bt_packet *packet)
{
	packet->discarded_event_counter_snapshot.base.avail =
		BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE;
	packet->packet_counter_snapshot.base.avail =
		BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE;
}

static inline
void reset_packet(struct bt_packet *packet)
{
	BT_ASSERT(packet);
	BT_LIB_LOGD("Resetting packet: %!+a", packet);
	bt_packet_set_is_frozen(packet, false);

	if (packet->header_field) {
		bt_field_set_is_frozen(packet->header_field->field, false);
		bt_field_reset(packet->header_field->field);
	}

	if (packet->context_field) {
		bt_field_set_is_frozen(packet->context_field->field, false);
		bt_field_reset(packet->context_field->field);
	}

	if (packet->default_beginning_cv) {
		bt_clock_value_reset(packet->default_beginning_cv);
	}

	if (packet->default_end_cv) {
		bt_clock_value_reset(packet->default_end_cv);
	}

	reset_counter_snapshots(packet);
}

static
void recycle_header_field(struct bt_field_wrapper *header_field,
		struct bt_trace_class *tc)
{
	BT_ASSERT(header_field);
	BT_LIB_LOGD("Recycling packet header field: "
		"addr=%p, %![tc-]+T, %![field-]+f", header_field,
		tc, header_field->field);
	bt_object_pool_recycle_object(&tc->packet_header_field_pool,
		header_field);
}

static
void recycle_context_field(struct bt_field_wrapper *context_field,
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
	 *    we put the stream reference because this bt_object_put_ref()
	 *    could destroy the stream, also destroying its
	 *    packet pool, thus also destroying our packet object (this
	 *    would result in an invalid write access).
	 *
	 * 3. Recycle the packet object.
	 *
	 * 4. Put our stream reference.
	 */
	reset_packet(packet);
	stream = packet->stream;
	BT_ASSERT(stream);
	packet->stream = NULL;
	bt_object_pool_recycle_object(&stream->packet_pool, packet);
	bt_object_put_no_null_check(&stream->base);
}

BT_HIDDEN
void bt_packet_destroy(struct bt_packet *packet)
{
	BT_LIB_LOGD("Destroying packet: %!+a", packet);

	if (packet->header_field) {
		if (packet->stream) {
			BT_LOGD_STR("Recycling packet's header field.");
			recycle_header_field(packet->header_field,
				bt_stream_class_borrow_trace_class_inline(
					packet->stream->class));
		} else {
			bt_field_wrapper_destroy(packet->header_field);
		}

		packet->header_field = NULL;
	}

	if (packet->context_field) {
		if (packet->stream) {
			BT_LOGD_STR("Recycling packet's context field.");
			recycle_context_field(packet->context_field,
				packet->stream->class);
		} else {
			bt_field_wrapper_destroy(packet->context_field);
		}

		packet->context_field = NULL;
	}

	if (packet->default_beginning_cv) {
		BT_LOGD_STR("Recycling beginning clock value.");
		bt_clock_value_recycle(packet->default_beginning_cv);
		packet->default_beginning_cv = NULL;
	}

	if (packet->default_end_cv) {
		BT_LOGD_STR("Recycling end clock value.");
		bt_clock_value_recycle(packet->default_end_cv);
		packet->default_end_cv = NULL;
	}

	BT_LOGD_STR("Putting packet's stream.");
	BT_OBJECT_PUT_REF_AND_RESET(packet->stream);
	g_free(packet);
}

BT_HIDDEN
struct bt_packet *bt_packet_new(struct bt_stream *stream)
{
	struct bt_packet *packet = NULL;
	struct bt_trace_class *trace_class = NULL;

	BT_ASSERT(stream);
	BT_LIB_LOGD("Creating packet object: %![stream-]+s", stream);
	packet = g_new0(struct bt_packet, 1);
	if (!packet) {
		BT_LOGE_STR("Failed to allocate one packet object.");
		goto error;
	}

	bt_object_init_shared(&packet->base,
		(bt_object_release_func) bt_packet_recycle);
	packet->stream = stream;
	bt_object_get_no_null_check(stream);
	trace_class = bt_stream_class_borrow_trace_class_inline(stream->class);
	BT_ASSERT(trace_class);

	if (trace_class->packet_header_fc) {
		BT_LOGD_STR("Creating initial packet header field.");
		packet->header_field = bt_field_wrapper_create(
			&trace_class->packet_header_field_pool,
			trace_class->packet_header_fc);
		if (!packet->header_field) {
			BT_LOGE_STR("Cannot create packet header field wrapper.");
			goto error;
		}
	}

	if (stream->class->packet_context_fc) {
		BT_LOGD_STR("Creating initial packet context field.");
		packet->context_field = bt_field_wrapper_create(
			&stream->class->packet_context_field_pool,
			stream->class->packet_context_fc);
		if (!packet->context_field) {
			BT_LOGE_STR("Cannot create packet context field wrapper.");
			goto error;
		}
	}

	if (stream->class->default_clock_class) {
		if (stream->class->packets_have_default_beginning_cv) {
			packet->default_beginning_cv = bt_clock_value_create(
				stream->class->default_clock_class);
			if (!packet->default_beginning_cv) {
				/* bt_clock_value_create() logs errors */
				goto error;
			}
		}

		if (stream->class->packets_have_default_end_cv) {
			packet->default_end_cv = bt_clock_value_create(
				stream->class->default_clock_class);
			if (!packet->default_end_cv) {
				/* bt_clock_value_create() logs errors */
				goto error;
			}
		}
	}

	reset_counter_snapshots(packet);
	BT_LIB_LOGD("Created packet object: %!+a", packet);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(packet);

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

	if (likely(!packet->stream)) {
		packet->stream = stream;
		bt_object_get_no_null_check_no_parent_check(
			&packet->stream->base);
	}

end:
	return (void *) packet;
}

int bt_packet_move_header_field(struct bt_packet *packet,
		struct bt_packet_header_field *header_field)
{
	struct bt_trace_class *tc;
	struct bt_field_wrapper *field_wrapper = (void *) header_field;

	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_NON_NULL(field_wrapper, "Header field");
	BT_ASSERT_PRE_PACKET_HOT(packet);
	tc = bt_stream_class_borrow_trace_class_inline(packet->stream->class);
	BT_ASSERT_PRE(tc->packet_header_fc,
		"Trace class has no packet header field class: %!+T", tc);
	BT_ASSERT_PRE(field_wrapper->field->class ==
		tc->packet_header_fc,
		"Unexpected packet header field's class: "
		"%![fc-]+F, %![expected-fc-]+F", field_wrapper->field->class,
		tc->packet_header_fc);

	/* Recycle current header field: always exists */
	BT_ASSERT(packet->header_field);
	recycle_header_field(packet->header_field, tc);

	/* Move new field */
	packet->header_field = field_wrapper;
	return 0;
}

int bt_packet_move_context_field(struct bt_packet *packet,
		struct bt_packet_context_field *context_field)
{
	struct bt_stream_class *stream_class;
	struct bt_field_wrapper *field_wrapper = (void *) context_field;

	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_NON_NULL(field_wrapper, "Context field");
	BT_ASSERT_PRE_HOT(packet, "Packet", ": %!+a", packet);
	stream_class = packet->stream->class;
	BT_ASSERT_PRE(stream_class->packet_context_fc,
		"Stream class has no packet context field class: %!+S",
		stream_class);
	BT_ASSERT_PRE(field_wrapper->field->class ==
		stream_class->packet_context_fc,
		"Unexpected packet header field's class: "
		"%![fc-]+F, %![expected-fc-]+F", field_wrapper->field->class,
		stream_class->packet_context_fc);

	/* Recycle current context field: always exists */
	BT_ASSERT(packet->context_field);
	recycle_context_field(packet->context_field, stream_class);

	/* Move new field */
	packet->context_field = field_wrapper;
	return 0;
}

void bt_packet_set_default_beginning_clock_value(struct bt_packet *packet,
		uint64_t value_cycles)
{
	struct bt_stream_class *sc;

	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_PACKET_HOT(packet);
	sc = packet->stream->class;
	BT_ASSERT(sc);
	BT_ASSERT_PRE(sc->default_clock_class,
		"Packet's stream class has no default clock class: "
		"%![packet-]+a, %![sc-]+S", packet, sc);
	BT_ASSERT_PRE(sc->packets_have_default_beginning_cv,
		"Packet's stream class indicates that its packets have "
		"no default beginning clock value: %![packet-]+a, %![sc-]+S",
		packet, sc);
	BT_ASSERT(packet->default_beginning_cv);
	bt_clock_value_set_value_inline(packet->default_beginning_cv,
		value_cycles);
	BT_LIB_LOGV("Set packet's default beginning clock value: "
		"%![packet-]+a, value=%" PRIu64, packet, value_cycles);
}

enum bt_clock_value_status bt_packet_borrow_default_beginning_clock_value(
		const struct bt_packet *packet,
		const struct bt_clock_value **clock_value)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_NON_NULL(clock_value, "Clock value (output)");
	*clock_value = packet->default_beginning_cv;
	return BT_CLOCK_VALUE_STATUS_KNOWN;
}

void bt_packet_set_default_end_clock_value(struct bt_packet *packet,
		uint64_t value_cycles)
{
	struct bt_stream_class *sc;

	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_PACKET_HOT(packet);
	sc = packet->stream->class;
	BT_ASSERT(sc);
	BT_ASSERT_PRE(sc->default_clock_class,
		"Packet's stream class has no default clock class: "
		"%![packet-]+a, %![sc-]+S", packet, sc);
	BT_ASSERT_PRE(sc->packets_have_default_end_cv,
		"Packet's stream class indicates that its packets have "
		"no default end clock value: %![packet-]+a, %![sc-]+S",
		packet, sc);
	BT_ASSERT(packet->default_end_cv);
	bt_clock_value_set_value_inline(packet->default_end_cv, value_cycles);
	BT_LIB_LOGV("Set packet's default end clock value: "
		"%![packet-]+a, value=%" PRIu64, packet, value_cycles);
}

enum bt_clock_value_status bt_packet_borrow_default_end_clock_value(
		const struct bt_packet *packet,
		const struct bt_clock_value **clock_value)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_NON_NULL(clock_value, "Clock value (output)");
	*clock_value = packet->default_end_cv;
	return BT_CLOCK_VALUE_STATUS_KNOWN;
}

enum bt_property_availability bt_packet_get_discarded_event_counter_snapshot(
		const struct bt_packet *packet, uint64_t *value)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_NON_NULL(value, "Value (output)");
	*value = packet->discarded_event_counter_snapshot.value;
	return packet->discarded_event_counter_snapshot.base.avail;
}

void bt_packet_set_discarded_event_counter_snapshot(struct bt_packet *packet,
		uint64_t value)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_PACKET_HOT(packet);
	BT_ASSERT_PRE(packet->stream->class->packets_have_discarded_event_counter_snapshot,
		"Packet's stream's discarded event counter is not enabled: "
		"%![packet-]+a", packet);
	bt_property_uint_set(&packet->discarded_event_counter_snapshot, value);
}

enum bt_property_availability bt_packet_get_packet_counter_snapshot(
		const struct bt_packet *packet, uint64_t *value)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_NON_NULL(value, "Value (output)");
	*value = packet->packet_counter_snapshot.value;
	return packet->packet_counter_snapshot.base.avail;
}

void bt_packet_set_packet_counter_snapshot(struct bt_packet *packet,
		uint64_t value)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_PACKET_HOT(packet);
	BT_ASSERT_PRE(packet->stream->class->packets_have_packet_counter_snapshot,
		"Packet's stream's packet counter is not enabled: "
		"%![packet-]+a", packet);
	bt_property_uint_set(&packet->packet_counter_snapshot, value);
}
