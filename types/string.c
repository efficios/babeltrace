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
				struct definition_scope *parent_scope,
				GQuark field_name, int index);
static
void _string_definition_free(struct definition *definition);

static
void _string_declaration_free(struct declaration *declaration)
{
	struct declaration_string *string_declaration =
		container_of(declaration, struct declaration_string, p);
	g_free(string_declaration);
}

struct declaration_string *
	string_declaration_new(enum ctf_string_encoding encoding)
{
	struct declaration_string *string_declaration;

	string_declaration = g_new(struct declaration_string, 1);
	string_declaration->p.id = CTF_TYPE_STRING;
	string_declaration->p.alignment = CHAR_BIT;
	string_declaration->p.declaration_free = _string_declaration_free;
	string_declaration->p.definition_new = _string_definition_new;
	string_declaration->p.definition_free = _string_definition_free;
	string_declaration->p.ref = 1;
	string_declaration->encoding = encoding;
	return string_declaration;
}

static
struct definition *
	_string_definition_new(struct declaration *declaration,
			       struct definition_scope *parent_scope,
			       GQuark field_name, int index)
{
	struct declaration_string *string_declaration =
		container_of(declaration, struct declaration_string, p);
	struct definition_string *string;

	string = g_new(struct definition_string, 1);
	declaration_ref(&string_declaration->p);
	string->p.declaration = declaration;
	string->declaration = string_declaration;
	string->p.ref = 1;
	string->p.index = index;
	string->p.name = field_name;
	string->value = NULL;
	string->len = 0;
	string->alloc_len = 0;
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
