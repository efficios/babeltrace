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
struct declaration *_struct_declaration_new(struct type *type,
				struct declaration_scope *parent_scope);
static
void _struct_declaration_free(struct declaration *declaration);

void struct_copy(struct stream_pos *dest, const struct format *fdest, 
		 struct stream_pos *src, const struct format *fsrc,
		 struct declaration *declaration)
{
	struct declaration_struct *_struct =
		container_of(declaration, struct declaration_struct, p);
	struct type_struct *struct_type = _struct->type;
	unsigned long i;

	fsrc->struct_begin(src, struct_type);
	fdest->struct_begin(dest, struct_type);

	for (i = 0; i < _struct->fields->len; i++) {
		struct field *field = &g_array_index(_struct->fields,
						     struct field, i);
		struct type *field_type = field->declaration->type;

		field_type->copy(dest, fdest, src, fsrc, field->declaration);

	}
	fsrc->struct_end(src, struct_type);
	fdest->struct_end(dest, struct_type);
}

static
void _struct_type_free(struct type *type)
{
	struct type_struct *struct_type =
		container_of(type, struct type_struct, p);
	unsigned long i;

	free_type_scope(struct_type->scope);
	g_hash_table_destroy(struct_type->fields_by_name);

	for (i = 0; i < struct_type->fields->len; i++) {
		struct type_field *type_field =
			&g_array_index(struct_type->fields,
				       struct type_field, i);
		type_unref(type_field->type);
	}
	g_array_free(struct_type->fields, true);
	g_free(struct_type);
}

struct type_struct *struct_type_new(const char *name,
				    struct type_scope *parent_scope)
{
	struct type_struct *struct_type;
	struct type *type;

	struct_type = g_new(struct type_struct, 1);
	type = &struct_type->p;
	struct_type->fields_by_name = g_hash_table_new(g_direct_hash,
						       g_direct_equal);
	struct_type->fields = g_array_sized_new(FALSE, TRUE,
						sizeof(struct type_field),
						DEFAULT_NR_STRUCT_FIELDS);
	struct_type->scope = new_type_scope(parent_scope);
	type->name = g_quark_from_string(name);
	type->alignment = 1;
	type->copy = struct_copy;
	type->type_free = _struct_type_free;
	type->declaration_new = _struct_declaration_new;
	type->declaration_free = _struct_declaration_free;
	type->ref = 1;
	return struct_type;
}

static
struct declaration *
	_struct_declaration_new(struct type *type,
				struct declaration_scope *parent_scope)
{
	struct type_struct *struct_type =
		container_of(type, struct type_struct, p);
	struct declaration_struct *_struct;
	unsigned long i;
	int ret;

	_struct = g_new(struct declaration_struct, 1);
	type_ref(&struct_type->p);
	_struct->p.type = type;
	_struct->type = struct_type;
	_struct->p.ref = 1;
	_struct->scope = new_declaration_scope(parent_scope);
	_struct->fields = g_array_sized_new(FALSE, TRUE,
					    sizeof(struct field),
					    DEFAULT_NR_STRUCT_FIELDS);
	g_array_set_size(_struct->fields, struct_type->fields->len);
	for (i = 0; i < struct_type->fields->len; i++) {
		struct type_field *type_field =
			&g_array_index(struct_type->fields,
				       struct type_field, i);
		struct field *field = &g_array_index(_struct->fields,
						     struct field, i);

		field->name = type_field->name;
		field->declaration =
			type_field->type->declaration_new(type_field->type,
							  _struct->scope);
		ret = register_declaration(field->name,
					   field->declaration, _struct->scope);
		assert(!ret);
	}
	return &_struct->p;
}

static
void _struct_declaration_free(struct declaration *declaration)
{
	struct declaration_struct *_struct =
		container_of(declaration, struct declaration_struct, p);
	unsigned long i;

	assert(_struct->fields->len == _struct->type->fields->len);
	for (i = 0; i < _struct->fields->len; i++) {
		struct field *field = &g_array_index(_struct->fields,
						     struct field, i);
		declaration_unref(field->declaration);
	}
	free_declaration_scope(_struct->scope);
	type_unref(_struct->p.type);
	g_free(_struct);
}

void struct_type_add_field(struct type_struct *struct_type,
			   const char *field_name,
			   struct type *field_type)
{
	struct type_field *field;
	unsigned long index;

	g_array_set_size(struct_type->fields, struct_type->fields->len + 1);
	index = struct_type->fields->len - 1;	/* last field (new) */
	field = &g_array_index(struct_type->fields, struct type_field, index);
	field->name = g_quark_from_string(field_name);
	type_ref(field_type);
	field->type = field_type;
	/* Keep index in hash rather than pointer, because array can relocate */
	g_hash_table_insert(struct_type->fields_by_name,
			    (gpointer) (unsigned long) field->name,
			    (gpointer) index);
	/*
	 * Alignment of structure is the max alignment of types contained
	 * therein.
	 */
	struct_type->p.alignment = max(struct_type->p.alignment,
				       field_type->alignment);
}

unsigned long
	struct_type_lookup_field_index(struct type_struct *struct_type,
				       GQuark field_name)
{
	unsigned long index;

	index = (unsigned long) g_hash_table_lookup(struct_type->fields_by_name,
						    (gconstpointer) (unsigned long) field_name);
	return index;
}

/*
 * field returned only valid as long as the field structure is not appended to.
 */
struct type_field *
	struct_type_get_field_from_index(struct type_struct *struct_type,
					 unsigned long index)
{
	return &g_array_index(struct_type->fields, struct type_field, index);
}

/*
 * field returned only valid as long as the field structure is not appended to.
 */
struct field *
struct_declaration_get_field_from_index(struct declaration_struct *_struct,
					unsigned long index)
{
	return &g_array_index(_struct->fields, struct field, index);
}
