/*
 * types.c
 *
 * BabelTrace - Converter
 *
 * Types registry.
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

#include <babeltrace/format.h>
#include <glib.h>
#include <errno.h>

static
struct type_class *lookup_type_class_scope(GQuark qname,
					   struct declaration_scope *scope)
{
	return g_hash_table_lookup(scope->type_classes,
				   (gconstpointer) (unsigned long) qname);
}

struct type_class *lookup_type_class(GQuark qname,
				     struct declaration_scope *scope)
{
	struct type_class *tc;

	while (scope) {
		tc = lookup_type_class_scope(qname, scope);
		if (tc)
			return tc;
		scope = scope->parent_scope;
	}
	return NULL;
}

static void free_type_class(struct type_class *type_class)
{
	type_class->class_free(type_class);
}

static void free_type(struct type *type)
{
	type->p.type_free(type);
}

int register_type_class(struct type_class *type_class,
			struct declaration_scope *scope)
{
	/* Only lookup in local scope */
	if (lookup_type_class_scope(type_class->name, scope))
		return -EEXIST;

	g_hash_table_insert(scope->type_classes,
			    (gpointer) (unsigned long) type_class->name,
			    type_class);
	return 0;
}

void type_class_ref(struct type_class *type_class)
{
	type_class->ref++;
}

void type_class_unref(struct type_class *type_class)
{
	if (!--type_class->ref)
		free_type_class(type_class);
}

void type_ref(struct type *type)
{
	type->ref++;
}

void type_unref(struct type *type)
{
	if (!--type->ref)
		free_type(type);
}

struct declaration_scope *
new_declaration_scope(struct declaration_scope *parent_scope)
{
	struct declaration_scope *scope = g_new(struct declaration_scope, 1);

	scope->type_classes = g_hash_table_new_full(g_direct_hash,
					g_direct_equal, NULL,
					(GDestroyNotify) type_class_unref);
	scope->parent_scope = parent_scope;
	return scope;
}

void free_declaration_scope(struct declaration_scope *scope)
{
	g_hash_table_destroy(scope->type_classes);
	g_free(scope);
}
