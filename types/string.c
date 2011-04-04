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
struct definition *_string_definition_new(struct type *type,
				struct definition_scope *parent_scope);
static
void _string_definition_free(struct definition *definition);

void string_copy(struct stream_pos *dest, const struct format *fdest, 
		 struct stream_pos *src, const struct format *fsrc,
		 struct definition *definition)
{
	struct definition_string *string =
		container_of(definition, struct definition_string, p);
	struct type_string *string_type = string->type;

	if (fsrc->string_copy == fdest->string_copy) {
		fsrc->string_copy(dest, src, string_type);
	} else {
		char *tmp = NULL;

		fsrc->string_read(&tmp, src, string_type);
		fdest->string_write(dest, tmp, string_type);
		fsrc->string_free_temp(tmp);
	}
}

static
void _string_type_free(struct type *type)
{
	struct type_string *string_type =
		container_of(type, struct type_string, p);
	g_free(string_type);
}

struct type_string *string_type_new(const char *name)
{
	struct type_string *string_type;

	string_type = g_new(struct type_string, 1);
	string_type->p.id = CTF_TYPE_STRING;
	string_type->p.name = g_quark_from_string(name);
	string_type->p.alignment = CHAR_BIT;
	string_type->p.copy = string_copy;
	string_type->p.type_free = _string_type_free;
	string_type->p.definition_new = _string_definition_new;
	string_type->p.definition_free = _string_definition_free;
	string_type->p.ref = 1;
	return string_type;
}

static
struct definition *
	_string_definition_new(struct type *type,
			       struct definition_scope *parent_scope)
{
	struct type_string *string_type =
		container_of(type, struct type_string, p);
	struct definition_string *string;

	string = g_new(struct definition_string, 1);
	type_ref(&string_type->p);
	string->p.type = type;
	string->type = string_type;
	string->p.ref = 1;
	string->value = NULL;
	return &string->p;
}

static
void _string_definition_free(struct definition *definition)
{
	struct definition_string *string =
		container_of(definition, struct definition_string, p);

	type_unref(string->p.type);
	g_free(string->value);
	g_free(string);
}
