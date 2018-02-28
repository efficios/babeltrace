/*
 * Copyright 2018 Philippe Proulx <pproulx@efficios.com>
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <wchar.h>
#include <glib.h>
#include <babeltrace/common-internal.h>
#include <babeltrace/lib-logging-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/ref-internal.h>
#include <babeltrace/values-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/event-class-internal.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ctf-ir/packet-internal.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/stream-internal.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/ctf-ir/clock-class-internal.h>
#include <babeltrace/ctf-ir/clock-value-internal.h>
#include <babeltrace/ctf-ir/field-path-internal.h>
#include <babeltrace/graph/clock-class-priority-map-internal.h>
#include <babeltrace/graph/component-class-internal.h>
#include <babeltrace/graph/component-class-sink-colander-internal.h>
#include <babeltrace/graph/component-filter-internal.h>
#include <babeltrace/graph/component-internal.h>
#include <babeltrace/graph/component-sink-internal.h>
#include <babeltrace/graph/component-source-internal.h>
#include <babeltrace/graph/connection-internal.h>
#include <babeltrace/graph/graph-internal.h>
#include <babeltrace/graph/notification-discarded-elements-internal.h>
#include <babeltrace/graph/notification-event-internal.h>
#include <babeltrace/graph/notification-heap-internal.h>
#include <babeltrace/graph/notification-inactivity-internal.h>
#include <babeltrace/graph/notification-internal.h>
#include <babeltrace/graph/notification-iterator-internal.h>
#include <babeltrace/graph/notification-packet-internal.h>
#include <babeltrace/graph/notification-stream-internal.h>
#include <babeltrace/graph/port-internal.h>
#include <babeltrace/plugin/plugin-internal.h>
#include <babeltrace/plugin/plugin-so-internal.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/ctf-writer/clock-internal.h>

#define LIB_LOGGING_BUF_SIZE	(4096 * 4)

static char __thread lib_logging_buf[LIB_LOGGING_BUF_SIZE];

#define BUF_APPEND(_fmt, ...)						\
	do {								\
		int _count;						\
		size_t _size = LIB_LOGGING_BUF_SIZE -			\
				(size_t) (*buf_ch - lib_logging_buf);	\
		_count = snprintf(*buf_ch, _size, (_fmt), __VA_ARGS__);	\
		BT_ASSERT(_count >= 0);					\
		*buf_ch += MIN(_count, _size);				\
		if (*buf_ch >= lib_logging_buf + LIB_LOGGING_BUF_SIZE - 1) { \
			return;						\
		}							\
	} while (0)

#define BUF_APPEND_UUID(_uuid)						\
	BUF_APPEND(", %suuid=\"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\"", \
		prefix,							\
		(unsigned int) (_uuid)[0],				\
		(unsigned int) (_uuid)[1],				\
		(unsigned int) (_uuid)[2],				\
		(unsigned int) (_uuid)[3],				\
		(unsigned int) (_uuid)[4],				\
		(unsigned int) (_uuid)[5],				\
		(unsigned int) (_uuid)[6],				\
		(unsigned int) (_uuid)[7],				\
		(unsigned int) (_uuid)[8],				\
		(unsigned int) (_uuid)[9],				\
		(unsigned int) (_uuid)[10],				\
		(unsigned int) (_uuid)[11],				\
		(unsigned int) (_uuid)[12],				\
		(unsigned int) (_uuid)[13],				\
		(unsigned int) (_uuid)[14],				\
		(unsigned int) (_uuid)[15])

#define PRFIELD(_expr)	prefix, (_expr)

#define SET_TMP_PREFIX(_prefix2)					\
	do {								\
		strcpy(tmp_prefix, prefix);				\
		strcat(tmp_prefix, (_prefix2));				\
	} while (0)

static inline void format_component(char **buf_ch, bool extended,
		const char *prefix, struct bt_component *component);

static inline void format_port(char **buf_ch, bool extended,
		const char *prefix, struct bt_port *port);

static inline void format_connection(char **buf_ch, bool extended,
		const char *prefix, struct bt_connection *connection);

static inline void format_ref_count(char **buf_ch, bool extended,
		const char *prefix, struct bt_object *obj)
{
	BUF_APPEND(", %sref-count=%lu", prefix, obj->ref_count.count);
}

static inline void format_field_type(char **buf_ch, bool extended,
		const char *prefix, struct bt_field_type *field_type)
{
	BUF_APPEND(", %stype-id=%s, %salignment=%u",
		PRFIELD(bt_field_type_id_string(field_type->id)),
		PRFIELD(field_type->alignment));

	if (extended) {
		BUF_APPEND(", %sis-frozen=%d", PRFIELD(field_type->frozen));
	} else {
		return;
	}

	if (field_type->id == BT_FIELD_TYPE_ID_UNKNOWN) {
		return;
	}

	switch (field_type->id) {
	case BT_FIELD_TYPE_ID_INTEGER:
	{
		struct bt_field_type_integer *integer = (void *) field_type;

		BUF_APPEND(", %ssize=%u, %sis-signed=%d, %sbyte-order=%s, "
			"%sbase=%d, %sencoding=%s, "
			"%smapped-clock-class-addr=%p",
			PRFIELD(integer->size), PRFIELD(integer->is_signed),
			PRFIELD(bt_byte_order_string(integer->user_byte_order)),
			PRFIELD(integer->base),
			PRFIELD(bt_string_encoding_string(integer->encoding)),
			PRFIELD(integer->mapped_clock));

		if (integer->mapped_clock) {
			BUF_APPEND(", %smapped-clock-class-name=\"%s\"",
				PRFIELD(bt_clock_class_get_name(integer->mapped_clock)));
		}
		break;
	}
	case BT_FIELD_TYPE_ID_FLOAT:
	{
		struct bt_field_type_floating_point *flt = (void *) field_type;

		BUF_APPEND(", %sexp-dig=%u, %smant-dig=%u, %sbyte-order=%s",
			PRFIELD(flt->exp_dig), PRFIELD(flt->mant_dig),
			PRFIELD(bt_byte_order_string(flt->user_byte_order)));
		break;
	}
	case BT_FIELD_TYPE_ID_ENUM:
	{
		struct bt_field_type_enumeration *enm = (void *) field_type;

		BUF_APPEND(", %smapping-count=%u",
			PRFIELD(enm->entries->len));
		break;
	}
	case BT_FIELD_TYPE_ID_STRING:
	{
		struct bt_field_type_string *str = (void *) field_type;

		BUF_APPEND(", %sencoding=%s",
			PRFIELD(bt_string_encoding_string(str->encoding)));
		break;
	}
	case BT_FIELD_TYPE_ID_STRUCT:
	{
		struct bt_field_type_structure *structure = (void *) field_type;

		BUF_APPEND(", %sfield-count=%u",
			PRFIELD(structure->fields->len));
		break;
	}
	case BT_FIELD_TYPE_ID_SEQUENCE:
	{
		struct bt_field_type_sequence *seq = (void *) field_type;

		BUF_APPEND(", %slength-ft-addr=\"%s\", %selem-ft-addr=%p",
			PRFIELD(seq->length_field_name->str),
			PRFIELD(seq->element_type));
		break;
	}
	case BT_FIELD_TYPE_ID_VARIANT:
	{
		struct bt_field_type_variant *variant = (void *) field_type;

		BUF_APPEND(", %stag-name=\"%s\", %stag-ft-addr=%p, "
			"%sfield-count=%u",
			PRFIELD(variant->tag_name->str),
			PRFIELD(variant->tag), PRFIELD(variant->fields->len));
		break;
	}
	default:
		break;
	}
}

static inline void format_field_integer_extended(char **buf_ch,
		const char *prefix, struct bt_field *field)
{
	struct bt_field_integer *integer = (void *) field;
	struct bt_field_type_integer *field_type = (void *) field->type;
	const char *fmt = NULL;

	if (field_type->base == 8) {
		fmt = ", %svalue=%" PRIo64;
	} else if (field_type->base == 16) {
		fmt = ", %svalue=%" PRIx64;
	}

	if (field_type->is_signed) {
		if (!fmt) {
			fmt = ", %svalue=%" PRId64;
		}

		BUF_APPEND(fmt, PRFIELD(integer->payload.signd));
	} else {
		if (!fmt) {
			fmt = ", %svalue=%" PRIu64;
		}

		BUF_APPEND(fmt, PRFIELD(integer->payload.unsignd));
	}
}

static inline void format_field(char **buf_ch, bool extended,
		const char *prefix, struct bt_field *field)
{
	BUF_APPEND(", %sis-set=%d", PRFIELD(field->payload_set));

	if (extended) {
		BUF_APPEND(", %sis-frozen=%d", PRFIELD(field->frozen));
	}

	BUF_APPEND(", %stype-addr=%p, %stype-id=%s",
		PRFIELD(field->type),
		PRFIELD(bt_field_type_id_string(field->type->id)));

	if (!extended || field->type->id == BT_FIELD_TYPE_ID_UNKNOWN ||
			!field->payload_set) {
		return;
	}

	switch (field->type->id) {
	case BT_FIELD_TYPE_ID_INTEGER:
	{
		format_field_integer_extended(buf_ch, prefix, field);
		break;
	}
	case BT_FIELD_TYPE_ID_FLOAT:
	{
		struct bt_field_floating_point *flt = (void *) field;

		BUF_APPEND(", %svalue=%f", PRFIELD(flt->payload));
		break;
	}
	case BT_FIELD_TYPE_ID_ENUM:
	{
		struct bt_field_enumeration *enm = (void *) field;

		if (enm->payload) {
			format_field_integer_extended(buf_ch, prefix,
				enm->payload);
		}
		break;
	}
	case BT_FIELD_TYPE_ID_STRING:
	{
		struct bt_field_string *str = (void *) field;

		BT_ASSERT(str->payload);
		BUF_APPEND(", %spartial-value=\"%.32s\"",
			PRFIELD(str->payload->str));
		break;
	}
	case BT_FIELD_TYPE_ID_SEQUENCE:
	{
		struct bt_field_sequence *seq = (void *) field;

		BUF_APPEND(", %slength-field-addr=%p", PRFIELD(seq->length));
		break;
	}
	case BT_FIELD_TYPE_ID_VARIANT:
	{
		struct bt_field_variant *variant = (void *) field;

		BUF_APPEND(", %stag-field-addr=%p, %svalue-field-addr=%p",
			PRFIELD(variant->tag), PRFIELD(variant->payload));
		break;
	}
	default:
		break;
	}
}

static inline void format_field_path(char **buf_ch, bool extended,
		const char *prefix, struct bt_field_path *field_path)
{
	uint64_t i;

	BT_ASSERT(field_path->indexes);
	BUF_APPEND(", %sindex-count=%u", PRFIELD(field_path->indexes->len));

	if (!extended) {
		return;
	}

	BUF_APPEND(", %spath=[%s", PRFIELD(bt_scope_string(field_path->root)));

	for (i = 0; i < field_path->indexes->len; i++) {
		int index = g_array_index(field_path->indexes, int, i);

		BUF_APPEND(", %d", index);
	}

	BUF_APPEND("%s", "]");
}

static inline void format_trace(char **buf_ch, bool extended,
		const char *prefix, struct bt_trace *trace)
{
	if (trace->name) {
		BUF_APPEND(", %sname=\"%s\"", PRFIELD(trace->name->str));
	}

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-frozen=%d, %sis-static=%d",
		PRFIELD(trace->frozen), PRFIELD(trace->is_static));

	if (trace->uuid_set) {
		BUF_APPEND_UUID(trace->uuid);
	}

	BUF_APPEND(", %sclock-class-count=%u, %sstream-class-count=%u, "
		"%sstream-count=%u, %sis-ctf-writer-trace=%d, "
		"%spacket-header-ft-addr=%p",
		PRFIELD(trace->clocks->len),
		PRFIELD(trace->stream_classes->len),
		PRFIELD(trace->streams->len),
		PRFIELD(trace->is_created_by_writer),
		PRFIELD(trace->packet_header_type));
}

static inline void format_stream_class(char **buf_ch, bool extended,
		const char *prefix, struct bt_stream_class *stream_class)
{
	struct bt_trace *trace;
	char tmp_prefix[64];

	if (stream_class->id_set) {
		BUF_APPEND(", %sid=%" PRId64, PRFIELD(stream_class->id));
	}

	if (stream_class->name) {
		BUF_APPEND(", %sname=\"%s\"", PRFIELD(stream_class->name->str));
	}

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-frozen=%d", PRFIELD(stream_class->frozen));

	if (stream_class->clock) {
		BUF_APPEND(", %sctf-writer-clock-addr=%p, "
			"%sctf-writer-clock-name=\"%s\"",
			PRFIELD(stream_class->clock),
			PRFIELD(bt_clock_class_get_name(stream_class->clock->clock_class)));
	}

	BUF_APPEND(", %sevent-class-count=%u, %spacket-context-ft-addr=%p, "
		"%sevent-header-ft-addr=%p, %sevent-context-ft-addr=%p",
		PRFIELD(stream_class->event_classes->len),
		PRFIELD(stream_class->packet_context_type),
		PRFIELD(stream_class->event_header_type),
		PRFIELD(stream_class->event_context_type));
	trace = bt_stream_class_borrow_trace(stream_class);
	if (!trace) {
		return;
	}

	BUF_APPEND(", %strace-addr=%p", PRFIELD(trace));
	SET_TMP_PREFIX("trace-");
	format_trace(buf_ch, false, tmp_prefix, trace);
}

static inline void format_event_class(char **buf_ch, bool extended,
		const char *prefix, struct bt_event_class *event_class)
{
	struct bt_stream_class *stream_class;
	struct bt_trace *trace;
	char tmp_prefix[64];

	BUF_APPEND(", %sid=%" PRId64, PRFIELD(event_class->id));

	if (event_class->name) {
		BUF_APPEND(", %sname=\"%s\"", PRFIELD(event_class->name->str));
	}

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-frozen=%d, %slog-level=%s",
		PRFIELD(event_class->frozen),
		PRFIELD(bt_event_class_log_level_string(event_class->log_level)));

	if (event_class->emf_uri) {
		BUF_APPEND(", %semf-uri=\"%s\"",
			PRFIELD(event_class->emf_uri->str));
	}

	BUF_APPEND(", %scontext-ft-addr=%p, %spayload-ft-addr=%p",
		PRFIELD(event_class->context),
		PRFIELD(event_class->fields));

	stream_class = bt_event_class_borrow_stream_class(event_class);
	if (!stream_class) {
		return;
	}

	BUF_APPEND(", %sstream-class-addr=%p", PRFIELD(stream_class));
	SET_TMP_PREFIX("stream-class-");
	format_stream_class(buf_ch, false, tmp_prefix, stream_class);
	trace = bt_stream_class_borrow_trace(stream_class);
	if (!trace) {
		return;
	}

	BUF_APPEND(", %strace-addr=%p", PRFIELD(trace));
	SET_TMP_PREFIX("trace-");
	format_trace(buf_ch, false, tmp_prefix, trace);
}

static inline void format_stream(char **buf_ch, bool extended,
		const char *prefix, struct bt_stream *stream)
{
	struct bt_stream_class *stream_class;
	struct bt_trace *trace;
	char tmp_prefix[64];

	BUF_APPEND(", %sid=%" PRId64, PRFIELD(stream->id));

	if (stream->name) {
		BUF_APPEND(", %sname=\"%s\"", PRFIELD(stream->name->str));
	}

	BUF_APPEND(", %sis-ctf-writer-stream=%d",
		PRFIELD(stream->pos.fd != -1));

	if (!extended) {
		return;
	}

	if (stream->pos.fd != -1) {
		BUF_APPEND(", %sheader-field-addr=%p, %scontext-field-addr=%p"
			", %sfd=%d, %smmap-offset=%zu, "
			"%smmap-base-offset=%zu, %spacket-size=%" PRIu64 ", "
			"%soffset=%" PRId64 ", %sevent-count=%u, "
			"%sflushed-packet-count=%u, "
			"%sdiscarded-event-count=%" PRIu64 ", "
			"%ssize=%" PRIu64 ", %slast-ts-end=%" PRIu64,
			PRFIELD(stream->packet_header),
			PRFIELD(stream->packet_context),
			PRFIELD(stream->pos.fd),
			PRFIELD((size_t) stream->pos.mmap_offset),
			PRFIELD((size_t) stream->pos.mmap_base_offset),
			PRFIELD(stream->pos.packet_size),
			PRFIELD(stream->pos.offset),
			PRFIELD(stream->events->len),
			PRFIELD(stream->flushed_packet_count),
			PRFIELD(stream->discarded_events),
			PRFIELD(stream->size), PRFIELD(stream->last_ts_end));
	}

	stream_class = bt_stream_borrow_stream_class(stream);
	if (!stream_class) {
		return;
	}

	BUF_APPEND(", %sstream-class-addr=%p", PRFIELD(stream_class));
	SET_TMP_PREFIX("stream-class-");
	format_stream_class(buf_ch, false, tmp_prefix, stream_class);
	trace = bt_stream_class_borrow_trace(stream_class);
	if (!trace) {
		return;
	}

	trace = (struct bt_trace *) bt_object_borrow_parent(stream);
	if (!trace) {
		return;
	}

	BUF_APPEND(", %strace-addr=%p", PRFIELD(trace));
	SET_TMP_PREFIX("trace-");
	format_trace(buf_ch, false, tmp_prefix, trace);
}

static inline void format_packet(char **buf_ch, bool extended,
		const char *prefix, struct bt_packet *packet)
{
	struct bt_stream *stream;
	struct bt_trace *trace;
	char tmp_prefix[64];

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-frozen=%d, %sheader-field-addr=%p, "
		"%scontext-field-addr=%p",
		PRFIELD(packet->frozen),
		PRFIELD(packet->header),
		PRFIELD(packet->context));
	stream = bt_packet_borrow_stream(packet);
	if (!stream) {
		return;
	}

	BUF_APPEND(", %sstream-addr=%p", PRFIELD(stream));
	SET_TMP_PREFIX("stream-");
	format_stream(buf_ch, false, tmp_prefix, stream);
	trace = (struct bt_trace *) bt_object_borrow_parent(stream);
	if (!trace) {
		return;
	}

	BUF_APPEND(", %strace-addr=%p", PRFIELD(trace));
	SET_TMP_PREFIX("trace-");
	format_trace(buf_ch, false, tmp_prefix, trace);
}

static inline void format_event(char **buf_ch, bool extended,
		const char *prefix, struct bt_event *event)
{
	struct bt_trace *trace;
	struct bt_stream_class *stream_class;
	struct bt_packet *packet;
	struct bt_stream *stream;
	char tmp_prefix[64];

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-frozen=%d, %sheader-field-addr=%p, "
		"%sstream-context-field-addr=%p, "
		"%scontext-field-addr=%p, %spayload-field-addr=%p, "
		"%sclock-value-count=%u",
		PRFIELD(event->frozen),
		PRFIELD(event->event_header),
		PRFIELD(event->stream_event_context),
		PRFIELD(event->context_payload),
		PRFIELD(event->fields_payload),
		PRFIELD(g_hash_table_size(event->clock_values)));
	BUF_APPEND(", %sevent-class-addr=%p", PRFIELD(event->event_class));
	SET_TMP_PREFIX("event-class-");
	format_event_class(buf_ch, false, tmp_prefix, event->event_class);
	stream_class = bt_event_class_borrow_stream_class(event->event_class);
	if (stream_class) {
		BUF_APPEND(", %sstream-class-addr=%p", PRFIELD(stream_class));
		SET_TMP_PREFIX("stream-class-");
		format_stream_class(buf_ch, false, tmp_prefix, stream_class);

		trace = bt_stream_class_borrow_trace(stream_class);
		if (trace) {
			BUF_APPEND(", %strace-addr=%p", PRFIELD(trace));
			SET_TMP_PREFIX("trace-");
			format_trace(buf_ch, false, tmp_prefix, trace);
		}
	}

	packet = bt_event_borrow_packet(event);
	if (!packet) {
		return;
	}

	BUF_APPEND(", %spacket-addr=%p", PRFIELD(packet));
	SET_TMP_PREFIX("packet-");
	format_packet(buf_ch, false, tmp_prefix, packet);
	stream = bt_packet_borrow_stream(packet);
	if (!stream) {
		return;
	}

	BUF_APPEND(", %sstream-addr=%p", PRFIELD(stream));
	SET_TMP_PREFIX("stream-");
	format_stream(buf_ch, false, tmp_prefix, stream);
}

static inline void format_clock_class(char **buf_ch, bool extended,
		const char *prefix, struct bt_clock_class *clock_class)
{
	BUF_APPEND(", %sname=\"%s\", %sfreq=%" PRIu64,
		PRFIELD(clock_class->name->str),
		PRFIELD(clock_class->frequency));

	if (!extended) {
		return;
	}

	if (clock_class->description) {
		BUF_APPEND(", %spartial-description=\"%.32s\"",
			PRFIELD(clock_class->description->str));
	}

	BUF_APPEND(", %sis-frozen=%d, %sprecision=%" PRIu64 ", "
		"%soffset-s=%" PRId64 ", "
		"%soffset-cycles=%" PRId64 ", %sis-absolute=%d",
		PRFIELD(clock_class->frozen), PRFIELD(clock_class->precision),
		PRFIELD(clock_class->offset_s), PRFIELD(clock_class->offset),
		PRFIELD(clock_class->absolute));

	if (clock_class->uuid_set) {
		BUF_APPEND_UUID(clock_class->uuid);
	}
}

static inline void format_clock_value(char **buf_ch, bool extended,
		const char *prefix, struct bt_clock_value *clock_value)
{
	char tmp_prefix[64];
	BUF_APPEND(", %svalue=%" PRIu64 ", %sns-from-epoch=%" PRId64,
		PRFIELD(clock_value->value),
		PRFIELD(clock_value->ns_from_epoch));

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sclock-class-addr=%p",
		PRFIELD(clock_value->clock_class));
	SET_TMP_PREFIX("clock-class-");
	format_clock_class(buf_ch, false, tmp_prefix, clock_value->clock_class);
}

static inline void format_value(char **buf_ch, bool extended,
		const char *prefix, struct bt_value *value)
{
	BUF_APPEND(", %stype=%s",
		PRFIELD(bt_value_type_string(bt_value_get_type(value))));

	if (!extended) {
		return;
	}

	switch (bt_value_get_type(value)) {
	case BT_VALUE_TYPE_BOOL:
	{
		bt_bool val;

		(void) bt_value_bool_get(value, &val);
		BUF_APPEND(", %svalue=%d", PRFIELD(val));
		break;
	}
	case BT_VALUE_TYPE_INTEGER:
	{
		int64_t val;

		(void) bt_value_integer_get(value, &val);
		BUF_APPEND(", %svalue=%" PRId64, PRFIELD(val));
		break;
	}
	case BT_VALUE_TYPE_FLOAT:
	{
		double val;

		(void) bt_value_float_get(value, &val);
		BUF_APPEND(", %svalue=%f", PRFIELD(val));
		break;
	}
	case BT_VALUE_TYPE_STRING:
	{
		const char *val;

		(void) bt_value_string_get(value, &val);
		BUF_APPEND(", %spartial-value=\"%.32s\"", PRFIELD(val));
		break;
	}
	case BT_VALUE_TYPE_ARRAY:
	{
		int64_t count = bt_value_array_size(value);

		BT_ASSERT(count >= 0);
		BUF_APPEND(", %selement-count=%" PRId64, PRFIELD(count));
		break;
	}
	case BT_VALUE_TYPE_MAP:
	{
		int64_t count = bt_value_map_size(value);

		BT_ASSERT(count >= 0);
		BUF_APPEND(", %selement-count=%" PRId64, PRFIELD(count));
		break;
	}
	default:
		break;
	}
}

static inline void format_notification(char **buf_ch, bool extended,
		const char *prefix, struct bt_notification *notif)
{
	char tmp_prefix[64];

	BUF_APPEND(", %stype=%s",
		PRFIELD(bt_notification_type_string(notif->type)));

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-frozen=%d", PRFIELD(notif->frozen));

	switch (notif->type) {
	case BT_NOTIFICATION_TYPE_EVENT:
	{
		struct bt_notification_event *notif_event = (void *) notif;

		SET_TMP_PREFIX("event-");
		format_event(buf_ch, true, tmp_prefix, notif_event->event);

		if (!notif_event->cc_prio_map) {
			return;
		}

		BUF_APPEND(", %scc-prio-map-addr=%p, %scc-prio-map-cc-count=%u",
			PRFIELD(notif_event->cc_prio_map),
			PRFIELD(notif_event->cc_prio_map->entries->len));
		break;
	}
	case BT_NOTIFICATION_TYPE_INACTIVITY:
	{
		struct bt_notification_inactivity *notif_inactivity =
			(void *) notif;

		if (!notif_inactivity->cc_prio_map) {
			return;
		}

		BUF_APPEND(", %scc-prio-map-addr=%p, %scc-prio-map-cc-count=%u",
			PRFIELD(notif_inactivity->cc_prio_map),
			PRFIELD(notif_inactivity->cc_prio_map->entries->len));
		break;
	}
	case BT_NOTIFICATION_TYPE_STREAM_BEGIN:
	{
		struct bt_notification_stream_begin *notif_stream =
			(void *) notif;

		SET_TMP_PREFIX("stream-");
		format_stream(buf_ch, true, tmp_prefix, notif_stream->stream);
		break;
	}
	case BT_NOTIFICATION_TYPE_STREAM_END:
	{
		struct bt_notification_stream_end *notif_stream =
			(void *) notif;

		SET_TMP_PREFIX("stream-");
		format_stream(buf_ch, true, tmp_prefix, notif_stream->stream);
		break;
	}
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
	{
		struct bt_notification_packet_begin *notif_packet =
			(void *) notif;

		SET_TMP_PREFIX("packet-");
		format_packet(buf_ch, true, tmp_prefix, notif_packet->packet);
		break;
	}
	case BT_NOTIFICATION_TYPE_PACKET_END:
	{
		struct bt_notification_packet_end *notif_packet =
			(void *) notif;

		SET_TMP_PREFIX("packet-");
		format_packet(buf_ch, true, tmp_prefix, notif_packet->packet);
		break;
	}
	case BT_NOTIFICATION_TYPE_DISCARDED_EVENTS:
	case BT_NOTIFICATION_TYPE_DISCARDED_PACKETS:
	{
		struct bt_notification_discarded_elements *notif_discarded =
			(void *) notif;

		BUF_APPEND(", %scount=%" PRId64,
			PRFIELD(notif_discarded->count));

		if (notif_discarded->begin_clock_value) {
			SET_TMP_PREFIX("begin-clock-value-");
			format_clock_value(buf_ch, true, tmp_prefix,
				notif_discarded->begin_clock_value);
		}

		if (notif_discarded->end_clock_value) {
			SET_TMP_PREFIX("end-clock-value-");
			format_clock_value(buf_ch, true, tmp_prefix,
				notif_discarded->end_clock_value);
		}
		break;
	}
	default:
		break;
	}
}

static inline void format_plugin_so_shared_lib_handle(char **buf_ch,
		const char *prefix,
		struct bt_plugin_so_shared_lib_handle *handle)
{
	BUF_APPEND(", %saddr=%p", PRFIELD(handle));

	if (handle->path) {
		BUF_APPEND(", %spath=\"%s\"", PRFIELD(handle->path->str));
	}
}

static inline void format_component_class(char **buf_ch, bool extended,
		const char *prefix, struct bt_component_class *comp_class)
{
	char tmp_prefix[64];

	BUF_APPEND(", %stype=%s, %sname=\"%s\"",
		PRFIELD(bt_component_class_type_string(comp_class->type)),
		PRFIELD(comp_class->name->str));

	if (comp_class->description) {
		BUF_APPEND(", %spartial-descr=\"%.32s\"",
			PRFIELD(comp_class->description->str));
	}

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-frozen=%d", PRFIELD(comp_class->frozen));

	if (comp_class->so_handle) {
		SET_TMP_PREFIX("so-handle-");
		format_plugin_so_shared_lib_handle(buf_ch, tmp_prefix,
			comp_class->so_handle);
	}
}

static inline void format_component(char **buf_ch, bool extended,
		const char *prefix, struct bt_component *component)
{
	char tmp_prefix[64];

	BUF_APPEND(", %sname=\"%s\"", PRFIELD(component->name->str));
	SET_TMP_PREFIX("class-");
	format_component_class(buf_ch, extended, tmp_prefix, component->class);

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sinput-port-count=%u, %soutput-port-count=%u",
		PRFIELD(component->input_ports ? component->input_ports->len : 0),
		PRFIELD(component->output_ports ? component->output_ports->len : 0));
}

static inline void format_port(char **buf_ch, bool extended,
		const char *prefix, struct bt_port *port)
{
	char tmp_prefix[64];

	BUF_APPEND(", %stype=%s, %sname=\"%s\"",
		PRFIELD(bt_port_type_string(port->type)),
		PRFIELD(port->name->str));

	if (!extended) {
		return;
	}

	if (port->connection) {
		SET_TMP_PREFIX("conn-");
		format_connection(buf_ch, false, tmp_prefix, port->connection);
	}
}

static inline void format_connection(char **buf_ch, bool extended,
		const char *prefix, struct bt_connection *connection)
{
	char tmp_prefix[64];

	if (!extended) {
		return;
	}

	if (connection->upstream_port) {
		SET_TMP_PREFIX("upstream-port-");
		format_port(buf_ch, false, tmp_prefix,
			connection->upstream_port);
	}

	if (connection->downstream_port) {
		SET_TMP_PREFIX("downstream-port-");
		format_port(buf_ch, false, tmp_prefix,
			connection->downstream_port);
	}
}

static inline void format_graph(char **buf_ch, bool extended,
		const char *prefix, struct bt_graph *graph)
{
	BUF_APPEND(", %sis-canceled=%d", PRFIELD(graph->canceled));

	if (!extended) {
		return;
	}

	BUF_APPEND(", %scomp-count=%u, %sconn-count=%u",
		PRFIELD(graph->components->len),
		PRFIELD(graph->connections->len));
}

static inline void format_notification_iterator(char **buf_ch,
		bool extended, const char *prefix,
		struct bt_notification_iterator *iterator)
{
	const char *type;
	char tmp_prefix[64];

	if (iterator->type == BT_NOTIFICATION_ITERATOR_TYPE_PRIVATE_CONNECTION) {
		type = "BT_NOTIFICATION_ITERATOR_TYPE_PRIVATE_CONNECTION";
	} else if (iterator->type == BT_NOTIFICATION_ITERATOR_TYPE_OUTPUT_PORT) {
		type = "BT_NOTIFICATION_ITERATOR_TYPE_OUTPUT_PORT";
	} else {
		type = "(unknown)";
	}

	BUF_APPEND(", %stype=%s", PRFIELD(type));

	switch (iterator->type) {
	case BT_NOTIFICATION_ITERATOR_TYPE_PRIVATE_CONNECTION:
	{
		struct bt_notification_iterator_private_connection *
			iter_priv_conn = (void *) iterator;

		if (iter_priv_conn->upstream_component) {
			SET_TMP_PREFIX("upstream-comp-");
			format_component(buf_ch, false, tmp_prefix,
				iter_priv_conn->upstream_component);
		}

		if (iter_priv_conn->upstream_port) {
			SET_TMP_PREFIX("upstream-port-");
			format_port(buf_ch, false, tmp_prefix,
				iter_priv_conn->upstream_port);
		}

		if (iter_priv_conn->connection) {
			SET_TMP_PREFIX("upstream-conn-");
			format_connection(buf_ch, false, tmp_prefix,
				iter_priv_conn->connection);
		}
		break;
	}
	case BT_NOTIFICATION_ITERATOR_TYPE_OUTPUT_PORT:
	{
		struct bt_notification_iterator_output_port *iter_output_port =
			(void *) iterator;

		SET_TMP_PREFIX("graph-");
		format_graph(buf_ch, false, tmp_prefix,
			iter_output_port->graph);
		SET_TMP_PREFIX("colander-comp-");
		format_component(buf_ch, false, tmp_prefix,
			iter_output_port->colander);
		SET_TMP_PREFIX("output-port-");
		format_port(buf_ch, false, tmp_prefix,
			iter_output_port->output_port);
		break;
	}
	default:
		break;
	}
}

static inline void format_plugin(char **buf_ch, bool extended,
		const char *prefix, struct bt_plugin *plugin)
{
	char tmp_prefix[64];

	BUF_APPEND(", %stype=%s", PRFIELD(bt_plugin_type_string(plugin->type)));

	if (plugin->info.path_set) {
		BUF_APPEND(", %spath=\"%s\"",
			PRFIELD(plugin->info.path->str));
	}

	if (plugin->info.name_set) {
		BUF_APPEND(", %sname=\"%s\"",
			PRFIELD(plugin->info.name->str));
	}

	if (!extended) {
		return;
	}

	if (plugin->info.author_set) {
		BUF_APPEND(", %sauthor=\"%s\"",
			PRFIELD(plugin->info.author->str));
	}

	if (plugin->info.license_set) {
		BUF_APPEND(", %slicense=\"%s\"",
			PRFIELD(plugin->info.license->str));
	}

	if (plugin->info.version_set) {
		BUF_APPEND(", %sversion=%u.%u.%u%s",
			PRFIELD(plugin->info.version.major),
			plugin->info.version.minor,
			plugin->info.version.patch,
			plugin->info.version.extra ?
				plugin->info.version.extra->str : "");
	}

	BUF_APPEND(", %sis-frozen=%d, %scomp-class-count=%u",
		PRFIELD(plugin->frozen),
		PRFIELD(plugin->comp_classes->len));

	if (plugin->spec_data) {
		struct bt_plugin_so_spec_data *spec_data =
			(void *) plugin->spec_data;

		if (spec_data->shared_lib_handle) {
			SET_TMP_PREFIX("so-handle-");
			format_plugin_so_shared_lib_handle(buf_ch, tmp_prefix,
				spec_data->shared_lib_handle);
		}
	}
}

static inline void format_ctf_writer(char **buf_ch, bool extended,
		const char *prefix, struct bt_ctf_writer *writer)
{
	/* TODO */
}

static inline void handle_conversion_specifier_bt(void *priv_data,
		char **buf_ch, size_t avail_size,
		const char **out_fmt_ch, va_list *args)
{
	const char *fmt_ch = *out_fmt_ch;
	bool extended = false;
	char prefix[64];
	char *prefix_ch = prefix;
	void *obj;

	/* skip "%!" */
	fmt_ch += 2;

	if (*fmt_ch == '[') {
		/* local prefix */
		fmt_ch++;

		while (true) {
			if (*fmt_ch == ']') {
				*prefix_ch = '\0';
				fmt_ch++;
				break;
			}

			*prefix_ch = *fmt_ch;
			prefix_ch++;
			fmt_ch++;
		}
	}

	*prefix_ch = '\0';

	if (*fmt_ch == '+') {
		extended = true;
		fmt_ch++;
	}

	obj = va_arg(*args, void *);
	BUF_APPEND("%saddr=%p", prefix, obj);

	if (!obj) {
		goto update_fmt;
	}

	switch (*fmt_ch) {
	case 'r':
		format_ref_count(buf_ch, extended, prefix, obj);
		break;
	case 'F':
		format_field_type(buf_ch, extended, prefix, obj);
		break;
	case 'f':
		format_field(buf_ch, extended, prefix, obj);
		break;
	case 'P':
		format_field_path(buf_ch, extended, prefix, obj);
		break;
	case 'E':
		format_event_class(buf_ch, extended, prefix, obj);
		break;
	case 'e':
		format_event(buf_ch, extended, prefix, obj);
		break;
	case 'S':
		format_stream_class(buf_ch, extended, prefix, obj);
		break;
	case 's':
		format_stream(buf_ch, extended, prefix, obj);
		break;
	case 'a':
		format_packet(buf_ch, extended, prefix, obj);
		break;
	case 't':
		format_trace(buf_ch, extended, prefix, obj);
		break;
	case 'K':
		format_clock_class(buf_ch, extended, prefix, obj);
		break;
	case 'k':
		format_clock_value(buf_ch, extended, prefix, obj);
		break;
	case 'v':
		format_value(buf_ch, extended, prefix, obj);
		break;
	case 'n':
		format_notification(buf_ch, extended, prefix, obj);
		break;
	case 'i':
		format_notification_iterator(buf_ch, extended, prefix, obj);
		break;
	case 'C':
		format_component_class(buf_ch, extended, prefix, obj);
		break;
	case 'c':
		format_component(buf_ch, extended, prefix, obj);
		break;
	case 'p':
		format_port(buf_ch, extended, prefix, obj);
		break;
	case 'x':
		format_connection(buf_ch, extended, prefix, obj);
		break;
	case 'u':
		format_plugin(buf_ch, extended, prefix, obj);
		break;
	case 'g':
		format_graph(buf_ch, extended, prefix, obj);
		break;
	case 'w':
		format_ctf_writer(buf_ch, extended, prefix, obj);
		break;
	default:
		abort();
	}

update_fmt:
	fmt_ch++;
	*out_fmt_ch = fmt_ch;
}

BT_HIDDEN
void bt_lib_log(const char *func, const char *file, unsigned line,
		int lvl, const char *tag, const char *fmt, ...)
{
	va_list args;

	BT_ASSERT(fmt);
	va_start(args, fmt);
	bt_common_custom_vsnprintf(lib_logging_buf, LIB_LOGGING_BUF_SIZE, '!',
		handle_conversion_specifier_bt, NULL, fmt, &args);
	va_end(args);
	_bt_log_write_d(func, file, line, lvl, tag, "%s", lib_logging_buf);
}
