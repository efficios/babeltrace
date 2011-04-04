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

/*
 * The caller should unref the GArray.
 */
GArray *ctf_enum_read(struct stream_pos *pos,
		      const struct declaration_enum *src)
{
	const struct declaration_integer *integer_declaration = src->integer_declaration;

	if (!integer_declaration->signedness) {
		uint64_t v;

		v = ctf_uint_read(pos, integer_declaration);
		return enum_uint_to_quark_set(src, v);
	} else {
		int64_t v;

		v = ctf_int_read(pos, integer_declaration);
		return enum_int_to_quark_set(src, v);
	}
}

/*
 * Arbitrarily choose the start of the first matching range.
 */
void ctf_enum_write(struct stream_pos *pos,
		    const struct declaration_enum *dest,
		    GQuark q)
{
	const struct declaration_integer *integer_declaration = dest->integer_declaration;
	GArray *array;

	array = enum_quark_to_range_set(dest, q);
	assert(array);

	if (!integer_declaration->signedness) {
		uint64_t v = g_array_index(array, struct enum_range, 0).start._unsigned;
		ctf_uint_write(pos, integer_declaration, v);
	} else {
		int64_t v = g_array_index(array, struct enum_range, 0).start._unsigned;
		ctf_int_write(pos, integer_declaration, v);
	}
}
