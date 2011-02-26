/*
 * string.c
 *
 * BabelTrace - String Type Converter
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
#include <babeltrace/align.h>
#include <babeltrace/format.h>

static
struct type_string *_string_type_new(struct type_class *type_class,
				     struct declaration_scope *parent_scope);
static
void _string_type_free(struct type *type);

void string_copy(struct stream_pos *dest, const struct format *fdest, 
		 struct stream_pos *src, const struct format *fsrc,
		 struct type *type)
{
	struct type_string *string = container_of(type, struct type_string, p);
	struct type_class_string *string_class = string->_class;

	if (fsrc->string_copy == fdest->string_copy) {
		fsrc->string_copy(dest, src, string_class);
	} else {
		char *tmp = NULL;

		fsrc->string_read(&tmp, src, string_class);
		fdest->string_write(dest, tmp, string_class);
		fsrc->string_free_temp(tmp);
	}
}

static
void _string_type_class_free(struct type_class_string *string_class)
{
	struct type_class_string *string_class =
		container_of(type_class, struct type_class_string, p);
	g_free(string_class);
}

struct type_class_string *
string_type_class_new(const char *name)
{
	struct type_class_string *string_class;
	int ret;

	string_class = g_new(struct type_class_string, 1);
	string_class->p.name = g_quark_from_string(name);
	string_class->p.alignment = CHAR_BIT;
	string_class->p.copy = string_copy;
	string_class->p.class_free = _string_type_class_free;
	string_class->p.type_new = _string_type_new;
	string_class->p.type_free = _string_type_free;
	string_class->p.ref = 1;
	if (string_class->p.name) {
		ret = register_type(&string_class->p);
		if (ret) {
			g_free(string_class);
			return NULL;
		}
	}
	return string_class;
}

static
struct type_string *_string_type_new(struct type_class *type_class,
				     struct declaration_scope *parent_scope)
{
	struct type_class_string *string_class =
		container_of(type_class, struct type_class_string, p);
	struct type_string *string;

	string = g_new(struct type_string, 1);
	type_class_ref(&string_class->p);
	string->p._class = string_class;
	string->p.ref = 1;
	string->value = NULL;
	return &string->p;
}

static
void _string_type_free(struct type *type)
{
	struct type_string *string =
		container_of(type, struct type_string, p);

	type_class_unref(string->p._class);
	g_free(string->value);
	g_free(string);
}
