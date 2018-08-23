#ifndef _CTF_META_H
#define _CTF_META_H

/*
 * Copyright 2018 - Philippe Proulx <pproulx@efficios.com>
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
#include <babeltrace/common-internal.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>
#include <stdint.h>
#include <string.h>

enum ctf_field_type_id {
	CTF_FIELD_TYPE_ID_INT,
	CTF_FIELD_TYPE_ID_ENUM,
	CTF_FIELD_TYPE_ID_FLOAT,
	CTF_FIELD_TYPE_ID_STRING,
	CTF_FIELD_TYPE_ID_STRUCT,
	CTF_FIELD_TYPE_ID_ARRAY,
	CTF_FIELD_TYPE_ID_SEQUENCE,
	CTF_FIELD_TYPE_ID_VARIANT,
};

enum ctf_field_type_meaning {
	CTF_FIELD_TYPE_MEANING_NONE,
	CTF_FIELD_TYPE_MEANING_PACKET_BEGINNING_TIME,
	CTF_FIELD_TYPE_MEANING_PACKET_END_TIME,
	CTF_FIELD_TYPE_MEANING_EVENT_CLASS_ID,
	CTF_FIELD_TYPE_MEANING_STREAM_CLASS_ID,
	CTF_FIELD_TYPE_MEANING_DATA_STREAM_ID,
	CTF_FIELD_TYPE_MEANING_MAGIC,
	CTF_FIELD_TYPE_MEANING_PACKET_COUNTER_SNAPSHOT,
	CTF_FIELD_TYPE_MEANING_DISC_EV_REC_COUNTER_SNAPSHOT,
	CTF_FIELD_TYPE_MEANING_EXP_PACKET_TOTAL_SIZE,
	CTF_FIELD_TYPE_MEANING_EXP_PACKET_CONTENT_SIZE,
	CTF_FIELD_TYPE_MEANING_UUID,
};

enum ctf_byte_order {
	CTF_BYTE_ORDER_DEFAULT,
	CTF_BYTE_ORDER_LITTLE,
	CTF_BYTE_ORDER_BIG,
};

enum ctf_encoding {
	CTF_ENCODING_NONE,
	CTF_ENCODING_UTF8,
};

struct ctf_field_type {
	enum ctf_field_type_id id;
	unsigned int alignment;
	bool is_compound;
	bool in_ir;

	/* Weak, set during translation. NULL if `in_ir` is false below. */
	struct bt_field_type *ir_ft;
};

struct ctf_field_type_bit_array {
	struct ctf_field_type base;
	enum ctf_byte_order byte_order;
	unsigned int size;
};

struct ctf_field_type_int {
	struct ctf_field_type_bit_array base;
	enum ctf_field_type_meaning meaning;
	bool is_signed;
	enum bt_field_type_integer_preferred_display_base disp_base;
	enum ctf_encoding encoding;
	int64_t storing_index;

	/* Owned by this */
	struct bt_clock_class *mapped_clock_class;
};

struct ctf_range {
	union {
		uint64_t u;
		int64_t i;
	} lower;

	union {
		uint64_t u;
		int64_t i;
	} upper;
};

struct ctf_field_type_enum_mapping {
	GString *label;
	struct ctf_range range;
};

struct ctf_field_type_enum {
	struct ctf_field_type_int base;

	/* Array of `struct ctf_field_type_enum_mapping` */
	GArray *mappings;
};

struct ctf_field_type_float {
	struct ctf_field_type_bit_array base;
};

struct ctf_field_type_string {
	struct ctf_field_type base;
	enum ctf_encoding encoding;
};

struct ctf_named_field_type {
	GString *name;

	/* Owned by this */
	struct ctf_field_type *ft;
};

struct ctf_field_type_struct {
	struct ctf_field_type base;

	/* Array of `struct ctf_named_field_type` */
	GArray *members;
};

struct ctf_field_path {
	enum bt_scope root;

	/* Array of `int64_t` */
	GArray *path;
};

struct ctf_field_type_variant_range {
	struct ctf_range range;
	uint64_t option_index;
};

struct ctf_field_type_variant {
	struct ctf_field_type base;
	GString *tag_ref;
	struct ctf_field_path tag_path;
	uint64_t stored_tag_index;

	/* Array of `struct ctf_named_field_type` */
	GArray *options;

	/* Array of `struct ctf_field_type_variant_range` */
	GArray *ranges;

	/* Weak */
	struct ctf_field_type_enum *tag_ft;
};

struct ctf_field_type_array_base {
	struct ctf_field_type base;
	struct ctf_field_type *elem_ft;
	bool is_text;
};

struct ctf_field_type_array {
	struct ctf_field_type_array_base base;
	enum ctf_field_type_meaning meaning;
	uint64_t length;
};

struct ctf_field_type_sequence {
	struct ctf_field_type_array_base base;
	GString *length_ref;
	struct ctf_field_path length_path;
	uint64_t stored_length_index;

	/* Weak */
	struct ctf_field_type_int *length_ft;
};

struct ctf_event_class {
	GString *name;
	uint64_t id;
	GString *emf_uri;
	enum bt_event_class_log_level log_level;
	bool is_translated;

	/* Owned by this */
	struct ctf_field_type *spec_context_ft;

	/* Owned by this */
	struct ctf_field_type *payload_ft;

	/* Weak, set during translation */
	struct bt_event_class *ir_ec;
};

struct ctf_stream_class {
	uint64_t id;
	bool is_translated;

	/* Owned by this */
	struct ctf_field_type *packet_context_ft;

	/* Owned by this */
	struct ctf_field_type *event_header_ft;

	/* Owned by this */
	struct ctf_field_type *event_common_context_ft;

	/* Array of `struct ctf_event_class *`, owned by this */
	GPtrArray *event_classes;

	/*
	 * Hash table mapping event class IDs to `struct ctf_event_class *`,
	 * weak.
	 */
	GHashTable *event_classes_by_id;

	/* Owned by this */
	struct bt_clock_class *default_clock_class;

	/* Weak, set during translation */
	struct bt_stream_class *ir_sc;
};

enum ctf_trace_class_env_entry_type {
	CTF_TRACE_CLASS_ENV_ENTRY_TYPE_INT,
	CTF_TRACE_CLASS_ENV_ENTRY_TYPE_STR,
};

struct ctf_trace_class_env_entry {
	enum ctf_trace_class_env_entry_type type;
	GString *name;

	struct {
		int64_t i;
		GString *str;
	} value;
};

struct ctf_trace_class {
	GString *name;
	unsigned int major;
	unsigned int minor;
	uint8_t uuid[16];
	bool is_uuid_set;
	enum ctf_byte_order default_byte_order;

	/* Owned by this */
	struct ctf_field_type *packet_header_ft;

	uint64_t stored_value_count;

	/* Array of `struct bt_clock_class *` (owned by this) */
	GPtrArray *clock_classes;

	/* Array of `struct ctf_stream_class *` */
	GPtrArray *stream_classes;

	/* Array of `struct ctf_trace_class_env_entry` */
	GArray *env_entries;

	bool is_translated;

	/* Weak, set during translation */
	struct bt_trace *ir_tc;
};

static inline
void ctf_field_type_destroy(struct ctf_field_type *ft);

static inline
void _ctf_field_type_init(struct ctf_field_type *ft, enum ctf_field_type_id id,
		unsigned int alignment)
{
	BT_ASSERT(ft);
	ft->id = id;
	ft->alignment = alignment;
	ft->in_ir = false;
}

static inline
void _ctf_field_type_bit_array_init(struct ctf_field_type_bit_array *ft,
		enum ctf_field_type_id id)
{
	_ctf_field_type_init((void *) ft, id, 1);
}

static inline
void _ctf_field_type_int_init(struct ctf_field_type_int *ft,
		enum ctf_field_type_id id)
{
	_ctf_field_type_bit_array_init((void *) ft, id);
	ft->meaning = CTF_FIELD_TYPE_MEANING_NONE;
	ft->storing_index = -1;
}

static inline
void ctf_field_path_init(struct ctf_field_path *field_path)
{
	BT_ASSERT(field_path);
	field_path->path = g_array_new(FALSE, TRUE, sizeof(int64_t));
	BT_ASSERT(field_path->path);
}

static inline
void ctf_field_path_fini(struct ctf_field_path *field_path)
{
	BT_ASSERT(field_path);

	if (field_path->path) {
		g_array_free(field_path->path, TRUE);
	}
}

static inline
void _ctf_named_field_type_init(struct ctf_named_field_type *named_ft)
{
	BT_ASSERT(named_ft);
	named_ft->name = g_string_new(NULL);
	BT_ASSERT(named_ft->name);
}

static inline
void _ctf_named_field_type_fini(struct ctf_named_field_type *named_ft)
{
	BT_ASSERT(named_ft);

	if (named_ft->name) {
		g_string_free(named_ft->name, TRUE);
	}

	ctf_field_type_destroy(named_ft->ft);
}

static inline
void _ctf_field_type_enum_mapping_init(
		struct ctf_field_type_enum_mapping *mapping)
{
	BT_ASSERT(mapping);
	mapping->label = g_string_new(NULL);
	BT_ASSERT(mapping->label);
}

static inline
void _ctf_field_type_enum_mapping_fini(
		struct ctf_field_type_enum_mapping *mapping)
{
	BT_ASSERT(mapping);

	if (mapping->label) {
		g_string_free(mapping->label, TRUE);
	}
}

static inline
struct ctf_field_type_int *ctf_field_type_int_create(void)
{
	struct ctf_field_type_int *ft = g_new0(struct ctf_field_type_int, 1);

	BT_ASSERT(ft);
	_ctf_field_type_int_init(ft, CTF_FIELD_TYPE_ID_INT);
	return ft;
}

static inline
struct ctf_field_type_float *ctf_field_type_float_create(void)
{
	struct ctf_field_type_float *ft =
		g_new0(struct ctf_field_type_float, 1);

	BT_ASSERT(ft);
	_ctf_field_type_bit_array_init((void *) ft, CTF_FIELD_TYPE_ID_FLOAT);
	return ft;
}

static inline
struct ctf_field_type_string *ctf_field_type_string_create(void)
{
	struct ctf_field_type_string *ft =
		g_new0(struct ctf_field_type_string, 1);

	BT_ASSERT(ft);
	_ctf_field_type_init((void *) ft, CTF_FIELD_TYPE_ID_STRING, 8);
	return ft;
}

static inline
struct ctf_field_type_enum *ctf_field_type_enum_create(void)
{
	struct ctf_field_type_enum *ft = g_new0(struct ctf_field_type_enum, 1);

	BT_ASSERT(ft);
	_ctf_field_type_int_init((void *) ft, CTF_FIELD_TYPE_ID_ENUM);
	ft->mappings = g_array_new(FALSE, TRUE,
		sizeof(struct ctf_field_type_enum_mapping));
	BT_ASSERT(ft->mappings);
	return ft;
}

static inline
struct ctf_field_type_struct *ctf_field_type_struct_create(void)
{
	struct ctf_field_type_struct *ft =
		g_new0(struct ctf_field_type_struct, 1);

	BT_ASSERT(ft);
	_ctf_field_type_init((void *) ft, CTF_FIELD_TYPE_ID_STRUCT, 1);
	ft->members = g_array_new(FALSE, TRUE,
		sizeof(struct ctf_named_field_type));
	BT_ASSERT(ft->members);
	ft->base.is_compound = true;
	return ft;
}

static inline
struct ctf_field_type_variant *ctf_field_type_variant_create(void)
{
	struct ctf_field_type_variant *ft =
		g_new0(struct ctf_field_type_variant, 1);

	BT_ASSERT(ft);
	_ctf_field_type_init((void *) ft, CTF_FIELD_TYPE_ID_VARIANT, 1);
	ft->options = g_array_new(FALSE, TRUE,
		sizeof(struct ctf_named_field_type));
	BT_ASSERT(ft->options);
	ft->ranges = g_array_new(FALSE, TRUE,
		sizeof(struct ctf_field_type_variant_range));
	BT_ASSERT(ft->ranges);
	ft->tag_ref = g_string_new(NULL);
	BT_ASSERT(ft->tag_ref);
	ctf_field_path_init(&ft->tag_path);
	ft->base.is_compound = true;
	return ft;
}

static inline
struct ctf_field_type_array *ctf_field_type_array_create(void)
{
	struct ctf_field_type_array *ft =
		g_new0(struct ctf_field_type_array, 1);

	BT_ASSERT(ft);
	_ctf_field_type_init((void *) ft, CTF_FIELD_TYPE_ID_ARRAY, 1);
	ft->base.base.is_compound = true;
	return ft;
}

static inline
struct ctf_field_type_sequence *ctf_field_type_sequence_create(void)
{
	struct ctf_field_type_sequence *ft =
		g_new0(struct ctf_field_type_sequence, 1);

	BT_ASSERT(ft);
	_ctf_field_type_init((void *) ft, CTF_FIELD_TYPE_ID_SEQUENCE, 1);
	ft->length_ref = g_string_new(NULL);
	BT_ASSERT(ft->length_ref);
	ctf_field_path_init(&ft->length_path);
	ft->base.base.is_compound = true;
	return ft;
}

static inline
void _ctf_field_type_int_destroy(struct ctf_field_type_int *ft)
{
	BT_ASSERT(ft);
	bt_put(ft->mapped_clock_class);
	g_free(ft);
}

static inline
void _ctf_field_type_enum_destroy(struct ctf_field_type_enum *ft)
{
	BT_ASSERT(ft);
	bt_put(ft->base.mapped_clock_class);

	if (ft->mappings) {
		uint64_t i;

		for (i = 0; i < ft->mappings->len; i++) {
			struct ctf_field_type_enum_mapping *mapping =
				&g_array_index(ft->mappings,
					struct ctf_field_type_enum_mapping, i);

			_ctf_field_type_enum_mapping_fini(mapping);
		}

		g_array_free(ft->mappings, TRUE);
	}

	g_free(ft);
}

static inline
void _ctf_field_type_float_destroy(struct ctf_field_type_float *ft)
{
	BT_ASSERT(ft);
	g_free(ft);
}

static inline
void _ctf_field_type_string_destroy(struct ctf_field_type_string *ft)
{
	BT_ASSERT(ft);
	g_free(ft);
}

static inline
void _ctf_field_type_struct_destroy(struct ctf_field_type_struct *ft)
{
	BT_ASSERT(ft);

	if (ft->members) {
		uint64_t i;

		for (i = 0; i < ft->members->len; i++) {
			struct ctf_named_field_type *named_ft =
				&g_array_index(ft->members,
					struct ctf_named_field_type, i);

			_ctf_named_field_type_fini(named_ft);
		}

		g_array_free(ft->members, TRUE);
	}

	g_free(ft);
}

static inline
void _ctf_field_type_array_base_fini(struct ctf_field_type_array_base *ft)
{
	BT_ASSERT(ft);
	ctf_field_type_destroy(ft->elem_ft);
}

static inline
void _ctf_field_type_array_destroy(struct ctf_field_type_array *ft)
{
	BT_ASSERT(ft);
	_ctf_field_type_array_base_fini((void *) ft);
	g_free(ft);
}

static inline
void _ctf_field_type_sequence_destroy(struct ctf_field_type_sequence *ft)
{
	BT_ASSERT(ft);
	_ctf_field_type_array_base_fini((void *) ft);

	if (ft->length_ref) {
		g_string_free(ft->length_ref, TRUE);
	}

	ctf_field_path_fini(&ft->length_path);
	g_free(ft);
}

static inline
void _ctf_field_type_variant_destroy(struct ctf_field_type_variant *ft)
{
	BT_ASSERT(ft);

	if (ft->options) {
		uint64_t i;

		for (i = 0; i < ft->options->len; i++) {
			struct ctf_named_field_type *named_ft =
				&g_array_index(ft->options,
					struct ctf_named_field_type, i);

			_ctf_named_field_type_fini(named_ft);
		}

		g_array_free(ft->options, TRUE);
	}

	if (ft->ranges) {
		g_array_free(ft->ranges, TRUE);
	}

	if (ft->tag_ref) {
		g_string_free(ft->tag_ref, TRUE);
	}

	ctf_field_path_fini(&ft->tag_path);
	g_free(ft);
}

static inline
void ctf_field_type_destroy(struct ctf_field_type *ft)
{
	if (!ft) {
		return;
	}

	switch (ft->id) {
	case CTF_FIELD_TYPE_ID_INT:
		_ctf_field_type_int_destroy((void *) ft);
		break;
	case CTF_FIELD_TYPE_ID_ENUM:
		_ctf_field_type_enum_destroy((void *) ft);
		break;
	case CTF_FIELD_TYPE_ID_FLOAT:
		_ctf_field_type_float_destroy((void *) ft);
		break;
	case CTF_FIELD_TYPE_ID_STRING:
		_ctf_field_type_string_destroy((void *) ft);
		break;
	case CTF_FIELD_TYPE_ID_STRUCT:
		_ctf_field_type_struct_destroy((void *) ft);
		break;
	case CTF_FIELD_TYPE_ID_ARRAY:
		_ctf_field_type_array_destroy((void *) ft);
		break;
	case CTF_FIELD_TYPE_ID_SEQUENCE:
		_ctf_field_type_sequence_destroy((void *) ft);
		break;
	case CTF_FIELD_TYPE_ID_VARIANT:
		_ctf_field_type_variant_destroy((void *) ft);
		break;
	default:
		abort();
	}
}

static inline
void ctf_field_type_enum_append_mapping(struct ctf_field_type_enum *ft,
		const char *label, uint64_t u_lower, uint64_t u_upper)
{
	struct ctf_field_type_enum_mapping *mapping;

	BT_ASSERT(ft);
	BT_ASSERT(label);
	g_array_set_size(ft->mappings, ft->mappings->len + 1);

	mapping = &g_array_index(ft->mappings,
		struct ctf_field_type_enum_mapping, ft->mappings->len - 1);
	_ctf_field_type_enum_mapping_init(mapping);
	g_string_assign(mapping->label, label);
	mapping->range.lower.u = u_lower;
	mapping->range.upper.u = u_upper;
}

static inline
struct ctf_field_type_enum_mapping *ctf_field_type_enum_borrow_mapping_by_index(
		struct ctf_field_type_enum *ft, uint64_t index)
{
	BT_ASSERT(ft);
	BT_ASSERT(index < ft->mappings->len);
	return &g_array_index(ft->mappings, struct ctf_field_type_enum_mapping,
		index);
}

static inline
struct ctf_named_field_type *ctf_field_type_struct_borrow_member_by_index(
		struct ctf_field_type_struct *ft, uint64_t index)
{
	BT_ASSERT(ft);
	BT_ASSERT(index < ft->members->len);
	return &g_array_index(ft->members, struct ctf_named_field_type,
		index);
}

static inline
struct ctf_named_field_type *ctf_field_type_struct_borrow_member_by_name(
		struct ctf_field_type_struct *ft, const char *name)
{
	uint64_t i;
	struct ctf_named_field_type *ret_named_ft = NULL;

	BT_ASSERT(ft);
	BT_ASSERT(name);

	for (i = 0; i < ft->members->len; i++) {
		struct ctf_named_field_type *named_ft =
			ctf_field_type_struct_borrow_member_by_index(ft, i);

		if (strcmp(name, named_ft->name->str) == 0) {
			ret_named_ft = named_ft;
			goto end;
		}
	}

end:
	return ret_named_ft;
}

static inline
struct ctf_field_type *ctf_field_type_struct_borrow_member_field_type_by_name(
		struct ctf_field_type_struct *struct_ft, const char *name)
{
	struct ctf_named_field_type *named_ft = NULL;
	struct ctf_field_type *ft = NULL;

	if (!struct_ft) {
		goto end;
	}

	named_ft = ctf_field_type_struct_borrow_member_by_name(struct_ft, name);
	if (!named_ft) {
		goto end;
	}

	ft = named_ft->ft;

end:
	return ft;
}

static inline
struct ctf_field_type_int *
ctf_field_type_struct_borrow_member_int_field_type_by_name(
		struct ctf_field_type_struct *struct_ft, const char *name)
{
	struct ctf_field_type_int *int_ft = NULL;

	int_ft = (void *)
		ctf_field_type_struct_borrow_member_field_type_by_name(
			struct_ft, name);
	if (!int_ft) {
		goto end;
	}

	if (int_ft->base.base.id != CTF_FIELD_TYPE_ID_INT &&
			int_ft->base.base.id != CTF_FIELD_TYPE_ID_ENUM) {
		int_ft = NULL;
		goto end;
	}

end:
	return int_ft;
}


static inline
void ctf_field_type_struct_append_member(struct ctf_field_type_struct *ft,
		const char *name, struct ctf_field_type *member_ft)
{
	struct ctf_named_field_type *named_ft;

	BT_ASSERT(ft);
	BT_ASSERT(name);
	g_array_set_size(ft->members, ft->members->len + 1);

	named_ft = &g_array_index(ft->members, struct ctf_named_field_type,
		ft->members->len - 1);
	_ctf_named_field_type_init(named_ft);
	g_string_assign(named_ft->name, name);
	named_ft->ft = member_ft;

	if (member_ft->alignment > ft->base.alignment) {
		ft->base.alignment = member_ft->alignment;
	}
}

static inline
struct ctf_named_field_type *ctf_field_type_variant_borrow_option_by_index(
		struct ctf_field_type_variant *ft, uint64_t index)
{
	BT_ASSERT(ft);
	BT_ASSERT(index < ft->options->len);
	return &g_array_index(ft->options, struct ctf_named_field_type,
		index);
}

static inline
struct ctf_named_field_type *ctf_field_type_variant_borrow_option_by_name(
		struct ctf_field_type_variant *ft, const char *name)
{
	uint64_t i;
	struct ctf_named_field_type *ret_named_ft = NULL;

	BT_ASSERT(ft);
	BT_ASSERT(name);

	for (i = 0; i < ft->options->len; i++) {
		struct ctf_named_field_type *named_ft =
			ctf_field_type_variant_borrow_option_by_index(ft, i);

		if (strcmp(name, named_ft->name->str) == 0) {
			ret_named_ft = named_ft;
			goto end;
		}
	}

end:
	return ret_named_ft;
}

static inline
struct ctf_field_type_variant_range *
ctf_field_type_variant_borrow_range_by_index(
		struct ctf_field_type_variant *ft, uint64_t index)
{
	BT_ASSERT(ft);
	BT_ASSERT(index < ft->ranges->len);
	return &g_array_index(ft->ranges, struct ctf_field_type_variant_range,
		index);
}

static inline
void ctf_field_type_variant_append_option(struct ctf_field_type_variant *ft,
		const char *name, struct ctf_field_type *option_ft)
{
	struct ctf_named_field_type *named_ft;

	BT_ASSERT(ft);
	BT_ASSERT(name);
	g_array_set_size(ft->options, ft->options->len + 1);

	named_ft = &g_array_index(ft->options, struct ctf_named_field_type,
		ft->options->len - 1);
	_ctf_named_field_type_init(named_ft);
	g_string_assign(named_ft->name, name);
	named_ft->ft = option_ft;
}

static inline
void ctf_field_type_variant_set_tag_field_type(
		struct ctf_field_type_variant *ft,
		struct ctf_field_type_enum *tag_ft)
{
	uint64_t option_i;

	BT_ASSERT(ft);
	BT_ASSERT(tag_ft);
	ft->tag_ft = tag_ft;

	for (option_i = 0; option_i < ft->options->len; option_i++) {
		uint64_t mapping_i;
		struct ctf_named_field_type *named_ft =
			ctf_field_type_variant_borrow_option_by_index(
				ft, option_i);

		for (mapping_i = 0; mapping_i < tag_ft->mappings->len;
				mapping_i++) {
			struct ctf_field_type_enum_mapping *mapping =
				ctf_field_type_enum_borrow_mapping_by_index(
					tag_ft, mapping_i);

			if (strcmp(named_ft->name->str,
					mapping->label->str) == 0) {
				struct ctf_field_type_variant_range range;

				range.range = mapping->range;
				range.option_index = option_i;
				g_array_append_val(ft->ranges, range);
			}
		}
	}
}

static inline
struct ctf_field_type *ctf_field_type_compound_borrow_field_type_by_index(
		struct ctf_field_type *comp_ft, uint64_t index)
{
	struct ctf_field_type *ft = NULL;

	switch (comp_ft->id) {
	case CTF_FIELD_TYPE_ID_STRUCT:
	{
		struct ctf_named_field_type *named_ft =
			ctf_field_type_struct_borrow_member_by_index(
				(void *) comp_ft, index);

		BT_ASSERT(named_ft);
		ft = named_ft->ft;
		break;
	}
	case CTF_FIELD_TYPE_ID_VARIANT:
	{
		struct ctf_named_field_type *named_ft =
			ctf_field_type_variant_borrow_option_by_index(
				(void *) comp_ft, index);

		BT_ASSERT(named_ft);
		ft = named_ft->ft;
		break;
	}
	case CTF_FIELD_TYPE_ID_ARRAY:
	case CTF_FIELD_TYPE_ID_SEQUENCE:
	{
		struct ctf_field_type_array_base *array_ft = (void *) comp_ft;

		ft = array_ft->elem_ft;
		break;
	}
	default:
		break;
	}

	return ft;
}

static inline
uint64_t ctf_field_type_compound_get_field_type_count(struct ctf_field_type *ft)
{
	uint64_t field_count;

	switch (ft->id) {
	case CTF_FIELD_TYPE_ID_STRUCT:
	{
		struct ctf_field_type_struct *struct_ft = (void *) ft;

		field_count = struct_ft->members->len;
		break;
	}
	case CTF_FIELD_TYPE_ID_VARIANT:
	{
		struct ctf_field_type_variant *var_ft = (void *) ft;

		field_count = var_ft->options->len;
		break;
	}
	case CTF_FIELD_TYPE_ID_ARRAY:
	case CTF_FIELD_TYPE_ID_SEQUENCE:
		/*
		 * Array and sequence types always contain a single
		 * member (the element type).
		 */
		field_count = 1;
		break;
	default:
		abort();
	}

	return field_count;
}

static inline
int64_t ctf_field_type_compound_get_field_type_index_from_name(
		struct ctf_field_type *ft, const char *name)
{
	int64_t ret_index = -1;
	uint64_t i;

	switch (ft->id) {
	case CTF_FIELD_TYPE_ID_STRUCT:
	{
		struct ctf_field_type_struct *struct_ft = (void *) ft;

		for (i = 0; i < struct_ft->members->len; i++) {
			struct ctf_named_field_type *named_ft =
				ctf_field_type_struct_borrow_member_by_index(
					struct_ft, i);

			if (strcmp(name, named_ft->name->str) == 0) {
				ret_index = (int64_t) i;
				goto end;
			}
		}

		break;
	}
	case CTF_FIELD_TYPE_ID_VARIANT:
	{
		struct ctf_field_type_variant *var_ft = (void *) ft;

		for (i = 0; i < var_ft->options->len; i++) {
			struct ctf_named_field_type *named_ft =
				ctf_field_type_variant_borrow_option_by_index(
					var_ft, i);

			if (strcmp(name, named_ft->name->str) == 0) {
				ret_index = (int64_t) i;
				goto end;
			}
		}

		break;
	}
	default:
		break;
	}

end:
	return ret_index;
}

static inline
void ctf_field_path_append_index(struct ctf_field_path *fp, int64_t index)
{
	BT_ASSERT(fp);
	g_array_append_val(fp->path, index);
}

static inline
int64_t ctf_field_path_borrow_index_by_index(struct ctf_field_path *fp,
		uint64_t index)
{
	BT_ASSERT(fp);
	BT_ASSERT(index < fp->path->len);
	return g_array_index(fp->path, int64_t, index);
}

static inline
void ctf_field_path_clear(struct ctf_field_path *fp)
{
	BT_ASSERT(fp);
	g_array_set_size(fp->path, 0);
}

static inline
GString *ctf_field_path_string(struct ctf_field_path *path)
{
	GString *str = g_string_new(NULL);
	uint64_t i;

	BT_ASSERT(path);

	if (!str) {
		goto end;
	}

	g_string_append_printf(str, "[%s", bt_common_scope_string(
		path->root));

	for (i = 0; i < path->path->len; i++) {
		g_string_append_printf(str, ", %" PRId64,
			ctf_field_path_borrow_index_by_index(path, i));
	}

	g_string_append(str, "]");

end:
	return str;
}

static inline
struct ctf_field_type *ctf_field_path_borrow_field_type(
		struct ctf_field_path *field_path,
		struct ctf_trace_class *tc,
		struct ctf_stream_class *sc,
		struct ctf_event_class *ec)
{
	uint64_t i;
	struct ctf_field_type *ft;

	switch (field_path->root) {
	case BT_SCOPE_PACKET_HEADER:
		ft = tc->packet_header_ft;
		break;
	case BT_SCOPE_PACKET_CONTEXT:
		ft = sc->packet_context_ft;
		break;
	case BT_SCOPE_EVENT_HEADER:
		ft = sc->event_header_ft;
		break;
	case BT_SCOPE_EVENT_COMMON_CONTEXT:
		ft = sc->event_common_context_ft;
		break;
	case BT_SCOPE_EVENT_SPECIFIC_CONTEXT:
		ft = ec->spec_context_ft;
		break;
	case BT_SCOPE_EVENT_PAYLOAD:
		ft = ec->payload_ft;
		break;
	default:
		abort();
	}

	BT_ASSERT(ft);

	for (i = 0; i < field_path->path->len; i++) {
		int64_t child_index =
			ctf_field_path_borrow_index_by_index(field_path, i);
		struct ctf_field_type *child_ft =
			ctf_field_type_compound_borrow_field_type_by_index(
				ft, child_index);
		BT_ASSERT(child_ft);
		ft = child_ft;
	}

	BT_ASSERT(ft);
	return ft;
}

static inline
struct ctf_field_type *ctf_field_type_copy(struct ctf_field_type *ft);

static inline
void ctf_field_type_bit_array_copy_content(
		struct ctf_field_type_bit_array *dst_ft,
		struct ctf_field_type_bit_array *src_ft)
{
	BT_ASSERT(dst_ft);
	BT_ASSERT(src_ft);
	dst_ft->byte_order = src_ft->byte_order;
	dst_ft->size = src_ft->size;
}

static inline
void ctf_field_type_int_copy_content(
		struct ctf_field_type_int *dst_ft,
		struct ctf_field_type_int *src_ft)
{
	ctf_field_type_bit_array_copy_content((void *) dst_ft, (void *) src_ft);
	dst_ft->meaning = src_ft->meaning;
	dst_ft->is_signed = src_ft->is_signed;
	dst_ft->disp_base = src_ft->disp_base;
	dst_ft->encoding = src_ft->encoding;
	dst_ft->mapped_clock_class = bt_get(src_ft->mapped_clock_class);
	dst_ft->storing_index = src_ft->storing_index;
}

static inline
struct ctf_field_type_int *_ctf_field_type_int_copy(
		struct ctf_field_type_int *ft)
{
	struct ctf_field_type_int *copy_ft = ctf_field_type_int_create();

	BT_ASSERT(copy_ft);
	ctf_field_type_int_copy_content(copy_ft, ft);
	return copy_ft;
}

static inline
struct ctf_field_type_enum *_ctf_field_type_enum_copy(
		struct ctf_field_type_enum *ft)
{
	struct ctf_field_type_enum *copy_ft = ctf_field_type_enum_create();
	uint64_t i;

	BT_ASSERT(copy_ft);
	ctf_field_type_int_copy_content((void *) copy_ft, (void *) ft);

	for (i = 0; i < ft->mappings->len; i++) {
		struct ctf_field_type_enum_mapping *mapping =
			&g_array_index(ft->mappings,
				struct ctf_field_type_enum_mapping, i);

		ctf_field_type_enum_append_mapping(copy_ft, mapping->label->str,
			mapping->range.lower.u, mapping->range.upper.u);
	}

	return copy_ft;
}

static inline
struct ctf_field_type_float *_ctf_field_type_float_copy(
		struct ctf_field_type_float *ft)
{
	struct ctf_field_type_float *copy_ft = ctf_field_type_float_create();

	BT_ASSERT(copy_ft);
	ctf_field_type_bit_array_copy_content((void *) copy_ft, (void *) ft);
	return copy_ft;
}

static inline
struct ctf_field_type_string *_ctf_field_type_string_copy(
		struct ctf_field_type_string *ft)
{
	struct ctf_field_type_string *copy_ft = ctf_field_type_string_create();

	BT_ASSERT(copy_ft);
	return copy_ft;
}

static inline
struct ctf_field_type_struct *_ctf_field_type_struct_copy(
		struct ctf_field_type_struct *ft)
{
	struct ctf_field_type_struct *copy_ft = ctf_field_type_struct_create();
	uint64_t i;

	BT_ASSERT(copy_ft);

	for (i = 0; i < ft->members->len; i++) {
		struct ctf_named_field_type *named_ft =
			&g_array_index(ft->members,
				struct ctf_named_field_type, i);

		ctf_field_type_struct_append_member(copy_ft,
			named_ft->name->str,
			ctf_field_type_copy(named_ft->ft));
	}

	return copy_ft;
}

static inline
void ctf_field_path_copy_content(struct ctf_field_path *dst_fp,
		struct ctf_field_path *src_fp)
{
	uint64_t i;

	BT_ASSERT(dst_fp);
	BT_ASSERT(src_fp);
	dst_fp->root = src_fp->root;
	ctf_field_path_clear(dst_fp);

	for (i = 0; i < src_fp->path->len; i++) {
		int64_t index = ctf_field_path_borrow_index_by_index(
			src_fp, i);

		ctf_field_path_append_index(dst_fp, index);
	}
}

static inline
struct ctf_field_type_variant *_ctf_field_type_variant_copy(
		struct ctf_field_type_variant *ft)
{
	struct ctf_field_type_variant *copy_ft =
		ctf_field_type_variant_create();
	uint64_t i;

	BT_ASSERT(copy_ft);

	for (i = 0; i < ft->options->len; i++) {
		struct ctf_named_field_type *named_ft =
			&g_array_index(ft->options,
				struct ctf_named_field_type, i);

		ctf_field_type_variant_append_option(copy_ft,
			named_ft->name->str,
			ctf_field_type_copy(named_ft->ft));
	}

	for (i = 0; i < ft->ranges->len; i++) {
		struct ctf_field_type_variant_range *range =
			&g_array_index(ft->ranges,
				struct ctf_field_type_variant_range, i);

		g_array_append_val(copy_ft->ranges, *range);
	}

	ctf_field_path_copy_content(&copy_ft->tag_path, &ft->tag_path);
	g_string_assign(copy_ft->tag_ref, ft->tag_ref->str);
	copy_ft->stored_tag_index = ft->stored_tag_index;
	return copy_ft;
}

static inline
void ctf_field_type_array_base_copy_content(
		struct ctf_field_type_array_base *dst_ft,
		struct ctf_field_type_array_base *src_ft)
{
	BT_ASSERT(dst_ft);
	BT_ASSERT(src_ft);
	dst_ft->elem_ft = ctf_field_type_copy(src_ft->elem_ft);
	dst_ft->is_text = src_ft->is_text;
}

static inline
struct ctf_field_type_array *_ctf_field_type_array_copy(
		struct ctf_field_type_array *ft)
{
	struct ctf_field_type_array *copy_ft = ctf_field_type_array_create();

	BT_ASSERT(copy_ft);
	ctf_field_type_array_base_copy_content((void *) copy_ft, (void *) ft);
	copy_ft->length = ft->length;
	return copy_ft;
}

static inline
struct ctf_field_type_sequence *_ctf_field_type_sequence_copy(
		struct ctf_field_type_sequence *ft)
{
	struct ctf_field_type_sequence *copy_ft =
		ctf_field_type_sequence_create();

	BT_ASSERT(copy_ft);
	ctf_field_type_array_base_copy_content((void *) copy_ft, (void *) ft);
	ctf_field_path_copy_content(&copy_ft->length_path, &ft->length_path);
	g_string_assign(copy_ft->length_ref, ft->length_ref->str);
	copy_ft->stored_length_index = ft->stored_length_index;
	return copy_ft;
}

static inline
struct ctf_field_type *ctf_field_type_copy(struct ctf_field_type *ft)
{
	struct ctf_field_type *copy_ft = NULL;

	if (!ft) {
		goto end;
	}

	/*
	 * Translation should not have happened yet.
	 */
	BT_ASSERT(!ft->ir_ft);

	switch (ft->id) {
	case CTF_FIELD_TYPE_ID_INT:
		copy_ft = (void *) _ctf_field_type_int_copy((void *) ft);
		break;
	case CTF_FIELD_TYPE_ID_ENUM:
		copy_ft = (void *) _ctf_field_type_enum_copy((void *) ft);
		break;
	case CTF_FIELD_TYPE_ID_FLOAT:
		copy_ft = (void *) _ctf_field_type_float_copy((void *) ft);
		break;
	case CTF_FIELD_TYPE_ID_STRING:
		copy_ft = (void *) _ctf_field_type_string_copy((void *) ft);
		break;
	case CTF_FIELD_TYPE_ID_STRUCT:
		copy_ft = (void *) _ctf_field_type_struct_copy((void *) ft);
		break;
	case CTF_FIELD_TYPE_ID_ARRAY:
		copy_ft = (void *) _ctf_field_type_array_copy((void *) ft);
		break;
	case CTF_FIELD_TYPE_ID_SEQUENCE:
		copy_ft = (void *) _ctf_field_type_sequence_copy((void *) ft);
		break;
	case CTF_FIELD_TYPE_ID_VARIANT:
		copy_ft = (void *) _ctf_field_type_variant_copy((void *) ft);
		break;
	default:
		abort();
	}

	copy_ft->id = ft->id;
	copy_ft->alignment = ft->alignment;
	copy_ft->in_ir = ft->in_ir;

end:
	return copy_ft;
}

static inline
struct ctf_event_class *ctf_event_class_create(void)
{
	struct ctf_event_class *ec = g_new0(struct ctf_event_class, 1);

	BT_ASSERT(ec);
	ec->name = g_string_new(NULL);
	BT_ASSERT(ec->name);
	ec->emf_uri = g_string_new(NULL);
	BT_ASSERT(ec->emf_uri);
	ec->log_level = -1;
	return ec;
}

static inline
void ctf_event_class_destroy(struct ctf_event_class *ec)
{
	if (!ec) {
		return;
	}

	if (ec->name) {
		g_string_free(ec->name, TRUE);
	}

	if (ec->emf_uri) {
		g_string_free(ec->emf_uri, TRUE);
	}

	ctf_field_type_destroy(ec->spec_context_ft);
	ctf_field_type_destroy(ec->payload_ft);
	g_free(ec);
}

static inline
struct ctf_stream_class *ctf_stream_class_create(void)
{
	struct ctf_stream_class *sc = g_new0(struct ctf_stream_class, 1);

	BT_ASSERT(sc);
	sc->event_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify) ctf_event_class_destroy);
	BT_ASSERT(sc->event_classes);
	sc->event_classes_by_id = g_hash_table_new(g_direct_hash,
		g_direct_equal);
	BT_ASSERT(sc->event_classes_by_id);
	return sc;
}

static inline
void ctf_stream_class_destroy(struct ctf_stream_class *sc)
{
	if (!sc) {
		return;
	}

	if (sc->event_classes) {
		g_ptr_array_free(sc->event_classes, TRUE);
	}

	if (sc->event_classes_by_id) {
		g_hash_table_destroy(sc->event_classes_by_id);
	}

	ctf_field_type_destroy(sc->packet_context_ft);
	ctf_field_type_destroy(sc->event_header_ft);
	ctf_field_type_destroy(sc->event_common_context_ft);
	bt_put(sc->default_clock_class);
	g_free(sc);
}

static inline
void ctf_stream_class_append_event_class(struct ctf_stream_class *sc,
		struct ctf_event_class *ec)
{
	g_ptr_array_add(sc->event_classes, ec);
	g_hash_table_insert(sc->event_classes_by_id,
		GUINT_TO_POINTER((guint) ec->id), ec);
}

static inline
struct ctf_event_class *ctf_stream_class_borrow_event_class_by_id(
		struct ctf_stream_class *sc, uint64_t id)
{
	BT_ASSERT(sc);
	return g_hash_table_lookup(sc->event_classes_by_id,
		GUINT_TO_POINTER((guint) id));
}

static inline
void _ctf_trace_class_env_entry_init(struct ctf_trace_class_env_entry *entry)
{
	BT_ASSERT(entry);
	entry->name = g_string_new(NULL);
	BT_ASSERT(entry->name);
	entry->value.str = g_string_new(NULL);
	BT_ASSERT(entry->value.str);
}

static inline
void _ctf_trace_class_env_entry_fini(struct ctf_trace_class_env_entry *entry)
{
	BT_ASSERT(entry);

	if (entry->name) {
		g_string_free(entry->name, TRUE);
	}

	if (entry->value.str) {
		g_string_free(entry->value.str, TRUE);
	}
}

static inline
struct ctf_trace_class *ctf_trace_class_create(void)
{
	struct ctf_trace_class *tc = g_new0(struct ctf_trace_class, 1);

	BT_ASSERT(tc);
	tc->name = g_string_new(NULL);
	tc->default_byte_order = -1;
	BT_ASSERT(tc->name);
	tc->clock_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_put);
	BT_ASSERT(tc->clock_classes);
	tc->stream_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify) ctf_stream_class_destroy);
	BT_ASSERT(tc->stream_classes);
	tc->env_entries = g_array_new(FALSE, TRUE,
		sizeof(struct ctf_trace_class_env_entry));
	return tc;
}

static inline
void ctf_trace_class_destroy(struct ctf_trace_class *tc)
{
	if (!tc) {
		return;
	}

	if (tc->name) {
		g_string_free(tc->name, TRUE);
	}

	ctf_field_type_destroy(tc->packet_header_ft);

	if (tc->clock_classes) {
		g_ptr_array_free(tc->clock_classes, TRUE);
	}

	if (tc->stream_classes) {
		g_ptr_array_free(tc->stream_classes, TRUE);
	}

	if (tc->env_entries) {
		uint64_t i;

		for (i = 0; i < tc->env_entries->len; i++) {
			struct ctf_trace_class_env_entry *entry =
				&g_array_index(tc->env_entries,
					struct ctf_trace_class_env_entry, i);

			_ctf_trace_class_env_entry_fini(entry);
		}

		g_array_free(tc->env_entries, TRUE);
	}

	g_free(tc);
}

static inline
void ctf_trace_class_append_env_entry(struct ctf_trace_class *tc,
		const char *name, enum ctf_trace_class_env_entry_type type,
		const char *str_value, int64_t i_value)
{
	struct ctf_trace_class_env_entry *entry;

	BT_ASSERT(tc);
	BT_ASSERT(name);
	g_array_set_size(tc->env_entries, tc->env_entries->len + 1);

	entry = &g_array_index(tc->env_entries,
		struct ctf_trace_class_env_entry, tc->env_entries->len - 1);
	entry->type = type;
	_ctf_trace_class_env_entry_init(entry);
	g_string_assign(entry->name, name);

	if (str_value) {
		g_string_assign(entry->value.str, str_value);
	}

	entry->value.i = i_value;
}

static inline
struct ctf_stream_class *ctf_trace_class_borrow_stream_class_by_id(
		struct ctf_trace_class *tc, uint64_t id)
{
	uint64_t i;
	struct ctf_stream_class *ret_sc = NULL;

	BT_ASSERT(tc);

	for (i = 0; i < tc->stream_classes->len; i++) {
		struct ctf_stream_class *sc = tc->stream_classes->pdata[i];

		if (sc->id == id) {
			ret_sc = sc;
			goto end;
		}
	}

end:
	return ret_sc;
}

static inline
struct bt_clock_class *ctf_trace_class_borrow_clock_class_by_name(
		struct ctf_trace_class *tc, const char *name)
{
	uint64_t i;
	struct bt_clock_class *ret_cc = NULL;

	BT_ASSERT(tc);
	BT_ASSERT(name);

	for (i = 0; i < tc->clock_classes->len; i++) {
		struct bt_clock_class *cc = tc->clock_classes->pdata[i];
		const char *cc_name = bt_clock_class_get_name(cc);

		BT_ASSERT(cc_name);
		if (strcmp(cc_name, name) == 0) {
			ret_cc = cc;
			goto end;
		}
	}

end:
	return ret_cc;
}

static inline
struct ctf_trace_class_env_entry *ctf_trace_class_borrow_env_entry_by_index(
		struct ctf_trace_class *tc, uint64_t index)
{
	BT_ASSERT(tc);
	BT_ASSERT(index < tc->env_entries->len);
	return &g_array_index(tc->env_entries, struct ctf_trace_class_env_entry,
		index);
}

static inline
struct ctf_trace_class_env_entry *ctf_trace_class_borrow_env_entry_by_name(
		struct ctf_trace_class *tc, const char *name)
{
	struct ctf_trace_class_env_entry *ret_entry = NULL;
	uint64_t i;

	BT_ASSERT(tc);
	BT_ASSERT(name);

	for (i = 0; i < tc->env_entries->len; i++) {
		struct ctf_trace_class_env_entry *env_entry =
			ctf_trace_class_borrow_env_entry_by_index(tc, i);

		if (strcmp(env_entry->name->str, name) == 0) {
			ret_entry = env_entry;
			goto end;
		}
	}

end:
	return ret_entry;
}

#endif /* _CTF_META_H */
