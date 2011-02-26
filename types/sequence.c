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
struct type *_sequence_type_new(struct type_class *type_class,
				struct declaration_scope *parent_scope);
static
void _sequence_type_free(struct type *type);

void sequence_copy(struct stream_pos *dest, const struct format *fdest, 
		   struct stream_pos *src, const struct format *fsrc,
		   struct type *type)
{
	struct type_sequence *sequence =
		container_of(type, struct type_sequence, p);
	struct type_class_sequence *sequence_class = sequence->_class;
	uint64_t i;

	fsrc->sequence_begin(src, sequence);
	fdest->sequence_begin(dest, sequence);

	sequence->len->p._class->copy(dest, fdest, src, fsrc,
				      &sequence->len->p);

	for (i = 0; i < sequence->len->value._unsigned; i++) {
		struct type *elem_type = sequence->current_element.type;
		elem_type->p._class->copy(dest, fdest, src, fsrc, elem_type);
	}
	fsrc->sequence_end(src, sequence);
	fdest->sequence_end(dest, sequence);
}

static
void _sequence_type_class_free(struct type_class *type_class)
{
	struct type_class_sequence *sequence_class =
		container_of(type_class, struct type_class_sequence, p);

	type_class_unref(&sequence_class->len_class->p);
	type_class_unref(sequence_class->elem);
	g_free(sequence_class);
}

struct type_class_sequence *
sequence_type_class_new(const char *name, struct type_class_integer *len_class,
			struct type_class *elem)
{
	struct type_class_sequence *sequence_class;
	struct type_class *type_class;
	int ret;

	sequence_class = g_new(struct type_class_sequence, 1);
	type_class = &sequence_class->p;
	assert(!len_class->signedness);
	type_ref(&len_class->p);
	sequence_class->len_class = len_class;
	type_ref(elem);
	sequence_class->elem = elem;
	type_class->name = g_quark_from_string(name);
	type_class->alignment = max(len_class->p.alignment,
				    elem->alignment);
	type_class->copy = sequence_copy;
	type_class->class_free = _sequence_type_class_free;
	type_class->type_new = _sequence_type_new;
	type_class->type_free = _sequence_type_free;
	type_class->ref = 1;

	if (type_class->name) {
		ret = register_type(type_class);
		if (ret)
			goto error_register;
	}
	return sequence_class;

error_register:
	type_class_unref(&len_class->p);
	type_class_unref(elem);
	g_free(sequence_class);
	return NULL;
}

static
struct type *_sequence_type_new(struct type_class *type_class,
				struct declaration_scope *parent_scope)
{
	struct type_class_sequence *sequence_class =
		container_of(type_class, struct type_class_sequence, p);
	struct type_sequence *sequence;

	sequence = g_new(struct type_sequence, 1);
	type_class_ref(&sequence_class->p);
	sequence->p._class = sequence_class;
	sequence->p.ref = 1;
	sequence->scope = new_declaration_scope(parent_scope);
	sequence->len.type =
		sequence_class->len_class.p->type_new(&sequence_class->len_class.p,
						      parent_scope);
	sequence->current_element.type =
		sequence_class->elem.p->type_new(&sequence_class->elem.p,
						 parent_scope);
	return &sequence->p;
}

static
void _sequence_type_free(struct type *type)
{
	struct type_sequence *sequence =
		container_of(type, struct type_sequence, p);
	struct type *len_type = sequence->len.type;
	struct type *elem_type = sequence->current_element.type;

	len_type->p._class->type_free(len_type);
	elem_type->p._class->type_free(elem_type);
	free_declaration_scope(sequence->scope);
	type_class_unref(sequence->p._class);
	g_free(sequence);
}
