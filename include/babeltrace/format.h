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
#include <glib.h>

struct format {
	GQuark name;

	uint64_t (*uint_read)(struct stream_pos *pos,
			const struct type_integer *integer_type);
	int64_t (*int_read)(struct stream_pos *pos,
			const struct type_integer *integer_type);
	void (*uint_write)(struct stream_pos *pos,
			   const struct type_integer *integer_type,
			   uint64_t v);
	void (*int_write)(struct stream_pos *pos,
			  const struct type_integer *integer_type,
			  int64_t v);

	void (*float_copy)(struct stream_pos *destp,
			   struct stream_pos *srcp,
			   const struct type_float *float_type);
	double (*double_read)(struct stream_pos *pos,
			      const struct type_float *float_type);
	void (*double_write)(struct stream_pos *pos,
			     const struct type_float *float_type,
			     double v);

	void (*string_copy)(struct stream_pos *dest, struct stream_pos *src,
			    const struct type_string *string_type);
	void (*string_read)(char **dest, struct stream_pos *src,
			    const struct type_string *string_type);
	void (*string_write)(struct stream_pos *dest, const char *src,
			     const struct type_string *string_type);
	void (*string_free_temp)(char *string);

	/*
	 * enum_read returns a GArray of GQuark. Must be released with
	 * g_array_unref().
	 */
	GArray *(*enum_read)(struct stream_pos *pos,
			    const struct type_enum *src);
	void (*enum_write)(struct stream_pos *pos,
			   const struct type_enum *dest,
			   GQuark q);
	void (*struct_begin)(struct stream_pos *pos,
			     const struct type_struct *struct_type);
	void (*struct_end)(struct stream_pos *pos,
			   const struct type_struct *struct_type);
	void (*variant_begin)(struct stream_pos *pos,
			      const struct type_variant *variant_type);
	void (*variant_end)(struct stream_pos *pos,
			    const struct type_variant *variant_type);
	void (*array_begin)(struct stream_pos *pos,
			     const struct type_array *array_type);
	void (*array_end)(struct stream_pos *pos,
			   const struct type_array *array_type);
	void (*sequence_begin)(struct stream_pos *pos,
			     const struct type_sequence *sequence_type);
	void (*sequence_end)(struct stream_pos *pos,
			   const struct type_sequence *sequence_type);
};

struct format *bt_lookup_format(GQuark qname);
int bt_register_format(struct format *format);

/* TBD: format unregistration */

#endif /* _BABELTRACE_FORMAT_H */
