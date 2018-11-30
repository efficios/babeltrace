/*
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

#define BT_LOG_TAG "EVENT"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/trace-ir/fields-internal.h>
#include <babeltrace/trace-ir/field-classes-internal.h>
#include <babeltrace/trace-ir/clock-class.h>
#include <babeltrace/trace-ir/clock-value.h>
#include <babeltrace/trace-ir/clock-value-internal.h>
#include <babeltrace/trace-ir/clock-class-internal.h>
#include <babeltrace/trace-ir/private-event.h>
#include <babeltrace/trace-ir/event-internal.h>
#include <babeltrace/trace-ir/event-class.h>
#include <babeltrace/trace-ir/event-class-internal.h>
#include <babeltrace/trace-ir/stream-class.h>
#include <babeltrace/trace-ir/stream-class-internal.h>
#include <babeltrace/trace-ir/stream-internal.h>
#include <babeltrace/trace-ir/packet.h>
#include <babeltrace/trace-ir/packet-internal.h>
#include <babeltrace/trace-ir/trace.h>
#include <babeltrace/trace-ir/trace-internal.h>
#include <babeltrace/trace-ir/packet-internal.h>
#include <babeltrace/object.h>
#include <babeltrace/trace-ir/attributes-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/assert-internal.h>
#include <inttypes.h>

BT_HIDDEN
void _bt_event_set_is_frozen(struct bt_event *event, bool is_frozen)
{
	BT_ASSERT(event);
	BT_LIB_LOGD("Setting event's frozen state: %!+e, is-frozen=%d",
		event, is_frozen);

	if (event->header_field) {
		BT_LOGD_STR("Setting event's header field's frozen state.");
		bt_field_set_is_frozen(
			event->header_field->field, is_frozen);
	}

	if (event->common_context_field) {
		BT_LOGD_STR("Setting event's common context field's frozen state.");
		bt_field_set_is_frozen(
			event->common_context_field, is_frozen);
	}

	if (event->specific_context_field) {
		BT_LOGD_STR("Setting event's specific context field's frozen state.");
		bt_field_set_is_frozen(event->specific_context_field,
			is_frozen);
	}

	if (event->payload_field) {
		BT_LOGD_STR("Setting event's payload field's frozen state.");
		bt_field_set_is_frozen(event->payload_field,
			is_frozen);
	}

	event->frozen = is_frozen;
	BT_LOGD_STR("Setting event's packet's frozen state.");
	bt_packet_set_is_frozen(event->packet, is_frozen);
}

static
void recycle_event_header_field(struct bt_field_wrapper *field_wrapper,
		struct bt_stream_class *stream_class)
{
	BT_ASSERT(field_wrapper);
	BT_LIB_LOGD("Recycling event header field: "
		"addr=%p, %![sc-]+S, %![field-]+f", field_wrapper,
		stream_class, field_wrapper->field);
	bt_object_pool_recycle_object(
		&stream_class->event_header_field_pool,
		field_wrapper);
}

static inline
struct bt_field_wrapper *create_event_header_field(
		struct bt_stream_class *stream_class)
{
	struct bt_field_wrapper *field_wrapper = NULL;

	field_wrapper = bt_field_wrapper_create(
		&stream_class->event_header_field_pool,
		bt_stream_class_borrow_event_header_field_class(stream_class));
	if (!field_wrapper) {
		goto error;
	}

	goto end;

error:
	if (field_wrapper) {
		recycle_event_header_field(field_wrapper, stream_class);
		field_wrapper = NULL;
	}

end:
	return field_wrapper;
}

BT_HIDDEN
struct bt_event *bt_event_new(struct bt_event_class *event_class)
{
	struct bt_event *event = NULL;
	struct bt_stream_class *stream_class;
	struct bt_field_class *fc;

	BT_ASSERT(event_class);
	event = g_new0(struct bt_event, 1);
	if (!event) {
		BT_LOGE_STR("Failed to allocate one event.");
		goto error;
	}

	bt_object_init_unique(&event->base);
	stream_class = bt_event_class_borrow_stream_class(event_class);
	BT_ASSERT(stream_class);

	if (bt_stream_class_borrow_event_header_field_class(stream_class)) {
		event->header_field = create_event_header_field(stream_class);
		if (!event->header_field) {
			BT_LOGE_STR("Cannot create event header field.");
			goto error;
		}
	}

	fc = bt_stream_class_borrow_event_common_context_field_class(
		stream_class);
	if (fc) {
		event->common_context_field = bt_field_create(fc);
		if (!event->common_context_field) {
			/* bt_field_create() logs errors */
			goto error;
		}
	}

	fc = bt_event_class_borrow_specific_context_field_class(event_class);
	if (fc) {
		event->specific_context_field = bt_field_create(fc);
		if (!event->specific_context_field) {
			/* bt_field_create() logs errors */
			goto error;
		}
	}

	fc = bt_event_class_borrow_payload_field_class(event_class);
	if (fc) {
		event->payload_field = bt_field_create(fc);
		if (!event->payload_field) {
			/* bt_field_create() logs errors */
			goto error;
		}
	}

	if (stream_class->default_clock_class) {
		event->default_cv = bt_clock_value_create(
			stream_class->default_clock_class);
		if (!event->default_cv) {
			/* bt_clock_value_create() logs errors */
			goto error;
		}
	}

	goto end;

error:
	if (event) {
		bt_event_destroy(event);
		event = NULL;
	}

end:
	return event;
}

struct bt_event_class *bt_event_borrow_class(struct bt_event *event)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	return event->class;
}

struct bt_stream *bt_event_borrow_stream(struct bt_event *event)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	return event->packet ? event->packet->stream : NULL;
}

struct bt_field *bt_event_borrow_header_field(struct bt_event *event)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	return event->header_field ? event->header_field->field : NULL;
}

struct bt_private_field *bt_private_event_borrow_header_field(
		struct bt_private_event *event)
{
	return (void *) bt_event_borrow_header_field((void *) event);
}

struct bt_field *bt_event_borrow_common_context_field(struct bt_event *event)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	return event->common_context_field;
}

struct bt_private_field *bt_private_event_borrow_common_context_field(
		struct bt_private_event *event)
{
	return (void *) bt_event_borrow_common_context_field((void *) event);
}

struct bt_field *bt_event_borrow_specific_context_field(struct bt_event *event)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	return event->specific_context_field;
}

struct bt_private_field *bt_private_event_borrow_specific_context_field(
		struct bt_private_event *event)
{
	return (void *) bt_event_borrow_specific_context_field((void *) event);
}

struct bt_field *bt_event_borrow_payload_field(struct bt_event *event)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	return event->payload_field;
}

struct bt_private_field *bt_private_event_borrow_payload_field(
		struct bt_private_event *event)
{
	return (void *) bt_event_borrow_payload_field((void *) event);
}

static
void release_event_header_field(struct bt_field_wrapper *field_wrapper,
		struct bt_event *event)
{
	if (!event->class) {
		bt_field_wrapper_destroy(field_wrapper);
	} else {
		struct bt_stream_class *stream_class =
			bt_event_class_borrow_stream_class(event->class);

		BT_ASSERT(stream_class);
		recycle_event_header_field(field_wrapper, stream_class);
	}
}

BT_HIDDEN
void bt_event_destroy(struct bt_event *event)
{
	BT_ASSERT(event);
	BT_LIB_LOGD("Destroying event: %!+e", event);

	if (event->header_field) {
		BT_LOGD_STR("Releasing event's header field.");
		release_event_header_field(event->header_field, event);
	}

	if (event->common_context_field) {
		BT_LOGD_STR("Destroying event's stream event context field.");
		bt_field_destroy(event->common_context_field);
	}

	if (event->specific_context_field) {
		BT_LOGD_STR("Destroying event's context field.");
		bt_field_destroy(event->specific_context_field);
	}

	if (event->payload_field) {
		BT_LOGD_STR("Destroying event's payload field.");
		bt_field_destroy(event->payload_field);
	}

	BT_LOGD_STR("Putting event's class.");
	bt_object_put_ref(event->class);

	if (event->default_cv) {
		bt_clock_value_recycle(event->default_cv);
	}

	BT_LOGD_STR("Putting event's packet.");
	bt_object_put_ref(event->packet);
	g_free(event);
}

void bt_private_event_set_default_clock_value(
		struct bt_private_event *priv_event, uint64_t value_cycles)
{
	struct bt_event *event = (void *) priv_event;
	struct bt_stream_class *sc;

	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_EVENT_HOT(event);
	sc = bt_event_class_borrow_stream_class_inline(event->class);
	BT_ASSERT(sc);
	BT_ASSERT_PRE(sc->default_clock_class,
		"Event's stream class has no default clock class: "
		"%![ev-]+e, %![sc-]+S", event, sc);
	BT_ASSERT(event->default_cv);
	bt_clock_value_set_value_inline(event->default_cv, value_cycles);
	BT_LIB_LOGV("Set event's default clock value: %![event-]+e, "
		"value=%" PRIu64, event, value_cycles);
}

enum bt_clock_value_status bt_event_borrow_default_clock_value(
		struct bt_event *event, struct bt_clock_value **clock_value)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_NON_NULL(clock_value, "Clock value (output)");
	*clock_value = event->default_cv;
	return BT_CLOCK_VALUE_STATUS_KNOWN;
}

struct bt_packet *bt_event_borrow_packet(struct bt_event *event)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	return event->packet;
}

struct bt_private_packet *bt_private_event_borrow_packet(
		struct bt_private_event *event)
{
	return (void *) bt_event_borrow_packet((void *) event);
}

int bt_private_event_move_header_field(
		struct bt_private_event *priv_event,
		struct bt_private_event_header_field *priv_header_field)
{
	struct bt_stream_class *stream_class;
	struct bt_event *event = (void *) priv_event;
	struct bt_event_class *event_class = (void *) event_class;
	struct bt_field_wrapper *field_wrapper = (void *) priv_header_field;

	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_NON_NULL(field_wrapper, "Header field");
	BT_ASSERT_PRE_EVENT_HOT(event);
	stream_class = bt_event_class_borrow_stream_class_inline(event->class);
	BT_ASSERT_PRE(stream_class->event_header_fc,
		"Stream class has no event header field classe: %!+S",
		stream_class);

	/* Recycle current header field: always exists */
	BT_ASSERT(event->header_field);
	recycle_event_header_field(event->header_field, stream_class);

	/* Move new field */
	event->header_field = field_wrapper;
	return 0;
}
