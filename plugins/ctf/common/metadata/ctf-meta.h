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

enum ctf_field_class_type {
	CTF_FIELD_CLASS_TYPE_INT,
	CTF_FIELD_CLASS_TYPE_ENUM,
	CTF_FIELD_CLASS_TYPE_FLOAT,
	CTF_FIELD_CLASS_TYPE_STRING,
	CTF_FIELD_CLASS_TYPE_STRUCT,
	CTF_FIELD_CLASS_TYPE_ARRAY,
	CTF_FIELD_CLASS_TYPE_SEQUENCE,
	CTF_FIELD_CLASS_TYPE_VARIANT,
};

enum ctf_field_class_meaning {
	CTF_FIELD_CLASS_MEANING_NONE,
	CTF_FIELD_CLASS_MEANING_PACKET_BEGINNING_TIME,
	CTF_FIELD_CLASS_MEANING_PACKET_END_TIME,
	CTF_FIELD_CLASS_MEANING_EVENT_CLASS_ID,
	CTF_FIELD_CLASS_MEANING_STREAM_CLASS_ID,
	CTF_FIELD_CLASS_MEANING_DATA_STREAM_ID,
	CTF_FIELD_CLASS_MEANING_MAGIC,
	CTF_FIELD_CLASS_MEANING_PACKET_COUNTER_SNAPSHOT,
	CTF_FIELD_CLASS_MEANING_DISC_EV_REC_COUNTER_SNAPSHOT,
	CTF_FIELD_CLASS_MEANING_EXP_PACKET_TOTAL_SIZE,
	CTF_FIELD_CLASS_MEANING_EXP_PACKET_CONTENT_SIZE,
	CTF_FIELD_CLASS_MEANING_UUID,
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

struct ctf_field_class {
	enum ctf_field_class_type type;
	unsigned int alignment;
	bool is_compound;
	bool in_ir;

	/* Weak, set during translation. NULL if `in_ir` is false below. */
	bt_field_class *ir_fc;
};

struct ctf_field_class_bit_array {
	struct ctf_field_class base;
	enum ctf_byte_order byte_order;
	unsigned int size;
};

struct ctf_field_class_int {
	struct ctf_field_class_bit_array base;
	enum ctf_field_class_meaning meaning;
	bool is_signed;
	enum bt_field_class_integer_preferred_display_base disp_base;
	enum ctf_encoding encoding;
	int64_t storing_index;

	/* Owned by this */
	bt_clock_class *mapped_clock_class;
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

struct ctf_field_class_enum_mapping {
	GString *label;
	struct ctf_range range;
};

struct ctf_field_class_enum {
	struct ctf_field_class_int base;

	/* Array of `struct ctf_field_class_enum_mapping` */
	GArray *mappings;
};

struct ctf_field_class_float {
	struct ctf_field_class_bit_array base;
};

struct ctf_field_class_string {
	struct ctf_field_class base;
	enum ctf_encoding encoding;
};

struct ctf_named_field_class {
	GString *name;

	/* Owned by this */
	struct ctf_field_class *fc;
};

struct ctf_field_class_struct {
	struct ctf_field_class base;

	/* Array of `struct ctf_named_field_class` */
	GArray *members;
};

struct ctf_field_path {
	enum bt_scope root;

	/* Array of `int64_t` */
	GArray *path;
};

struct ctf_field_class_variant_range {
	struct ctf_range range;
	uint64_t option_index;
};

struct ctf_field_class_variant {
	struct ctf_field_class base;
	GString *tag_ref;
	struct ctf_field_path tag_path;
	uint64_t stored_tag_index;

	/* Array of `struct ctf_named_field_class` */
	GArray *options;

	/* Array of `struct ctf_field_class_variant_range` */
	GArray *ranges;

	/* Weak */
	struct ctf_field_class_enum *tag_fc;
};

struct ctf_field_class_array_base {
	struct ctf_field_class base;
	struct ctf_field_class *elem_fc;
	bool is_text;
};

struct ctf_field_class_array {
	struct ctf_field_class_array_base base;
	enum ctf_field_class_meaning meaning;
	uint64_t length;
};

struct ctf_field_class_sequence {
	struct ctf_field_class_array_base base;
	GString *length_ref;
	struct ctf_field_path length_path;
	uint64_t stored_length_index;

	/* Weak */
	struct ctf_field_class_int *length_fc;
};

struct ctf_event_class {
	GString *name;
	uint64_t id;
	GString *emf_uri;
	enum bt_event_class_log_level log_level;
	bool is_translated;

	/* Owned by this */
	struct ctf_field_class *spec_context_fc;

	/* Owned by this */
	struct ctf_field_class *payload_fc;

	/* Weak, set during translation */
	bt_event_class *ir_ec;
};

struct ctf_stream_class {
	uint64_t id;
	bool is_translated;

	/* Owned by this */
	struct ctf_field_class *packet_context_fc;

	/* Owned by this */
	struct ctf_field_class *event_header_fc;

	/* Owned by this */
	struct ctf_field_class *event_common_context_fc;

	/* Array of `struct ctf_event_class *`, owned by this */
	GPtrArray *event_classes;

	/*
	 * Hash table mapping event class IDs to `struct ctf_event_class *`,
	 * weak.
	 */
	GHashTable *event_classes_by_id;

	/* Owned by this */
	bt_clock_class *default_clock_class;

	/* Weak, set during translation */
	bt_stream_class *ir_sc;
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
	unsigned int major;
	unsigned int minor;
	uint8_t uuid[16];
	bool is_uuid_set;
	enum ctf_byte_order default_byte_order;

	/* Owned by this */
	struct ctf_field_class *packet_header_fc;

	uint64_t stored_value_count;

	/* Array of `bt_clock_class *` (owned by this) */
	GPtrArray *clock_classes;

	/* Array of `struct ctf_stream_class *` */
	GPtrArray *stream_classes;

	/* Array of `struct ctf_trace_class_env_entry` */
	GArray *env_entries;

	bool is_translated;

	/* Weak, set during translation */
	bt_trace_class *ir_tc;
};

static inline
void ctf_field_class_destroy(struct ctf_field_class *fc);

static inline
void _ctf_field_class_init(struct ctf_field_class *fc,
		enum ctf_field_class_type type, unsigned int alignment)
{
	BT_ASSERT(fc);
	fc->type = type;
	fc->alignment = alignment;
	fc->in_ir = false;
}

static inline
void _ctf_field_class_bit_array_init(struct ctf_field_class_bit_array *fc,
		enum ctf_field_class_type type)
{
	_ctf_field_class_init((void *) fc, type, 1);
}

static inline
void _ctf_field_class_int_init(struct ctf_field_class_int *fc,
		enum ctf_field_class_type type)
{
	_ctf_field_class_bit_array_init((void *) fc, type);
	fc->meaning = CTF_FIELD_CLASS_MEANING_NONE;
	fc->storing_index = -1;
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
void _ctf_named_field_class_init(struct ctf_named_field_class *named_fc)
{
	BT_ASSERT(named_fc);
	named_fc->name = g_string_new(NULL);
	BT_ASSERT(named_fc->name);
}

static inline
void _ctf_named_field_class_fini(struct ctf_named_field_class *named_fc)
{
	BT_ASSERT(named_fc);

	if (named_fc->name) {
		g_string_free(named_fc->name, TRUE);
	}

	ctf_field_class_destroy(named_fc->fc);
}

static inline
void _ctf_field_class_enum_mapping_init(
		struct ctf_field_class_enum_mapping *mapping)
{
	BT_ASSERT(mapping);
	mapping->label = g_string_new(NULL);
	BT_ASSERT(mapping->label);
}

static inline
void _ctf_field_class_enum_mapping_fini(
		struct ctf_field_class_enum_mapping *mapping)
{
	BT_ASSERT(mapping);

	if (mapping->label) {
		g_string_free(mapping->label, TRUE);
	}
}

static inline
struct ctf_field_class_int *ctf_field_class_int_create(void)
{
	struct ctf_field_class_int *fc = g_new0(struct ctf_field_class_int, 1);

	BT_ASSERT(fc);
	_ctf_field_class_int_init(fc, CTF_FIELD_CLASS_TYPE_INT);
	return fc;
}

static inline
struct ctf_field_class_float *ctf_field_class_float_create(void)
{
	struct ctf_field_class_float *fc =
		g_new0(struct ctf_field_class_float, 1);

	BT_ASSERT(fc);
	_ctf_field_class_bit_array_init((void *) fc, CTF_FIELD_CLASS_TYPE_FLOAT);
	return fc;
}

static inline
struct ctf_field_class_string *ctf_field_class_string_create(void)
{
	struct ctf_field_class_string *fc =
		g_new0(struct ctf_field_class_string, 1);

	BT_ASSERT(fc);
	_ctf_field_class_init((void *) fc, CTF_FIELD_CLASS_TYPE_STRING, 8);
	return fc;
}

static inline
struct ctf_field_class_enum *ctf_field_class_enum_create(void)
{
	struct ctf_field_class_enum *fc = g_new0(struct ctf_field_class_enum, 1);

	BT_ASSERT(fc);
	_ctf_field_class_int_init((void *) fc, CTF_FIELD_CLASS_TYPE_ENUM);
	fc->mappings = g_array_new(FALSE, TRUE,
		sizeof(struct ctf_field_class_enum_mapping));
	BT_ASSERT(fc->mappings);
	return fc;
}

static inline
struct ctf_field_class_struct *ctf_field_class_struct_create(void)
{
	struct ctf_field_class_struct *fc =
		g_new0(struct ctf_field_class_struct, 1);

	BT_ASSERT(fc);
	_ctf_field_class_init((void *) fc, CTF_FIELD_CLASS_TYPE_STRUCT, 1);
	fc->members = g_array_new(FALSE, TRUE,
		sizeof(struct ctf_named_field_class));
	BT_ASSERT(fc->members);
	fc->base.is_compound = true;
	return fc;
}

static inline
struct ctf_field_class_variant *ctf_field_class_variant_create(void)
{
	struct ctf_field_class_variant *fc =
		g_new0(struct ctf_field_class_variant, 1);

	BT_ASSERT(fc);
	_ctf_field_class_init((void *) fc, CTF_FIELD_CLASS_TYPE_VARIANT, 1);
	fc->options = g_array_new(FALSE, TRUE,
		sizeof(struct ctf_named_field_class));
	BT_ASSERT(fc->options);
	fc->ranges = g_array_new(FALSE, TRUE,
		sizeof(struct ctf_field_class_variant_range));
	BT_ASSERT(fc->ranges);
	fc->tag_ref = g_string_new(NULL);
	BT_ASSERT(fc->tag_ref);
	ctf_field_path_init(&fc->tag_path);
	fc->base.is_compound = true;
	return fc;
}

static inline
struct ctf_field_class_array *ctf_field_class_array_create(void)
{
	struct ctf_field_class_array *fc =
		g_new0(struct ctf_field_class_array, 1);

	BT_ASSERT(fc);
	_ctf_field_class_init((void *) fc, CTF_FIELD_CLASS_TYPE_ARRAY, 1);
	fc->base.base.is_compound = true;
	return fc;
}

static inline
struct ctf_field_class_sequence *ctf_field_class_sequence_create(void)
{
	struct ctf_field_class_sequence *fc =
		g_new0(struct ctf_field_class_sequence, 1);

	BT_ASSERT(fc);
	_ctf_field_class_init((void *) fc, CTF_FIELD_CLASS_TYPE_SEQUENCE, 1);
	fc->length_ref = g_string_new(NULL);
	BT_ASSERT(fc->length_ref);
	ctf_field_path_init(&fc->length_path);
	fc->base.base.is_compound = true;
	return fc;
}

static inline
void _ctf_field_class_int_destroy(struct ctf_field_class_int *fc)
{
	BT_ASSERT(fc);
	bt_clock_class_put_ref(fc->mapped_clock_class);
	g_free(fc);
}

static inline
void _ctf_field_class_enum_destroy(struct ctf_field_class_enum *fc)
{
	BT_ASSERT(fc);
	bt_clock_class_put_ref(fc->base.mapped_clock_class);

	if (fc->mappings) {
		uint64_t i;

		for (i = 0; i < fc->mappings->len; i++) {
			struct ctf_field_class_enum_mapping *mapping =
				&g_array_index(fc->mappings,
					struct ctf_field_class_enum_mapping, i);

			_ctf_field_class_enum_mapping_fini(mapping);
		}

		g_array_free(fc->mappings, TRUE);
	}

	g_free(fc);
}

static inline
void _ctf_field_class_float_destroy(struct ctf_field_class_float *fc)
{
	BT_ASSERT(fc);
	g_free(fc);
}

static inline
void _ctf_field_class_string_destroy(struct ctf_field_class_string *fc)
{
	BT_ASSERT(fc);
	g_free(fc);
}

static inline
void _ctf_field_class_struct_destroy(struct ctf_field_class_struct *fc)
{
	BT_ASSERT(fc);

	if (fc->members) {
		uint64_t i;

		for (i = 0; i < fc->members->len; i++) {
			struct ctf_named_field_class *named_fc =
				&g_array_index(fc->members,
					struct ctf_named_field_class, i);

			_ctf_named_field_class_fini(named_fc);
		}

		g_array_free(fc->members, TRUE);
	}

	g_free(fc);
}

static inline
void _ctf_field_class_array_base_fini(struct ctf_field_class_array_base *fc)
{
	BT_ASSERT(fc);
	ctf_field_class_destroy(fc->elem_fc);
}

static inline
void _ctf_field_class_array_destroy(struct ctf_field_class_array *fc)
{
	BT_ASSERT(fc);
	_ctf_field_class_array_base_fini((void *) fc);
	g_free(fc);
}

static inline
void _ctf_field_class_sequence_destroy(struct ctf_field_class_sequence *fc)
{
	BT_ASSERT(fc);
	_ctf_field_class_array_base_fini((void *) fc);

	if (fc->length_ref) {
		g_string_free(fc->length_ref, TRUE);
	}

	ctf_field_path_fini(&fc->length_path);
	g_free(fc);
}

static inline
void _ctf_field_class_variant_destroy(struct ctf_field_class_variant *fc)
{
	BT_ASSERT(fc);

	if (fc->options) {
		uint64_t i;

		for (i = 0; i < fc->options->len; i++) {
			struct ctf_named_field_class *named_fc =
				&g_array_index(fc->options,
					struct ctf_named_field_class, i);

			_ctf_named_field_class_fini(named_fc);
		}

		g_array_free(fc->options, TRUE);
	}

	if (fc->ranges) {
		g_array_free(fc->ranges, TRUE);
	}

	if (fc->tag_ref) {
		g_string_free(fc->tag_ref, TRUE);
	}

	ctf_field_path_fini(&fc->tag_path);
	g_free(fc);
}

static inline
void ctf_field_class_destroy(struct ctf_field_class *fc)
{
	if (!fc) {
		return;
	}

	switch (fc->type) {
	case CTF_FIELD_CLASS_TYPE_INT:
		_ctf_field_class_int_destroy((void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_ENUM:
		_ctf_field_class_enum_destroy((void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_FLOAT:
		_ctf_field_class_float_destroy((void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_STRING:
		_ctf_field_class_string_destroy((void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_STRUCT:
		_ctf_field_class_struct_destroy((void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_ARRAY:
		_ctf_field_class_array_destroy((void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_SEQUENCE:
		_ctf_field_class_sequence_destroy((void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_VARIANT:
		_ctf_field_class_variant_destroy((void *) fc);
		break;
	default:
		abort();
	}
}

static inline
void ctf_field_class_enum_append_mapping(struct ctf_field_class_enum *fc,
		const char *label, uint64_t u_lower, uint64_t u_upper)
{
	struct ctf_field_class_enum_mapping *mapping;

	BT_ASSERT(fc);
	BT_ASSERT(label);
	g_array_set_size(fc->mappings, fc->mappings->len + 1);

	mapping = &g_array_index(fc->mappings,
		struct ctf_field_class_enum_mapping, fc->mappings->len - 1);
	_ctf_field_class_enum_mapping_init(mapping);
	g_string_assign(mapping->label, label);
	mapping->range.lower.u = u_lower;
	mapping->range.upper.u = u_upper;
}

static inline
struct ctf_field_class_enum_mapping *ctf_field_class_enum_borrow_mapping_by_index(
		struct ctf_field_class_enum *fc, uint64_t index)
{
	BT_ASSERT(fc);
	BT_ASSERT(index < fc->mappings->len);
	return &g_array_index(fc->mappings, struct ctf_field_class_enum_mapping,
		index);
}

static inline
struct ctf_named_field_class *ctf_field_class_struct_borrow_member_by_index(
		struct ctf_field_class_struct *fc, uint64_t index)
{
	BT_ASSERT(fc);
	BT_ASSERT(index < fc->members->len);
	return &g_array_index(fc->members, struct ctf_named_field_class,
		index);
}

static inline
struct ctf_named_field_class *ctf_field_class_struct_borrow_member_by_name(
		struct ctf_field_class_struct *fc, const char *name)
{
	uint64_t i;
	struct ctf_named_field_class *ret_named_fc = NULL;

	BT_ASSERT(fc);
	BT_ASSERT(name);

	for (i = 0; i < fc->members->len; i++) {
		struct ctf_named_field_class *named_fc =
			ctf_field_class_struct_borrow_member_by_index(fc, i);

		if (strcmp(name, named_fc->name->str) == 0) {
			ret_named_fc = named_fc;
			goto end;
		}
	}

end:
	return ret_named_fc;
}

static inline
struct ctf_field_class *ctf_field_class_struct_borrow_member_field_class_by_name(
		struct ctf_field_class_struct *struct_fc, const char *name)
{
	struct ctf_named_field_class *named_fc = NULL;
	struct ctf_field_class *fc = NULL;

	if (!struct_fc) {
		goto end;
	}

	named_fc = ctf_field_class_struct_borrow_member_by_name(struct_fc, name);
	if (!named_fc) {
		goto end;
	}

	fc = named_fc->fc;

end:
	return fc;
}

static inline
struct ctf_field_class_int *
ctf_field_class_struct_borrow_member_int_field_class_by_name(
		struct ctf_field_class_struct *struct_fc, const char *name)
{
	struct ctf_field_class_int *int_fc = NULL;

	int_fc = (void *)
		ctf_field_class_struct_borrow_member_field_class_by_name(
			struct_fc, name);
	if (!int_fc) {
		goto end;
	}

	if (int_fc->base.base.type != CTF_FIELD_CLASS_TYPE_INT &&
			int_fc->base.base.type != CTF_FIELD_CLASS_TYPE_ENUM) {
		int_fc = NULL;
		goto end;
	}

end:
	return int_fc;
}


static inline
void ctf_field_class_struct_append_member(struct ctf_field_class_struct *fc,
		const char *name, struct ctf_field_class *member_fc)
{
	struct ctf_named_field_class *named_fc;

	BT_ASSERT(fc);
	BT_ASSERT(name);
	g_array_set_size(fc->members, fc->members->len + 1);

	named_fc = &g_array_index(fc->members, struct ctf_named_field_class,
		fc->members->len - 1);
	_ctf_named_field_class_init(named_fc);
	g_string_assign(named_fc->name, name);
	named_fc->fc = member_fc;

	if (member_fc->alignment > fc->base.alignment) {
		fc->base.alignment = member_fc->alignment;
	}
}

static inline
struct ctf_named_field_class *ctf_field_class_variant_borrow_option_by_index(
		struct ctf_field_class_variant *fc, uint64_t index)
{
	BT_ASSERT(fc);
	BT_ASSERT(index < fc->options->len);
	return &g_array_index(fc->options, struct ctf_named_field_class,
		index);
}

static inline
struct ctf_named_field_class *ctf_field_class_variant_borrow_option_by_name(
		struct ctf_field_class_variant *fc, const char *name)
{
	uint64_t i;
	struct ctf_named_field_class *ret_named_fc = NULL;

	BT_ASSERT(fc);
	BT_ASSERT(name);

	for (i = 0; i < fc->options->len; i++) {
		struct ctf_named_field_class *named_fc =
			ctf_field_class_variant_borrow_option_by_index(fc, i);

		if (strcmp(name, named_fc->name->str) == 0) {
			ret_named_fc = named_fc;
			goto end;
		}
	}

end:
	return ret_named_fc;
}

static inline
struct ctf_field_class_variant_range *
ctf_field_class_variant_borrow_range_by_index(
		struct ctf_field_class_variant *fc, uint64_t index)
{
	BT_ASSERT(fc);
	BT_ASSERT(index < fc->ranges->len);
	return &g_array_index(fc->ranges, struct ctf_field_class_variant_range,
		index);
}

static inline
void ctf_field_class_variant_append_option(struct ctf_field_class_variant *fc,
		const char *name, struct ctf_field_class *option_fc)
{
	struct ctf_named_field_class *named_fc;

	BT_ASSERT(fc);
	BT_ASSERT(name);
	g_array_set_size(fc->options, fc->options->len + 1);

	named_fc = &g_array_index(fc->options, struct ctf_named_field_class,
		fc->options->len - 1);
	_ctf_named_field_class_init(named_fc);
	g_string_assign(named_fc->name, name);
	named_fc->fc = option_fc;
}

static inline
void ctf_field_class_variant_set_tag_field_class(
		struct ctf_field_class_variant *fc,
		struct ctf_field_class_enum *tag_fc)
{
	uint64_t option_i;

	BT_ASSERT(fc);
	BT_ASSERT(tag_fc);
	fc->tag_fc = tag_fc;

	for (option_i = 0; option_i < fc->options->len; option_i++) {
		uint64_t mapping_i;
		struct ctf_named_field_class *named_fc =
			ctf_field_class_variant_borrow_option_by_index(
				fc, option_i);

		for (mapping_i = 0; mapping_i < tag_fc->mappings->len;
				mapping_i++) {
			struct ctf_field_class_enum_mapping *mapping =
				ctf_field_class_enum_borrow_mapping_by_index(
					tag_fc, mapping_i);

			if (strcmp(named_fc->name->str,
					mapping->label->str) == 0) {
				struct ctf_field_class_variant_range range;

				range.range = mapping->range;
				range.option_index = option_i;
				g_array_append_val(fc->ranges, range);
			}
		}
	}
}

static inline
struct ctf_field_class *ctf_field_class_compound_borrow_field_class_by_index(
		struct ctf_field_class *comp_fc, uint64_t index)
{
	struct ctf_field_class *fc = NULL;

	switch (comp_fc->type) {
	case CTF_FIELD_CLASS_TYPE_STRUCT:
	{
		struct ctf_named_field_class *named_fc =
			ctf_field_class_struct_borrow_member_by_index(
				(void *) comp_fc, index);

		BT_ASSERT(named_fc);
		fc = named_fc->fc;
		break;
	}
	case CTF_FIELD_CLASS_TYPE_VARIANT:
	{
		struct ctf_named_field_class *named_fc =
			ctf_field_class_variant_borrow_option_by_index(
				(void *) comp_fc, index);

		BT_ASSERT(named_fc);
		fc = named_fc->fc;
		break;
	}
	case CTF_FIELD_CLASS_TYPE_ARRAY:
	case CTF_FIELD_CLASS_TYPE_SEQUENCE:
	{
		struct ctf_field_class_array_base *array_fc = (void *) comp_fc;

		fc = array_fc->elem_fc;
		break;
	}
	default:
		break;
	}

	return fc;
}

static inline
uint64_t ctf_field_class_compound_get_field_class_count(struct ctf_field_class *fc)
{
	uint64_t field_count;

	switch (fc->type) {
	case CTF_FIELD_CLASS_TYPE_STRUCT:
	{
		struct ctf_field_class_struct *struct_fc = (void *) fc;

		field_count = struct_fc->members->len;
		break;
	}
	case CTF_FIELD_CLASS_TYPE_VARIANT:
	{
		struct ctf_field_class_variant *var_fc = (void *) fc;

		field_count = var_fc->options->len;
		break;
	}
	case CTF_FIELD_CLASS_TYPE_ARRAY:
	case CTF_FIELD_CLASS_TYPE_SEQUENCE:
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
int64_t ctf_field_class_compound_get_field_class_index_from_name(
		struct ctf_field_class *fc, const char *name)
{
	int64_t ret_index = -1;
	uint64_t i;

	switch (fc->type) {
	case CTF_FIELD_CLASS_TYPE_STRUCT:
	{
		struct ctf_field_class_struct *struct_fc = (void *) fc;

		for (i = 0; i < struct_fc->members->len; i++) {
			struct ctf_named_field_class *named_fc =
				ctf_field_class_struct_borrow_member_by_index(
					struct_fc, i);

			if (strcmp(name, named_fc->name->str) == 0) {
				ret_index = (int64_t) i;
				goto end;
			}
		}

		break;
	}
	case CTF_FIELD_CLASS_TYPE_VARIANT:
	{
		struct ctf_field_class_variant *var_fc = (void *) fc;

		for (i = 0; i < var_fc->options->len; i++) {
			struct ctf_named_field_class *named_fc =
				ctf_field_class_variant_borrow_option_by_index(
					var_fc, i);

			if (strcmp(name, named_fc->name->str) == 0) {
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
struct ctf_field_class *ctf_field_path_borrow_field_class(
		struct ctf_field_path *field_path,
		struct ctf_trace_class *tc,
		struct ctf_stream_class *sc,
		struct ctf_event_class *ec)
{
	uint64_t i;
	struct ctf_field_class *fc;

	switch (field_path->root) {
	case BT_SCOPE_PACKET_HEADER:
		fc = tc->packet_header_fc;
		break;
	case BT_SCOPE_PACKET_CONTEXT:
		fc = sc->packet_context_fc;
		break;
	case BT_SCOPE_EVENT_HEADER:
		fc = sc->event_header_fc;
		break;
	case BT_SCOPE_EVENT_COMMON_CONTEXT:
		fc = sc->event_common_context_fc;
		break;
	case BT_SCOPE_EVENT_SPECIFIC_CONTEXT:
		fc = ec->spec_context_fc;
		break;
	case BT_SCOPE_EVENT_PAYLOAD:
		fc = ec->payload_fc;
		break;
	default:
		abort();
	}

	BT_ASSERT(fc);

	for (i = 0; i < field_path->path->len; i++) {
		int64_t child_index =
			ctf_field_path_borrow_index_by_index(field_path, i);
		struct ctf_field_class *child_fc =
			ctf_field_class_compound_borrow_field_class_by_index(
				fc, child_index);
		BT_ASSERT(child_fc);
		fc = child_fc;
	}

	BT_ASSERT(fc);
	return fc;
}

static inline
struct ctf_field_class *ctf_field_class_copy(struct ctf_field_class *fc);

static inline
void ctf_field_class_bit_array_copy_content(
		struct ctf_field_class_bit_array *dst_fc,
		struct ctf_field_class_bit_array *src_fc)
{
	BT_ASSERT(dst_fc);
	BT_ASSERT(src_fc);
	dst_fc->byte_order = src_fc->byte_order;
	dst_fc->size = src_fc->size;
}

static inline
void ctf_field_class_int_copy_content(
		struct ctf_field_class_int *dst_fc,
		struct ctf_field_class_int *src_fc)
{
	ctf_field_class_bit_array_copy_content((void *) dst_fc, (void *) src_fc);
	dst_fc->meaning = src_fc->meaning;
	dst_fc->is_signed = src_fc->is_signed;
	dst_fc->disp_base = src_fc->disp_base;
	dst_fc->encoding = src_fc->encoding;
	dst_fc->mapped_clock_class = src_fc->mapped_clock_class;
	bt_clock_class_get_ref(dst_fc->mapped_clock_class);
	dst_fc->storing_index = src_fc->storing_index;
}

static inline
struct ctf_field_class_int *_ctf_field_class_int_copy(
		struct ctf_field_class_int *fc)
{
	struct ctf_field_class_int *copy_fc = ctf_field_class_int_create();

	BT_ASSERT(copy_fc);
	ctf_field_class_int_copy_content(copy_fc, fc);
	return copy_fc;
}

static inline
struct ctf_field_class_enum *_ctf_field_class_enum_copy(
		struct ctf_field_class_enum *fc)
{
	struct ctf_field_class_enum *copy_fc = ctf_field_class_enum_create();
	uint64_t i;

	BT_ASSERT(copy_fc);
	ctf_field_class_int_copy_content((void *) copy_fc, (void *) fc);

	for (i = 0; i < fc->mappings->len; i++) {
		struct ctf_field_class_enum_mapping *mapping =
			&g_array_index(fc->mappings,
				struct ctf_field_class_enum_mapping, i);

		ctf_field_class_enum_append_mapping(copy_fc, mapping->label->str,
			mapping->range.lower.u, mapping->range.upper.u);
	}

	return copy_fc;
}

static inline
struct ctf_field_class_float *_ctf_field_class_float_copy(
		struct ctf_field_class_float *fc)
{
	struct ctf_field_class_float *copy_fc = ctf_field_class_float_create();

	BT_ASSERT(copy_fc);
	ctf_field_class_bit_array_copy_content((void *) copy_fc, (void *) fc);
	return copy_fc;
}

static inline
struct ctf_field_class_string *_ctf_field_class_string_copy(
		struct ctf_field_class_string *fc)
{
	struct ctf_field_class_string *copy_fc = ctf_field_class_string_create();

	BT_ASSERT(copy_fc);
	return copy_fc;
}

static inline
struct ctf_field_class_struct *_ctf_field_class_struct_copy(
		struct ctf_field_class_struct *fc)
{
	struct ctf_field_class_struct *copy_fc = ctf_field_class_struct_create();
	uint64_t i;

	BT_ASSERT(copy_fc);

	for (i = 0; i < fc->members->len; i++) {
		struct ctf_named_field_class *named_fc =
			&g_array_index(fc->members,
				struct ctf_named_field_class, i);

		ctf_field_class_struct_append_member(copy_fc,
			named_fc->name->str,
			ctf_field_class_copy(named_fc->fc));
	}

	return copy_fc;
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
struct ctf_field_class_variant *_ctf_field_class_variant_copy(
		struct ctf_field_class_variant *fc)
{
	struct ctf_field_class_variant *copy_fc =
		ctf_field_class_variant_create();
	uint64_t i;

	BT_ASSERT(copy_fc);

	for (i = 0; i < fc->options->len; i++) {
		struct ctf_named_field_class *named_fc =
			&g_array_index(fc->options,
				struct ctf_named_field_class, i);

		ctf_field_class_variant_append_option(copy_fc,
			named_fc->name->str,
			ctf_field_class_copy(named_fc->fc));
	}

	for (i = 0; i < fc->ranges->len; i++) {
		struct ctf_field_class_variant_range *range =
			&g_array_index(fc->ranges,
				struct ctf_field_class_variant_range, i);

		g_array_append_val(copy_fc->ranges, *range);
	}

	ctf_field_path_copy_content(&copy_fc->tag_path, &fc->tag_path);
	g_string_assign(copy_fc->tag_ref, fc->tag_ref->str);
	copy_fc->stored_tag_index = fc->stored_tag_index;
	return copy_fc;
}

static inline
void ctf_field_class_array_base_copy_content(
		struct ctf_field_class_array_base *dst_fc,
		struct ctf_field_class_array_base *src_fc)
{
	BT_ASSERT(dst_fc);
	BT_ASSERT(src_fc);
	dst_fc->elem_fc = ctf_field_class_copy(src_fc->elem_fc);
	dst_fc->is_text = src_fc->is_text;
}

static inline
struct ctf_field_class_array *_ctf_field_class_array_copy(
		struct ctf_field_class_array *fc)
{
	struct ctf_field_class_array *copy_fc = ctf_field_class_array_create();

	BT_ASSERT(copy_fc);
	ctf_field_class_array_base_copy_content((void *) copy_fc, (void *) fc);
	copy_fc->length = fc->length;
	return copy_fc;
}

static inline
struct ctf_field_class_sequence *_ctf_field_class_sequence_copy(
		struct ctf_field_class_sequence *fc)
{
	struct ctf_field_class_sequence *copy_fc =
		ctf_field_class_sequence_create();

	BT_ASSERT(copy_fc);
	ctf_field_class_array_base_copy_content((void *) copy_fc, (void *) fc);
	ctf_field_path_copy_content(&copy_fc->length_path, &fc->length_path);
	g_string_assign(copy_fc->length_ref, fc->length_ref->str);
	copy_fc->stored_length_index = fc->stored_length_index;
	return copy_fc;
}

static inline
struct ctf_field_class *ctf_field_class_copy(struct ctf_field_class *fc)
{
	struct ctf_field_class *copy_fc = NULL;

	if (!fc) {
		goto end;
	}

	/*
	 * Translation should not have happened yet.
	 */
	BT_ASSERT(!fc->ir_fc);

	switch (fc->type) {
	case CTF_FIELD_CLASS_TYPE_INT:
		copy_fc = (void *) _ctf_field_class_int_copy((void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_ENUM:
		copy_fc = (void *) _ctf_field_class_enum_copy((void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_FLOAT:
		copy_fc = (void *) _ctf_field_class_float_copy((void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_STRING:
		copy_fc = (void *) _ctf_field_class_string_copy((void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_STRUCT:
		copy_fc = (void *) _ctf_field_class_struct_copy((void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_ARRAY:
		copy_fc = (void *) _ctf_field_class_array_copy((void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_SEQUENCE:
		copy_fc = (void *) _ctf_field_class_sequence_copy((void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_VARIANT:
		copy_fc = (void *) _ctf_field_class_variant_copy((void *) fc);
		break;
	default:
		abort();
	}

	copy_fc->type = fc->type;
	copy_fc->alignment = fc->alignment;
	copy_fc->in_ir = fc->in_ir;

end:
	return copy_fc;
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

	ctf_field_class_destroy(ec->spec_context_fc);
	ctf_field_class_destroy(ec->payload_fc);
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

	ctf_field_class_destroy(sc->packet_context_fc);
	ctf_field_class_destroy(sc->event_header_fc);
	ctf_field_class_destroy(sc->event_common_context_fc);
	bt_clock_class_put_ref(sc->default_clock_class);
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
		struct ctf_stream_class *sc, uint64_t type)
{
	BT_ASSERT(sc);
	return g_hash_table_lookup(sc->event_classes_by_id,
		GUINT_TO_POINTER((guint) type));
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
	tc->default_byte_order = -1;
	tc->clock_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_clock_class_put_ref);
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

	ctf_field_class_destroy(tc->packet_header_fc);

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
bt_clock_class *ctf_trace_class_borrow_clock_class_by_name(
		struct ctf_trace_class *tc, const char *name)
{
	uint64_t i;
	bt_clock_class *ret_cc = NULL;

	BT_ASSERT(tc);
	BT_ASSERT(name);

	for (i = 0; i < tc->clock_classes->len; i++) {
		bt_clock_class *cc = tc->clock_classes->pdata[i];
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
