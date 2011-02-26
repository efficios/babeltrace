/*
 * struct.c
 *
 * BabelTrace - Structure Type Converter
 *
 * Copyright 2010, 2011 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <babeltrace/compiler.h>
#include <babeltrace/format.h>

#ifndef max
#define max(a, b)	((a) < (b) ? (b) : (a))
#endif

static
struct type *_struct_type_new(struct type_class *type_class,
			     struct declaration_scope *parent_scope);
static
void _struct_type_free(struct type *type);

void struct_copy(struct stream_pos *dest, const struct format *fdest, 
		 struct stream_pos *src, const struct format *fsrc,
		 struct type *type)
{
	struct type_struct *_struct = container_of(type, struct type_struct, p);
	struct type_class_struct *struct_class = _struct->_class;
	unsigned long i;

	fsrc->struct_begin(src, struct_class);
	fdest->struct_begin(dest, struct_class);

	for (i = 0; i < _struct->fields->len; i++) {
		struct field *field = &g_array_index(_struct->fields,
						     struct field, i);
		struct type_class *field_class = field->type->p._class;

		field_class->copy(dest, fdest, src, fsrc, &field->type->p);

	}
	fsrc->struct_end(src, struct_class);
	fdest->struct_end(dest, struct_class);
}

static
void _struct_type_class_free(struct type_class *type_class)
{
	struct type_class_struct *struct_class =
		container_of(type_class, struct type_class_struct, p);
	unsigned long i;

	g_hash_table_destroy(struct_class->fields_by_name);

	for (i = 0; i < struct_class->fields->len; i++) {
		struct field *type_class_field =
			&g_array_index(struct_class->fields,
				       struct type_class_field, i);
		type_class_unref(field->type_class);
	}
	g_array_free(struct_class->fields, true);
	g_free(struct_class);
}

struct type_class_struct *
struct_type_class_new(const char *name)
{
	struct type_class_struct *struct_class;
	struct type_class *type_class;
	int ret;

	struct_class = g_new(struct type_class_struct, 1);
	type_class = &struct_class->p;
	struct_class->fields_by_name = g_hash_table_new(g_direct_hash,
							g_direct_equal);
	struct_class->fields = g_array_sized_new(FALSE, TRUE,
						 sizeof(struct type_class_field),
						 DEFAULT_NR_STRUCT_FIELDS);
	type_class->name = g_quark_from_string(name);
	type_class->alignment = 1;
	type_class->copy = struct_copy;
	type_class->class_free = _struct_type_class_free;
	type_class->type_new = _struct_type_new;
	type_class->type_free = _struct_type_free;
	type_class->ref = 1;

	if (type_class->name) {
		ret = register_type(type_class);
		if (ret)
			goto error_register;
	}
	return struct_class;

error_register:
	g_hash_table_destroy(struct_class->fields_by_name);
	g_array_free(struct_class->fields, true);
	g_free(struct_class);
	return NULL;
}

static
struct type_struct *_struct_type_new(struct type_class *type_class,
				     struct declaration_scope *parent_scope)
{
	struct type_class_struct *_struct_class =
		container_of(type_class, struct type_class_struct, p);
	struct type_struct *_struct;

	_struct = g_new(struct type_struct, 1);
	type_class_ref(&_struct_class->p);
	_struct->p._class = _struct_class;
	_struct->p.ref = 1;
	_struct->scope = new_declaration_scope(parent_scope);
	_struct->fields = g_array_sized_new(FALSE, TRUE,
					    sizeof(struct field),
					    DEFAULT_NR_STRUCT_FIELDS);
	return &_struct->p;
}

static
void _struct_type_free(struct type *type)
{
	struct type_struct *_struct = container_of(type, struct type_struct, p);
	unsigned long i;

	for (i = 0; i < _struct->fields->len; i++) {
		struct field *field = &g_array_index(_struct->fields,
						     struct field, i);
		type_unref(field->type);
	}
	free_declaration_scope(_struct->scope);
	type_class_unref(_struct->p._class);
	g_free(_struct);
}

void struct_type_class_add_field(struct type_class_struct *struct_class,
				 const char *field_name,
				 struct type_class *type_class)
{
	struct type_class_field *field;
	unsigned long index;

	g_array_set_size(struct_class->fields, struct_class->fields->len + 1);
	index = struct_class->fields->len - 1;	/* last field (new) */
	field = &g_array_index(struct_class->fields, struct type_class_field, index);
	field->name = g_quark_from_string(field_name);
	type_ref(type_class);
	field->type_class = type_class;
	/* Keep index in hash rather than pointer, because array can relocate */
	g_hash_table_insert(struct_class->fields_by_name,
			    (gpointer) (unsigned long) field->name,
			    (gpointer) index);
	/*
	 * Alignment of structure is the max alignment of types contained
	 * therein.
	 */
	struct_class->p.alignment = max(struct_class->p.alignment,
					type_class->alignment);
}

unsigned long
struct_type_class_lookup_field_index(struct type_class_struct *struct_class,
				     GQuark field_name)
{
	unsigned long index;

	index = (unsigned long) g_hash_table_lookup(struct_class->fields_by_name,
						    (gconstpointer) (unsigned long) field_name);
	return index;
}

/*
 * field returned only valid as long as the field structure is not appended to.
 */
struct type_class_field *
struct_type_class_get_field_from_index(struct type_class_struct *struct_class,
				       unsigned long index)
{
	return &g_array_index(struct_class->fields, struct type_class_field, index);
}

/*
 * field returned only valid as long as the field structure is not appended to.
 */
struct field *
struct_type_get_field_from_index(struct type_struct *struct_type,
				 unsigned long index)
{
	return &g_array_index(struct_type->fields, struct field, index);
}
