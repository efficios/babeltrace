/*
 * sequence.c
 *
 * BabelTrace - Sequence Type Converter
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
struct declaration *_sequence_declaration_new(struct type *type,
					struct declaration_scope *parent_scope);
static
void _sequence_declaration_free(struct declaration *declaration);

void sequence_copy(struct stream_pos *dest, const struct format *fdest, 
		   struct stream_pos *src, const struct format *fsrc,
		   struct declaration *declaration)
{
	struct declaration_sequence *sequence =
		container_of(declaration, struct declaration_sequence, p);
	struct type_sequence *sequence_type = sequence->type;
	uint64_t i;

	fsrc->sequence_begin(src, sequence_type);
	fdest->sequence_begin(dest, sequence_type);

	sequence->len->p.type->copy(dest, fdest, src, fsrc,
				    &sequence->len->p);

	for (i = 0; i < sequence->len->value._unsigned; i++) {
		struct declaration *elem =
			sequence->current_element.declaration;
		elem->type->copy(dest, fdest, src, fsrc, elem);
	}
	fsrc->sequence_end(src, sequence_type);
	fdest->sequence_end(dest, sequence_type);
}

static
void _sequence_type_free(struct type *type)
{
	struct type_sequence *sequence_type =
		container_of(type, struct type_sequence, p);

	free_type_scope(sequence_type->scope);
	type_unref(&sequence_type->len_type->p);
	type_unref(sequence_type->elem);
	g_free(sequence_type);
}

struct type_sequence *
	sequence_type_new(const char *name, struct type_integer *len_type,
			  struct type *elem_type,
			  struct type_scope *parent_scope)
{
	struct type_sequence *sequence_type;
	struct type *type;

	sequence_type = g_new(struct type_sequence, 1);
	type = &sequence_type->p;
	assert(!len_type->signedness);
	type_ref(&len_type->p);
	sequence_type->len_type = len_type;
	type_ref(elem_type);
	sequence_type->elem = elem_type;
	sequence_type->scope = new_type_scope(parent_scope);
	type->name = g_quark_from_string(name);
	type->alignment = max(len_type->p.alignment, elem_type->alignment);
	type->copy = sequence_copy;
	type->type_free = _sequence_type_free;
	type->declaration_new = _sequence_declaration_new;
	type->declaration_free = _sequence_declaration_free;
	type->ref = 1;
	return sequence_type;
}

static
struct declaration *_sequence_declaration_new(struct type *type,
				struct declaration_scope *parent_scope)
{
	struct type_sequence *sequence_type =
		container_of(type, struct type_sequence, p);
	struct declaration_sequence *sequence;
	struct declaration *len_parent;

	sequence = g_new(struct declaration_sequence, 1);
	type_ref(&sequence_type->p);
	sequence->p.type = type;
	sequence->type = sequence_type;
	sequence->p.ref = 1;
	sequence->scope = new_declaration_scope(parent_scope);
	len_parent = sequence_type->len_type->p.declaration_new(&sequence_type->len_type->p,
								parent_scope);
	sequence->len =
		container_of(len_parent, struct declaration_integer, p);
	sequence->current_element.declaration =
		sequence_type->elem->declaration_new(sequence_type->elem,
						     parent_scope);
	return &sequence->p;
}

static
void _sequence_declaration_free(struct declaration *declaration)
{
	struct declaration_sequence *sequence =
		container_of(declaration, struct declaration_sequence, p);
	struct declaration *len_declaration = &sequence->len->p;
	struct declaration *elem_declaration =
		sequence->current_element.declaration;

	len_declaration->type->declaration_free(len_declaration);
	elem_declaration->type->declaration_free(elem_declaration);
	free_declaration_scope(sequence->scope);
	type_unref(sequence->p.type);
	g_free(sequence);
}
