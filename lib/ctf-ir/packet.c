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
void bt_packet_reset_avail(struct bt_packet *packet)
{
	/* Previous packet */
	packet->prev_packet_info.avail =
		BT_PACKET_PREVIOUS_PACKET_AVAILABILITY_NOT_AVAILABLE;
	packet->prev_packet_info.discarded_event_counter.avail =
		BT_PACKET_PROPERTY_AVAILABILITY_NOT_AVAILABLE;
	packet->prev_packet_info.seq_num.avail =
		BT_PACKET_PROPERTY_AVAILABILITY_NOT_AVAILABLE;
	packet->prev_packet_info.default_end_cv.avail =
		BT_PACKET_PROPERTY_AVAILABILITY_NOT_AVAILABLE;

	/* Current packet */
	packet->discarded_event_counter.avail =
		BT_PACKET_PROPERTY_AVAILABILITY_NOT_AVAILABLE;
	packet->seq_num.avail =
		BT_PACKET_PROPERTY_AVAILABILITY_NOT_AVAILABLE;

	/* Computed */
	packet->discarded_event_count.avail =
		BT_PACKET_PROPERTY_AVAILABILITY_NOT_AVAILABLE;
	packet->discarded_packet_count.avail =
		BT_PACKET_PROPERTY_AVAILABILITY_NOT_AVAILABLE;
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
	bt_packet_reset_avail(packet);
	bt_packet_invalidate_properties(packet);

	if (packet->prev_packet_info.default_end_cv.cv) {
		bt_clock_value_recycle(packet->prev_packet_info.default_end_cv.cv);
		packet->prev_packet_info.default_end_cv.cv = NULL;
	}
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
	bt_object_put_no_null_check(&stream->base);
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
		stream->stream_class,
		bt_stream_class_get_name(stream->stream_class),
		bt_stream_class_get_id(stream->stream_class));
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

	if (trace->packet_header_field_type) {
		BT_LOGD("Creating initial packet header field: ft-addr=%p",
			trace->packet_header_field_type);
		packet->header = bt_field_wrapper_create(
			&trace->packet_header_field_pool,
			(void *) trace->packet_header_field_type);
		if (!packet->header) {
			BT_LOGE("Cannot create packet header field wrapper.");
			BT_PUT(packet);
			goto end;
		}
	}

	if (stream->stream_class->packet_context_field_type) {
		BT_LOGD("Creating initial packet context field: ft-addr=%p",
			stream->stream_class->packet_context_field_type);
		packet->context = bt_field_wrapper_create(
			&stream_class->packet_context_field_pool,
			(void *) stream->stream_class->packet_context_field_type);
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

	bt_packet_reset_avail(packet);
	BT_LOGD("Created packet object: addr=%p", packet);

end:
	return packet;
}

static inline
uint64_t get_uint_field_value(struct bt_field *parent_field, const char *name)
{
	uint64_t val = UINT64_C(-1);
	struct bt_field *field = bt_field_structure_borrow_field_by_name(
		parent_field, name);
	int ret;

	if (!field) {
		goto end;
	}

	BT_ASSERT(bt_field_is_integer(field));
	BT_ASSERT(!bt_field_type_integer_is_signed(
		bt_field_borrow_type(field)));
	ret = bt_field_integer_unsigned_get_value(field, &val);
	BT_ASSERT(ret == 0);

end:
	return val;
}

static inline
void set_packet_prop_uint64(struct bt_packet_prop_uint64 *prop, uint64_t val)
{
	BT_ASSERT(prop);
	prop->value = val;
	prop->avail = BT_PACKET_PROPERTY_AVAILABILITY_AVAILABLE;
}

static inline
int set_packet_default_clock_value(struct bt_field *pkt_ctx_field,
		const char *field_name, struct bt_clock_value_set *cv_set)
{
	int ret = 0;
	uint64_t val = UINT64_C(-1);
	struct bt_field *field = bt_field_structure_borrow_field_by_name(
		pkt_ctx_field, field_name);
	struct bt_clock_class *clock_class;

	if (!field) {
		goto end;
	}

	BT_ASSERT(bt_field_is_integer(field));
	BT_ASSERT(!bt_field_type_integer_is_signed(
		bt_field_borrow_type(field)));
	clock_class = bt_field_type_integer_borrow_mapped_clock_class(
		bt_field_borrow_type(field));
	if (!clock_class) {
		goto end;
	}

	ret = bt_field_integer_unsigned_get_value(field, &val);
	BT_ASSERT(ret == 0);
	ret = bt_clock_value_set_set_clock_value(cv_set, clock_class,
		val, true);

end:
	return ret;
}

BT_HIDDEN
int bt_packet_set_properties(struct bt_packet *packet)
{
	struct bt_field *pkt_context_field;
	uint64_t val;
	int ret = 0;

	BT_ASSERT(!packet->props_are_set);

	pkt_context_field = bt_packet_borrow_context(packet);
	if (!pkt_context_field) {
		goto end;
	}

	/* Discarded event counter */
	val = get_uint_field_value(pkt_context_field, "events_discarded");
	if (val != UINT64_C(-1)) {
		set_packet_prop_uint64(&packet->discarded_event_counter, val);
	}

	/* Sequence number */
	val = get_uint_field_value(pkt_context_field, "packet_seq_num");
	if (val != UINT64_C(-1)) {
		set_packet_prop_uint64(&packet->seq_num, val);
	}

	/* Beginning and end times */
	ret = set_packet_default_clock_value(pkt_context_field,
		"timestamp_begin", &packet->begin_cv_set);
	if (ret) {
		goto end;
	}

	ret = set_packet_default_clock_value(pkt_context_field,
		"timestamp_end", &packet->end_cv_set);
	if (ret) {
		goto end;
	}

	/* Information from previous packet */
	if (packet->prev_packet_info.avail ==
			BT_PACKET_PREVIOUS_PACKET_AVAILABILITY_AVAILABLE) {
		/* Discarded event count */
		if (packet->prev_packet_info.discarded_event_counter.avail ==
				BT_PACKET_PROPERTY_AVAILABILITY_AVAILABLE) {
			BT_ASSERT(packet->discarded_event_counter.avail ==
				BT_PACKET_PROPERTY_AVAILABILITY_AVAILABLE);
			set_packet_prop_uint64(&packet->discarded_event_count,
				packet->discarded_event_counter.value -
				packet->prev_packet_info.discarded_event_counter.value);
		}

		/* Discarded packet count */
		if (packet->prev_packet_info.seq_num.avail ==
				BT_PACKET_PROPERTY_AVAILABILITY_AVAILABLE) {
			BT_ASSERT(packet->seq_num.avail ==
				BT_PACKET_PROPERTY_AVAILABILITY_AVAILABLE);
			set_packet_prop_uint64(&packet->discarded_packet_count,
				packet->seq_num.value -
				packet->prev_packet_info.seq_num.value - 1);
		}
	}

end:
	return ret;
}

static
int snapshot_prev_packet_properties(struct bt_packet *packet,
		enum bt_packet_previous_packet_availability prev_packet_avail,
		struct bt_packet *prev_packet)
{
	int ret = 0;
	struct bt_clock_value *prev_packet_default_end_cv;

	if (!prev_packet) {
		goto end;
	}

	if (!prev_packet->props_are_set) {
		ret = bt_packet_set_properties(prev_packet);
		if (ret) {
			BT_LIB_LOGE("Cannot update previous packet's properties: "
				"%![prev-packet-]+a", prev_packet);
			goto end;
		}
	}

	packet->prev_packet_info.avail = prev_packet_avail;
	prev_packet_default_end_cv = prev_packet->end_cv_set.default_cv;

	/* End time */
	if (prev_packet_default_end_cv) {
		/* Copy clock value */
		packet->prev_packet_info.default_end_cv.cv =
			bt_clock_value_create(
				prev_packet_default_end_cv->clock_class);
		if (!packet->prev_packet_info.default_end_cv.cv) {
			BT_LIB_LOGE("Cannot create a clock value from a clock class: "
				"%![cc-]+K",
				prev_packet_default_end_cv->clock_class);
			ret = -1;
			goto end;
		}

		bt_clock_value_set_raw_value(
			packet->prev_packet_info.default_end_cv.cv,
			prev_packet_default_end_cv->value);
		packet->prev_packet_info.default_end_cv.avail =
			BT_PACKET_PROPERTY_AVAILABILITY_AVAILABLE;
	}

	/* Discarded event counter */
	packet->prev_packet_info.discarded_event_counter =
		prev_packet->discarded_event_counter;

	/* Sequence number */
	packet->prev_packet_info.seq_num = prev_packet->seq_num;

end:
	return ret;
}

struct bt_packet *bt_packet_create(struct bt_stream *stream,
		enum bt_packet_previous_packet_availability prev_packet_avail,
		struct bt_packet *prev_packet)
{
	struct bt_packet *packet = NULL;
	int ret;

	BT_ASSERT_PRE_NON_NULL(stream, "Stream");
	BT_ASSERT_PRE(!prev_packet || prev_packet->stream == stream,
		"New packet's and previous packet's stream are not the same: "
		"%![new-packet-stream-]+s, %![prev-packet]+a, "
		"%![prev-packet-stream]+s", stream, prev_packet,
		prev_packet->stream);
	BT_ASSERT_PRE(
		prev_packet_avail == BT_PACKET_PREVIOUS_PACKET_AVAILABILITY_AVAILABLE ||
		prev_packet_avail == BT_PACKET_PREVIOUS_PACKET_AVAILABILITY_NOT_AVAILABLE ||
		prev_packet_avail == BT_PACKET_PREVIOUS_PACKET_AVAILABILITY_NONE,
		"Invalid previous packet availability value: val=%d",
		prev_packet_avail);
	BT_ASSERT_PRE(!prev_packet ||
		prev_packet_avail == BT_PACKET_PREVIOUS_PACKET_AVAILABILITY_AVAILABLE,
		"Previous packet is available, but previous packet is NULL.");
	packet = bt_object_pool_create_object(&stream->packet_pool);
	if (unlikely(!packet)) {
		BT_LIB_LOGE("Cannot allocate one packet from stream's packet pool: "
			"%![stream-]+s", stream);
		goto end;
	}

	if (unlikely(!packet->stream)) {
		packet->stream = stream;
		bt_object_get_no_null_check_no_parent_check(
			&packet->stream->base);
	}

	ret = snapshot_prev_packet_properties(packet, prev_packet_avail,
		prev_packet);
	if (ret) {
		/* Recycle */
		BT_PUT(packet);
		goto end;
	}

	if (prev_packet) {
		bt_packet_validate_properties(prev_packet);
		bt_packet_set_is_frozen(prev_packet, true);
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
	BT_ASSERT_PRE(trace->packet_header_field_type,
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
	BT_ASSERT_PRE(stream_class->packet_context_field_type,
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

enum bt_packet_property_availability
bt_packet_borrow_default_beginning_clock_value(struct bt_packet *packet,
		struct bt_clock_value **clock_value)
{
	enum bt_packet_property_availability avail =
		BT_PACKET_PROPERTY_AVAILABILITY_AVAILABLE;

	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_NON_NULL(clock_value, "Clock value");
	*clock_value = packet->begin_cv_set.default_cv;
	if (!*clock_value) {
		avail = BT_PACKET_PROPERTY_AVAILABILITY_NOT_AVAILABLE;
	}

	return avail;
}

enum bt_packet_property_availability
bt_packet_borrow_default_end_clock_value(struct bt_packet *packet,
		struct bt_clock_value **clock_value)
{
	enum bt_packet_property_availability avail =
		BT_PACKET_PROPERTY_AVAILABILITY_AVAILABLE;

	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_NON_NULL(clock_value, "Clock value");
	*clock_value = packet->end_cv_set.default_cv;
	if (!*clock_value) {
		avail = BT_PACKET_PROPERTY_AVAILABILITY_NOT_AVAILABLE;
	}

	return avail;
}

enum bt_packet_previous_packet_availability
bt_packet_get_previous_packet_availability(struct bt_packet *packet)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	return packet->prev_packet_info.avail;
}

enum bt_packet_property_availability
bt_packet_borrow_previous_packet_default_end_clock_value(
		struct bt_packet *packet, struct bt_clock_value **clock_value)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_NON_NULL(clock_value, "Clock value");
	*clock_value = packet->prev_packet_info.default_end_cv.cv;
	return packet->prev_packet_info.default_end_cv.avail;
}

enum bt_packet_property_availability bt_packet_get_discarded_event_counter(
		struct bt_packet *packet, uint64_t *counter)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_NON_NULL(counter, "Counter");
	*counter = packet->discarded_event_counter.value;
	return packet->discarded_event_counter.avail;
}

enum bt_packet_property_availability bt_packet_get_sequence_number(
		struct bt_packet *packet, uint64_t *sequence_number)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_NON_NULL(sequence_number, "Sequence number");
	*sequence_number = packet->seq_num.value;
	return packet->seq_num.avail;
}

enum bt_packet_property_availability bt_packet_get_discarded_event_count(
		struct bt_packet *packet, uint64_t *count)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_NON_NULL(count, "Count");
	*count = packet->discarded_event_count.value;
	return packet->discarded_event_count.avail;
}

enum bt_packet_property_availability bt_packet_get_discarded_packet_count(
		struct bt_packet *packet, uint64_t *count)
{
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_NON_NULL(count, "Count");
	*count = packet->discarded_packet_count.value;
	return packet->discarded_packet_count.avail;
}

