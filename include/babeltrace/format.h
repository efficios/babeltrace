#ifndef _BABELTRACE_FORMAT_H
#define _BABELTRACE_FORMAT_H

/*
 * BabelTrace
 *
 * Trace Format Header
 *
 * Copyright (c) 2010 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdint.h>
#include <glib.h>

struct format {
	GQuark name;

	uint64_t (*uint_read)(struct stream_pos *pos,
			const struct type_class_integer *int_class);
	int64_t (*int_read)(struct stream_pos *pos,
			const struct type_class_integer *int_class);
	void (*uint_write)(struct stream_pos *pos,
			   const struct type_class_integer *int_class,
			   uint64_t v);
	void (*int_write)(struct stream_pos *pos,
			  const struct type_class_integer *int_class,
			  int64_t v);

	void (*float_copy)(struct stream_pos *dest,
			   struct stream_pos *src,
			   const struct type_class_float *src);
	double (*double_read)(struct stream_pos *pos,
			      const struct type_class_float *float_class);
	void (*double_write)(struct stream_pos *pos,
			     const struct type_class_float *float_class,
			     double v);

	void (*string_copy)(struct stream_pos *dest, struct stream_pos *src,
			    const struct type_class_string *string_class);
	void (*string_read)(unsigned char **dest, struct stream_pos *src,
			    const struct type_class_string *string_class);
	void (*string_write)(struct stream_pos *dest, const unsigned char *src,
			     const struct type_class_string *string_class);
	void (*string_free_temp)(unsigned char *string);

	GQuark (*enum_read)(struct stream_pos *pos,
			    const struct type_class_enum *src);
	void (*enum_write)(struct stream_pos *pos,
			   const struct type_class_enum *dest,
			   GQuark q);

};

struct format *bt_lookup_format(GQuark qname);
int bt_register_format(const struct format *format);

/* TBD: format unregistration */

#endif /* _BABELTRACE_FORMAT_H */
