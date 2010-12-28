/*
 * Common Trace Format
 *
 * Enumeration mapping strings (quarks) from/to integers.
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

GQuark ctf_enum_read(struct stream_pos *pos,
		     const struct type_class_enum *src)
{
	struct type_class_integer *int_class = &src->p;

	if (!int_class->signedness) {
		uint64_t v;

		v = ctf_uint_read(pos, int_class);
		return enum_uint_to_quark(src, v);
	} else {
		int64_t v;

		v = fsrc->ctf_int_read(pos, int_class);
		return enum_int_to_quark(src, v);
	}
}

size_t ctf_enum_write(struct stream_pos *pos,
		      const struct type_class_enum *dest,
		      GQuark q)
{
	struct type_class_integer *int_class = &dest->p;

	if (!int_class->signedness) {
		uint64_t v;

		v = enum_quark_to_uint(dest, q);
		return ctf_uint_write(pos, int_class, v);
	} else {
		int64_t v;

		v = enum_quark_to_int(dest, q);
		return ctf_int_write(pos, int_class, v);
	}
}
