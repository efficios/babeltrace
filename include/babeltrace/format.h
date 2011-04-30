#ifndef _BABELTRACE_FORMAT_H
#define _BABELTRACE_FORMAT_H

/*
 * BabelTrace
 *
 * Trace Format Header
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

#include <babeltrace/types.h>
#include <stdint.h>
#include <stdio.h>
#include <glib.h>

struct trace_descriptor;

struct format {
	GQuark name;

	uint64_t (*uint_read)(struct stream_pos *pos,
		const struct declaration_integer *integer_declaration);
	int64_t (*int_read)(struct stream_pos *pos,
		const struct declaration_integer *integer_declaration);
	void (*uint_write)(struct stream_pos *pos,
		const struct declaration_integer *integer_declaration,
		uint64_t v);
	void (*int_write)(struct stream_pos *pos,
		const struct declaration_integer *integer_declaration,
		int64_t v);

	void (*float_copy)(struct stream_pos *destp,
		struct stream_pos *srcp,
		const struct declaration_float *float_declaration);
	double (*double_read)(struct stream_pos *pos,
		const struct declaration_float *float_declaration);
	void (*double_write)(struct stream_pos *pos,
		const struct declaration_float *float_declaration,
		double v);

	void (*string_copy)(struct stream_pos *dest, struct stream_pos *src,
		const struct declaration_string *string_declaration);
	void (*string_read)(char **dest, struct stream_pos *src,
		const struct declaration_string *string_declaration);
	void (*string_write)(struct stream_pos *dest, const char *src,
		const struct declaration_string *string_declaration);
	void (*string_free_temp)(char *string);

	/*
	 * enum_read returns a GArray of GQuark. Must be released with
	 * g_array_unref().
	 */
	GArray *(*enum_read)(struct stream_pos *pos,
		const struct declaration_enum *src);
	void (*enum_write)(struct stream_pos *pos,
		const struct declaration_enum *dest,
		GQuark q);
	void (*struct_begin)(struct stream_pos *pos,
		const struct declaration_struct *struct_declaration);
	void (*struct_end)(struct stream_pos *pos,
		const struct declaration_struct *struct_declaration);
	void (*variant_begin)(struct stream_pos *pos,
		const struct declaration_variant *variant_declaration);
	void (*variant_end)(struct stream_pos *pos,
		const struct declaration_variant *variant_declaration);
	void (*array_begin)(struct stream_pos *pos,
		const struct declaration_array *array_declaration);
	void (*array_end)(struct stream_pos *pos,
		const struct declaration_array *array_declaration);
	void (*sequence_begin)(struct stream_pos *pos,
		const struct declaration_sequence *sequence_declaration);
	void (*sequence_end)(struct stream_pos *pos,
		const struct declaration_sequence *sequence_declaration);
	struct trace_descriptor *(*open_trace)(const char *path, int flags);
	void (*close_trace)(struct trace_descriptor *descriptor);
};

struct format *bt_lookup_format(GQuark qname);
void bt_fprintf_format_list(FILE *fp);
int bt_register_format(struct format *format);

/* TBD: format unregistration */

#endif /* _BABELTRACE_FORMAT_H */
