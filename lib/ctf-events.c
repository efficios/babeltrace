/*
 * ctf_events.c
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
 */

#include <babeltrace/babeltrace.h>
#include <babeltrace/format.h>
#include <babeltrace/ctf/events.h>
#include <babeltrace/ctf-ir/metadata.h>
#include <glib.h>

/*
 * thread local storage to store the last error that occured
 * while reading a field, this variable must be accessed by
 * bt_ctf_field_error only
 */
__thread int bt_ctf_last_field_error = 0;

struct definition *bt_ctf_get_top_level_scope(struct bt_ctf_event *event,
		enum bt_ctf_scope scope)
{
	struct definition *tmp = NULL;

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
		if (!event->event)
			goto error;
		if (event->event->event_context)
			tmp = &event->event->event_context->p;
		break;
	case BT_EVENT_FIELDS:
		if (!event->event)
			goto error;
		if (event->event->event_fields)
			tmp = &event->event->event_fields->p;
		break;
	}
	return tmp;

error:
	return NULL;
}

struct definition *bt_ctf_get_field(struct bt_ctf_event *event,
		struct definition *scope,
		const char *field)
{
	struct definition *def;

	if (scope) {
		def = lookup_definition(scope, field);
		if (bt_ctf_field_type(def) == CTF_TYPE_VARIANT) {
			struct definition_variant *variant_definition;
			variant_definition = container_of(def,
					struct definition_variant, p);
			return variant_definition->current_field;
		}
		return def;
	}
	return NULL;
}

struct definition *bt_ctf_get_index(struct bt_ctf_event *event,
		struct definition *field,
		unsigned int index)
{
	struct definition *ret = NULL;

	if (bt_ctf_field_type(field) == CTF_TYPE_ARRAY) {
		struct definition_array *array_definition;
		array_definition = container_of(field,
				struct definition_array, p);
		ret = array_index(array_definition, index);
	} else if (bt_ctf_field_type(field) == CTF_TYPE_SEQUENCE) {
		struct definition_sequence *sequence_definition;
		sequence_definition = container_of(field,
				struct definition_sequence, p);
		ret = sequence_index(sequence_definition, index);
	}
	return ret;
}

const char *bt_ctf_event_name(struct bt_ctf_event *event)
{
	struct ctf_event *event_class;
	struct ctf_stream_class *stream_class;

	if (!event)
		return NULL;
	stream_class = event->stream->stream_class;
	event_class = g_ptr_array_index(stream_class->events_by_id,
			event->stream->event_id);
	return g_quark_to_string(event_class->name);
}

const char *bt_ctf_field_name(const struct definition *def)
{
	if (def)
		return g_quark_to_string(def->name);
	return NULL;
}

enum ctf_type_id bt_ctf_field_type(struct definition *def)
{
	if (def)
		return def->declaration->id;
	return CTF_TYPE_UNKNOWN;
}

int bt_ctf_get_field_list(struct bt_ctf_event *event,
		struct definition *scope,
		struct definition const * const **list,
		unsigned int *count)
{
	switch (bt_ctf_field_type(scope)) {
	case CTF_TYPE_INTEGER:
	case CTF_TYPE_FLOAT:
	case CTF_TYPE_STRING:
	case CTF_TYPE_ENUM:
		goto error;
	case CTF_TYPE_STRUCT:
	{
		struct definition_struct *def_struct;

		def_struct = container_of(scope, struct definition_struct, p);
		if (!def_struct)
			goto error;
		if (def_struct->fields->pdata) {
			*list = (struct definition const* const*) def_struct->fields->pdata;
			*count = def_struct->fields->len;
			goto end;
		} else {
			goto error;
		}
	}
	case CTF_TYPE_UNTAGGED_VARIANT:
		goto error;
	case CTF_TYPE_VARIANT:
	{
		struct definition_variant *def_variant;

		def_variant = container_of(scope, struct definition_variant, p);
		if (!def_variant)
			goto error;
		if (def_variant->fields->pdata) {
			*list = (struct definition const* const*) def_variant->fields->pdata;
			*count = def_variant->fields->len;
			goto end;
		} else {
			goto error;
		}
	}
	case CTF_TYPE_ARRAY:
	{
		struct definition_array *def_array;

		def_array = container_of(scope, struct definition_array, p);
		if (!def_array)
			goto error;
		if (def_array->elems->pdata) {
			*list = (struct definition const* const*) def_array->elems->pdata;
			*count = def_array->elems->len;
			goto end;
		} else {
			goto error;
		}
	}
	case CTF_TYPE_SEQUENCE:
	{
		struct definition_sequence *def_sequence;

		def_sequence = container_of(scope, struct definition_sequence, p);
		if (!def_sequence)
			goto error;
		if (def_sequence->elems->pdata) {
			*list = (struct definition const* const*) def_sequence->elems->pdata;
			*count = def_sequence->elems->len;
			goto end;
		} else {
			goto error;
		}
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

uint64_t bt_ctf_get_timestamp(struct bt_ctf_event *event)
{
	if (event && event->stream->has_timestamp)
		return event->stream->timestamp;
	else
		return 0;
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

uint64_t bt_ctf_get_uint64(struct definition *field)
{
	unsigned int ret = 0;

	if (field && bt_ctf_field_type(field) == CTF_TYPE_INTEGER)
		ret = get_unsigned_int(field);
	else
		bt_ctf_field_set_error(-EINVAL);

	return ret;
}

int64_t bt_ctf_get_signed_int64(struct definition *field)
{
	int ret = 0;

	if (field && bt_ctf_field_type(field) == CTF_TYPE_INTEGER)
		ret = get_signed_int(field);
	else
		bt_ctf_field_set_error(-EINVAL);

	return ret;

}

char *bt_ctf_get_char_array(struct definition *field)
{
	char *ret = NULL;

	if (field && bt_ctf_field_type(field) == CTF_TYPE_ARRAY)
		ret = get_char_array(field)->str;
	else
		bt_ctf_field_set_error(-EINVAL);

	return ret;
}

char *bt_ctf_get_string(struct definition *field)
{
	char *ret = NULL;

	if (field && bt_ctf_field_type(field) == CTF_TYPE_STRING)
		ret = get_string(field);
	else
		bt_ctf_field_set_error(-EINVAL);

	return ret;
}
