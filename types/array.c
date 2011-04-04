/*
 * array.c
 *
 * BabelTrace - Array Type Converter
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

static
struct definition *_array_definition_new(struct type *type,
			struct definition_scope *parent_scope);
static
void _array_definition_free(struct definition *definition);

void array_copy(struct stream_pos *dest, const struct format *fdest, 
		struct stream_pos *src, const struct format *fsrc,
		struct definition *definition)
{
	struct definition_array *array =
		container_of(definition, struct definition_array, p);
	struct type_array *array_type = array->type;
	uint64_t i;

	fsrc->array_begin(src, array_type);
	fdest->array_begin(dest, array_type);

	for (i = 0; i < array_type->len; i++) {
		struct definition *elem = array->current_element.definition;
		elem->type->copy(dest, fdest, src, fsrc, elem);
	}
	fsrc->array_end(src, array_type);
	fdest->array_end(dest, array_type);
}

static
void _array_type_free(struct type *type)
{
	struct type_array *array_type =
		container_of(type, struct type_array, p);

	free_type_scope(array_type->scope);
	type_unref(array_type->elem);
	g_free(array_type);
}

struct type_array *
	array_type_new(const char *name, size_t len, struct type *elem_type,
		       struct type_scope *parent_scope)
{
	struct type_array *array_type;
	struct type *type;

	array_type = g_new(struct type_array, 1);
	type = &array_type->p;
	array_type->len = len;
	type_ref(elem_type);
	array_type->elem = elem_type;
	array_type->scope = new_type_scope(parent_scope);
	type->id = CTF_TYPE_ARRAY;
	type->name = g_quark_from_string(name);
	/* No need to align the array, the first element will align itself */
	type->alignment = 1;
	type->copy = array_copy;
	type->type_free = _array_type_free;
	type->definition_new = _array_definition_new;
	type->definition_free = _array_definition_free;
	type->ref = 1;
	return array_type;
}

static
struct definition *
	_array_definition_new(struct type *type,
			      struct definition_scope *parent_scope)
{
	struct type_array *array_type =
		container_of(type, struct type_array, p);
	struct definition_array *array;

	array = g_new(struct definition_array, 1);
	type_ref(&array_type->p);
	array->p.type = type;
	array->type = array_type;
	array->p.ref = 1;
	array->scope = new_definition_scope(parent_scope);
	array->current_element.definition =
		array_type->elem->definition_new(array_type->elem,
						  parent_scope);
	return &array->p;
}

static
void _array_definition_free(struct definition *definition)
{
	struct definition_array *array =
		container_of(definition, struct definition_array, p);
	struct definition *elem_definition =
		array->current_element.definition;

	elem_definition->type->definition_free(elem_definition);
	free_definition_scope(array->scope);
	type_unref(array->p.type);
	g_free(array);
}
