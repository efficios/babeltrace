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
struct type *
	lookup_type_scope(GQuark type_name, struct declaration_scope *scope)
{
	return g_hash_table_lookup(scope->types,
				   (gconstpointer) (unsigned long) type_name);
}

struct type *lookup_type(GQuark type_name, struct declaration_scope *scope)
{
	struct type *type;

	while (scope) {
		type = lookup_type_scope(type_name, scope);
		if (type)
			return type;
		scope = scope->parent_scope;
	}
	return NULL;
}

int register_type(struct type *type, struct declaration_scope *scope)
{
	if (!type->name)
		return -EPERM;

	/* Only lookup in local scope */
	if (lookup_type_scope(type->name, scope))
		return -EEXIST;

	g_hash_table_insert(scope->types,
			    (gpointer) (unsigned long) type->name,
			    type);
	type_ref(type);
	return 0;
}

static
struct declaration *
	lookup_declaration_scope(GQuark field_name, struct declaration_scope *scope)
{
	return g_hash_table_lookup(scope->declarations,
				   (gconstpointer) (unsigned long) field_name);
}

struct declaration *
	lookup_declaration(GQuark field_name, struct declaration_scope *scope)
{
	struct declaration *declaration;

	while (scope) {
		declaration = lookup_declaration_scope(field_name, scope);
		if (declaration)
			return declaration;
		scope = scope->parent_scope;
	}
	return NULL;
}

int register_declaration(GQuark field_name, struct declaration *declaration,
			 struct declaration_scope *scope)
{
	if (!field_name)
		return -EPERM;

	/* Only lookup in local scope */
	if (lookup_declaration_scope(field_name, scope))
		return -EEXIST;

	g_hash_table_insert(scope->declarations,
			    (gpointer) (unsigned long) field_name,
			    declaration);
	declaration_ref(declaration);
	return 0;
}

void type_ref(struct type *type)
{
	type->ref++;
}

void type_unref(struct type *type)
{
	if (!--type->ref)
		type->type_free(type);
}

void declaration_ref(struct declaration *declaration)
{
	declaration->ref++;
}

void declaration_unref(struct declaration *declaration)
{
	if (!--declaration->ref)
		declaration->type->declaration_free(declaration);
}

struct declaration_scope *
	new_declaration_scope(struct declaration_scope *parent_scope)
{
	struct declaration_scope *scope = g_new(struct declaration_scope, 1);

	scope->types = g_hash_table_new_full(g_direct_hash,
					g_direct_equal, NULL,
					(GDestroyNotify) type_unref);
	scope->declarations = g_hash_table_new_full(g_direct_hash,
					g_direct_equal, NULL,
					(GDestroyNotify) declaration_unref);
	scope->parent_scope = parent_scope;
	return scope;
}

void free_declaration_scope(struct declaration_scope *scope)
{
	g_hash_table_destroy(scope->declarations);
	g_hash_table_destroy(scope->types);
	g_free(scope);
}
