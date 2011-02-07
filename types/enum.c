/*
 * enum.c
 *
 * BabelTrace - Enumeration Type
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
#include <babeltrace/format.h>
#include <stdint.h>
#include <glib.h>

#if (__WORDSIZE == 32)
GQuark enum_uint_to_quark(const struct type_class_enum *enum_class, uint64_t v)
{
	gconstpointer q = g_hash_table_lookup(enum_class->table.value_to_quark,
					      &v);
	return (GQuark) (unsigned long) q;
}

GQuark enum_int_to_quark(const struct type_class_enum *enum_class, uint64_t v)
{
	gconstpointer q = g_hash_table_lookup(enum_class->table.value_to_quark,
					      &v);
	return (GQuark) (unsigned long) q;
}

uint64_t enum_quark_to_uint(const struct type_class_enum *enum_class,
			    GQuark q)
{
	gconstpointer v = g_hash_table_lookup(enum_class->table.quark_to_value,
					      (gconstpointer) q);
	return *(const uint64_t *) v;
}

int64_t enum_quark_to_int(const struct type_class_enum *enum_class,
			  GQuark q)
{
	gconstpointer v = g_hash_table_lookup(enum_class->table.quark_to_value,
					      (gconstpointer) q);
	return *(const int64_t *) v;
}

guint enum_val_hash(gconstpointer key)
{
	int64_t ukey = *(const int64_t *)key;

	return (guint)ukey ^ (guint)(ukey >> 32);
}

gboolean enum_val_equal(gconstpointer a, gconstpointer b)
{
	int64_t ua = *(const int64_t *)a;
	int64_t ub = *(const int64_t *)b;

	return ua == ub;
}

void enum_val_free(void *ptr)
{
	g_free(ptr);
}

void enum_signed_insert(struct type_class_enum *enum_class, int64_t v, GQuark q)
{
	int64_t *valuep = g_new(int64_t, 1);

	g_hash_table_insert(enum_class->table.value_to_quark, valuep,
			    (gpointer) (unsigned long) q);
	g_hash_table_insert(enum_class->table.quark_to_value,
			    (gpointer) (unsigned long) q,
			    valuep);
}

void enum_unsigned_insert(struct type_class_enum *enum_class, uint64_t v, GQuark q)
{
	uint64_t *valuep = g_new(uint64_t, 1);

	g_hash_table_insert(enum_class->table.value_to_quark, valuep,
			    (gpointer) (unsigned long) q);
	g_hash_table_insert(enum_class->table.quark_to_value,
			    (gpointer) (unsigned long) q,
			    valuep);
}
#else	/* __WORDSIZE != 32 */
GQuark enum_uint_to_quark(const struct type_class_enum *enum_class, uint64_t v)
{
	gconstpointer q = g_hash_table_lookup(enum_class->table.value_to_quark,
					      (gconstpointer) v);
	return (GQuark) (unsigned long) q;
}

GQuark enum_int_to_quark(const struct type_class_enum *enum_class, uint64_t v)
{
	gconstpointer q = g_hash_table_lookup(enum_class->table.value_to_quark,
					      (gconstpointer) v);
	return (GQuark) (unsigned long) q;
}

uint64_t enum_quark_to_uint(const struct type_class_enum *enum_class,
			    GQuark q)
{
	gconstpointer v = g_hash_table_lookup(enum_class->table.quark_to_value,
					      (gconstpointer) (unsigned long) q);
	return *(const uint64_t *) v;
}

int64_t enum_quark_to_int(const struct type_class_enum *enum_class,
			  GQuark q)
{
	gconstpointer v = g_hash_table_lookup(enum_class->table.quark_to_value,
					      (gconstpointer) (unsigned long) q);
	return *(const int64_t *) v;
}

guint enum_val_hash(gconstpointer key)
{
	return g_direct_hash(key);
}

gboolean enum_val_equal(gconstpointer a, gconstpointer b)
{
	return g_direct_equal(a, b);
}

void enum_val_free(void *ptr)
{
}

void enum_signed_insert(struct type_class_enum *enum_class,
                        int64_t v, GQuark q)
{
	g_hash_table_insert(enum_class->table.value_to_quark, (gpointer) v,
			    (gpointer) (unsigned long) q);
	g_hash_table_insert(enum_class->table.quark_to_value,
			    (gpointer) (unsigned long) q,
			    (gpointer) v);
}

void enum_unsigned_insert(struct type_class_enum *enum_class,
			  uint64_t v, GQuark q)
{
	g_hash_table_insert(enum_class->table.value_to_quark, (gpointer) v,
			    (gpointer) (unsigned long) q);
	g_hash_table_insert(enum_class->table.quark_to_value,
			    (gpointer) (unsigned long) q,
			    (gpointer) v);
}
#endif	/* __WORDSIZE != 32 */

void enum_copy(struct stream_pos *dest, const struct format *fdest, 
	       struct stream_pos *src, const struct format *fsrc,
	       const struct type_class *type_class)
{
	struct type_class_enum *enum_class =
		container_of(type_class, struct type_class_enum, p.p);
	struct type_class_integer *int_class = &enum_class->p;
	GQuark v;

	v = fsrc->enum_read(src, enum_class);
	return fdest->enum_write(dest, enum_class, v);
}

void enum_type_free(struct type_class_enum *enum_class)
{
	g_hash_table_destroy(enum_class->table.value_to_quark);
	g_hash_table_destroy(enum_class->table.quark_to_value);
	g_free(enum_class);
}

static
void _enum_type_free(struct type_class *type_class)
{
	struct type_class_enum *enum_class =
		container_of(type_class, struct type_class_enum, p.p);
	enum_type_free(enum_class);
}

struct type_class_enum *enum_type_new(const char *name,
				      size_t len, int byte_order,
				      int signedness,
				      size_t alignment)
{
	struct type_class_enum *enum_class;
	struct type_class_integer *int_class;
	int ret;

	enum_class = g_new(struct type_class_enum, 1);
	enum_class->table.value_to_quark = g_hash_table_new(enum_val_hash,
							    enum_val_equal);
	enum_class->table.quark_to_value = g_hash_table_new_full(g_direct_hash,
							g_direct_equal,
							NULL, enum_val_free);
	int_class = &enum_class->p;
	int_class->p.name = g_quark_from_string(name);
	int_class->p.alignment = alignment;
	int_class->p.copy = enum_copy;
	int_class->p.free = _enum_type_free;
	int_class->p.ref = 1;
	int_class->len = len;
	int_class->byte_order = byte_order;
	int_class->signedness = signedness;
	if (int_class->p.name) {
		ret = register_type(&int_class->p);
		if (ret) {
			g_hash_table_destroy(enum_class->table.value_to_quark);
			g_hash_table_destroy(enum_class->table.quark_to_value);
			g_free(enum_class);
			return NULL;
		}
	}
	return enum_class;
}
