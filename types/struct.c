/*
 * BabelTrace - Structure Type Converter
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

#include <babeltrace/compiler.h>
#include <babeltrace/types.h>

void struct_copy(struct stream_pos *dest, const struct format *fdest, 
		 struct stream_pos *src, const struct format *fsrc,
		 const struct type_class *type_class)
{
	struct type_class_struct *struct_class =
		container_of(type_class, struct type_class_struct, p);
	unsigned int i;

	fsrc->struct_begin(src, struct_class);
	fdest->struct_begin(dest, struct_class);

	for (i = 0; i < struct_class->fields->len; i++) {
		struct field *field = &g_array_index(struct_class->fields,
						     struct field, i);
		struct type_class *field_class = field->type_class;

		field_class->copy(dest, fdest, src, fsrc, &field_class->p);

	}
	fsrc->struct_end(src, struct_class);
	fdest->struct_end(dest, struct_class);
}

void struct_type_free(struct type_class_struct *struct_class)
{
	g_hash_table_destroy(struct_class->fields_by_name);
	g_array_free(struct_class->fields, true);
	g_free(struct_class);
}

static void _struct_type_free(struct type_class *type_class)
{
	struct type_class_struct *struct_class =
		container_of(type_class, struct type_class_struct, p);
	struct_type_free(struct_class);
}

struct type_class_struct *struct_type_new(const char *name)
{
	struct type_class_struct *struct_class;
	int ret;

	struct_class = g_new(struct type_class_struct, 1);
	type_class = &float_class->p;

	struct_class->fields_by_name = g_hash_table_new(g_direct_hash,
							g_direct_equal);
	struct_class->fields = g_array_sized_new(false, false,
						 sizeof(struct field),
						 DEFAULT_NR_STRUCT_FIELDS)
	type_class->name = g_quark_from_string(name);
	type_class->alignment = 1;
	type_class->copy = struct_copy;
	type_class->free = _struct_type_free;

	if (type_class->name) {
		ret = ctf_register_type(type_class);
		if (ret)
			goto error_register;
	}
	return struct_class;

error_register:
	g_free(struct_class);
	return NULL;
}

void struct_type_add_field(struct type_class_struct *struct_class,
			   const char *field_name,
			   struct type_class *type_class)
{
	struct field *field;
	unsigned long index;

	g_array_set_size(struct_class->fields, struct_class->fields->len + 1);
	index = struct_class->fields->len - 1;	/* last field (new) */
	field = &g_array_index(struct_class->fields, struct field, index);
	field->name = g_quark_from_string(field_name);
	field->type_class = type_class;
	/* Keep index in hash rather than pointer, because array can relocate */
	g_hash_table_insert(struct_class->fields_by_name,
			    (gpointer) (unsigned long) field->name,
			    (gpointer) index);
	/*
	 * Alignment of structure is the max alignment of types contained
	 * therein.
	 */
	struct_class->p.alignment = max(struct_class->p.alignment,
					type_class->alignment);
}

unsigned long
struct_type_lookup_field_index(struct type_class_struct *struct_class,
			       GQuark field_name)
{
	unsigned long index;

	index = (unsigned long) g_hash_table_lookup(struct_class->fields_by_name,
						    field_name);
	return index;
}

/*
 * field returned only valid as long as the field structure is not appended to.
 */
struct field *
struct_type_get_field_from_index(struct type_class_struct *struct_class,
				 unsigned long index)
{
	return &g_array_index(struct_class->fields, struct field, index);
}
