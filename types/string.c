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
struct definition *_string_definition_new(struct declaration *declaration,
				struct definition_scope *parent_scope);
static
void _string_definition_free(struct definition *definition);

void string_copy(struct stream_pos *dest, const struct format *fdest, 
		 struct stream_pos *src, const struct format *fsrc,
		 struct definition *definition)
{
	struct definition_string *string =
		container_of(definition, struct definition_string, p);
	struct declaration_string *string_declaration = string->declaration;

	if (fsrc->string_copy == fdest->string_copy) {
		fsrc->string_copy(dest, src, string_declaration);
	} else {
		char *tmp = NULL;

		fsrc->string_read(&tmp, src, string_declaration);
		fdest->string_write(dest, tmp, string_declaration);
		fsrc->string_free_temp(tmp);
	}
}

static
void _string_declaration_free(struct declaration *declaration)
{
	struct declaration_string *string_declaration =
		container_of(declaration, struct declaration_string, p);
	g_free(string_declaration);
}

struct declaration_string *string_declaration_new(const char *name)
{
	struct declaration_string *string_declaration;

	string_declaration = g_new(struct declaration_string, 1);
	string_declaration->p.id = CTF_TYPE_STRING;
	string_declaration->p.name = g_quark_from_string(name);
	string_declaration->p.alignment = CHAR_BIT;
	string_declaration->p.copy = string_copy;
	string_declaration->p.declaration_free = _string_declaration_free;
	string_declaration->p.definition_new = _string_definition_new;
	string_declaration->p.definition_free = _string_definition_free;
	string_declaration->p.ref = 1;
	return string_declaration;
}

static
struct definition *
	_string_definition_new(struct declaration *declaration,
			       struct definition_scope *parent_scope)
{
	struct declaration_string *string_declaration =
		container_of(declaration, struct declaration_string, p);
	struct definition_string *string;

	string = g_new(struct definition_string, 1);
	declaration_ref(&string_declaration->p);
	string->p.declaration = declaration;
	string->declaration = string_declaration;
	string->p.ref = 1;
	string->value = NULL;
	return &string->p;
}

static
void _string_definition_free(struct definition *definition)
{
	struct definition_string *string =
		container_of(definition, struct definition_string, p);

	declaration_unref(string->p.declaration);
	g_free(string->value);
	g_free(string);
}
