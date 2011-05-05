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
#include <inttypes.h>

static
struct definition *_array_definition_new(struct declaration *declaration,
			struct definition_scope *parent_scope,
			GQuark field_name, int index);
static
void _array_definition_free(struct definition *definition);

void array_copy(struct stream_pos *dest, const struct format *fdest, 
		struct stream_pos *src, const struct format *fsrc,
		struct definition *definition)
{
	struct definition_array *array =
		container_of(definition, struct definition_array, p);
	struct declaration_array *array_declaration = array->declaration;
	uint64_t i;

	fsrc->array_begin(src, array_declaration);
	if (fdest)
		fdest->array_begin(dest, array_declaration);

	for (i = 0; i < array_declaration->len; i++) {
		struct definition *elem =
			g_array_index(array->elems, struct field, i).definition;
		elem->declaration->copy(dest, fdest, src, fsrc, elem);
	}
	fsrc->array_end(src, array_declaration);
	if (fdest)
		fdest->array_end(dest, array_declaration);
}

static
void _array_declaration_free(struct declaration *declaration)
{
	struct declaration_array *array_declaration =
		container_of(declaration, struct declaration_array, p);

	free_declaration_scope(array_declaration->scope);
	declaration_unref(array_declaration->elem);
	g_free(array_declaration);
}

struct declaration_array *
	array_declaration_new(size_t len,
			      struct declaration *elem_declaration,
			      struct declaration_scope *parent_scope)
{
	struct declaration_array *array_declaration;
	struct declaration *declaration;

	array_declaration = g_new(struct declaration_array, 1);
	declaration = &array_declaration->p;
	array_declaration->len = len;
	declaration_ref(elem_declaration);
	array_declaration->elem = elem_declaration;
	array_declaration->scope = new_declaration_scope(parent_scope);
	declaration->id = CTF_TYPE_ARRAY;
	/* No need to align the array, the first element will align itself */
	declaration->alignment = 1;
	declaration->copy = array_copy;
	declaration->declaration_free = _array_declaration_free;
	declaration->definition_new = _array_definition_new;
	declaration->definition_free = _array_definition_free;
	declaration->ref = 1;
	return array_declaration;
}

static
struct definition *
	_array_definition_new(struct declaration *declaration,
			      struct definition_scope *parent_scope,
			      GQuark field_name, int index)
{
	struct declaration_array *array_declaration =
		container_of(declaration, struct declaration_array, p);
	struct definition_array *array;
	uint64_t i;

	array = g_new(struct definition_array, 1);
	declaration_ref(&array_declaration->p);
	array->p.declaration = declaration;
	array->declaration = array_declaration;
	array->p.ref = 1;
	array->p.index = index;
	array->scope = new_definition_scope(parent_scope, field_name);
	array->elems = g_array_sized_new(FALSE, TRUE, sizeof(struct field),
					 array_declaration->len);
	g_array_set_size(array->elems, array_declaration->len);
	for (i = 0; i < array_declaration->len; i++) {
		struct field *field;
		GString *str;
		GQuark name;

		str = g_string_new("");
		g_string_printf(str, "[%" PRIu64 "]", i);
		name = g_quark_from_string(str->str);
		(void) g_string_free(str, TRUE);

		field = &g_array_index(array->elems, struct field, i);
		field->name = name;
		field->definition = array_declaration->elem->definition_new(array_declaration->elem,
					  array->scope,
					  name, i);
	}
	return &array->p;
}

static
void _array_definition_free(struct definition *definition)
{
	struct definition_array *array =
		container_of(definition, struct definition_array, p);
	uint64_t i;

	for (i = 0; i < array->elems->len; i++) {
		struct field *field;

		field = &g_array_index(array->elems, struct field, i);
		field->definition->declaration->definition_free(field->definition);
	}
	(void) g_array_free(array->elems, TRUE);
	free_definition_scope(array->scope);
	declaration_unref(array->p.declaration);
	g_free(array);
}

uint64_t array_len(struct definition_array *array)
{
	return array->elems->len;
}

struct definition *array_index(struct definition_array *array, uint64_t i)
{
	if (i >= array->elems->len)
		return NULL;
	return g_array_index(array->elems, struct field, i).definition;
}
