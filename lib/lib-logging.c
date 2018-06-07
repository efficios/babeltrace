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

#define BT_LOG_TAG "LIB-LOGGING"

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
#include <babeltrace/ref-internal.h>
#include <babeltrace/values-internal.h>
#include <babeltrace/object-pool-internal.h>
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
#include <babeltrace/ctf-ir/utils-internal.h>
#include <babeltrace/ctf-writer/field-types-internal.h>
#include <babeltrace/ctf-writer/fields-internal.h>
#include <babeltrace/ctf-writer/event-internal.h>
#include <babeltrace/ctf-writer/stream-class-internal.h>
#include <babeltrace/ctf-writer/stream-internal.h>
#include <babeltrace/ctf-writer/trace-internal.h>
#include <babeltrace/ctf-writer/clock-internal.h>
#include <babeltrace/ctf-writer/writer-internal.h>
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
#include <babeltrace/graph/notification-inactivity-internal.h>
#include <babeltrace/graph/notification-internal.h>
#include <babeltrace/graph/notification-iterator-internal.h>
#include <babeltrace/graph/notification-packet-internal.h>
#include <babeltrace/graph/notification-stream-internal.h>
#include <babeltrace/graph/port-internal.h>
#include <babeltrace/plugin/plugin-internal.h>
#include <babeltrace/plugin/plugin-so-internal.h>

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

typedef void (*format_func)(char **, bool, const char *, void *);

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

static inline void format_object_pool(char **buf_ch, bool extended,
		const char *prefix, struct bt_object_pool *pool)
{
	BUF_APPEND(", %ssize=%zu", PRFIELD(pool->size));

	if (pool->objects) {
		BUF_APPEND(", %scap=%u", PRFIELD(pool->objects->len));
	}
}

static inline void format_field_type_common(char **buf_ch, bool extended,
		const char *prefix, struct bt_field_type_common *field_type)
{
	BUF_APPEND(", %stype-id=%s, %salignment=%u",
		PRFIELD(bt_common_field_type_id_string(field_type->id)),
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
		struct bt_field_type_common_integer *integer =
			BT_FROM_COMMON(field_type);

		BUF_APPEND(", %ssize=%u, %sis-signed=%d, %sbyte-order=%s, "
			"%sbase=%d, %sencoding=%s, "
			"%smapped-clock-class-addr=%p",
			PRFIELD(integer->size), PRFIELD(integer->is_signed),
			PRFIELD(bt_common_byte_order_string(integer->user_byte_order)),
			PRFIELD(integer->base),
			PRFIELD(bt_common_string_encoding_string(integer->encoding)),
			PRFIELD(integer->mapped_clock_class));

		if (integer->mapped_clock_class) {
			BUF_APPEND(", %smapped-clock-class-name=\"%s\"",
				PRFIELD(bt_clock_class_get_name(integer->mapped_clock_class)));
		}
		break;
	}
	case BT_FIELD_TYPE_ID_FLOAT:
	{
		struct bt_field_type_common_floating_point *flt =
			BT_FROM_COMMON(field_type);

		BUF_APPEND(", %sexp-dig=%u, %smant-dig=%u, %sbyte-order=%s",
			PRFIELD(flt->exp_dig), PRFIELD(flt->mant_dig),
			PRFIELD(bt_common_byte_order_string(flt->user_byte_order)));
		break;
	}
	case BT_FIELD_TYPE_ID_ENUM:
	{
		struct bt_field_type_common_enumeration *enm =
			BT_FROM_COMMON(field_type);

		BUF_APPEND(", %smapping-count=%u",
			PRFIELD(enm->entries->len));
		break;
	}
	case BT_FIELD_TYPE_ID_STRING:
	{
		struct bt_field_type_common_string *str =
			BT_FROM_COMMON(field_type);

		BUF_APPEND(", %sencoding=%s",
			PRFIELD(bt_common_string_encoding_string(str->encoding)));
		break;
	}
	case BT_FIELD_TYPE_ID_STRUCT:
	{
		struct bt_field_type_common_structure *structure =
			BT_FROM_COMMON(field_type);

		BUF_APPEND(", %sfield-count=%u",
			PRFIELD(structure->fields->len));
		break;
	}
	case BT_FIELD_TYPE_ID_SEQUENCE:
	{
		struct bt_field_type_common_sequence *seq =
			BT_FROM_COMMON(field_type);

		BUF_APPEND(", %slength-ft-addr=\"%s\", %selem-ft-addr=%p",
			PRFIELD(seq->length_field_name->str),
			PRFIELD(seq->element_ft));
		break;
	}
	case BT_FIELD_TYPE_ID_VARIANT:
	{
		struct bt_field_type_common_variant *variant =
			BT_FROM_COMMON(field_type);

		BUF_APPEND(", %stag-name=\"%s\", %sfield-count=%u",
			PRFIELD(variant->tag_name->str),
			PRFIELD(variant->choices->len));
		break;
	}
	default:
		break;
	}
}

static inline void format_field_type(char **buf_ch, bool extended,
		const char *prefix, struct bt_field_type *field_type)
{
	format_field_type_common(buf_ch, extended, prefix,
		(void *) field_type);
}

static inline void format_writer_field_type(char **buf_ch, bool extended,
		const char *prefix, struct bt_ctf_field_type *field_type)
{
	format_field_type_common(buf_ch, extended, prefix,
		(void *) field_type);
}

static inline void format_field_common_integer_extended(char **buf_ch,
		const char *prefix, struct bt_field_common *field)
{
	struct bt_field_common_integer *integer = BT_FROM_COMMON(field);
	struct bt_field_type_common_integer *field_type =
		BT_FROM_COMMON(field->type);
	const char *fmt = NULL;

	BT_ASSERT(field_type);

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

static inline void format_field_common(char **buf_ch, bool extended,
		const char *prefix, struct bt_field_common *field)
{
	BUF_APPEND(", %sis-set=%d", PRFIELD(field->payload_set));

	if (extended) {
		BUF_APPEND(", %sis-frozen=%d", PRFIELD(field->frozen));
	}

	BUF_APPEND(", %stype-addr=%p", PRFIELD(field->type));

	if (!field->type) {
		return;
	}

	BUF_APPEND(", %stype-id=%s",
		PRFIELD(bt_common_field_type_id_string(field->type->id)));

	if (!extended || field->type->id == BT_FIELD_TYPE_ID_UNKNOWN ||
			!field->payload_set) {
		return;
	}

	switch (field->type->id) {
	case BT_FIELD_TYPE_ID_INTEGER:
	{
		format_field_common_integer_extended(buf_ch, prefix, field);
		break;
	}
	case BT_FIELD_TYPE_ID_FLOAT:
	{
		struct bt_field_common_floating_point *flt =
			BT_FROM_COMMON(field);

		BUF_APPEND(", %svalue=%f", PRFIELD(flt->payload));
		break;
	}
	case BT_FIELD_TYPE_ID_STRING:
	{
		struct bt_field_common_string *str =
			BT_FROM_COMMON(field);

		if (str->buf) {
			BT_ASSERT(str->buf->data);
			BUF_APPEND(", %spartial-value=\"%.32s\"",
				PRFIELD(str->buf->data));

		}
		break;
	}
	case BT_FIELD_TYPE_ID_SEQUENCE:
	{
		struct bt_field_common_sequence *seq =
			BT_FROM_COMMON(field);

		BUF_APPEND(", %slength=%" PRIu64, PRFIELD(seq->length));

		if (seq->elements) {
			BUF_APPEND(", %sallocated-length=%u",
				PRFIELD(seq->elements->len));
		}
		break;
	}
	case BT_FIELD_TYPE_ID_VARIANT:
	{
		struct bt_field_common_variant *variant =
			BT_FROM_COMMON(field);

		BUF_APPEND(", %scur-field-addr=%p",
			PRFIELD(variant->current_field));
		break;
	}
	default:
		break;
	}
}

static inline void format_field(char **buf_ch, bool extended,
		const char *prefix, struct bt_field *field)
{
	struct bt_field_common *common_field = (void *) field;

	format_field_common(buf_ch, extended, prefix, (void *) field);

	if (!extended) {
		return;
	}

	if (!common_field->type) {
		return;
	}

	switch (common_field->type->id) {
	case BT_FIELD_TYPE_ID_ENUM:
	{
		struct bt_field_common_integer *integer = (void *) field;
		struct bt_field_type_common_enumeration *enum_ft =
			BT_FROM_COMMON(common_field->type);

		if (enum_ft->container_ft) {
			if (enum_ft->container_ft->is_signed) {
				BUF_APPEND(", %svalue=%" PRId64,
					PRFIELD(integer->payload.signd));
			} else {
				BUF_APPEND(", %svalue=%" PRIu64,
					PRFIELD(integer->payload.unsignd));
			}
		}
		break;
	}
	default:
		break;
	}
}

static inline void format_writer_field(char **buf_ch, bool extended,
		const char *prefix, struct bt_ctf_field *field)
{
	struct bt_field_common *common_field = (void *) field;

	format_field_common(buf_ch, extended, prefix, (void *) field);

	if (!extended) {
		return;
	}

	if (!common_field->type) {
		return;
	}

	switch (common_field->type->id) {
	case BT_FIELD_TYPE_ID_ENUM:
	{
		struct bt_ctf_field_enumeration *enumeration = (void *) field;

		if (enumeration->container) {
			format_writer_field(buf_ch, extended, prefix,
				(void *) enumeration->container);
		}
		break;
	}
	case BT_FIELD_TYPE_ID_VARIANT:
	{
		struct bt_ctf_field_variant *variant = (void *) field;

		BUF_APPEND(", %stag-field-addr=%p", PRFIELD(variant->tag));
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

	if (field_path->indexes) {
		BT_ASSERT(field_path->indexes);
		BUF_APPEND(", %sindex-count=%u", PRFIELD(field_path->indexes->len));
	}

	if (!extended || !field_path->indexes) {
		return;
	}

	BUF_APPEND(", %spath=[%s", PRFIELD(bt_common_scope_string(field_path->root)));

	for (i = 0; i < field_path->indexes->len; i++) {
		int index = g_array_index(field_path->indexes, int, i);

		BUF_APPEND(", %d", index);
	}

	BUF_APPEND("%s", "]");
}

static inline void format_trace_common(char **buf_ch, bool extended,
		const char *prefix, struct bt_trace_common *trace)
{
	if (trace->name) {
		BUF_APPEND(", %sname=\"%s\"", PRFIELD(trace->name->str));
	}

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-frozen=%d", PRFIELD(trace->frozen));

	if (trace->uuid_set) {
		BUF_APPEND_UUID(trace->uuid);
	}

	if (trace->clock_classes) {
		BUF_APPEND(", %sclock-class-count=%u",
			PRFIELD(trace->clock_classes->len));
	}

	if (trace->stream_classes) {
		BUF_APPEND(", %sstream-class-count=%u",
			PRFIELD(trace->stream_classes->len));
	}

	if (trace->streams) {
		BUF_APPEND(", %sstream-count=%u",
			PRFIELD(trace->streams->len));
	}

	BUF_APPEND(", %spacket-header-ft-addr=%p",
		PRFIELD(trace->packet_header_field_type));
}

static inline void format_trace(char **buf_ch, bool extended,
		const char *prefix, struct bt_trace *trace)
{
	char tmp_prefix[64];

	format_trace_common(buf_ch, extended, prefix, BT_TO_COMMON(trace));

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-static=%d", PRFIELD(trace->is_static));
	SET_TMP_PREFIX("phf-pool-");
	format_object_pool(buf_ch, extended, prefix,
		&trace->packet_header_field_pool);
}

static inline void format_writer_trace(char **buf_ch, bool extended,
		const char *prefix, struct bt_ctf_trace *trace)
{
	format_trace_common(buf_ch, extended, prefix, BT_TO_COMMON(trace));
}

static inline void format_stream_class_common(char **buf_ch, bool extended,
		const char *prefix, struct bt_stream_class_common *stream_class,
		format_func trace_format_func)
{
	struct bt_trace_common *trace;
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

	if (stream_class->event_classes) {
		BUF_APPEND(", %sevent-class-count=%u",
			PRFIELD(stream_class->event_classes->len));
	}

	BUF_APPEND(", %spacket-context-ft-addr=%p, "
		"%sevent-header-ft-addr=%p, %sevent-context-ft-addr=%p",
		PRFIELD(stream_class->packet_context_field_type),
		PRFIELD(stream_class->event_header_field_type),
		PRFIELD(stream_class->event_context_field_type));
	trace = bt_stream_class_common_borrow_trace(stream_class);
	if (!trace) {
		return;
	}

	BUF_APPEND(", %strace-addr=%p", PRFIELD(trace));
	SET_TMP_PREFIX("trace-");
	trace_format_func(buf_ch, false, tmp_prefix, trace);
}

static inline void format_stream_class(char **buf_ch, bool extended,
		const char *prefix, struct bt_stream_class *stream_class)
{
	char tmp_prefix[64];

	format_stream_class_common(buf_ch, extended, prefix,
		BT_TO_COMMON(stream_class), (format_func) format_trace);

	if (!extended) {
		return;
	}

	SET_TMP_PREFIX("ehf-pool-");
	format_object_pool(buf_ch, extended, prefix,
		&stream_class->event_header_field_pool);
	SET_TMP_PREFIX("pcf-pool-");
	format_object_pool(buf_ch, extended, prefix,
		&stream_class->packet_context_field_pool);
}

static inline void format_writer_stream_class(char **buf_ch, bool extended,
		const char *prefix, struct bt_ctf_stream_class *stream_class)
{
	format_stream_class_common(buf_ch, extended, prefix,
		BT_TO_COMMON(stream_class), (format_func) format_writer_trace);

	if (extended && stream_class->clock) {
		BUF_APPEND(", %sctf-writer-clock-addr=%p, "
			"%sctf-writer-clock-name=\"%s\"",
			PRFIELD(stream_class->clock),
			PRFIELD(bt_clock_class_get_name(
				BT_TO_COMMON(stream_class->clock->clock_class))));
	}
}

static inline void format_event_class_common(char **buf_ch, bool extended,
		const char *prefix, struct bt_event_class_common *event_class,
		format_func format_stream_class_func,
		format_func format_trace_func)
{
	struct bt_stream_class_common *stream_class;
	struct bt_trace_common *trace;
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
		PRFIELD(bt_common_event_class_log_level_string(event_class->log_level)));

	if (event_class->emf_uri) {
		BUF_APPEND(", %semf-uri=\"%s\"",
			PRFIELD(event_class->emf_uri->str));
	}

	BUF_APPEND(", %scontext-ft-addr=%p, %spayload-ft-addr=%p",
		PRFIELD(event_class->context_field_type),
		PRFIELD(event_class->payload_field_type));

	stream_class = bt_event_class_common_borrow_stream_class(event_class);
	if (!stream_class) {
		return;
	}

	BUF_APPEND(", %sstream-class-addr=%p", PRFIELD(stream_class));
	SET_TMP_PREFIX("stream-class-");
	format_stream_class_func(buf_ch, false, tmp_prefix, stream_class);
	trace = bt_stream_class_common_borrow_trace(stream_class);
	if (!trace) {
		return;
	}

	BUF_APPEND(", %strace-addr=%p", PRFIELD(trace));
	SET_TMP_PREFIX("trace-");
	format_trace_func(buf_ch, false, tmp_prefix, trace);
}

static inline void format_event_class(char **buf_ch, bool extended,
		const char *prefix, struct bt_event_class *event_class)
{
	char tmp_prefix[64];

	format_event_class_common(buf_ch, extended, prefix,
		BT_TO_COMMON(event_class), (format_func) format_stream_class,
		(format_func) format_trace);

	if (!extended) {
		return;
	}

	SET_TMP_PREFIX("event-pool-");
	format_object_pool(buf_ch, extended, prefix, &event_class->event_pool);
}

static inline void format_writer_event_class(char **buf_ch, bool extended,
		const char *prefix, struct bt_ctf_event_class *event_class)
{
	format_event_class_common(buf_ch, extended, prefix,
		BT_TO_COMMON(event_class),
		(format_func) format_writer_stream_class,
		(format_func) format_writer_trace);
}

static inline void format_stream_common(char **buf_ch, bool extended,
		const char *prefix, struct bt_stream_common *stream,
		format_func format_stream_class_func,
		format_func format_trace_func)
{
	struct bt_stream_class_common *stream_class;
	struct bt_trace_common *trace;
	char tmp_prefix[64];

	BUF_APPEND(", %sid=%" PRId64, PRFIELD(stream->id));

	if (stream->name) {
		BUF_APPEND(", %sname=\"%s\"", PRFIELD(stream->name->str));
	}

	if (!extended) {
		return;
	}

	stream_class = bt_stream_common_borrow_class(stream);
	if (!stream_class) {
		return;
	}

	BUF_APPEND(", %sstream-class-addr=%p", PRFIELD(stream_class));
	SET_TMP_PREFIX("stream-class-");
	format_stream_class_func(buf_ch, false, tmp_prefix, stream_class);
	trace = bt_stream_class_common_borrow_trace(stream_class);
	if (!trace) {
		return;
	}

	trace = (void *) bt_object_borrow_parent(stream);
	if (!trace) {
		return;
	}

	BUF_APPEND(", %strace-addr=%p", PRFIELD(trace));
	SET_TMP_PREFIX("trace-");
	format_trace_common(buf_ch, false, tmp_prefix, trace);
}

static inline void format_stream(char **buf_ch, bool extended,
		const char *prefix, struct bt_stream *stream)
{
	char tmp_prefix[64];

	format_stream_common(buf_ch, extended, prefix, BT_TO_COMMON(stream),
		(format_func) format_stream_class,
		(format_func) format_trace);
	SET_TMP_PREFIX("packet-pool-");
	format_object_pool(buf_ch, extended, prefix, &stream->packet_pool);
}

static inline void format_writer_stream(char **buf_ch, bool extended,
		const char *prefix, struct bt_ctf_stream *stream)
{
	format_stream_common(buf_ch, extended, prefix, BT_TO_COMMON(stream),
		(format_func) format_writer_stream_class,
		(format_func) format_writer_trace);

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

	if (stream->events) {
		BUF_APPEND(", %sevent-count=%u", PRFIELD(stream->events->len));
	}

	BUF_APPEND(", %sheader-field-addr=%p, %scontext-field-addr=%p"
		", %sfd=%d, %smmap-offset=%zu, "
		"%smmap-base-offset=%zu, %spacket-size=%" PRIu64 ", "
		"%soffset=%" PRId64 ", "
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
		PRFIELD(stream->flushed_packet_count),
		PRFIELD(stream->discarded_events),
		PRFIELD(stream->size), PRFIELD(stream->last_ts_end));
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
		PRFIELD(packet->header ? packet->header->field : NULL),
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

static inline void format_event_common(char **buf_ch, bool extended,
		const char *prefix, struct bt_event_common *event,
		format_func format_event_class_func,
		format_func format_stream_class_func,
		format_func format_trace_func)
{
	struct bt_trace_common *trace;
	struct bt_stream_class_common *stream_class;
	char tmp_prefix[64];

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-frozen=%d, %sheader-field-addr=%p, "
		"%sstream-context-field-addr=%p, "
		"%scontext-field-addr=%p, %spayload-field-addr=%p, ",
		PRFIELD(event->frozen),
		PRFIELD(event->header_field ? event->header_field->field : NULL),
		PRFIELD(event->stream_event_context_field),
		PRFIELD(event->context_field),
		PRFIELD(event->payload_field));
	BUF_APPEND(", %sevent-class-addr=%p", PRFIELD(event->class));

	if (!event->class) {
		return;
	}

	SET_TMP_PREFIX("event-class-");
	format_event_class_func(buf_ch, false, tmp_prefix, event->class);
	stream_class = bt_event_class_common_borrow_stream_class(event->class);
	if (stream_class) {
		BUF_APPEND(", %sstream-class-addr=%p", PRFIELD(stream_class));
		SET_TMP_PREFIX("stream-class-");
		format_stream_class_func(buf_ch, false, tmp_prefix,
			stream_class);

		trace = bt_stream_class_common_borrow_trace(stream_class);
		if (trace) {
			BUF_APPEND(", %strace-addr=%p", PRFIELD(trace));
			SET_TMP_PREFIX("trace-");
			format_trace_func(buf_ch, false, tmp_prefix, trace);
		}
	}
}

static inline void format_event(char **buf_ch, bool extended,
		const char *prefix, struct bt_event *event)
{
	struct bt_packet *packet;
	struct bt_stream *stream;
	char tmp_prefix[64];

	format_event_common(buf_ch, extended, prefix, BT_TO_COMMON(event),
		(format_func) format_event_class,
		(format_func) format_stream_class, (format_func) format_trace);

	if (!extended) {
		return;
	}

	if (event->clock_values) {
		BUF_APPEND(", %sclock-value-count=%u",
			PRFIELD(g_hash_table_size(event->clock_values)));
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

static inline void format_writer_event(char **buf_ch, bool extended,
		const char *prefix, struct bt_event *event)
{
	format_event_common(buf_ch, extended, prefix, BT_TO_COMMON(event),
		(format_func) format_writer_event_class,
		(format_func) format_writer_stream_class,
		(format_func) format_writer_trace);
}

static inline void format_clock_class(char **buf_ch, bool extended,
		const char *prefix, struct bt_clock_class *clock_class)
{
	char tmp_prefix[64];

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

	SET_TMP_PREFIX("cv-pool-");
	format_object_pool(buf_ch, extended, prefix, &clock_class->cv_pool);
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

	BUF_APPEND(", %sis-frozen=%d, %sis-set=%d",
		PRFIELD(clock_value->frozen), PRFIELD(clock_value->is_set));
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

	BUF_APPEND(", %sis-frozen=%d, %sgraph-addr=%p",
		PRFIELD(notif->frozen), PRFIELD(notif->graph));

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
		PRFIELD(comp_class->name ? comp_class->name->str : NULL));

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

	if (component->input_ports) {
		BUF_APPEND(", %sinput-port-count=%u",
			PRFIELD(component->input_ports->len));
	}

	if (component->output_ports) {
		BUF_APPEND(", %soutput-port-count=%u",
			PRFIELD(component->output_ports->len));
	}
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
	char tmp_prefix[64];

	BUF_APPEND(", %sis-canceled=%d", PRFIELD(graph->canceled));

	if (!extended) {
		return;
	}

	if (graph->components) {
		BUF_APPEND(", %scomp-count=%u",
			PRFIELD(graph->components->len));
	}

	if (graph->connections) {
		BUF_APPEND(", %sconn-count=%u",
			PRFIELD(graph->connections->len));
	}

	SET_TMP_PREFIX("en-pool-");
	format_object_pool(buf_ch, extended, prefix,
		&graph->event_notif_pool);
	SET_TMP_PREFIX("pbn-pool-");
	format_object_pool(buf_ch, extended, prefix,
		&graph->packet_begin_notif_pool);
	SET_TMP_PREFIX("pen-pool-");
	format_object_pool(buf_ch, extended, prefix,
		&graph->packet_end_notif_pool);
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

static inline void format_stream_class_common_common(char **buf_ch,
		bool extended, const char *prefix,
		struct bt_stream_class_common *stream_class)
{
	format_stream_class_common(buf_ch, extended, prefix, stream_class,
		(format_func) format_trace_common);
}

static inline void format_event_class_common_common(char **buf_ch,
		bool extended, const char *prefix,
		struct bt_event_class_common *event_class)
{
	format_event_class_common(buf_ch, extended, prefix, event_class,
		(format_func) format_stream_class_common,
		(format_func) format_trace_common);
}

static inline void format_event_common_common(char **buf_ch,
		bool extended, const char *prefix,
		struct bt_event_common *event)
{
	format_event_common(buf_ch, extended, prefix, event,
		(format_func) format_event_class_common,
		(format_func) format_stream_class_common,
		(format_func) format_trace_common);
}

static inline void format_stream_common_common(char **buf_ch, bool extended,
		const char *prefix, struct bt_stream_common *stream)
{
	format_stream_common(buf_ch, extended, prefix, stream,
		(format_func) format_stream_class_common,
		(format_func) format_trace_common);
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
	enum {
		CAT_DEFAULT,
		CAT_WRITER,
		CAT_COMMON,
	} cat = CAT_DEFAULT;

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

	if (*fmt_ch == 'w') {
		cat = CAT_WRITER;
		fmt_ch++;
	} else if (*fmt_ch == '_') {
		cat = CAT_COMMON;
		fmt_ch++;
	}

	obj = va_arg(*args, void *);
	BUF_APPEND("%saddr=%p", prefix, obj);

	if (!obj) {
		goto update_fmt;
	}

	switch (cat) {
	case CAT_DEFAULT:
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
		case 'o':
			format_object_pool(buf_ch, extended, prefix, obj);
			break;
		default:
			abort();
		}
		break;
	case CAT_WRITER:
		switch (*fmt_ch) {
		case 'F':
			format_writer_field_type(buf_ch, extended, prefix, obj);
			break;
		case 'f':
			format_writer_field(buf_ch, extended, prefix, obj);
			break;
		case 'E':
			format_writer_event_class(buf_ch, extended, prefix, obj);
			break;
		case 'e':
			format_writer_event(buf_ch, extended, prefix, obj);
			break;
		case 'S':
			format_writer_stream_class(buf_ch, extended, prefix, obj);
			break;
		case 's':
			format_writer_stream(buf_ch, extended, prefix, obj);
			break;
		case 't':
			format_writer_trace(buf_ch, extended, prefix, obj);
			break;
		case 'w':
			format_ctf_writer(buf_ch, extended, prefix, obj);
			break;
		default:
			abort();
		}
		break;
	case CAT_COMMON:
		switch (*fmt_ch) {
		case 'F':
			format_field_type_common(buf_ch, extended, prefix, obj);
			break;
		case 'f':
			format_field_common(buf_ch, extended, prefix, obj);
			break;
		case 'E':
			format_event_class_common_common(buf_ch, extended, prefix, obj);
			break;
		case 'e':
			format_event_common_common(buf_ch, extended, prefix, obj);
			break;
		case 'S':
			format_stream_class_common_common(buf_ch, extended, prefix, obj);
			break;
		case 's':
			format_stream_common_common(buf_ch, extended, prefix, obj);
			break;
		case 't':
			format_trace_common(buf_ch, extended, prefix, obj);
			break;
		default:
			abort();
		}
		break;
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
