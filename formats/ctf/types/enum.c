/*
 * Common Trace Format
 *
 * Enumeration mapping strings (quarks) from/to integers.
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

#include <babeltrace/ctf/types.h>
#include <stdint.h>
#include <glib.h>

void ctf_enum_read(struct stream_pos *ppos, struct definition *definition)
{
	struct definition_enum *enum_definition =
		container_of(definition, struct definition_enum, p);
	const struct declaration_enum *enum_declaration =
		enum_definition->declaration;
	struct definition_integer *integer_definition =
		enum_definition->integer;
	const struct declaration_integer *integer_declaration =
		integer_definition->declaration;
	GArray *qs;

	ctf_integer_read(ppos, &integer_definition->p);
	if (!integer_declaration->signedness)
		qs = enum_uint_to_quark_set(enum_declaration,
			integer_definition->value._unsigned);
	else
		qs = enum_int_to_quark_set(enum_declaration,
			integer_definition->value._signed);
	assert(qs);
	/* unref previous quark set */
	if (enum_definition->value)
		g_array_unref(enum_definition->value);
	enum_definition->value = qs;
}

void ctf_enum_write(struct stream_pos *pos, struct definition *definition)
{
	struct definition_enum *enum_definition =
		container_of(definition, struct definition_enum, p);
	struct definition_integer *integer_definition =
		enum_definition->integer;

	ctf_integer_write(pos, &integer_definition->p);
}
