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
struct definition *_sequence_definition_new(struct type *type,
					struct definition_scope *parent_scope);
static
void _sequence_definition_free(struct definition *definition);

void sequence_copy(struct stream_pos *dest, const struct format *fdest, 
		   struct stream_pos *src, const struct format *fsrc,
		   struct definition *definition)
{
	struct definition_sequence *sequence =
		container_of(definition, struct definition_sequence, p);
	struct type_sequence *sequence_type = sequence->type;
	uint64_t i;

	fsrc->sequence_begin(src, sequence_type);
	fdest->sequence_begin(dest, sequence_type);

	sequence->len->p.type->copy(dest, fdest, src, fsrc,
				    &sequence->len->p);

	for (i = 0; i < sequence->len->value._unsigned; i++) {
		struct definition *elem =
			sequence->current_element.definition;
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
	type->id = CTF_TYPE_SEQUENCE;
	type->name = g_quark_from_string(name);
	type->alignment = max(len_type->p.alignment, elem_type->alignment);
	type->copy = sequence_copy;
	type->type_free = _sequence_type_free;
	type->definition_new = _sequence_definition_new;
	type->definition_free = _sequence_definition_free;
	type->ref = 1;
	return sequence_type;
}

static
struct definition *_sequence_definition_new(struct type *type,
				struct definition_scope *parent_scope)
{
	struct type_sequence *sequence_type =
		container_of(type, struct type_sequence, p);
	struct definition_sequence *sequence;
	struct definition *len_parent;

	sequence = g_new(struct definition_sequence, 1);
	type_ref(&sequence_type->p);
	sequence->p.type = type;
	sequence->type = sequence_type;
	sequence->p.ref = 1;
	sequence->scope = new_definition_scope(parent_scope);
	len_parent = sequence_type->len_type->p.definition_new(&sequence_type->len_type->p,
								parent_scope);
	sequence->len =
		container_of(len_parent, struct definition_integer, p);
	sequence->current_element.definition =
		sequence_type->elem->definition_new(sequence_type->elem,
						     parent_scope);
	return &sequence->p;
}

static
void _sequence_definition_free(struct definition *definition)
{
	struct definition_sequence *sequence =
		container_of(definition, struct definition_sequence, p);
	struct definition *len_definition = &sequence->len->p;
	struct definition *elem_definition =
		sequence->current_element.definition;

	len_definition->type->definition_free(len_definition);
	elem_definition->type->definition_free(elem_definition);
	free_definition_scope(sequence->scope);
	type_unref(sequence->p.type);
	g_free(sequence);
}
