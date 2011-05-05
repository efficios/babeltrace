/*
 * enum.c
 *
 * BabelTrace - Enumeration Type
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

#include <babeltrace/compiler.h>
#include <babeltrace/format.h>
#include <stdint.h>
#include <glib.h>

static
struct definition *_enum_definition_new(struct declaration *declaration,
					struct definition_scope *parent_scope,
					GQuark field_name, int index);
static
void _enum_definition_free(struct definition *definition);

static
void enum_range_set_free(void *ptr)
{
	g_array_unref(ptr);
}

/*
 * Returns a GArray or NULL.
 * Caller must release the GArray with g_array_unref().
 */
GArray *enum_uint_to_quark_set(const struct declaration_enum *enum_declaration,
			       uint64_t v)
{
	struct enum_range_to_quark *iter;
	GArray *qs, *ranges = NULL;

	/* Single values lookup */
	qs = g_hash_table_lookup(enum_declaration->table.value_to_quark_set, &v);

	/* Range lookup */
	cds_list_for_each_entry(iter, &enum_declaration->table.range_to_quark, node) {
		if (iter->range.start._unsigned > v || iter->range.end._unsigned < v)
			continue;
		if (!ranges) {
			size_t qs_len = 0;

			if (qs)
				qs_len = qs->len;
			ranges = g_array_sized_new(FALSE, TRUE,
					sizeof(struct enum_range),
					qs_len + 1);
			g_array_set_size(ranges, qs_len + 1);
			if (qs)
				memcpy(ranges->data, qs->data,
				       sizeof(struct enum_range) * qs_len);
			g_array_index(ranges, struct enum_range, qs_len) = iter->range;
		} else {
			g_array_set_size(ranges, ranges->len + 1);
			g_array_index(ranges, struct enum_range, ranges->len) = iter->range;
		}
	}
	if (!ranges) {
		ranges = qs;
		g_array_ref(ranges);
	}
	return ranges;
}

/*
 * Returns a GArray or NULL.
 * Caller must release the GArray with g_array_unref().
 */
GArray *enum_int_to_quark_set(const struct declaration_enum *enum_declaration,
			      uint64_t v)
{
	struct enum_range_to_quark *iter;
	GArray *qs, *ranges = NULL;

	/* Single values lookup */
	qs = g_hash_table_lookup(enum_declaration->table.value_to_quark_set, &v);

	/* Range lookup */
	cds_list_for_each_entry(iter, &enum_declaration->table.range_to_quark, node) {
		if (iter->range.start._signed > v || iter->range.end._signed < v)
			continue;
		if (!ranges) {
			size_t qs_len = 0;

			if (qs)
				qs_len = qs->len;
			ranges = g_array_sized_new(FALSE, TRUE,
					sizeof(struct enum_range),
					qs_len + 1);
			g_array_set_size(ranges, qs_len + 1);
			if (qs)
				memcpy(ranges->data, qs->data,
				       sizeof(struct enum_range) * qs_len);
			g_array_index(ranges, struct enum_range, qs_len) = iter->range;
		} else {
			g_array_set_size(ranges, ranges->len + 1);
			g_array_index(ranges, struct enum_range, ranges->len) = iter->range;
		}
	}
	if (!ranges) {
		ranges = qs;
		g_array_ref(ranges);
	}
	return ranges;
}

#if (__WORDSIZE == 32)
static
guint enum_val_hash(gconstpointer key)
{
	int64_t ukey = *(const int64_t *)key;

	return (guint)ukey ^ (guint)(ukey >> 32);
}

static
gboolean enum_val_equal(gconstpointer a, gconstpointer b)
{
	int64_t ua = *(const int64_t *)a;
	int64_t ub = *(const int64_t *)b;

	return ua == ub;
}

static
void enum_val_free(void *ptr)
{
	g_free(ptr);
}

static
void enum_signed_insert_value_to_quark_set(struct declaration_enum *enum_declaration,
			int64_t v, GQuark q)
{
	int64_t *valuep;
	GArray *array;

	array = g_hash_table_lookup(enum_declaration->table.value_to_quark_set, &v);
	if (!array) {
		array = g_array_sized_new(FALSE, TRUE, sizeof(GQuark), 1);
		g_array_set_size(array, 1);
		g_array_index(array, GQuark, array->len - 1) = q;
		valuep = g_new(int64_t, 1);
		*valuep = v;
		g_hash_table_insert(enum_declaration->table.value_to_quark_set, valuep, array);
	} else {
		g_array_set_size(array, array->len + 1);
		g_array_index(array, GQuark, array->len - 1) = q;
	}
}

static
void enum_unsigned_insert_value_to_quark_set(struct declaration_enum *enum_declaration,
			 uint64_t v, GQuark q)
{
	uint64_t *valuep;
	GArray *array;

	array = g_hash_table_lookup(enum_declaration->table.value_to_quark_set, &v);
	if (!array) {
		array = g_array_sized_new(FALSE, TRUE, sizeof(GQuark), 1);
		g_array_set_size(array, 1);
		g_array_index(array, GQuark, array->len - 1) = q;
		valuep = g_new(uint64_t, 1);
		*valuep = v;
		g_hash_table_insert(enum_declaration->table.value_to_quark_set, valuep, array);
	} else {
		g_array_set_size(array, array->len + 1);
		g_array_index(array, GQuark, array->len - 1) = q;
	}
}
#else  /* __WORDSIZE != 32 */
static
guint enum_val_hash(gconstpointer key)
{
	return g_direct_hash(key);
}

static
gboolean enum_val_equal(gconstpointer a, gconstpointer b)
{
	return g_direct_equal(a, b);
}

static
void enum_val_free(void *ptr)
{
}

static
void enum_signed_insert_value_to_quark_set(struct declaration_enum *enum_declaration,
			int64_t v, GQuark q)
{
	GArray *array;

	array = g_hash_table_lookup(enum_declaration->table.value_to_quark_set,
				    (gconstpointer) v);
	if (!array) {
		array = g_array_sized_new(FALSE, TRUE, sizeof(GQuark), 1);
		g_array_set_size(array, 1);
		g_array_index(array, GQuark, array->len - 1) = q;
		g_hash_table_insert(enum_declaration->table.value_to_quark_set,
				    (gpointer) v, array);
	} else {
		g_array_set_size(array, array->len + 1);
		g_array_index(array, GQuark, array->len - 1) = q;
	}
}

static
void enum_unsigned_insert_value_to_quark_set(struct declaration_enum *enum_declaration,
			 uint64_t v, GQuark q)
{
	GArray *array;

	array = g_hash_table_lookup(enum_declaration->table.value_to_quark_set,
				    (gconstpointer) v);
	if (!array) {
		array = g_array_sized_new(FALSE, TRUE, sizeof(GQuark), 1);
		g_array_set_size(array, 1);
		g_array_index(array, GQuark, array->len - 1) = q;
		g_hash_table_insert(enum_declaration->table.value_to_quark_set,
				    (gpointer) v, array);
	} else {
		g_array_set_size(array, array->len + 1);
		g_array_index(array, GQuark, array->len - 1) = q;
	}
}
#endif /* __WORDSIZE != 32 */

GArray *enum_quark_to_range_set(const struct declaration_enum *enum_declaration,
				GQuark q)
{
	return g_hash_table_lookup(enum_declaration->table.quark_to_range_set,
				   (gconstpointer) (unsigned long) q);
}

static
void enum_signed_insert_range_to_quark(struct declaration_enum *enum_declaration,
                        int64_t start, int64_t end, GQuark q)
{
	struct enum_range_to_quark *rtoq;

	rtoq = g_new(struct enum_range_to_quark, 1);
	cds_list_add(&rtoq->node, &enum_declaration->table.range_to_quark);
	rtoq->range.start._signed = start;
	rtoq->range.end._signed = end;
	rtoq->quark = q;
}

static
void enum_unsigned_insert_range_to_quark(struct declaration_enum *enum_declaration,
                        uint64_t start, uint64_t end, GQuark q)
{
	struct enum_range_to_quark *rtoq;

	rtoq = g_new(struct enum_range_to_quark, 1);
	cds_list_add(&rtoq->node, &enum_declaration->table.range_to_quark);
	rtoq->range.start._unsigned = start;
	rtoq->range.end._unsigned = end;
	rtoq->quark = q;
}

void enum_signed_insert(struct declaration_enum *enum_declaration,
                        int64_t start, int64_t end, GQuark q)
{
	GArray *array;
	struct enum_range *range;

	if (start == end) {
		enum_signed_insert_value_to_quark_set(enum_declaration, start, q);
	} else {
		if (start > end) {
			uint64_t tmp;

			tmp = start;
			start = end;
			end = tmp;
		}
		enum_signed_insert_range_to_quark(enum_declaration, start, end, q);
	}

	array = g_hash_table_lookup(enum_declaration->table.quark_to_range_set,
				    (gconstpointer) (unsigned long) q);
	if (!array) {
		array = g_array_sized_new(FALSE, TRUE,
					  sizeof(struct enum_range), 1);
		g_hash_table_insert(enum_declaration->table.quark_to_range_set,
				    (gpointer) (unsigned long) q,
				    array);
	}
	g_array_set_size(array, array->len + 1);
	range = &g_array_index(array, struct enum_range, array->len - 1);
	range->start._signed = start;
	range->end._signed = end;
}

void enum_unsigned_insert(struct declaration_enum *enum_declaration,
			  uint64_t start, uint64_t end, GQuark q)
{
	GArray *array;
	struct enum_range *range;


	if (start == end) {
		enum_unsigned_insert_value_to_quark_set(enum_declaration, start, q);
	} else {
		if (start > end) {
			uint64_t tmp;

			tmp = start;
			start = end;
			end = tmp;
		}
		enum_unsigned_insert_range_to_quark(enum_declaration, start, end, q);
	}

	array = g_hash_table_lookup(enum_declaration->table.quark_to_range_set,
				    (gconstpointer) (unsigned long) q);
	if (!array) {
		array = g_array_sized_new(FALSE, TRUE,
					  sizeof(struct enum_range), 1);
		g_hash_table_insert(enum_declaration->table.quark_to_range_set,
				    (gpointer) (unsigned long) q,
				    array);
	}
	g_array_set_size(array, array->len + 1);
	range = &g_array_index(array, struct enum_range, array->len - 1);
	range->start._unsigned = start;
	range->end._unsigned = end;
}

size_t enum_get_nr_enumerators(struct declaration_enum *enum_declaration)
{
	return g_hash_table_size(enum_declaration->table.quark_to_range_set);
}

void enum_copy(struct stream_pos *dest, const struct format *fdest, 
	       struct stream_pos *src, const struct format *fsrc,
	       struct definition *definition)
{
	struct definition_enum *_enum =
		container_of(definition, struct definition_enum, p);
	struct declaration_enum *enum_declaration= _enum->declaration;
	GArray *array;
	GQuark v;

	array = fsrc->enum_read(src, enum_declaration);
	assert(array);
	/* unref previous array */
	if (_enum->value)
		g_array_unref(_enum->value);
	_enum->value = array;
	/*
	 * Arbitrarily choose the first one.
	 * TODO: use direct underlying declaration read/write intead. Not doing it for
	 * now to test enum read and write code.
	 */
	v = g_array_index(array, GQuark, 0);
	if (fdest)
		fdest->enum_write(dest, enum_declaration, v);
}

static
void _enum_declaration_free(struct declaration *declaration)
{
	struct declaration_enum *enum_declaration =
		container_of(declaration, struct declaration_enum, p);
	struct enum_range_to_quark *iter, *tmp;

	g_hash_table_destroy(enum_declaration->table.value_to_quark_set);
	cds_list_for_each_entry_safe(iter, tmp, &enum_declaration->table.range_to_quark, node) {
		cds_list_del(&iter->node);
		g_free(iter);
	}
	g_hash_table_destroy(enum_declaration->table.quark_to_range_set);
	declaration_unref(&enum_declaration->integer_declaration->p);
	g_free(enum_declaration);
}

struct declaration_enum *
	enum_declaration_new(struct declaration_integer *integer_declaration)
{
	struct declaration_enum *enum_declaration;

	enum_declaration = g_new(struct declaration_enum, 1);

	enum_declaration->table.value_to_quark_set = g_hash_table_new_full(enum_val_hash,
							    enum_val_equal,
							    enum_val_free,
							    enum_range_set_free);
	CDS_INIT_LIST_HEAD(&enum_declaration->table.range_to_quark);
	enum_declaration->table.quark_to_range_set = g_hash_table_new_full(g_direct_hash,
							g_direct_equal,
							NULL, enum_range_set_free);
	declaration_ref(&integer_declaration->p);
	enum_declaration->integer_declaration = integer_declaration;
	enum_declaration->p.id = CTF_TYPE_ENUM;
	enum_declaration->p.alignment = 1;
	enum_declaration->p.copy = enum_copy;
	enum_declaration->p.declaration_free = _enum_declaration_free;
	enum_declaration->p.definition_new = _enum_definition_new;
	enum_declaration->p.definition_free = _enum_definition_free;
	enum_declaration->p.ref = 1;
	return enum_declaration;
}

static
struct definition *
	_enum_definition_new(struct declaration *declaration,
			     struct definition_scope *parent_scope,
			     GQuark field_name, int index)
{
	struct declaration_enum *enum_declaration =
		container_of(declaration, struct declaration_enum, p);
	struct definition_enum *_enum;
	struct definition *definition_integer_parent;

	_enum = g_new(struct definition_enum, 1);
	declaration_ref(&enum_declaration->p);
	_enum->p.declaration = declaration;
	_enum->declaration = enum_declaration;
	_enum->p.ref = 1;
	_enum->p.index = index;
	_enum->value = NULL;
	definition_integer_parent =
		enum_declaration->integer_declaration->p.definition_new(&enum_declaration->integer_declaration->p,
					parent_scope, field_name, 0);
	_enum->integer = container_of(definition_integer_parent,
				      struct definition_integer, p);
	return &_enum->p;
}

static
void _enum_definition_free(struct definition *definition)
{
	struct definition_enum *_enum =
		container_of(definition, struct definition_enum, p);

	definition_unref(&_enum->integer->p);
	declaration_unref(_enum->p.declaration);
	if (_enum->value)
		g_array_unref(_enum->value);
	g_free(_enum);
}
