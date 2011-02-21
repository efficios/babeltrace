/*
 * string.c
 *
 * BabelTrace - String Type Converter
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

#include <babeltrace/compiler.h>
#include <babeltrace/align.h>
#include <babeltrace/format.h>

void string_copy(struct stream_pos *dest, const struct format *fdest, 
		 struct stream_pos *src, const struct format *fsrc,
		 const struct type_class *type_class)
{
	struct type_class_string *string_class =
		container_of(type_class, struct type_class_string, p);

	if (fsrc->string_copy == fdest->string_copy) {
		fsrc->string_copy(dest, src, string_class);
	} else {
		unsigned char *tmp = NULL;

		fsrc->string_read(&tmp, src, string_class);
		fdest->string_write(dest, tmp, string_class);
		fsrc->string_free_temp(tmp);
	}
}

void string_type_free(struct type_class_string *string_class)
{
	g_free(string_class);
}

static void _string_type_free(struct type_class *type_class)
{
	struct type_class_string *string_class =
		container_of(type_class, struct type_class_string, p);
	string_type_free(string_class);
}

struct type_class_string *string_type_new(const char *name)
{
	struct type_class_string *string_class;
	int ret;

	string_class = g_new(struct type_class_string, 1);
	string_class->p.name = g_quark_from_string(name);
	string_class->p.alignment = CHAR_BIT;
	string_class->p.copy = string_copy;
	string_class->p.free = _string_type_free;
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
