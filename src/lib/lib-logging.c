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

#define BT_LOG_TAG "LIB/LIB-LOGGING"
#include "lib/logging.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>
#include <glib.h>
#include "common/common.h"
#include "common/uuid.h"
#include <babeltrace2/babeltrace.h>

#include "logging.h"
#include "assert-pre.h"
#include "assert-post.h"
#include "value.h"
#include "integer-range-set.h"
#include "object-pool.h"
#include "graph/interrupter.h"
#include "graph/component-class.h"
#include "graph/component-filter.h"
#include "graph/component.h"
#include "graph/component-sink.h"
#include "graph/component-source.h"
#include "graph/connection.h"
#include "graph/graph.h"
#include "graph/message/discarded-items.h"
#include "graph/message/event.h"
#include "graph/message/iterator.h"
#include "graph/message/message.h"
#include "graph/message/message-iterator-inactivity.h"
#include "graph/message/packet.h"
#include "graph/message/stream.h"
#include "graph/port.h"
#include "plugin/plugin.h"
#include "plugin/plugin-so.h"
#include "trace-ir/clock-class.h"
#include "trace-ir/clock-snapshot.h"
#include "trace-ir/event-class.h"
#include "trace-ir/event.h"
#include "trace-ir/field-class.h"
#include "trace-ir/field.h"
#include "trace-ir/field-path.h"
#include "trace-ir/packet.h"
#include "trace-ir/stream-class.h"
#include "trace-ir/stream.h"
#include "trace-ir/trace-class.h"
#include "trace-ir/trace.h"
#include "trace-ir/utils.h"
#include "error.h"
#include "assert-pre.h"

#define LIB_LOGGING_BUF_SIZE	(4096 * 4)

static __thread char lib_logging_buf[LIB_LOGGING_BUF_SIZE];

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

#define TMP_PREFIX_LEN 128
#define SET_TMP_PREFIX(_prefix2)					\
	do {								\
		int snprintf_ret =					\
			snprintf(tmp_prefix, TMP_PREFIX_LEN - 1, "%s%s", \
				prefix, (_prefix2));			\
									\
		if (snprintf_ret < 0 || snprintf_ret >= TMP_PREFIX_LEN - 1) { \
			bt_common_abort();					\
		}							\
									\
		tmp_prefix[TMP_PREFIX_LEN - 1] = '\0';			\
	} while (0)

static inline void format_component(char **buf_ch, bool extended,
		const char *prefix, const struct bt_component *component);

static inline void format_port(char **buf_ch, bool extended,
		const char *prefix, const struct bt_port *port);

static inline void format_connection(char **buf_ch, bool extended,
		const char *prefix, const struct bt_connection *connection);

static inline void format_clock_snapshot(char **buf_ch, bool extended,
		const char *prefix, const struct bt_clock_snapshot *clock_snapshot);

static inline void format_field_path(char **buf_ch, bool extended,
		const char *prefix, const struct bt_field_path *field_path);

static inline void format_object(char **buf_ch, bool extended,
		const char *prefix, const struct bt_object *obj)
{
	BUF_APPEND(", %sref-count=%llu", prefix, obj->ref_count);
}

static inline void format_uuid(char **buf_ch, bt_uuid uuid)
{
	BUF_APPEND("\"" BT_UUID_FMT "\"", BT_UUID_FMT_VALUES(uuid));
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
	char tmp_prefix[TMP_PREFIX_LEN];

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
	case BT_FIELD_CLASS_TYPE_BIT_ARRAY:
	{
		const struct bt_field_class_bit_array *ba_fc =
			(const void *) field_class;

		BUF_APPEND(", %slength=%" PRIu64, PRFIELD(ba_fc->length));
		break;
	}
	case BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER:
	case BT_FIELD_CLASS_TYPE_SIGNED_INTEGER:
	{
		format_integer_field_class(buf_ch, extended, prefix, field_class);
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
		const struct bt_field_class_array_static *array_fc =
			(const void *) field_class;

		format_array_field_class(buf_ch, extended, prefix, field_class);
		BUF_APPEND(", %slength=%" PRIu64, PRFIELD(array_fc->length));
		break;
	}
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITHOUT_LENGTH_FIELD:
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD:
	{
		const struct bt_field_class_array_dynamic *array_fc =
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
	case BT_FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD:
	case BT_FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD:
	case BT_FIELD_CLASS_TYPE_OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD:
	case BT_FIELD_CLASS_TYPE_OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD:
	{
		const struct bt_field_class_option *opt_fc =
			(const void *) field_class;

		BUF_APPEND(", %scontent-fc-addr=%p, %scontent-fc-type=%s",
			PRFIELD(opt_fc->content_fc),
			PRFIELD(bt_common_field_class_type_string(opt_fc->content_fc->type)));

		if (field_class->type !=
				BT_FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD) {
			const struct bt_field_class_option_with_selector_field *opt_with_sel_fc =
				(const void *) field_class;

			if (opt_with_sel_fc->selector_fc) {
				SET_TMP_PREFIX("selector-fc-");
				format_field_class(buf_ch, extended, tmp_prefix,
					opt_with_sel_fc->selector_fc);
			}

			if (opt_with_sel_fc->selector_field_path) {
				SET_TMP_PREFIX("selector-field-path-");
				format_field_path(buf_ch, extended, tmp_prefix,
					opt_with_sel_fc->selector_field_path);
			}
		}

		break;
	}
	case BT_FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD:
	case BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD:
	case BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD:
	{
		const struct bt_field_class_variant *var_fc =
			(const void *) field_class;

		if (var_fc->common.named_fcs) {
			BUF_APPEND(", %soption-count=%u",
				PRFIELD(var_fc->common.named_fcs->len));
		}

		if (field_class->type == BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD ||
				field_class->type == BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD) {
			const struct bt_field_class_variant_with_selector_field *var_with_sel_fc =
				(const void *) var_fc;

			if (var_with_sel_fc->selector_fc) {
				SET_TMP_PREFIX("selector-fc-");
				format_field_class(buf_ch, extended, tmp_prefix,
					var_with_sel_fc->selector_fc);
			}

			if (var_with_sel_fc->selector_field_path) {
				SET_TMP_PREFIX("selector-field-path-");
				format_field_path(buf_ch, extended, tmp_prefix,
					var_with_sel_fc->selector_field_path);
			}
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
	case BT_FIELD_CLASS_TYPE_BOOL:
	{
		const struct bt_field_bool *bool_field = (const void *) field;

		BUF_APPEND(", %svalue=%d", PRFIELD(bool_field->value));
		break;
	}
	case BT_FIELD_CLASS_TYPE_BIT_ARRAY:
	{
		const struct bt_field_bit_array *ba_field = (const void *) field;

		BUF_APPEND(", %svalue-as-int=%" PRIx64,
			PRFIELD(ba_field->value_as_int));
		break;
	}
	case BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER:
	case BT_FIELD_CLASS_TYPE_SIGNED_INTEGER:
	case BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION:
	case BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION:
	{
		format_field_integer_extended(buf_ch, prefix, field);
		break;
	}
	case BT_FIELD_CLASS_TYPE_SINGLE_PRECISION_REAL:
	case BT_FIELD_CLASS_TYPE_DOUBLE_PRECISION_REAL:
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
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITHOUT_LENGTH_FIELD:
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD:
	{
		const struct bt_field_array *array_field = (const void *) field;

		BUF_APPEND(", %slength=%" PRIu64, PRFIELD(array_field->length));

		if (array_field->fields) {
			BUF_APPEND(", %sallocated-length=%u",
				PRFIELD(array_field->fields->len));
		}

		break;
	}
	case BT_FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD:
	case BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD:
	case BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD:
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

	if (field_path->items) {
		BT_ASSERT(field_path->items);
		BUF_APPEND(", %sitem-count=%u",
			PRFIELD(field_path->items->len));
	}

	if (!extended || !field_path->items) {
		return;
	}

	BUF_APPEND(", %spath=[%s",
		PRFIELD(bt_common_scope_string(field_path->root)));

	for (i = 0; i < bt_field_path_get_item_count(field_path); i++) {
		const struct bt_field_path_item *fp_item =
			bt_field_path_borrow_item_by_index_const(field_path, i);

		switch (bt_field_path_item_get_type(fp_item)) {
		case BT_FIELD_PATH_ITEM_TYPE_INDEX:
			BUF_APPEND(", %" PRIu64,
				bt_field_path_item_index_get_index(fp_item));
			break;
		case BT_FIELD_PATH_ITEM_TYPE_CURRENT_ARRAY_ELEMENT:
			BUF_APPEND("%s", ", <CUR>");
			break;
		default:
			bt_common_abort();
		}
	}

	BUF_APPEND("%s", "]");
}

static inline void format_trace_class(char **buf_ch, bool extended,
		const char *prefix, const struct bt_trace_class *trace_class)
{
	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-frozen=%d", PRFIELD(trace_class->frozen));

	if (trace_class->stream_classes) {
		BUF_APPEND(", %sstream-class-count=%u",
			PRFIELD(trace_class->stream_classes->len));
	}

	BUF_APPEND(", %sassigns-auto-sc-id=%d",
		PRFIELD(trace_class->assigns_automatic_stream_class_id));
}

static inline void format_trace(char **buf_ch, bool extended,
		const char *prefix, const struct bt_trace *trace)
{
	char tmp_prefix[TMP_PREFIX_LEN];

	if (trace->name.value) {
		BUF_APPEND(", %sname=\"%s\"", PRFIELD(trace->name.value));
	}

	if (!extended) {
		return;
	}

	if (trace->uuid.value) {
		BUF_APPEND_UUID(trace->uuid.value);
	}

	BUF_APPEND(", %sis-frozen=%d", PRFIELD(trace->frozen));

	if (trace->streams) {
		BUF_APPEND(", %sstream-count=%u",
			PRFIELD(trace->streams->len));
	}

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
	char tmp_prefix[TMP_PREFIX_LEN];

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
		"%sevent-common-context-fc-addr=%p",
		PRFIELD(stream_class->packet_context_fc),
		PRFIELD(stream_class->event_common_context_fc));
	trace_class = bt_stream_class_borrow_trace_class_inline(stream_class);
	if (!trace_class) {
		return;
	}

	BUF_APPEND(", %sassigns-auto-ec-id=%d, %sassigns-auto-stream-id=%d, "
		"%ssupports-packets=%d, "
		"%spackets-have-begin-default-cs=%d, "
		"%spackets-have-end-default-cs=%d, "
		"%ssupports-discarded-events=%d, "
		"%sdiscarded-events-have-default-cs=%d, "
		"%ssupports-discarded-packets=%d, "
		"%sdiscarded-packets-have-default-cs=%d",
		PRFIELD(stream_class->assigns_automatic_event_class_id),
		PRFIELD(stream_class->assigns_automatic_stream_id),
		PRFIELD(stream_class->supports_packets),
		PRFIELD(stream_class->packets_have_beginning_default_clock_snapshot),
		PRFIELD(stream_class->packets_have_end_default_clock_snapshot),
		PRFIELD(stream_class->supports_discarded_events),
		PRFIELD(stream_class->discarded_events_have_default_clock_snapshots),
		PRFIELD(stream_class->supports_discarded_packets),
		PRFIELD(stream_class->discarded_packets_have_default_clock_snapshots));
	BUF_APPEND(", %strace-class-addr=%p", PRFIELD(trace_class));
	SET_TMP_PREFIX("trace-class-");
	format_trace_class(buf_ch, false, tmp_prefix, trace_class);
	SET_TMP_PREFIX("pcf-pool-");
	format_object_pool(buf_ch, extended, tmp_prefix,
		&stream_class->packet_context_field_pool);
}

static inline void format_event_class(char **buf_ch, bool extended,
		const char *prefix, const struct bt_event_class *event_class)
{
	const struct bt_stream_class *stream_class;
	const struct bt_trace_class *trace_class;
	char tmp_prefix[TMP_PREFIX_LEN];

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
	format_object_pool(buf_ch, extended, tmp_prefix,
		&event_class->event_pool);
}

static inline void format_stream(char **buf_ch, bool extended,
		const char *prefix, const struct bt_stream *stream)
{
	const struct bt_stream_class *stream_class;
	const struct bt_trace_class *trace_class = NULL;
	const struct bt_trace *trace = NULL;
	char tmp_prefix[TMP_PREFIX_LEN];

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
	format_object_pool(buf_ch, extended, tmp_prefix, &stream->packet_pool);
}

static inline void format_packet(char **buf_ch, bool extended,
		const char *prefix, const struct bt_packet *packet)
{
	const struct bt_stream *stream;
	const struct bt_trace_class *trace_class;
	char tmp_prefix[TMP_PREFIX_LEN];

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-frozen=%d, %scontext-field-addr=%p",
		PRFIELD(packet->frozen),
		PRFIELD(packet->context_field ? packet->context_field->field : NULL));
	stream = bt_packet_borrow_stream_const(packet);
	if (!stream) {
		return;
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
	const struct bt_trace_class *trace_class;
	const struct bt_stream_class *stream_class;
	char tmp_prefix[TMP_PREFIX_LEN];

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-frozen=%d, "
		"%scommon-context-field-addr=%p, "
		"%sspecific-context-field-addr=%p, "
		"%spayload-field-addr=%p, ",
		PRFIELD(event->frozen),
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

	if (event->stream) {
		BUF_APPEND(", %sstream-addr=%p", PRFIELD(event->stream));
		SET_TMP_PREFIX("stream-");
		format_stream(buf_ch, false, tmp_prefix, event->stream);
	}

	if (event->packet) {
		BUF_APPEND(", %spacket-addr=%p", PRFIELD(event->packet));
		SET_TMP_PREFIX("packet-");
		format_packet(buf_ch, false, tmp_prefix, event->packet);
	}
}

static inline void format_clock_class(char **buf_ch, bool extended,
		const char *prefix, const struct bt_clock_class *clock_class)
{
	char tmp_prefix[TMP_PREFIX_LEN];

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
		"%soffset-cycles=%" PRIu64 ", %sorigin-is-unix-epoch=%d, "
		"%sbase-offset-ns=%" PRId64,
		PRFIELD(clock_class->frozen), PRFIELD(clock_class->precision),
		PRFIELD(clock_class->offset_seconds),
		PRFIELD(clock_class->offset_cycles),
		PRFIELD(clock_class->origin_is_unix_epoch),
		PRFIELD(clock_class->base_offset.value_ns));

	SET_TMP_PREFIX("cs-pool-");
	format_object_pool(buf_ch, extended, tmp_prefix,
		&clock_class->cs_pool);
}

static inline void format_clock_snapshot(char **buf_ch, bool extended,
		const char *prefix, const struct bt_clock_snapshot *clock_snapshot)
{
	char tmp_prefix[TMP_PREFIX_LEN];
	BUF_APPEND(", %svalue=%" PRIu64 ", %sns-from-origin=%" PRId64,
		PRFIELD(clock_snapshot->value_cycles),
		PRFIELD(clock_snapshot->ns_from_origin));

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-set=%d", PRFIELD(clock_snapshot->is_set));

	if (clock_snapshot->clock_class) {
		BUF_APPEND(", %sclock-class-addr=%p",
			PRFIELD(clock_snapshot->clock_class));
		SET_TMP_PREFIX("clock-class-");
		format_clock_class(buf_ch, false, tmp_prefix,
			clock_snapshot->clock_class);
	}
}

static inline void format_interrupter(char **buf_ch, bool extended,
		const char *prefix, const struct bt_interrupter *intr)
{
	BUF_APPEND(", %sis-set=%d", PRFIELD(intr->is_set));
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
	case BT_VALUE_TYPE_UNSIGNED_INTEGER:
	{
		BUF_APPEND(", %svalue=%" PRIu64,
			PRFIELD(bt_value_integer_unsigned_get(value)));
		break;
	}
	case BT_VALUE_TYPE_SIGNED_INTEGER:
	{
		BUF_APPEND(", %svalue=%" PRId64,
			PRFIELD(bt_value_integer_signed_get(value)));
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
		uint64_t count = bt_value_array_get_length(value);

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

static inline void format_integer_range_set(char **buf_ch, bool extended,
		const char *prefix,
		const struct bt_integer_range_set *range_set)
{
	BUF_APPEND(", %srange-count=%u", PRFIELD(range_set->ranges->len));

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-frozen=%d", PRFIELD(range_set->frozen));
}

static inline void format_message(char **buf_ch, bool extended,
		const char *prefix, const struct bt_message *msg)
{
	char tmp_prefix[TMP_PREFIX_LEN];

	BUF_APPEND(", %stype=%s",
		PRFIELD(bt_message_type_string(msg->type)));

	if (!extended) {
		return;
	}

	BUF_APPEND(", %sis-frozen=%d, %sgraph-addr=%p",
		PRFIELD(msg->frozen), PRFIELD(msg->graph));

	switch (msg->type) {
	case BT_MESSAGE_TYPE_EVENT:
	{
		const struct bt_message_event *msg_event =
			(const void *) msg;

		if (msg_event->event) {
			SET_TMP_PREFIX("event-");
			format_event(buf_ch, true, tmp_prefix,
				msg_event->event);
		}

		if (msg_event->default_cs) {
			SET_TMP_PREFIX("default-cs-");
			format_clock_snapshot(buf_ch, true, tmp_prefix,
				msg_event->default_cs);
		}

		break;
	}
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
	case BT_MESSAGE_TYPE_STREAM_END:
	{
		const struct bt_message_stream *msg_stream = (const void *) msg;

		if (msg_stream->stream) {
			SET_TMP_PREFIX("stream-");
			format_stream(buf_ch, true, tmp_prefix,
				msg_stream->stream);
		}

		BUF_APPEND(", %sdefault-cs-state=%s",
			PRFIELD(bt_message_stream_clock_snapshot_state_string(
				msg_stream->default_cs_state)));

		if (msg_stream->default_cs_state == BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN) {
			SET_TMP_PREFIX("default-cs-");
			format_clock_snapshot(buf_ch, true, tmp_prefix,
				msg_stream->default_cs);
		}

		break;
	}
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
	case BT_MESSAGE_TYPE_PACKET_END:
	{
		const struct bt_message_packet *msg_packet = (const void *) msg;

		if (msg_packet->packet) {
			SET_TMP_PREFIX("packet-");
			format_packet(buf_ch, true, tmp_prefix,
				msg_packet->packet);
		}

		if (msg_packet->default_cs) {
			SET_TMP_PREFIX("default-cs-");
			format_clock_snapshot(buf_ch, true, tmp_prefix,
				msg_packet->default_cs);
		}

		break;
	}
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
	{
		const struct bt_message_discarded_items *msg_disc_items =
			(const void *) msg;

		if (msg_disc_items->stream) {
			SET_TMP_PREFIX("stream-");
			format_stream(buf_ch, true, tmp_prefix,
				msg_disc_items->stream);
		}

		if (msg_disc_items->default_begin_cs) {
			SET_TMP_PREFIX("begin-default-cs-");
			format_clock_snapshot(buf_ch, true, tmp_prefix,
				msg_disc_items->default_begin_cs);
		}

		if (msg_disc_items->default_end_cs) {
			SET_TMP_PREFIX("end-default-cs-");
			format_clock_snapshot(buf_ch, true, tmp_prefix,
				msg_disc_items->default_end_cs);
		}

		if (msg_disc_items->count.base.avail) {
			BUF_APPEND(", %scount=%" PRIu64,
				PRFIELD(msg_disc_items->count.value));
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
	char tmp_prefix[TMP_PREFIX_LEN];

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
	char tmp_prefix[TMP_PREFIX_LEN];

	BUF_APPEND(", %sname=\"%s\", %slog-level=%s",
		PRFIELD_GSTRING(component->name),
		PRFIELD(bt_common_logging_level_string(component->log_level)));

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
	char tmp_prefix[TMP_PREFIX_LEN];

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
	char tmp_prefix[TMP_PREFIX_LEN];

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
	char tmp_prefix[TMP_PREFIX_LEN];

	BUF_APPEND(", %scan-consume=%d, %sconfig-state=%s",
		PRFIELD(graph->can_consume),
		PRFIELD(bt_graph_configuration_state_string(graph->config_state)));

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
	format_object_pool(buf_ch, extended, tmp_prefix,
		&graph->event_msg_pool);
	SET_TMP_PREFIX("pbn-pool-");
	format_object_pool(buf_ch, extended, tmp_prefix,
		&graph->packet_begin_msg_pool);
	SET_TMP_PREFIX("pen-pool-");
	format_object_pool(buf_ch, extended, tmp_prefix,
		&graph->packet_end_msg_pool);
}

static inline void format_message_iterator_class(char **buf_ch,
		bool extended, const char *prefix,
		const struct bt_message_iterator_class *iterator_class)
{
	/* Empty, the address is automatically printed. */
}

static inline void format_message_iterator(char **buf_ch,
		bool extended, const char *prefix,
		const struct bt_message_iterator *iterator)
{
	char tmp_prefix[TMP_PREFIX_LEN];
	const struct bt_message_iterator *
		port_in_iter = (const void *) iterator;

	if (port_in_iter->upstream_component) {
		SET_TMP_PREFIX("upstream-comp-");
		format_component(buf_ch, false, tmp_prefix,
			port_in_iter->upstream_component);
	}

	if (!extended) {
		goto end;
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

end:
	return;
}

static inline void format_plugin(char **buf_ch, bool extended,
		const char *prefix, const struct bt_plugin *plugin)
{
	char tmp_prefix[TMP_PREFIX_LEN];

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

static inline void format_error_cause(char **buf_ch, bool extended,
		const char *prefix, const struct bt_error_cause *cause)
{
	const struct bt_error_cause_component_class_id *comp_class_id = NULL;

	BUF_APPEND(", %sactor-type=%s, %smodule-name=\"%s\"",
		PRFIELD(bt_error_cause_actor_type_string(cause->actor_type)),
		PRFIELD_GSTRING(cause->module_name));

	if (!extended) {
		return;
	}

	BUF_APPEND(", %spartial-msg=\"%.32s\"",
		PRFIELD_GSTRING(cause->message));

	switch (cause->actor_type) {
	case BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT:
	{
		const struct bt_error_cause_component_actor *spec_cause =
			(const void *) cause;

		BUF_APPEND(", %scomp-name=\"%s\"",
			PRFIELD_GSTRING(spec_cause->comp_name));
		comp_class_id = &spec_cause->comp_class_id;
		break;
	}
	case BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT_CLASS:
	{
		const struct bt_error_cause_component_class_actor *spec_cause =
			(const void *) cause;

		comp_class_id = &spec_cause->comp_class_id;
		break;
	}
	case BT_ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR:
	{
		const struct bt_error_cause_message_iterator_actor *spec_cause =
			(const void *) cause;

		BUF_APPEND(", %scomp-name=\"%s\", %scomp-out-port-name=\"%s\"",
			PRFIELD_GSTRING(spec_cause->comp_name),
			PRFIELD_GSTRING(spec_cause->output_port_name));
		comp_class_id = &spec_cause->comp_class_id;
		break;
	}
	default:
		break;
	}

	if (comp_class_id) {
		BUF_APPEND(", %scomp-cls-type=%s, %scomp-cls-name=\"%s\", "
			"%splugin-name=\"%s\"",
			PRFIELD(bt_component_class_type_string(
				comp_class_id->type)),
			PRFIELD_GSTRING(comp_class_id->name),
			PRFIELD_GSTRING(comp_class_id->plugin_name));
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
		format_clock_snapshot(buf_ch, extended, prefix, obj);
		break;
	case 'v':
		format_value(buf_ch, extended, prefix, obj);
		break;
	case 'R':
		format_integer_range_set(buf_ch, extended, prefix, obj);
		break;
	case 'n':
		format_message(buf_ch, extended, prefix, obj);
		break;
	case 'I':
		format_message_iterator_class(buf_ch, extended, prefix, obj);
		break;
	case 'i':
		format_message_iterator(buf_ch, extended, prefix, obj);
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
	case 'z':
		format_interrupter(buf_ch, extended, prefix, obj);
		break;
	case 'o':
		format_object_pool(buf_ch, extended, prefix, obj);
		break;
	case 'O':
		format_object(buf_ch, extended, prefix, obj);
		break;
	case 'r':
		format_error_cause(buf_ch, extended, prefix, obj);
		break;
	default:
		bt_common_abort();
	}

update_fmt:
	fmt_ch++;
	*out_fmt_ch = fmt_ch;
}

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

void bt_lib_maybe_log_and_append_cause(const char *func, const char *file,
		unsigned line, int lvl, const char *tag,
		const char *fmt, ...)
{
	va_list args;
	bt_current_thread_error_append_cause_status status;

	BT_ASSERT(fmt);
	va_start(args, fmt);
	bt_common_custom_vsnprintf(lib_logging_buf, LIB_LOGGING_BUF_SIZE, '!',
		handle_conversion_specifier_bt, NULL, fmt, &args);
	va_end(args);

	/* Log conditionally, but always append the error cause */
	if (BT_LOG_ON(lvl)) {
		_bt_log_write_d(func, file, line, lvl, tag, "%s",
			lib_logging_buf);
	}

	status = bt_current_thread_error_append_cause_from_unknown(
		BT_LIB_LOG_LIBBABELTRACE2_NAME, file, line, "%s",
		lib_logging_buf);
	if (status) {
		/*
		 * Worst case: this error cause is not appended to the
		 * current thread's error.
		 *
		 * We can accept this as it's an almost impossible
		 * scenario and returning an error here would mean you
		 * need to check the return value of each
		 * BT_LIB_LOG*_APPEND_CAUSE() macro and that would be
		 * cumbersome.
		 */
		BT_LOGE("Cannot append error cause to current thread's "
			"error object: status=%s",
			bt_common_func_status_string(status));
	}
}
