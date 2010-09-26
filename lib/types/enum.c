/*
 * Common Trace Format
 *
 * Enumeration mapping strings (quarks) from/to integers.
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Dual LGPL v2.1/GPL v2 license.
 */

#include <ctf/ctf-types.h>
#include <stdint.h>
#include <glib.h>

struct enum_table {
	GHashTable *value_to_quark;	/* Tuples (value, GQuark) */
	GHashTable *quark_to_value;	/* Tuples (GQuark, value) */
};

#if (__WORDSIZE == 32)
GQuark enum_uint_to_quark(const struct enum_table *table, uint64_t v)
{
	gconstpointer q = g_hash_table_lookup(table->value_to_quark, &v);
	return (GQuark) (unsigned long) q;
}

GQuark enum_int_to_quark(const struct enum_table *table, uint64_t v)
{
	gconstpointer q = g_hash_table_lookup(table->value_to_quark, &v);
	return (GQuark) (unsigned long) q;
}

uint64_t enum_quark_to_uint(size_t len, int byte_order, GQuark q)
{
	gconstpointer v = g_hash_table_lookup(table->quark_to_value,
					      (gconstpointer) q);
	return *(const uint64_t *) v;
}

int64_t enum_quark_to_int(size_t len, int byte_order, GQuark q)
{
	gconstpointer v = g_hash_table_lookup(table->quark_to_value,
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

void enum_signed_insert(struct enum_table *table, int64_t v, GQuark q)
{
	int64_t *valuep = g_new(int64_t, 1);

	g_hash_table_insert(table->value_to_quark, valuep,
			    (gpointer) (unsigned long) q);
	g_hash_table_insert(table->quark_to_value, (gpointer) (unsigned long) q,
			    valuep);
}

void enum_unsigned_insert(struct enum_table *table, uint64_t v, GQuark q)
{
	uint64_t *valuep = g_new(uint64_t, 1);

	g_hash_table_insert(table->value_to_quark, valuep,
			    (gpointer) (unsigned long) q);
	g_hash_table_insert(table->quark_to_value, (gpointer) (unsigned long) q,
			    valuep);
}
#else	/* __WORDSIZE != 32 */
GQuark enum_uint_to_quark(const struct enum_table *table, uint64_t v)
{
	gconstpointer q = g_hash_table_lookup(table->value_to_quark,
					      (gconstpointer) v);
	return (GQuark) (unsigned long) q;
}

GQuark enum_int_to_quark(const struct enum_table *table, uint64_t v)
{
	gconstpointer q = g_hash_table_lookup(table->value_to_quark,
					      (gconstpointer) v);
	return (GQuark) (unsigned long) q;
}

uint64_t enum_quark_to_uint(size_t len, int byte_order, GQuark q)
{
	gconstpointer v = g_hash_table_lookup(table->quark_to_value,
					      (gconstpointer) (unsigned long) q);
	return *(const uint64_t *) v;
}

int64_t enum_quark_to_int(size_t len, int byte_order, GQuark q)
{
	gconstpointer v = g_hash_table_lookup(table->quark_to_value,
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

void enum_signed_insert(struct enum_table *table, int64_t v, GQuark q)
{
	g_hash_table_insert(table->value_to_quark, (gpointer) v,
			    (gpointer) (unsigned long) q);
	g_hash_table_insert(table->quark_to_value, (gpointer) (unsigned long) q,
			    valuep);
}

void enum_unsigned_insert(struct enum_table *table, uint64_t v, GQuark q)
{
	g_hash_table_insert(table->value_to_quark, (gpointer) v,
			    (gpointer) (unsigned long) q);
	g_hash_table_insert(table->quark_to_value, (gpointer) (unsigned long) q,
			    valuep);
}
#endif	/* __WORDSIZE != 32 */

struct enum_table *enum_new(void)
{
	struct enum_table *table;

	table = g_new(struct enum_table, 1);
	table->value_to_quark = g_hash_table_new_full(enum_val_hash,
						      enum_val_equal,
						      enum_val_free, NULL);
	table->quark_to_value = g_hash_table_new(g_direct_hash, g_direct_equal);
}

void enum_destroy(struct enum_table *table)
{
	g_hash_table_destroy(table->value_to_quark);
	g_hash_table_destroy(table->quark_to_value);
	g_free(table);
}
