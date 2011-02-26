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
struct type *_array_type_new(struct type_class *type_class,
			     struct declaration_scope *parent_scope);
static
void _array_type_free(struct type *type);

void array_copy(struct stream_pos *dest, const struct format *fdest, 
		struct stream_pos *src, const struct format *fsrc,
		struct type *type)
{
	struct type_sequence *array = container_of(type, struct type_array, p);
	struct type_class_array *array_class = array->_class;
	uint64_t i;

	fsrc->array_begin(src, array_class);
	fdest->array_begin(dest, array_class);

	for (i = 0; i < array_class->len; i++) {
		struct type_class *elem_class = array->current_element.type;
		elem_type->p._class->copy(dest, fdest, src, fsrc, elem_type);
	}
	fsrc->array_end(src, array_class);
	fdest->array_end(dest, array_class);
}

static
void _array_type_class_free(struct type_class *type_class)
{
	struct type_class_array *array_class =
		container_of(type_class, struct type_class_array, p);

	type_class_unref(array_class->elem);
	g_free(array_class);
}

struct type_class_array *
array_type_class_new(const char *name, size_t len, struct type_class *elem)
{
	struct type_class_array *array_class;
	struct type_class *type_class;
	int ret;

	array_class = g_new(struct type_class_array, 1);
	type_class = &array_class->p;
	array_class->len = len;
	type_ref(elem);
	array_class->elem = elem;
	type_class->name = g_quark_from_string(name);
	/* No need to align the array, the first element will align itself */
	type_class->alignment = 1;
	type_class->copy = array_copy;
	type_class->class_free = _array_type_class_free;
	type_class->type_new = _array_type_new;
	type_class->type_free = _array_type_free;
	type_class->ref = 1;

	if (type_class->name) {
		ret = register_type(type_class);
		if (ret)
			goto error_register;
	}
	return array_class;

error_register:
	type_class_unref(array_class->elem);
	g_free(array_class);
	return NULL;
}

static
struct type_array *_array_type_new(struct type_class *type_class,
				   struct declaration_scope *parent_scope)
{
	struct type_class_array *array_class =
		container_of(type_class, struct type_class_array, p);
	struct type_array *array;

	array = g_new(struct type_array, 1);
	type_class_ref(&array_class->p);
	array->p._class = array_class;
	array->p.ref = 1;
	array->scope = new_declaration_scope(parent_scope);
	array->current_element.type =
		array_class->elem.p->type_new(&array_class->elem.p,
						 parent_scope);
	return &array->p;
}

static
void _array_type_free(struct type *type)
{
	struct type_array *array =
		container_of(type, struct type_array, p);
	struct type *elem_type = array->current_element.type;

	elem_type->p._class->type_free(elem_type);
	free_declaration_scope(array->scope);
	type_class_unref(array->p._class);
	g_free(array);
}
