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
#include <babeltrace/values-internal.h>
#include <babeltrace/values-internal.h>
#include <babeltrace/object-pool-internal.h>
#include <babeltrace/trace-ir/field-classes-internal.h>
#include <babeltrace/trace-ir/fields-internal.h>
#include <babeltrace/trace-ir/event-class-internal.h>
#include <babeltrace/trace-ir/event-const.h>
#include <babeltrace/trace-ir/event-internal.h>
#include <babeltrace/trace-ir/packet-const.h>
#include <babeltrace/trace-ir/packet-internal.h>
#include <babeltrace/trace-ir/stream-class-internal.h>
#include <babeltrace/trace-ir/stream-internal.h>
#include <babeltrace/trace-ir/stream-const.h>
#include <babeltrace/trace-ir/trace-internal.h>
#include <babeltrace/trace-ir/trace-class-internal.h>
#include <babeltrace/trace-ir/clock-class-internal.h>
#include <babeltrace/trace-ir/clock-value-internal.h>
#include <babeltrace/trace-ir/field-path-internal.h>
#include <babeltrace/trace-ir/utils-internal.h>
#include <babeltrace/graph/component-class-internal.h>
#include <babeltrace/graph/component-class-sink-colander-internal.h>
#include <babeltrace/graph/component-filter-internal.h>
#include <babeltrace/graph/component-internal.h>
#include <babeltrace/graph/component-sink-internal.h>
#include <babeltrace/graph/component-source-internal.h>
#include <babeltrace/graph/connection-internal.h>
#include <babeltrace/graph/graph-internal.h>
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
	do {								\
		BUF_APPEND(", %suuid=", prefix);			\
		format_uuid(buf_ch, (_uuid));				\
	} while (0)

#define PRFIELD(_expr)	prefix, (_expr)

#define PRFIELD_GSTRING(_expr)	PRFIELD((_expr) ? (_expr)->str : NULL)

#define SET_TMP_PREFIX(_prefix2)					\
	do {								\
		strcpy(tmp_prefix, prefix);				\
		strcat(tmp_prefix, (_prefix2));				\
	} while (0)

static inline void format_component(char **buf_ch, bool extended,
		const char *prefix, const struct bt_component *component);

static inline void format_port(char **buf_ch, bool extended,
		const char *prefix, const struct bt_port *port);

static inline void format_connection(char **buf_ch, bool extended,
		const char *prefix, const struct bt_connection *connection);

static inline void format_clock_value(char **buf_ch, bool extended,
		const char *prefix, const struct bt_clock_value *clock_value);

static inline void format_field_path(char **buf_ch, bool extended,
		const char *prefix, const struct bt_field_path *field_path);

static inline void format_object(char **buf_ch, bool extended,
		const char *prefix, const struct bt_object *obj)
{
	BUF_APPEND(", %sref-count=%llu", prefix, obj->ref_count);
}

static inline void format_uuid(char **buf_ch, bt_uuid uuid)
{
	BUF_APPEND("\"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\"",
		(unsigned int) uuid[0],
		(unsigned int) uuid[1],
		(unsigned int) uuid[2],
		(unsigned int) uuid[3],
		(unsigned int) uuid[4],
		(unsigned int) uuid[5],
		(unsigned int) uuid[6],
		(unsigned int) uuid[7],
		(unsigned int) uuid[8],
		(unsigned int) uuid[9],
		(unsigned int) uuid[10],
		(unsigned int) uuid[11],
		(unsigned int) uuid[12],
		(unsigned int) uuid[13],
		(unsigned int) uuid[14],
		(unsigned int) uuid[15]);
}

static inline void format_object_pool(char **buf_ch, bool extended,
		const char *prefix, const struct bt_object_pool *pool)
{
	BUF_APPEND(", %ssize=%zu", PRFIELD(pool->size));

	if (pool->objects) {
		BUF_APPEND(", %scap=%u", PRFIELD(pool->objects->len));
	}
}

static inline void format_integer_field_class(char **buf_ch,
		bool extended, const char *prefix,
		const struct bt_field_class *field_class)
{
	const struct bt_field_class_integer *int_fc =
		(const void *) field_class;

	BUF_APPEND(", %srange-size=%" PRIu64 ", %sbase=%s",
		PRFIELD(int_fc->range),
		PRFIELD(bt_common_field_class_integer_preferred_display_base_string(int_fc->base)));
}

static inline void format_array_field_class(char **buf_ch,
		bool extended, const char *prefix,
		const struct bt_field_class *field_class)
{
	const struct bt_field_class_array *array_fc =
		(const void *) field_class;

	BUF_APPEND(", %selement-fc-addr=%p, %selement-fc-type=%s",
		PRFIELD(array_fc->element_fc),
		PRFIELD(bt_common_field_class_type_string(array_fc->element_fc->type)));
}

static inline void format_field_class(char **buf_ch, bool extended,
		const char *prefix, const struct bt_field_class *field_class)
{
	char tmp_prefix[64];

	BUF_APPEND(", %stype=%s",
		PRFIELD(bt_common_field_class_type_string(field_class->type)));

	if (extended) {
		BUF_APPEND(", %sis-frozen=%d", PRFIELD(field_class->frozen));
		BUF_APPEND(", %sis-part-of-trace-class=%d",
			PRFIELD(field_class->part_of_trace_class));
	} else {
		return;
	}

	switch (field_class->type) {
	case BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER:
	case BT_FIELD_CLASS_TYPE_SIGNED_INTEGER:
	{
		format_integer_field_class(buf_ch, extended, prefix, field_class);
		break;
	}
	case BT_FIELD_CLASS_TYPE_REAL:
	{
		const struct bt_field_class_real *real_fc = (void *) field_class;

		BUF_APPEND(", %sis-single-precision=%d",
			PRFIELD(real_fc->is_single_precision));
		break;
	}
	case BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION:
	case BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION:
	{
		const struct bt_field_class_enumeration *enum_fc =
			(const void *) field_class;

		format_integer_field_class(buf_ch, extended, prefix, field_class);
		BUF_APPEND(", %smapping-count=%u",
			PRFIELD(enum_fc->mappings->len));
		break;
	}
	case BT_FIELD_CLASS_TYPE_STRUCTURE:
	{
		const struct bt_field_class_structure *struct_fc =
			(const void *) field_class;

		if (struct_fc->common.named_fcs) {
			BUF_APPEND(", %smember-count=%u",
				PRFIELD(struct_fc->common.named_fcs->len));
		}

		break;
	}
	case BT_FIELD_CLASS_TYPE_STATIC_ARRAY:
	{
		const struct bt_field_class_static_array *array_fc =
			(const void *) field_class;

		format_array_field_class(buf_ch, extended, prefix, field_class);
		BUF_APPEND(", %slength=%" PRIu64, PRFIELD(array_fc->length));
		break;
	}
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY:
	{
		const struct bt_field_class_dynamic_array *array_fc =
			(const void *) field_class;

		format_array_field_class(buf_ch, extended, prefix, field_class);

		if (array_fc->length_fc) {
			SET_TMP_PREFIX("length-fc-");
			format_field_class(buf_ch, extended, tmp_prefix,
				array_fc->length_fc);
		}

		if (array_fc->length_field_path) {
			SET_TMP_PREFIX("length-field-path-");
			format_field_path(buf_ch, extended, tmp_prefix,
				array_fc->length_field_path);
		}

		break;
	}
	case BT_FIELD_CLASS_TYPE_VARIANT:
	{
		const struct bt_field_class_variant *var_fc =
			(const void *) field_class;

		if (var_fc->common.named_fcs) {
			BUF_APPEND(", %soption-count=%u",
				PRFIELD(var_fc->common.named_fcs->len));
		}

		if (var_fc->selector_fc) {
			SET_TMP_PREFIX("selector-fc-");
			format_field_class(buf_ch, extended, tmp_prefix,
				var_fc->selector_fc);
		}

		if (var_fc->selector_field_path) {
			SET_TMP_PREFIX("selector-field-path-");
			format_field_path(buf_ch, extended, tmp_prefix,
				var_fc->selector_field_path);
		}

		break;
	}
	default:
		break;
	}
}

static inline void format_field_integer_extended(char **buf_ch,
		const char *prefix, const struct bt_field *field)
{
	const struct bt_field_integer *integer = (void *) field;
	const struct bt_field_class_integer *field_class =
		(void *) field->class;
	const char *fmt = NULL;

	BT_ASSERT(field_class);

	if (field_class->base == BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL) {
		fmt = ", %svalue=%" PRIo64;
	} else if (field_class->base == BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL) {
		fmt = ", %svalue=%" PRIx64;
	}

	if (field_class->common.type == BT_FIELD_CLASS_TYPE_SIGNED_INTEGER ||
			field_class->common.type == BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION) {
		if (!fmt) {
			fmt = ", %svalue=%" PRId64;
		}

		BUF_APPEND(fmt, PRFIELD(integer->value.i));
	} else {
		if (!fmt) {
			fmt = ", %svalue=%" PRIu64;
		}

		BUF_APPEND(fmt, PRFIELD(integer->value.u));
	}
}

static inline void format_field(char **buf_ch, bool extended,
		const char *prefix, const struct bt_field *field)
{
	BUF_APPEND(", %sis-set=%d", PRFIELD(field->is_set));

	if (extended) {
		BUF_APPEND(", %sis-frozen=%d", PRFIELD(field->frozen));
	}

	BUF_APPEND(", %sclass-addr=%p", PRFIELD(field->class));

	if (!field->class) {
		return;
	}

	BUF_APPEND(", %sclass-type=%s",
		PRFIELD(bt_common_field_class_type_string(field->class->type)));

	if (!extended || !field->is_set) {
		return;
	}

	switch (field->class->type) {
	case BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER:
	case BT_FIELD_CLASS_TYPE_SIGNED_INTEGER:
	case BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION:
	case BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION:
	{
		format_field_integer_extended(buf_ch, prefix, field);
		break;
	}
	case BT_FIELD_CLASS_TYPE_REAL:
	{
		const struct bt_field_real *real_field = (const void *) field;

		BUF_APPEND(", %svalue=%f", PRFIELD(real_field->value));
		break;
	}
	case BT_FIELD_CLASS_TYPE_STRING:
	{
		const struct bt_field_string *str = (const void *) field;

		if (str->buf) {
			BT_ASSERT(str->buf->data);
			BUF_APPEND(", %spartial-value=\"%.32s\"",
				PRFIELD(str->buf->data));
		}

		break;
	}
	case BT_FIELD_CLASS_TYPE_STATIC_ARRAY:
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY:
	{
		const struct bt_field_array *array_field = (const void *) field;

		BUF_APPEND(", %slength=%" PRIu64, PRFIELD(array_field->length));

		if (array_field->fields) {
			BUF_APPEND(", %sallocated-length=%u",
				PRFIELD(array_field->fields->len));
		}

		break;
	}
	case BT_FIELD_CLASS_TYPE_VARIANT:
	{
		const struct bt_field_variant *var_field = (const void *) field;

		BUF_APPEND(", %sselected-field-index=%" PRIu64,
			PRFIELD(var_field->selected_index));
		break;
	}
	default:
		break;
	}
}

static inline void format_field_path(char **buf_ch, bool extended,
		const char *prefix, const struct bt_field_path *field_path)
{
	uint64_t i;

	if (field_path->indexes) {
		BT_ASSERT(field_path->indexes);
		BUF_APPEND(", %sindex-count=%u",
			PRFIELD(field_path->indexes->len));
	}

	if (!extended || !field_path->indexes) {
		return;
	}

	BUF_APPEND(", %spath=[%s",
		PRFIELD(bt_common_scope_string(field_path->root)));

	for (i = 0; i < field_path->indexes->len; i++) {
		uint64_t index = bt_field_path_get_index_by_index_inline(
			field_path, i);

		BUF_APPEND(", %" PRIu64, index);
	}

	BUF_APPEND("%s", "]");
}

static inline void format_trace_class(char **buf_ch, bool extended,
		const char *prefix, const struct bt_trace_class *trace_class)
{
	char tmp_prefix[64];

	if (trace_class->name.value) {
		BUF_APPEND(", %sname=\"%s\"",
			PRFIELD(trace_class->name.value));
	}

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-frozen=%d", PRFIELD(trace_class->frozen));

	if (trace_class->uuid.value) {
		BUF_APPEND_UUID(trace_class->uuid.value);
	}

	if (trace_class->stream_classes) {
		BUF_APPEND(", %sstream-class-count=%u",
			PRFIELD(trace_class->stream_classes->len));
	}

	BUF_APPEND(", %spacket-header-fc-addr=%p, "
		"%sassigns-auto-sc-id=%d",
		PRFIELD(trace_class->packet_header_fc),
		PRFIELD(trace_class->assigns_automatic_stream_class_id));
	SET_TMP_PREFIX("phf-pool-");
	format_object_pool(buf_ch, extended, prefix,
		&trace_class->packet_header_field_pool);
}

static inline void format_trace(char **buf_ch, bool extended,
		const char *prefix, const struct bt_trace *trace)
{
	char tmp_prefix[64];

	if (trace->name.value) {
		BUF_APPEND(", %sname=\"%s\"", PRFIELD(trace->name.value));
	}

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-frozen=%d", PRFIELD(trace->frozen));

	if (trace->streams) {
		BUF_APPEND(", %sstream-count=%u",
			PRFIELD(trace->streams->len));
	}

	BUF_APPEND(", %sis-static=%d", PRFIELD(trace->is_static));

	if (!trace->class) {
		return;
	}

	BUF_APPEND(", %strace-class-addr=%p", PRFIELD(trace->class));
	SET_TMP_PREFIX("trace-class-");
	format_trace_class(buf_ch, false, tmp_prefix, trace->class);
}

static inline void format_stream_class(char **buf_ch, bool extended,
		const char *prefix,
		const struct bt_stream_class *stream_class)
{
	const struct bt_trace_class *trace_class;
	char tmp_prefix[64];

	BUF_APPEND(", %sid=%" PRIu64, PRFIELD(stream_class->id));

	if (stream_class->name.value) {
		BUF_APPEND(", %sname=\"%s\"",
			PRFIELD(stream_class->name.value));
	}

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-frozen=%d", PRFIELD(stream_class->frozen));

	if (stream_class->event_classes) {
		BUF_APPEND(", %sevent-class-count=%u",
			PRFIELD(stream_class->event_classes->len));
	}

	BUF_APPEND(", %spacket-context-fc-addr=%p, "
		"%sevent-header-fc-addr=%p, %sevent-common-context-fc-addr=%p",
		PRFIELD(stream_class->packet_context_fc),
		PRFIELD(stream_class->event_header_fc),
		PRFIELD(stream_class->event_common_context_fc));
	trace_class = bt_stream_class_borrow_trace_class_inline(stream_class);
	if (!trace_class) {
		return;
	}

	BUF_APPEND(", %sassigns-auto-ec-id=%d, %sassigns-auto-stream-id=%d, "
		"%spackets-have-discarded-ev-counter-snapshot=%d, "
		"%spackets-have-packet-counter-snapshot=%d, "
		"%spackets-have-default-begin-cv=%d, "
		"%spackets-have-default-end-cv=%d",
		PRFIELD(stream_class->assigns_automatic_event_class_id),
		PRFIELD(stream_class->assigns_automatic_stream_id),
		PRFIELD(stream_class->packets_have_discarded_event_counter_snapshot),
		PRFIELD(stream_class->packets_have_packet_counter_snapshot),
		PRFIELD(stream_class->packets_have_default_beginning_cv),
		PRFIELD(stream_class->packets_have_default_end_cv));
	BUF_APPEND(", %strace-class-addr=%p", PRFIELD(trace_class));
	SET_TMP_PREFIX("trace-class-");
	format_trace_class(buf_ch, false, tmp_prefix, trace_class);
	SET_TMP_PREFIX("ehf-pool-");
	format_object_pool(buf_ch, extended, prefix,
		&stream_class->event_header_field_pool);
	SET_TMP_PREFIX("pcf-pool-");
	format_object_pool(buf_ch, extended, prefix,
		&stream_class->packet_context_field_pool);
}

static inline void format_event_class(char **buf_ch, bool extended,
		const char *prefix, const struct bt_event_class *event_class)
{
	const struct bt_stream_class *stream_class;
	const struct bt_trace_class *trace_class;
	char tmp_prefix[64];

	BUF_APPEND(", %sid=%" PRIu64, PRFIELD(event_class->id));

	if (event_class->name.value) {
		BUF_APPEND(", %sname=\"%s\"",
			PRFIELD(event_class->name.value));
	}

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-frozen=%d", PRFIELD(event_class->frozen));

	if (event_class->log_level.base.avail) {
		BUF_APPEND(", %slog-level=%s",
			PRFIELD(bt_common_event_class_log_level_string(
				(int) event_class->log_level.value)));
	}

	if (event_class->emf_uri.value) {
		BUF_APPEND(", %semf-uri=\"%s\"",
			PRFIELD(event_class->emf_uri.value));
	}

	BUF_APPEND(", %sspecific-context-fc-addr=%p, %spayload-fc-addr=%p",
		PRFIELD(event_class->specific_context_fc),
		PRFIELD(event_class->payload_fc));

	stream_class = bt_event_class_borrow_stream_class_const(event_class);
	if (!stream_class) {
		return;
	}

	BUF_APPEND(", %sstream-class-addr=%p", PRFIELD(stream_class));
	SET_TMP_PREFIX("stream-class-");
	format_stream_class(buf_ch, false, tmp_prefix, stream_class);
	trace_class = bt_stream_class_borrow_trace_class_inline(stream_class);
	if (!trace_class) {
		return;
	}

	BUF_APPEND(", %strace-class-addr=%p", PRFIELD(trace_class));
	SET_TMP_PREFIX("trace-class-");
	format_trace_class(buf_ch, false, tmp_prefix, trace_class);
	SET_TMP_PREFIX("event-pool-");
	format_object_pool(buf_ch, extended, prefix, &event_class->event_pool);
}

static inline void format_stream(char **buf_ch, bool extended,
		const char *prefix, const struct bt_stream *stream)
{
	const struct bt_stream_class *stream_class;
	const struct bt_trace_class *trace_class = NULL;
	const struct bt_trace *trace = NULL;
	char tmp_prefix[64];

	BUF_APPEND(", %sid=%" PRIu64, PRFIELD(stream->id));

	if (stream->name.value) {
		BUF_APPEND(", %sname=\"%s\"", PRFIELD(stream->name.value));
	}

	if (!extended) {
		return;
	}

	stream_class = bt_stream_borrow_class_const(stream);
	if (stream_class) {
		BUF_APPEND(", %sstream-class-addr=%p", PRFIELD(stream_class));
		SET_TMP_PREFIX("stream-class-");
		format_stream_class(buf_ch, false, tmp_prefix, stream_class);
		trace_class = bt_stream_class_borrow_trace_class_inline(stream_class);
	}

	if (trace_class) {
		BUF_APPEND(", %strace-class-addr=%p", PRFIELD(trace_class));
		SET_TMP_PREFIX("trace-class-");
		format_trace_class(buf_ch, false, tmp_prefix, trace_class);
	}

	trace = bt_stream_borrow_trace_inline(stream);
	if (trace) {
		BUF_APPEND(", %strace-addr=%p", PRFIELD(trace));
		SET_TMP_PREFIX("trace-");
		format_trace(buf_ch, false, tmp_prefix, trace);
	}

	SET_TMP_PREFIX("packet-pool-");
	format_object_pool(buf_ch, extended, prefix, &stream->packet_pool);
}

static inline void format_packet(char **buf_ch, bool extended,
		const char *prefix, const struct bt_packet *packet)
{
	const struct bt_stream *stream;
	const struct bt_trace_class *trace_class;
	char tmp_prefix[64];

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-frozen=%d, %sheader-field-addr=%p, "
		"%scontext-field-addr=%p",
		PRFIELD(packet->frozen),
		PRFIELD(packet->header_field ? packet->header_field->field : NULL),
		PRFIELD(packet->context_field ? packet->context_field->field : NULL));
	stream = bt_packet_borrow_stream_const(packet);
	if (!stream) {
		return;
	}

	if (packet->default_beginning_cv) {
		SET_TMP_PREFIX("default-begin-cv-");
		format_clock_value(buf_ch, true, tmp_prefix,
			packet->default_beginning_cv);
	}

	if (packet->default_end_cv) {
		SET_TMP_PREFIX("default-end-cv-");
		format_clock_value(buf_ch, true, tmp_prefix,
			packet->default_end_cv);
	}

	if (packet->discarded_event_counter_snapshot.base.avail) {
		BUF_APPEND(", %sdiscarded-ev-counter-snapshot=%" PRIu64,
			PRFIELD(packet->discarded_event_counter_snapshot.value));
	}

	if (packet->packet_counter_snapshot.base.avail) {
		BUF_APPEND(", %spacket-counter-snapshot=%" PRIu64,
			PRFIELD(packet->packet_counter_snapshot.value));
	}

	BUF_APPEND(", %sstream-addr=%p", PRFIELD(stream));
	SET_TMP_PREFIX("stream-");
	format_stream(buf_ch, false, tmp_prefix, stream);
	trace_class = (const struct bt_trace_class *) bt_object_borrow_parent(&stream->base);
	if (!trace_class) {
		return;
	}

	BUF_APPEND(", %strace-class-addr=%p", PRFIELD(trace_class));
	SET_TMP_PREFIX("trace-class-");
	format_trace_class(buf_ch, false, tmp_prefix, trace_class);
}

static inline void format_event(char **buf_ch, bool extended,
		const char *prefix, const struct bt_event *event)
{
	const struct bt_packet *packet;
	const struct bt_stream *stream;
	const struct bt_trace_class *trace_class;
	const struct bt_stream_class *stream_class;
	char tmp_prefix[64];

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-frozen=%d, %sheader-field-addr=%p, "
		"%scommon-context-field-addr=%p, "
		"%sspecific-context-field-addr=%p, "
		"%spayload-field-addr=%p, ",
		PRFIELD(event->frozen),
		PRFIELD(event->header_field ?
			event->header_field->field : NULL),
		PRFIELD(event->common_context_field),
		PRFIELD(event->specific_context_field),
		PRFIELD(event->payload_field));
	BUF_APPEND(", %sevent-class-addr=%p", PRFIELD(event->class));

	if (!event->class) {
		return;
	}

	SET_TMP_PREFIX("event-class-");
	format_event_class(buf_ch, false, tmp_prefix, event->class);
	stream_class = bt_event_class_borrow_stream_class(event->class);
	if (stream_class) {
		BUF_APPEND(", %sstream-class-addr=%p", PRFIELD(stream_class));
		SET_TMP_PREFIX("stream-class-");
		format_stream_class(buf_ch, false, tmp_prefix,
			stream_class);

		trace_class = bt_stream_class_borrow_trace_class_inline(
			stream_class);
		if (trace_class) {
			BUF_APPEND(", %strace-class-addr=%p",
				PRFIELD(trace_class));
			SET_TMP_PREFIX("trace-class-");
			format_trace_class(buf_ch, false, tmp_prefix,
				trace_class);
		}
	}

	if (event->default_cv) {
		SET_TMP_PREFIX("default-cv-");
		format_clock_value(buf_ch, true, tmp_prefix,
			event->default_cv);
	}

	packet = bt_event_borrow_packet_const(event);
	if (!packet) {
		return;
	}

	BUF_APPEND(", %spacket-addr=%p", PRFIELD(packet));
	SET_TMP_PREFIX("packet-");
	format_packet(buf_ch, false, tmp_prefix, packet);
	stream = bt_packet_borrow_stream_const(packet);
	if (!stream) {
		return;
	}

	BUF_APPEND(", %sstream-addr=%p", PRFIELD(stream));
	SET_TMP_PREFIX("stream-");
	format_stream(buf_ch, false, tmp_prefix, stream);
}

static inline void format_clock_class(char **buf_ch, bool extended,
		const char *prefix, const struct bt_clock_class *clock_class)
{
	char tmp_prefix[64];

	if (clock_class->name.value) {
		BUF_APPEND(", %sname=\"%s\"", PRFIELD(clock_class->name.value));
	}

	BUF_APPEND(", %sfreq=%" PRIu64, PRFIELD(clock_class->frequency));

	if (!extended) {
		return;
	}

	if (clock_class->description.value) {
		BUF_APPEND(", %spartial-descr=\"%.32s\"",
			PRFIELD(clock_class->description.value));
	}

	if (clock_class->uuid.value) {
		BUF_APPEND_UUID(clock_class->uuid.value);
	}

	BUF_APPEND(", %sis-frozen=%d, %sprecision=%" PRIu64 ", "
		"%soffset-s=%" PRId64 ", "
		"%soffset-cycles=%" PRIu64 ", %sis-absolute=%d, "
		"%sbase-offset-ns=%" PRId64,
		PRFIELD(clock_class->frozen), PRFIELD(clock_class->precision),
		PRFIELD(clock_class->offset_seconds),
		PRFIELD(clock_class->offset_cycles),
		PRFIELD(clock_class->is_absolute),
		PRFIELD(clock_class->base_offset.value_ns));

	SET_TMP_PREFIX("cv-pool-");
	format_object_pool(buf_ch, extended, prefix, &clock_class->cv_pool);
}

static inline void format_clock_value(char **buf_ch, bool extended,
		const char *prefix, const struct bt_clock_value *clock_value)
{
	char tmp_prefix[64];
	BUF_APPEND(", %svalue=%" PRIu64 ", %sns-from-origin=%" PRId64,
		PRFIELD(clock_value->value_cycles),
		PRFIELD(clock_value->ns_from_origin));

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-set=%d", PRFIELD(clock_value->is_set));

	if (clock_value->clock_class) {
		BUF_APPEND(", %sclock-class-addr=%p",
			PRFIELD(clock_value->clock_class));
		SET_TMP_PREFIX("clock-class-");
		format_clock_class(buf_ch, false, tmp_prefix,
			clock_value->clock_class);
	}
}

static inline void format_value(char **buf_ch, bool extended,
		const char *prefix, const struct bt_value *value)
{
	BUF_APPEND(", %stype=%s",
		PRFIELD(bt_common_value_type_string(bt_value_get_type(value))));

	if (!extended) {
		return;
	}

	switch (bt_value_get_type(value)) {
	case BT_VALUE_TYPE_BOOL:
	{
		bt_bool val = bt_value_bool_get(value);

		BUF_APPEND(", %svalue=%d", PRFIELD(val));
		break;
	}
	case BT_VALUE_TYPE_INTEGER:
	{
		int64_t val = bt_value_integer_get(value);

		BUF_APPEND(", %svalue=%" PRId64, PRFIELD(val));
		break;
	}
	case BT_VALUE_TYPE_REAL:
	{
		double val = bt_value_real_get(value);

		BUF_APPEND(", %svalue=%f", PRFIELD(val));
		break;
	}
	case BT_VALUE_TYPE_STRING:
	{
		const char *val = bt_value_string_get(value);

		BUF_APPEND(", %spartial-value=\"%.32s\"", PRFIELD(val));
		break;
	}
	case BT_VALUE_TYPE_ARRAY:
	{
		int64_t count = bt_value_array_get_size(value);

		BT_ASSERT(count >= 0);
		BUF_APPEND(", %selement-count=%" PRId64, PRFIELD(count));
		break;
	}
	case BT_VALUE_TYPE_MAP:
	{
		int64_t count = bt_value_map_get_size(value);

		BT_ASSERT(count >= 0);
		BUF_APPEND(", %selement-count=%" PRId64, PRFIELD(count));
		break;
	}
	default:
		break;
	}
}

static inline void format_notification(char **buf_ch, bool extended,
		const char *prefix, const struct bt_notification *notif)
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
		const struct bt_notification_event *notif_event =
			(const void *) notif;

		if (notif_event->event) {
			SET_TMP_PREFIX("event-");
			format_event(buf_ch, true, tmp_prefix, notif_event->event);
		}

		break;
	}
	case BT_NOTIFICATION_TYPE_STREAM_BEGINNING:
	{
		const struct bt_notification_stream_beginning *notif_stream =
			(const void *) notif;

		if (notif_stream->stream) {
			SET_TMP_PREFIX("stream-");
			format_stream(buf_ch, true, tmp_prefix, notif_stream->stream);
		}

		break;
	}
	case BT_NOTIFICATION_TYPE_STREAM_END:
	{
		const struct bt_notification_stream_end *notif_stream =
			(const void *) notif;

		if (notif_stream->stream) {
			SET_TMP_PREFIX("stream-");
			format_stream(buf_ch, true, tmp_prefix, notif_stream->stream);
		}

		break;
	}
	case BT_NOTIFICATION_TYPE_PACKET_BEGINNING:
	{
		const struct bt_notification_packet_beginning *notif_packet =
			(const void *) notif;

		if (notif_packet->packet) {
			SET_TMP_PREFIX("packet-");
			format_packet(buf_ch, true, tmp_prefix, notif_packet->packet);
		}

		break;
	}
	case BT_NOTIFICATION_TYPE_PACKET_END:
	{
		const struct bt_notification_packet_end *notif_packet =
			(const void *) notif;

		if (notif_packet->packet) {
			SET_TMP_PREFIX("packet-");
			format_packet(buf_ch, true, tmp_prefix, notif_packet->packet);
		}

		break;
	}
	default:
		break;
	}
}

static inline void format_plugin_so_shared_lib_handle(char **buf_ch,
		const char *prefix,
		const struct bt_plugin_so_shared_lib_handle *handle)
{
	BUF_APPEND(", %saddr=%p", PRFIELD(handle));

	if (handle->path) {
		BUF_APPEND(", %spath=\"%s\"", PRFIELD_GSTRING(handle->path));
	}
}

static inline void format_component_class(char **buf_ch, bool extended,
		const char *prefix,
		const struct bt_component_class *comp_class)
{
	char tmp_prefix[64];

	BUF_APPEND(", %stype=%s, %sname=\"%s\"",
		PRFIELD(bt_component_class_type_string(comp_class->type)),
		PRFIELD_GSTRING(comp_class->name));

	if (comp_class->description) {
		BUF_APPEND(", %spartial-descr=\"%.32s\"",
			PRFIELD_GSTRING(comp_class->description));
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
		const char *prefix, const struct bt_component *component)
{
	char tmp_prefix[64];

	BUF_APPEND(", %sname=\"%s\"",
		PRFIELD_GSTRING(component->name));

	if (component->class) {
		SET_TMP_PREFIX("class-");
		format_component_class(buf_ch, extended, tmp_prefix,
			component->class);
	}

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
		const char *prefix, const struct bt_port *port)
{
	char tmp_prefix[64];

	BUF_APPEND(", %stype=%s, %sname=\"%s\"",
		PRFIELD(bt_port_type_string(port->type)),
		PRFIELD_GSTRING(port->name));

	if (!extended) {
		return;
	}

	if (port->connection) {
		SET_TMP_PREFIX("conn-");
		format_connection(buf_ch, false, tmp_prefix, port->connection);
	}
}

static inline void format_connection(char **buf_ch, bool extended,
		const char *prefix, const struct bt_connection *connection)
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
		const char *prefix, const struct bt_graph *graph)
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
		const struct bt_notification_iterator *iterator)
{
	const char *type;
	char tmp_prefix[64];

	if (iterator->type == BT_NOTIFICATION_ITERATOR_TYPE_SELF_COMPONENT_PORT_INPUT) {
		type = "BT_NOTIFICATION_ITERATOR_TYPE_SELF_COMPONENT_PORT_INPUT";
	} else if (iterator->type == BT_NOTIFICATION_ITERATOR_TYPE_PORT_OUTPUT) {
		type = "BT_NOTIFICATION_ITERATOR_TYPE_PORT_OUTPUT";
	} else {
		type = "(unknown)";
	}

	BUF_APPEND(", %stype=%s", PRFIELD(type));

	switch (iterator->type) {
	case BT_NOTIFICATION_ITERATOR_TYPE_SELF_COMPONENT_PORT_INPUT:
	{
		const struct bt_self_component_port_input_notification_iterator *
			port_in_iter = (const void *) iterator;

		if (port_in_iter->upstream_component) {
			SET_TMP_PREFIX("upstream-comp-");
			format_component(buf_ch, false, tmp_prefix,
				port_in_iter->upstream_component);
		}

		if (port_in_iter->upstream_port) {
			SET_TMP_PREFIX("upstream-port-");
			format_port(buf_ch, false, tmp_prefix,
				port_in_iter->upstream_port);
		}

		if (port_in_iter->connection) {
			SET_TMP_PREFIX("upstream-conn-");
			format_connection(buf_ch, false, tmp_prefix,
				port_in_iter->connection);
		}
		break;
	}
	case BT_NOTIFICATION_ITERATOR_TYPE_PORT_OUTPUT:
	{
		const struct bt_port_output_notification_iterator *port_out_iter =
			(const void *) iterator;

		if (port_out_iter->graph) {
			SET_TMP_PREFIX("graph-");
			format_graph(buf_ch, false, tmp_prefix,
				port_out_iter->graph);
		}

		if (port_out_iter->colander) {
			SET_TMP_PREFIX("colander-comp-");
			format_component(buf_ch, false, tmp_prefix,
				(void *) port_out_iter->colander);
		}

		break;
	}
	default:
		break;
	}
}

static inline void format_plugin(char **buf_ch, bool extended,
		const char *prefix, const struct bt_plugin *plugin)
{
	char tmp_prefix[64];

	BUF_APPEND(", %stype=%s", PRFIELD(bt_plugin_type_string(plugin->type)));

	if (plugin->info.path_set) {
		BUF_APPEND(", %spath=\"%s\"",
			PRFIELD_GSTRING(plugin->info.path));
	}

	if (plugin->info.name_set) {
		BUF_APPEND(", %sname=\"%s\"",
			PRFIELD_GSTRING(plugin->info.name));
	}

	if (!extended) {
		return;
	}

	if (plugin->info.author_set) {
		BUF_APPEND(", %sauthor=\"%s\"",
			PRFIELD_GSTRING(plugin->info.author));
	}

	if (plugin->info.license_set) {
		BUF_APPEND(", %slicense=\"%s\"",
			PRFIELD_GSTRING(plugin->info.license));
	}

	if (plugin->info.version_set) {
		BUF_APPEND(", %sversion=%u.%u.%u%s",
			PRFIELD(plugin->info.version.major),
			plugin->info.version.minor,
			plugin->info.version.patch,
			plugin->info.version.extra ?
				plugin->info.version.extra->str : "");
	}

	BUF_APPEND(", %ssrc-comp-class-count=%u, %sflt-comp-class-count=%u, "
		"%ssink-comp-class-count=%u",
		PRFIELD(plugin->src_comp_classes->len),
		PRFIELD(plugin->flt_comp_classes->len),
		PRFIELD(plugin->sink_comp_classes->len));

	if (plugin->spec_data) {
		const struct bt_plugin_so_spec_data *spec_data =
			(const void *) plugin->spec_data;

		if (spec_data->shared_lib_handle) {
			SET_TMP_PREFIX("so-handle-");
			format_plugin_so_shared_lib_handle(buf_ch, tmp_prefix,
				spec_data->shared_lib_handle);
		}
	}
}

static inline void handle_conversion_specifier_bt(void *priv_data,
		char **buf_ch, size_t avail_size,
		const char **out_fmt_ch, va_list *args)
{
	const char *fmt_ch = *out_fmt_ch;
	bool extended = false;
	char prefix[64];
	char *prefix_ch = prefix;
	const void *obj;

	/* skip "%!" */
	fmt_ch += 2;

	if (*fmt_ch == 'u') {
		/* UUID */
		obj = va_arg(*args, void *);
		format_uuid(buf_ch, obj);
		goto update_fmt;
	}

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
	case 'F':
		format_field_class(buf_ch, extended, prefix, obj);
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
	case 'T':
		format_trace_class(buf_ch, extended, prefix, obj);
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
	case 'l':
		format_plugin(buf_ch, extended, prefix, obj);
		break;
	case 'g':
		format_graph(buf_ch, extended, prefix, obj);
		break;
	case 'o':
		format_object_pool(buf_ch, extended, prefix, obj);
		break;
	case 'O':
		format_object(buf_ch, extended, prefix, obj);
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
