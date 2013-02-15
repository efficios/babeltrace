/*
 * ctf/events.c
 *
 * Babeltrace Library
 *
 * Copyright 2011-2012 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *         Julien Desfossez <julien.desfossez@efficios.com>
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

#include <babeltrace/babeltrace.h>
#include <babeltrace/format.h>
#include <babeltrace/ctf/events.h>
#include <babeltrace/ctf-ir/metadata.h>
#include <babeltrace/prio_heap.h>
#include <babeltrace/iterator-internal.h>
#include <babeltrace/ctf/events-internal.h>
#include <babeltrace/ctf/metadata.h>
#include <glib.h>

#include "events-private.h"

/*
 * thread local storage to store the last error that occured
 * while reading a field, this variable must be accessed by
 * bt_ctf_field_get_error only
 */
__thread int bt_ctf_last_field_error = 0;

const struct bt_definition *bt_ctf_get_top_level_scope(const struct bt_ctf_event *ctf_event,
		enum bt_ctf_scope scope)
{
	const struct bt_definition *tmp = NULL;
	const struct ctf_event_definition *event;

	if (!ctf_event)
		return NULL;

	event = ctf_event->parent;
	switch (scope) {
	case BT_TRACE_PACKET_HEADER:
		if (!event->stream)
			goto error;
		if (event->stream->trace_packet_header)
			tmp = &event->stream->trace_packet_header->p;
		break;
	case BT_STREAM_PACKET_CONTEXT:
		if (!event->stream)
			goto error;
		if (event->stream->stream_packet_context)
			tmp = &event->stream->stream_packet_context->p;
		break;
	case BT_STREAM_EVENT_HEADER:
		if (!event->stream)
			goto error;
		if (event->stream->stream_event_header)
			tmp = &event->stream->stream_event_header->p;
		break;
	case BT_STREAM_EVENT_CONTEXT:
		if (!event->stream)
			goto error;
		if (event->stream->stream_event_context)
			tmp = &event->stream->stream_event_context->p;
		break;
	case BT_EVENT_CONTEXT:
		if (event->event_context)
			tmp = &event->event_context->p;
		break;
	case BT_EVENT_FIELDS:
		if (event->event_fields)
			tmp = &event->event_fields->p;
		break;
	}
	return tmp;

error:
	return NULL;
}

const struct bt_definition *bt_ctf_get_field(const struct bt_ctf_event *ctf_event,
		const struct bt_definition *scope,
		const char *field)
{
	const struct bt_definition *def;
	char *field_underscore;

	if (!ctf_event || !scope || !field)
		return NULL;

	def = bt_lookup_definition(scope, field);
	/*
	 * optionally a field can have an underscore prefix, try
	 * to lookup the field with this prefix if it failed
	 */
	if (!def) {
		field_underscore = g_new(char, strlen(field) + 2);
		field_underscore[0] = '_';
		strcpy(&field_underscore[1], field);
		def = bt_lookup_definition(scope, field_underscore);
		g_free(field_underscore);
	}
	if (bt_ctf_field_type(bt_ctf_get_decl_from_def(def)) == CTF_TYPE_VARIANT) {
		const struct definition_variant *variant_definition;
		variant_definition = container_of(def,
				const struct definition_variant, p);
		return variant_definition->current_field;
	}
	return def;
}

const struct bt_definition *bt_ctf_get_index(const struct bt_ctf_event *ctf_event,
		const struct bt_definition *field,
		unsigned int index)
{
	struct bt_definition *ret = NULL;

	if (!ctf_event || !field)
		return NULL;

	if (bt_ctf_field_type(bt_ctf_get_decl_from_def(field)) == CTF_TYPE_ARRAY) {
		struct definition_array *array_definition;
		array_definition = container_of(field,
				struct definition_array, p);
		ret = bt_array_index(array_definition, index);
	} else if (bt_ctf_field_type(bt_ctf_get_decl_from_def(field)) == CTF_TYPE_SEQUENCE) {
		struct definition_sequence *sequence_definition;
		sequence_definition = container_of(field,
				struct definition_sequence, p);
		ret = bt_sequence_index(sequence_definition, index);
	}
	return ret;
}

const char *bt_ctf_event_name(const struct bt_ctf_event *ctf_event)
{
	const struct ctf_event_declaration *event_class;
	const struct ctf_stream_declaration *stream_class;
	const struct ctf_event_definition *event;

	if (!ctf_event)
		return NULL;

	event = ctf_event->parent;
	stream_class = event->stream->stream_class;
	event_class = g_ptr_array_index(stream_class->events_by_id,
			event->stream->event_id);
	return g_quark_to_string(event_class->name);
}

const char *bt_ctf_field_name(const struct bt_definition *def)
{
	if (!def || !def->name)
		return NULL;

	return rem_(g_quark_to_string(def->name));
}

enum ctf_type_id bt_ctf_field_type(const struct bt_declaration *decl)
{
	if (!decl)
		return CTF_TYPE_UNKNOWN;

	return decl->id;
}

int bt_ctf_get_field_list(const struct bt_ctf_event *ctf_event,
		const struct bt_definition *scope,
		struct bt_definition const * const **list,
		unsigned int *count)
{
	if (!ctf_event || !scope || !list || !count)
		return -EINVAL;

	switch (bt_ctf_field_type(bt_ctf_get_decl_from_def(scope))) {
	case CTF_TYPE_INTEGER:
	case CTF_TYPE_FLOAT:
	case CTF_TYPE_STRING:
	case CTF_TYPE_ENUM:
		goto error;
	case CTF_TYPE_STRUCT:
	{
		const struct definition_struct *def_struct;

		def_struct = container_of(scope, const struct definition_struct, p);
		if (!def_struct)
			goto error;
		if (def_struct->fields->pdata) {
			*list = (struct bt_definition const* const*) def_struct->fields->pdata;
			*count = def_struct->fields->len;
			goto end;
		} else {
			goto error;
		}
		break;
	}
	case CTF_TYPE_UNTAGGED_VARIANT:
		goto error;
	case CTF_TYPE_VARIANT:
	{
		const struct definition_variant *def_variant;

		def_variant = container_of(scope, const struct definition_variant, p);
		if (!def_variant)
			goto error;
		if (def_variant->fields->pdata) {
			*list = (struct bt_definition const* const*) def_variant->fields->pdata;
			*count = def_variant->fields->len;
			goto end;
		} else {
			goto error;
		}
		break;
	}
	case CTF_TYPE_ARRAY:
	{
		const struct definition_array *def_array;

		def_array = container_of(scope, const struct definition_array, p);
		if (!def_array)
			goto error;
		if (def_array->elems->pdata) {
			*list = (struct bt_definition const* const*) def_array->elems->pdata;
			*count = def_array->elems->len;
			goto end;
		} else {
			goto error;
		}
		break;
	}
	case CTF_TYPE_SEQUENCE:
	{
		const struct definition_sequence *def_sequence;

		def_sequence = container_of(scope, const struct definition_sequence, p);
		if (!def_sequence)
			goto error;
		if (def_sequence->elems->pdata) {
			*list = (struct bt_definition const* const*) def_sequence->elems->pdata;
			*count = def_sequence->elems->len;
			goto end;
		} else {
			goto error;
		}
		break;
	}
	default:
		break;
	}

end:
	return 0;

error:
	*list = NULL;
	*count = 0;
	return -1;
}

struct bt_context *bt_ctf_event_get_context(const struct bt_ctf_event *ctf_event)
{
	struct bt_context *ret = NULL;
	const struct ctf_file_stream *cfs;
	const struct ctf_trace *trace;
	const struct ctf_event_definition *event;

	if (!ctf_event)
		return NULL;

	event = ctf_event->parent;
	cfs = container_of(event->stream, const struct ctf_file_stream,
			parent);
	trace = cfs->parent.stream_class->trace;
	if (trace->ctx)
		ret = trace->ctx;

	return ret;
}

int bt_ctf_event_get_handle_id(const struct bt_ctf_event *ctf_event)
{
	int ret = -1;
	const struct ctf_file_stream *cfs;
	const struct ctf_trace *trace;
	const struct ctf_event_definition *event;

	if (!ctf_event)
		return -EINVAL;

	event = ctf_event->parent;
	cfs = container_of(event->stream, const struct ctf_file_stream,
			parent);
	trace = cfs->parent.stream_class->trace;
	if (trace->handle)
		ret = trace->handle->id;

	return ret;
}

uint64_t bt_ctf_get_timestamp(const struct bt_ctf_event *ctf_event)
{
	const struct ctf_event_definition *event;

	if (!ctf_event)
		return -1ULL;

	event = ctf_event->parent;
	if (event && event->stream->has_timestamp)
		return event->stream->real_timestamp;
	else
		return -1ULL;
}

uint64_t bt_ctf_get_cycles(const struct bt_ctf_event *ctf_event)
{
	const struct ctf_event_definition *event;

	if (!ctf_event)
		return -1ULL;

	event = ctf_event->parent;
	if (event && event->stream->has_timestamp)
		return event->stream->cycles_timestamp;
	else
		return -1ULL;
}

static void bt_ctf_field_set_error(int error)
{
	bt_ctf_last_field_error = error;
}

int bt_ctf_field_get_error(void)
{
	int ret;
	ret = bt_ctf_last_field_error;
	bt_ctf_last_field_error = 0;

	return ret;
}

static const struct declaration_integer *
get_declaration_integer(const struct bt_declaration *decl)
{
	if (!decl || bt_ctf_field_type(decl) != CTF_TYPE_INTEGER)
		return NULL;
	return container_of(decl, const struct declaration_integer, p);
}

static const struct declaration_string *
get_declaration_string(const struct bt_declaration *decl)
{
	if (!decl || bt_ctf_field_type(decl) != CTF_TYPE_STRING)
		return NULL;
	return container_of(decl, const struct declaration_string, p);
}

static const struct declaration_array *
get_declaration_array(const struct bt_declaration *decl)
{
	if (!decl || bt_ctf_field_type(decl) != CTF_TYPE_ARRAY)
		return NULL;
	return container_of(decl, const struct declaration_array, p);
}

static const struct declaration_sequence *
get_declaration_sequence(const struct bt_declaration *decl)
{
	if (!decl || bt_ctf_field_type(decl) != CTF_TYPE_SEQUENCE)
		return NULL;
	return container_of(decl, const struct declaration_sequence, p);
}

int bt_ctf_get_int_signedness(const struct bt_declaration *decl)
{
	const struct declaration_integer *integer;

	integer = get_declaration_integer(decl);
	if (!integer) {
		bt_ctf_field_set_error(-EINVAL);
		return -EINVAL;
	}
	return integer->signedness;
}

int bt_ctf_get_int_base(const struct bt_declaration *decl)
{
	const struct declaration_integer *integer;

	integer = get_declaration_integer(decl);
	if (!integer) {
		bt_ctf_field_set_error(-EINVAL);
		return -EINVAL;
	}
	return integer->base;
}

int bt_ctf_get_int_byte_order(const struct bt_declaration *decl)
{
	const struct declaration_integer *integer;

	integer = get_declaration_integer(decl);
	if (!integer) {
		bt_ctf_field_set_error(-EINVAL);
		return -EINVAL;
	}
	return integer->byte_order;
}

ssize_t bt_ctf_get_int_len(const struct bt_declaration *decl)
{
	const struct declaration_integer *integer;

	integer = get_declaration_integer(decl);
	if (!integer) {
		bt_ctf_field_set_error(-EINVAL);
		return -EINVAL;
	}
	return (ssize_t) integer->len;
}

const struct bt_definition *bt_ctf_get_enum_int(const struct bt_definition *field)
{
	const struct definition_enum *def_enum;

	if (!field || bt_ctf_field_type(bt_ctf_get_decl_from_def(field)) != CTF_TYPE_ENUM) {
		bt_ctf_field_set_error(-EINVAL);
		return NULL;
	}
	def_enum = container_of(field, const struct definition_enum, p);
	return &def_enum->integer->p;
}

const char *bt_ctf_get_enum_str(const struct bt_definition *field)
{
	const struct definition_enum *def_enum;
	const struct declaration_enum *decl_enum;
	GArray *array;
	const char *ret;

	if (!field || bt_ctf_field_type(bt_ctf_get_decl_from_def(field)) != CTF_TYPE_ENUM) {
		bt_ctf_field_set_error(-EINVAL);
		return NULL;
	}
	def_enum = container_of(field, const struct definition_enum, p);
	decl_enum = def_enum->declaration;
	if (bt_get_int_signedness(&def_enum->integer->p)) {
		array = bt_enum_int_to_quark_set(decl_enum,
			bt_get_signed_int(&def_enum->integer->p));
	} else {
		array = bt_enum_uint_to_quark_set(decl_enum,
			bt_get_unsigned_int(&def_enum->integer->p));
	}
	if (!array) {
		bt_ctf_field_set_error(-ENOENT);
		return NULL;
	}

	if (array->len == 0) {
		g_array_unref(array);
		bt_ctf_field_set_error(-ENOENT);
		return NULL;
	}	
	/* Return first string. Arbitrary choice. */
	ret = g_quark_to_string(g_array_index(array, GQuark, 0));
	g_array_unref(array);
	return ret;
}

enum ctf_string_encoding bt_ctf_get_encoding(const struct bt_declaration *decl)
{
	enum ctf_string_encoding ret = 0;
	enum ctf_type_id type;
	const struct declaration_integer *integer;
	const struct declaration_string *string;
	const struct declaration_array *array;
	const struct declaration_sequence *sequence;

	if (!decl)
		goto error;

	type = bt_ctf_field_type(decl);

	switch (type) {
	case CTF_TYPE_ARRAY:
		array = get_declaration_array(decl);
		if (!array)
			goto error;
		integer = get_declaration_integer(array->elem);
		if (!integer)
			goto error;
		ret = integer->encoding;
		break;
	case CTF_TYPE_SEQUENCE:
		sequence = get_declaration_sequence(decl);
		if (!sequence)
			goto error;
		integer = get_declaration_integer(sequence->elem);
		if (!integer)
			goto error;
		ret = integer->encoding;
		break;
	case CTF_TYPE_STRING:
		string = get_declaration_string(decl);
		if (!string)
			goto error;
		ret = string->encoding;
		break;
	case CTF_TYPE_INTEGER:
		integer = get_declaration_integer(decl);
		if (!integer)
			goto error;
		ret = integer->encoding;
		break;
	default:
		goto error;
	}
	return ret;

error:
	bt_ctf_field_set_error(-EINVAL);
	return -1;
}

int bt_ctf_get_array_len(const struct bt_declaration *decl)
{
	const struct declaration_array *array;

	array = get_declaration_array(decl);
	if (!array)
		goto error;
	return array->len;

error:
	bt_ctf_field_set_error(-EINVAL);
	return -1;
}

uint64_t bt_ctf_get_uint64(const struct bt_definition *field)
{
	uint64_t ret = 0;

	if (field && bt_ctf_field_type(bt_ctf_get_decl_from_def(field)) == CTF_TYPE_INTEGER)
		ret = bt_get_unsigned_int(field);
	else
		bt_ctf_field_set_error(-EINVAL);

	return ret;
}

int64_t bt_ctf_get_int64(const struct bt_definition *field)
{
	int64_t ret = 0;

	if (field && bt_ctf_field_type(bt_ctf_get_decl_from_def(field)) == CTF_TYPE_INTEGER)
		ret = bt_get_signed_int(field);
	else
		bt_ctf_field_set_error(-EINVAL);

	return ret;
}

char *bt_ctf_get_char_array(const struct bt_definition *field)
{
	char *ret = NULL;
	GString *char_array;

	if (field && bt_ctf_field_type(bt_ctf_get_decl_from_def(field)) == CTF_TYPE_ARRAY) {
		char_array = bt_get_char_array(field);
		if (char_array) {
			ret = char_array->str;
			goto end;
		}
	}
	bt_ctf_field_set_error(-EINVAL);

end:
	return ret;
}

char *bt_ctf_get_string(const struct bt_definition *field)
{
	char *ret = NULL;

	if (field && bt_ctf_field_type(bt_ctf_get_decl_from_def(field)) == CTF_TYPE_STRING)
		ret = bt_get_string(field);
	else
		bt_ctf_field_set_error(-EINVAL);

	return ret;
}

int bt_ctf_get_event_decl_list(int handle_id, struct bt_context *ctx,
		struct bt_ctf_event_decl * const **list,
		unsigned int *count)
{
	struct bt_trace_handle *handle;
	struct bt_trace_descriptor *td;
	struct ctf_trace *tin;

	if (!ctx || !list || !count)
		goto error;

	handle = g_hash_table_lookup(ctx->trace_handles,
			(gpointer) (unsigned long) handle_id);
	if (!handle)
		goto error;

	td = handle->td;
	tin = container_of(td, struct ctf_trace, parent);

	*list = (struct bt_ctf_event_decl * const*) tin->event_declarations->pdata;
	*count = tin->event_declarations->len;
	return 0;

error:
	return -1;
}

const char *bt_ctf_get_decl_event_name(const struct bt_ctf_event_decl *event)
{
	if (!event)
		return NULL;

	return g_quark_to_string(event->parent.name);
}

int bt_ctf_get_decl_fields(struct bt_ctf_event_decl *event_decl,
		enum bt_ctf_scope scope,
		struct bt_ctf_field_decl const * const **list,
		unsigned int *count)
{
	int i;
	GArray *fields = NULL;
	gpointer *ret_list = NULL;
	GPtrArray *fields_array = NULL;
	int ret = 0;
	*count = 0;

	if (!event_decl || !list || !count)
		return -EINVAL;

	switch (scope) {
	case BT_EVENT_CONTEXT:
		if (event_decl->context_decl) {
			ret_list = event_decl->context_decl->pdata;
			*count = event_decl->context_decl->len;
			goto end;
		}
		event_decl->context_decl = g_ptr_array_new();
		if (!event_decl->parent.context_decl) {
			ret = -1;
			goto end;
		}
		fields = event_decl->parent.context_decl->fields;
		fields_array = event_decl->context_decl;
		break;
	case BT_EVENT_FIELDS:
		if (event_decl->fields_decl) {
			ret_list = event_decl->fields_decl->pdata;
			*count = event_decl->fields_decl->len;
			goto end;
		}
		event_decl->fields_decl = g_ptr_array_new();
		if (!event_decl->parent.fields_decl) {
			ret = -1;
			goto end;
		}
		fields = event_decl->parent.fields_decl->fields;
		fields_array = event_decl->fields_decl;
		break;
	case BT_STREAM_PACKET_CONTEXT:
		if (event_decl->packet_context_decl) {
			ret_list = event_decl->packet_context_decl->pdata;
			*count = event_decl->packet_context_decl->len;
			goto end;
		}
		event_decl->packet_context_decl = g_ptr_array_new();
		if (!event_decl->parent.stream->packet_context_decl) {
			ret = -1;
			goto end;
		}
		fields = event_decl->parent.stream->packet_context_decl->fields;
		fields_array = event_decl->packet_context_decl;
		break;
	case BT_STREAM_EVENT_CONTEXT:
		if (event_decl->event_context_decl) {
			ret_list = event_decl->event_context_decl->pdata;
			*count = event_decl->event_context_decl->len;
			goto end;
		}
		event_decl->event_context_decl = g_ptr_array_new();
		if (!event_decl->parent.stream->event_context_decl) {
			ret = -1;
			goto end;
		}
		fields = event_decl->parent.stream->event_context_decl->fields;
		fields_array = event_decl->event_context_decl;
		break;
	case BT_STREAM_EVENT_HEADER:
		if (event_decl->event_header_decl) {
			ret_list = event_decl->event_header_decl->pdata;
			*count = event_decl->event_header_decl->len;
			goto end;
		}
		event_decl->event_header_decl = g_ptr_array_new();
		if (!event_decl->parent.stream->event_header_decl) {
			ret = -1;
			goto end;
		}
		fields = event_decl->parent.stream->event_header_decl->fields;
		fields_array = event_decl->event_header_decl;
		break;
	case BT_TRACE_PACKET_HEADER:
		if (event_decl->packet_header_decl) {
			ret_list = event_decl->packet_header_decl->pdata;
			*count = event_decl->packet_header_decl->len;
			goto end;
		}
		event_decl->packet_header_decl = g_ptr_array_new();
		if (!event_decl->parent.stream->trace->packet_header_decl) {
			ret = -1;
			goto end;
		}
		fields = event_decl->parent.stream->trace->packet_header_decl->fields;
		fields_array = event_decl->packet_header_decl;
		break;
	}

	for (i = 0; i < fields->len; i++) {
		g_ptr_array_add(fields_array,
				&g_array_index(fields,
					struct declaration_field, i));
	}
	ret_list = fields_array->pdata;
	*count = fields->len;

end:
	*list = (struct bt_ctf_field_decl const* const*) ret_list;

	return ret;
}

const char *bt_ctf_get_decl_field_name(const struct bt_ctf_field_decl *field)
{
	if (!field)
		return NULL;

	return rem_(g_quark_to_string(((struct declaration_field *) field)->name));
}

const struct bt_declaration *bt_ctf_get_decl_from_def(const struct bt_definition *def)
{
	if (def)
		return def->declaration;

	return NULL;
}

const struct bt_declaration *bt_ctf_get_decl_from_field_decl(
		const struct bt_ctf_field_decl *field)
{
	if (field)
		return ((struct declaration_field *) field)->declaration;

	return NULL;
}
