#ifndef BABELTRACE_PLUGIN_CTF_FS_SINK_FS_SINK_CTF_META_H
#define BABELTRACE_PLUGIN_CTF_FS_SINK_FS_SINK_CTF_META_H

/*
 * Copyright 2018-2019 - Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace2/babeltrace.h>
#include "common/common.h"
#include "common/assert.h"
#include "common/uuid.h"
#include <glib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

enum fs_sink_ctf_field_class_type {
	FS_SINK_CTF_FIELD_CLASS_TYPE_BOOL,
	FS_SINK_CTF_FIELD_CLASS_TYPE_BIT_ARRAY,
	FS_SINK_CTF_FIELD_CLASS_TYPE_INT,
	FS_SINK_CTF_FIELD_CLASS_TYPE_FLOAT,
	FS_SINK_CTF_FIELD_CLASS_TYPE_STRING,
	FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT,
	FS_SINK_CTF_FIELD_CLASS_TYPE_ARRAY,
	FS_SINK_CTF_FIELD_CLASS_TYPE_SEQUENCE,
	FS_SINK_CTF_FIELD_CLASS_TYPE_OPTION,
	FS_SINK_CTF_FIELD_CLASS_TYPE_VARIANT,
};

struct fs_sink_ctf_field_class {
	enum fs_sink_ctf_field_class_type type;

	/* Weak */
	const bt_field_class *ir_fc;

	unsigned int alignment;

	/* Index of the field class within its own parent */
	uint64_t index_in_parent;
};

struct fs_sink_ctf_field_class_bit_array {
	struct fs_sink_ctf_field_class base;
	unsigned int size;
};

struct fs_sink_ctf_field_class_bool {
	struct fs_sink_ctf_field_class_bit_array base;
};

struct fs_sink_ctf_field_class_int {
	struct fs_sink_ctf_field_class_bit_array base;
	bool is_signed;
};

struct fs_sink_ctf_field_class_float {
	struct fs_sink_ctf_field_class_bit_array base;
};

struct fs_sink_ctf_field_class_string {
	struct fs_sink_ctf_field_class base;
};

struct fs_sink_ctf_named_field_class {
	GString *name;

	/* Owned by this */
	struct fs_sink_ctf_field_class *fc;
};

struct fs_sink_ctf_field_class_struct {
	struct fs_sink_ctf_field_class base;

	/* Array of `struct fs_sink_ctf_named_field_class` */
	GArray *members;
};

struct fs_sink_ctf_field_class_option {
	struct fs_sink_ctf_field_class base;
	struct fs_sink_ctf_field_class *content_fc;
	GString *tag_ref;
};

struct fs_sink_ctf_field_class_variant {
	struct fs_sink_ctf_field_class base;
	GString *tag_ref;
	bool tag_is_before;

	/* Array of `struct fs_sink_ctf_named_field_class` */
	GArray *options;
};

struct fs_sink_ctf_field_class_array_base {
	struct fs_sink_ctf_field_class base;
	struct fs_sink_ctf_field_class *elem_fc;
};

struct fs_sink_ctf_field_class_array {
	struct fs_sink_ctf_field_class_array_base base;
	uint64_t length;
};

struct fs_sink_ctf_field_class_sequence {
	struct fs_sink_ctf_field_class_array_base base;
	GString *length_ref;
	bool length_is_before;
};

struct fs_sink_ctf_stream_class;

struct fs_sink_ctf_event_class {
	/* Weak */
	const bt_event_class *ir_ec;

	/* Weak */
	struct fs_sink_ctf_stream_class *sc;

	/* Owned by this */
	struct fs_sink_ctf_field_class *spec_context_fc;

	/* Owned by this */
	struct fs_sink_ctf_field_class *payload_fc;
};

struct fs_sink_ctf_trace;

struct fs_sink_ctf_stream_class {
	/* Weak */
	struct fs_sink_ctf_trace *trace;

	/* Weak */
	const bt_stream_class *ir_sc;

	/* Weak */
	const bt_clock_class *default_clock_class;

	GString *default_clock_class_name;
	bool has_packets;
	bool packets_have_ts_begin;
	bool packets_have_ts_end;
	bool has_discarded_events;
	bool discarded_events_has_ts;
	bool discarded_packets_has_ts;

	/* Owned by this */
	struct fs_sink_ctf_field_class *packet_context_fc;

	/* Owned by this */
	struct fs_sink_ctf_field_class *event_common_context_fc;

	/* Array of `struct fs_sink_ctf_event_class *` (owned by this) */
	GPtrArray *event_classes;

	/*
	 * `const bt_event_class *` (weak) ->
	 * `struct fs_sink_ctf_event_class *` (weak)
	 */
	GHashTable *event_classes_from_ir;
};

struct fs_sink_ctf_trace {
	/* Weak */
	const bt_trace *ir_trace;

	/* Weak */
	const bt_trace_class *ir_tc;

	bt_uuid_t uuid;

	/* Array of `struct fs_sink_ctf_stream_class *` (owned by this) */
	GPtrArray *stream_classes;
};

static inline
void fs_sink_ctf_field_class_destroy(struct fs_sink_ctf_field_class *fc);

static inline
void _fs_sink_ctf_field_class_init(struct fs_sink_ctf_field_class *fc,
		enum fs_sink_ctf_field_class_type type,
		const bt_field_class *ir_fc, unsigned int alignment,
		uint64_t index_in_parent)
{
	BT_ASSERT(fc);
	fc->type = type;
	fc->ir_fc = ir_fc;
	fc->alignment = alignment;
	fc->index_in_parent = index_in_parent;
}

static inline
void _fs_sink_ctf_field_class_bit_array_init(
		struct fs_sink_ctf_field_class_bit_array *fc,
		enum fs_sink_ctf_field_class_type type,
		const bt_field_class *ir_fc, unsigned int size,
		uint64_t index_in_parent)
{
	_fs_sink_ctf_field_class_init((void *) fc, type, ir_fc,
		size % 8 == 0 ? 8 : 1, index_in_parent);
	fc->size = size;
}

static inline
void _fs_sink_ctf_field_class_int_init(struct fs_sink_ctf_field_class_int *fc,
		enum fs_sink_ctf_field_class_type type,
		const bt_field_class *ir_fc, uint64_t index_in_parent)
{
	bt_field_class_type ir_fc_type = bt_field_class_get_type(ir_fc);

	_fs_sink_ctf_field_class_bit_array_init((void *) fc, type, ir_fc,
		(unsigned int) bt_field_class_integer_get_field_value_range(
			ir_fc),
		index_in_parent);
	fc->is_signed = bt_field_class_type_is(ir_fc_type,
		BT_FIELD_CLASS_TYPE_SIGNED_INTEGER);
}

static inline
void _fs_sink_ctf_named_field_class_init(
		struct fs_sink_ctf_named_field_class *named_fc)
{
	BT_ASSERT(named_fc);
	named_fc->name = g_string_new(NULL);
	BT_ASSERT(named_fc->name);
}

static inline
void _fs_sink_ctf_named_field_class_fini(
		struct fs_sink_ctf_named_field_class *named_fc)
{
	BT_ASSERT(named_fc);

	if (named_fc->name) {
		g_string_free(named_fc->name, TRUE);
		named_fc->name = NULL;
	}

	fs_sink_ctf_field_class_destroy(named_fc->fc);
	named_fc->fc = NULL;
}

static inline
struct fs_sink_ctf_field_class_bit_array *
fs_sink_ctf_field_class_bit_array_create(
		const bt_field_class *ir_fc, uint64_t index_in_parent)
{
	struct fs_sink_ctf_field_class_bit_array *fc =
		g_new0(struct fs_sink_ctf_field_class_bit_array, 1);

	BT_ASSERT(fc);
	_fs_sink_ctf_field_class_bit_array_init((void *) fc,
		FS_SINK_CTF_FIELD_CLASS_TYPE_BIT_ARRAY, ir_fc,
		(unsigned int) bt_field_class_bit_array_get_length(ir_fc),
		index_in_parent);
	return fc;
}

static inline
struct fs_sink_ctf_field_class_bool *fs_sink_ctf_field_class_bool_create(
		const bt_field_class *ir_fc, uint64_t index_in_parent)
{
	struct fs_sink_ctf_field_class_bool *fc =
		g_new0(struct fs_sink_ctf_field_class_bool, 1);

	BT_ASSERT(fc);

	/*
	 * CTF 1.8 has no boolean field class type, so this component
	 * translates it to an 8-bit unsigned integer field class.
	 */
	_fs_sink_ctf_field_class_bit_array_init((void *) fc,
		FS_SINK_CTF_FIELD_CLASS_TYPE_BOOL, ir_fc,
		8, index_in_parent);
	return fc;
}

static inline
struct fs_sink_ctf_field_class_int *fs_sink_ctf_field_class_int_create(
		const bt_field_class *ir_fc, uint64_t index_in_parent)
{
	struct fs_sink_ctf_field_class_int *fc =
		g_new0(struct fs_sink_ctf_field_class_int, 1);

	BT_ASSERT(fc);
	_fs_sink_ctf_field_class_int_init(fc, FS_SINK_CTF_FIELD_CLASS_TYPE_INT,
		ir_fc, index_in_parent);
	return fc;
}

static inline
struct fs_sink_ctf_field_class_float *fs_sink_ctf_field_class_float_create(
		const bt_field_class *ir_fc, uint64_t index_in_parent)
{
	struct fs_sink_ctf_field_class_float *fc =
		g_new0(struct fs_sink_ctf_field_class_float, 1);

	BT_ASSERT(fc);
	_fs_sink_ctf_field_class_bit_array_init((void *) fc,
		FS_SINK_CTF_FIELD_CLASS_TYPE_FLOAT,
		ir_fc,
		bt_field_class_get_type(ir_fc) ==
			BT_FIELD_CLASS_TYPE_SINGLE_PRECISION_REAL ? 32 : 64,
		index_in_parent);
	return fc;
}

static inline
struct fs_sink_ctf_field_class_string *fs_sink_ctf_field_class_string_create(
		const bt_field_class *ir_fc, uint64_t index_in_parent)
{
	struct fs_sink_ctf_field_class_string *fc =
		g_new0(struct fs_sink_ctf_field_class_string, 1);

	BT_ASSERT(fc);
	_fs_sink_ctf_field_class_init((void *) fc,
		FS_SINK_CTF_FIELD_CLASS_TYPE_STRING, ir_fc,
		8, index_in_parent);
	return fc;
}

static inline
struct fs_sink_ctf_field_class_struct *fs_sink_ctf_field_class_struct_create_empty(
		const bt_field_class *ir_fc, uint64_t index_in_parent)
{
	struct fs_sink_ctf_field_class_struct *fc =
		g_new0(struct fs_sink_ctf_field_class_struct, 1);

	BT_ASSERT(fc);
	_fs_sink_ctf_field_class_init((void *) fc,
		FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT, ir_fc, 1, index_in_parent);
	fc->members = g_array_new(FALSE, TRUE,
		sizeof(struct fs_sink_ctf_named_field_class));
	BT_ASSERT(fc->members);
	return fc;
}

static inline
struct fs_sink_ctf_field_class_option *fs_sink_ctf_field_class_option_create_empty(
		const bt_field_class *ir_fc, uint64_t index_in_parent)
{
	struct fs_sink_ctf_field_class_option *fc =
		g_new0(struct fs_sink_ctf_field_class_option, 1);

	BT_ASSERT(fc);
	_fs_sink_ctf_field_class_init((void *) fc,
		FS_SINK_CTF_FIELD_CLASS_TYPE_OPTION, ir_fc,
		1, index_in_parent);
	fc->tag_ref = g_string_new(NULL);
	BT_ASSERT(fc->tag_ref);
	return fc;
}

static inline
struct fs_sink_ctf_field_class_variant *fs_sink_ctf_field_class_variant_create_empty(
		const bt_field_class *ir_fc, uint64_t index_in_parent)
{
	struct fs_sink_ctf_field_class_variant *fc =
		g_new0(struct fs_sink_ctf_field_class_variant, 1);

	BT_ASSERT(fc);
	_fs_sink_ctf_field_class_init((void *) fc,
		FS_SINK_CTF_FIELD_CLASS_TYPE_VARIANT, ir_fc,
		1, index_in_parent);
	fc->options = g_array_new(FALSE, TRUE,
		sizeof(struct fs_sink_ctf_named_field_class));
	BT_ASSERT(fc->options);
	fc->tag_ref = g_string_new(NULL);
	BT_ASSERT(fc->tag_ref);
	fc->tag_is_before =
		bt_field_class_get_type(fc->base.ir_fc) ==
		BT_FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD;
	return fc;
}

static inline
struct fs_sink_ctf_field_class_array *fs_sink_ctf_field_class_array_create_empty(
		const bt_field_class *ir_fc, uint64_t index_in_parent)
{
	struct fs_sink_ctf_field_class_array *fc =
		g_new0(struct fs_sink_ctf_field_class_array, 1);

	BT_ASSERT(fc);
	_fs_sink_ctf_field_class_init((void *) fc,
		FS_SINK_CTF_FIELD_CLASS_TYPE_ARRAY, ir_fc,
		1, index_in_parent);
	fc->length = bt_field_class_array_static_get_length(ir_fc);
	return fc;
}

static inline
struct fs_sink_ctf_field_class_sequence *fs_sink_ctf_field_class_sequence_create_empty(
		const bt_field_class *ir_fc, uint64_t index_in_parent)
{
	struct fs_sink_ctf_field_class_sequence *fc =
		g_new0(struct fs_sink_ctf_field_class_sequence, 1);

	BT_ASSERT(fc);
	_fs_sink_ctf_field_class_init((void *) fc,
		FS_SINK_CTF_FIELD_CLASS_TYPE_SEQUENCE,
		ir_fc, 1, index_in_parent);
	fc->length_ref = g_string_new(NULL);
	BT_ASSERT(fc->length_ref);
	fc->length_is_before =
		bt_field_class_get_type(ir_fc) ==
			BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITHOUT_LENGTH_FIELD;
	return fc;
}

static inline
struct fs_sink_ctf_named_field_class *
fs_sink_ctf_field_class_struct_borrow_member_by_index(
		struct fs_sink_ctf_field_class_struct *fc, uint64_t index);

static inline
struct fs_sink_ctf_named_field_class *
fs_sink_ctf_field_class_variant_borrow_option_by_index(
		struct fs_sink_ctf_field_class_variant *fc, uint64_t index);

static inline
void _fs_sink_ctf_field_class_fini(struct fs_sink_ctf_field_class *fc)
{
	BT_ASSERT(fc);
}

static inline
void _fs_sink_ctf_field_class_bit_array_destroy(
		struct fs_sink_ctf_field_class_int *fc)
{
	BT_ASSERT(fc);
	_fs_sink_ctf_field_class_fini((void *) fc);
	g_free(fc);
}

static inline
void _fs_sink_ctf_field_class_bool_destroy(
		struct fs_sink_ctf_field_class_int *fc)
{
	BT_ASSERT(fc);
	_fs_sink_ctf_field_class_fini((void *) fc);
	g_free(fc);
}

static inline
void _fs_sink_ctf_field_class_int_destroy(
		struct fs_sink_ctf_field_class_int *fc)
{
	BT_ASSERT(fc);
	_fs_sink_ctf_field_class_fini((void *) fc);
	g_free(fc);
}

static inline
void _fs_sink_ctf_field_class_float_destroy(
		struct fs_sink_ctf_field_class_float *fc)
{
	BT_ASSERT(fc);
	_fs_sink_ctf_field_class_fini((void *) fc);
	g_free(fc);
}

static inline
void _fs_sink_ctf_field_class_string_destroy(
		struct fs_sink_ctf_field_class_string *fc)
{
	BT_ASSERT(fc);
	_fs_sink_ctf_field_class_fini((void *) fc);
	g_free(fc);
}

static inline
void _fs_sink_ctf_field_class_struct_destroy(
		struct fs_sink_ctf_field_class_struct *fc)
{
	BT_ASSERT(fc);
	_fs_sink_ctf_field_class_fini((void *) fc);

	if (fc->members) {
		uint64_t i;

		for (i = 0; i < fc->members->len; i++) {
			struct fs_sink_ctf_named_field_class *named_fc =
				fs_sink_ctf_field_class_struct_borrow_member_by_index(
					fc, i);

			_fs_sink_ctf_named_field_class_fini(named_fc);
		}

		g_array_free(fc->members, TRUE);
		fc->members = NULL;
	}

	g_free(fc);
}

static inline
void _fs_sink_ctf_field_class_array_base_fini(
		struct fs_sink_ctf_field_class_array_base *fc)
{
	BT_ASSERT(fc);
	_fs_sink_ctf_field_class_fini((void *) fc);
	fs_sink_ctf_field_class_destroy(fc->elem_fc);
	fc->elem_fc = NULL;
}

static inline
void _fs_sink_ctf_field_class_array_destroy(
		struct fs_sink_ctf_field_class_array *fc)
{
	BT_ASSERT(fc);
	_fs_sink_ctf_field_class_array_base_fini((void *) fc);
	g_free(fc);
}

static inline
void _fs_sink_ctf_field_class_sequence_destroy(
		struct fs_sink_ctf_field_class_sequence *fc)
{
	BT_ASSERT(fc);
	_fs_sink_ctf_field_class_array_base_fini((void *) fc);

	if (fc->length_ref) {
		g_string_free(fc->length_ref, TRUE);
		fc->length_ref = NULL;
	}

	g_free(fc);
}

static inline
void _fs_sink_ctf_field_class_option_destroy(
		struct fs_sink_ctf_field_class_option *fc)
{
	BT_ASSERT(fc);
	_fs_sink_ctf_field_class_fini((void *) fc);
	fs_sink_ctf_field_class_destroy(fc->content_fc);

	if (fc->tag_ref) {
		g_string_free(fc->tag_ref, TRUE);
		fc->tag_ref = NULL;
	}

	g_free(fc);
}

static inline
void _fs_sink_ctf_field_class_variant_destroy(
		struct fs_sink_ctf_field_class_variant *fc)
{
	BT_ASSERT(fc);
	_fs_sink_ctf_field_class_fini((void *) fc);

	if (fc->options) {
		uint64_t i;

		for (i = 0; i < fc->options->len; i++) {
			struct fs_sink_ctf_named_field_class *named_fc =
				fs_sink_ctf_field_class_variant_borrow_option_by_index(
					fc, i);

			_fs_sink_ctf_named_field_class_fini(named_fc);
		}

		g_array_free(fc->options, TRUE);
		fc->options = NULL;
	}

	if (fc->tag_ref) {
		g_string_free(fc->tag_ref, TRUE);
		fc->tag_ref = NULL;
	}

	g_free(fc);
}

static inline
void fs_sink_ctf_field_class_destroy(struct fs_sink_ctf_field_class *fc)
{
	if (!fc) {
		return;
	}

	switch (fc->type) {
	case FS_SINK_CTF_FIELD_CLASS_TYPE_BOOL:
		_fs_sink_ctf_field_class_bool_destroy((void *) fc);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_BIT_ARRAY:
		_fs_sink_ctf_field_class_bit_array_destroy((void *) fc);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_INT:
		_fs_sink_ctf_field_class_int_destroy((void *) fc);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_FLOAT:
		_fs_sink_ctf_field_class_float_destroy((void *) fc);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_STRING:
		_fs_sink_ctf_field_class_string_destroy((void *) fc);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT:
		_fs_sink_ctf_field_class_struct_destroy((void *) fc);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_ARRAY:
		_fs_sink_ctf_field_class_array_destroy((void *) fc);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_SEQUENCE:
		_fs_sink_ctf_field_class_sequence_destroy((void *) fc);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_OPTION:
		_fs_sink_ctf_field_class_option_destroy((void *) fc);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_VARIANT:
		_fs_sink_ctf_field_class_variant_destroy((void *) fc);
		break;
	default:
		bt_common_abort();
	}
}

static inline
struct fs_sink_ctf_named_field_class *
fs_sink_ctf_field_class_struct_borrow_member_by_index(
		struct fs_sink_ctf_field_class_struct *fc, uint64_t index)
{
	BT_ASSERT_DBG(fc);
	BT_ASSERT_DBG(index < fc->members->len);
	return &g_array_index(fc->members, struct fs_sink_ctf_named_field_class,
		index);
}

static inline
struct fs_sink_ctf_named_field_class *
fs_sink_ctf_field_class_struct_borrow_member_by_name(
		struct fs_sink_ctf_field_class_struct *fc, const char *name)
{
	uint64_t i;
	struct fs_sink_ctf_named_field_class *ret_named_fc = NULL;

	BT_ASSERT_DBG(fc);
	BT_ASSERT_DBG(name);

	for (i = 0; i < fc->members->len; i++) {
		struct fs_sink_ctf_named_field_class *named_fc =
			fs_sink_ctf_field_class_struct_borrow_member_by_index(
				fc, i);

		if (strcmp(name, named_fc->name->str) == 0) {
			ret_named_fc = named_fc;
			goto end;
		}
	}

end:
	return ret_named_fc;
}

static inline
struct fs_sink_ctf_field_class *
fs_sink_ctf_field_class_struct_borrow_member_field_class_by_name(
		struct fs_sink_ctf_field_class_struct *struct_fc, const char *name)
{
	struct fs_sink_ctf_named_field_class *named_fc = NULL;
	struct fs_sink_ctf_field_class *fc = NULL;

	if (!struct_fc) {
		goto end;
	}

	named_fc = fs_sink_ctf_field_class_struct_borrow_member_by_name(
		struct_fc, name);
	if (!named_fc) {
		goto end;
	}

	fc = named_fc->fc;

end:
	return fc;
}

static inline
struct fs_sink_ctf_field_class_int *
fs_sink_ctf_field_class_struct_borrow_member_int_field_class_by_name(
		struct fs_sink_ctf_field_class_struct *struct_fc,
		const char *name)
{
	struct fs_sink_ctf_field_class_int *int_fc = NULL;

	int_fc = (void *)
		fs_sink_ctf_field_class_struct_borrow_member_field_class_by_name(
			struct_fc, name);
	if (!int_fc) {
		goto end;
	}

	if (int_fc->base.base.type != FS_SINK_CTF_FIELD_CLASS_TYPE_INT) {
		int_fc = NULL;
		goto end;
	}

end:
	return int_fc;
}

static inline
void fs_sink_ctf_field_class_struct_align_at_least(
		struct fs_sink_ctf_field_class_struct *fc,
		unsigned int alignment)
{
	if (alignment > fc->base.alignment) {
		fc->base.alignment = alignment;
	}
}

static inline
void fs_sink_ctf_field_class_struct_append_member(
		struct fs_sink_ctf_field_class_struct *fc,
		const char *name, struct fs_sink_ctf_field_class *member_fc)
{
	struct fs_sink_ctf_named_field_class *named_fc;

	BT_ASSERT(fc);
	BT_ASSERT(name);
	g_array_set_size(fc->members, fc->members->len + 1);

	named_fc = &g_array_index(fc->members,
		struct fs_sink_ctf_named_field_class, fc->members->len - 1);
	_fs_sink_ctf_named_field_class_init(named_fc);
	g_string_assign(named_fc->name, name);
	named_fc->fc = member_fc;
	fs_sink_ctf_field_class_struct_align_at_least(fc, member_fc->alignment);
}

static inline
struct fs_sink_ctf_named_field_class *
fs_sink_ctf_field_class_variant_borrow_option_by_index(
		struct fs_sink_ctf_field_class_variant *fc, uint64_t index)
{
	BT_ASSERT_DBG(fc);
	BT_ASSERT_DBG(index < fc->options->len);
	return &g_array_index(fc->options, struct fs_sink_ctf_named_field_class,
		index);
}

static inline
struct fs_sink_ctf_named_field_class *
fs_sink_ctf_field_class_variant_borrow_option_by_name(
		struct fs_sink_ctf_field_class_variant *fc, const char *name)
{
	uint64_t i;
	struct fs_sink_ctf_named_field_class *ret_named_fc = NULL;

	BT_ASSERT_DBG(fc);
	BT_ASSERT_DBG(name);

	for (i = 0; i < fc->options->len; i++) {
		struct fs_sink_ctf_named_field_class *named_fc =
			fs_sink_ctf_field_class_variant_borrow_option_by_index(
				fc, i);

		if (strcmp(name, named_fc->name->str) == 0) {
			ret_named_fc = named_fc;
			goto end;
		}
	}

end:
	return ret_named_fc;
}

static inline
void fs_sink_ctf_field_class_variant_append_option(
		struct fs_sink_ctf_field_class_variant *fc,
		const char *name, struct fs_sink_ctf_field_class *option_fc)
{
	struct fs_sink_ctf_named_field_class *named_fc;

	BT_ASSERT(fc);
	BT_ASSERT(name);
	g_array_set_size(fc->options, fc->options->len + 1);

	named_fc = &g_array_index(fc->options,
		struct fs_sink_ctf_named_field_class, fc->options->len - 1);
	_fs_sink_ctf_named_field_class_init(named_fc);
	g_string_assign(named_fc->name, name);
	named_fc->fc = option_fc;
}

static inline
struct fs_sink_ctf_event_class *fs_sink_ctf_event_class_create(
		struct fs_sink_ctf_stream_class *sc,
		const bt_event_class *ir_ec)
{
	struct fs_sink_ctf_event_class *ec =
		g_new0(struct fs_sink_ctf_event_class, 1);

	BT_ASSERT(sc);
	BT_ASSERT(ir_ec);
	BT_ASSERT(ec);
	ec->ir_ec = ir_ec;
	ec->sc = sc;
	g_ptr_array_add(sc->event_classes, ec);
	g_hash_table_insert(sc->event_classes_from_ir, (gpointer) ir_ec, ec);
	return ec;
}

static inline
void fs_sink_ctf_event_class_destroy(struct fs_sink_ctf_event_class *ec)
{
	if (!ec) {
		return;
	}

	fs_sink_ctf_field_class_destroy(ec->spec_context_fc);
	ec->spec_context_fc = NULL;
	fs_sink_ctf_field_class_destroy(ec->payload_fc);
	ec->payload_fc = NULL;
	g_free(ec);
}

static inline
struct fs_sink_ctf_stream_class *fs_sink_ctf_stream_class_create(
		struct fs_sink_ctf_trace *trace,
		const bt_stream_class *ir_sc)
{
	struct fs_sink_ctf_stream_class *sc =
		g_new0(struct fs_sink_ctf_stream_class, 1);

	BT_ASSERT(trace);
	BT_ASSERT(ir_sc);
	BT_ASSERT(sc);
	sc->trace = trace;
	sc->ir_sc = ir_sc;
	sc->default_clock_class =
		bt_stream_class_borrow_default_clock_class_const(ir_sc);
	sc->default_clock_class_name = g_string_new(NULL);
	BT_ASSERT(sc->default_clock_class_name);
	sc->event_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify) fs_sink_ctf_event_class_destroy);
	BT_ASSERT(sc->event_classes);
	sc->event_classes_from_ir = g_hash_table_new(g_direct_hash,
		g_direct_equal);
	BT_ASSERT(sc->event_classes_from_ir);
	sc->has_packets = bt_stream_class_supports_packets(ir_sc);
	sc->packets_have_ts_begin =
		bt_stream_class_packets_have_beginning_default_clock_snapshot(
			ir_sc);
	sc->packets_have_ts_end =
		bt_stream_class_packets_have_end_default_clock_snapshot(ir_sc);
	sc->has_discarded_events =
		bt_stream_class_supports_discarded_events(ir_sc);

	if (sc->has_discarded_events) {
		sc->discarded_events_has_ts =
			bt_stream_class_discarded_events_have_default_clock_snapshots(
				ir_sc);
	}

	if (bt_stream_class_supports_discarded_packets(ir_sc)) {
		sc->discarded_packets_has_ts =
			bt_stream_class_discarded_packets_have_default_clock_snapshots(
				ir_sc);
	}

	g_ptr_array_add(trace->stream_classes, sc);
	return sc;
}

static inline
void fs_sink_ctf_stream_class_destroy(struct fs_sink_ctf_stream_class *sc)
{
	if (!sc) {
		return;
	}

	if (sc->default_clock_class_name) {
		g_string_free(sc->default_clock_class_name, TRUE);
		sc->default_clock_class_name = NULL;
	}

	if (sc->event_classes) {
		g_ptr_array_free(sc->event_classes, TRUE);
		sc->event_classes = NULL;
	}

	if (sc->event_classes_from_ir) {
		g_hash_table_destroy(sc->event_classes_from_ir);
		sc->event_classes_from_ir = NULL;
	}

	fs_sink_ctf_field_class_destroy(sc->packet_context_fc);
	sc->packet_context_fc = NULL;
	fs_sink_ctf_field_class_destroy(sc->event_common_context_fc);
	sc->event_common_context_fc = NULL;
	g_free(sc);
}

static inline
void fs_sink_ctf_stream_class_append_event_class(
		struct fs_sink_ctf_stream_class *sc,
		struct fs_sink_ctf_event_class *ec)
{
	g_ptr_array_add(sc->event_classes, ec);
}

static inline
void fs_sink_ctf_trace_destroy(struct fs_sink_ctf_trace *trace)
{
	if (!trace) {
		return;
	}

	if (trace->stream_classes) {
		g_ptr_array_free(trace->stream_classes, TRUE);
		trace->stream_classes = NULL;
	}

	g_free(trace);
}

static inline
struct fs_sink_ctf_trace *fs_sink_ctf_trace_create(const bt_trace *ir_trace)
{
	struct fs_sink_ctf_trace *trace =
		g_new0(struct fs_sink_ctf_trace, 1);

	BT_ASSERT(trace);

	bt_uuid_generate(trace->uuid);

	trace->ir_trace = ir_trace;
	trace->ir_tc = bt_trace_borrow_class_const(ir_trace);
	trace->stream_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify) fs_sink_ctf_stream_class_destroy);
	BT_ASSERT(trace->stream_classes);

	return trace;
}

#endif /* BABELTRACE_PLUGIN_CTF_FS_SINK_FS_SINK_CTF_META_H */
