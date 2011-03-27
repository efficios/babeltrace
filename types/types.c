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
struct declaration *
	lookup_type_declaration_scope(GQuark type_name, struct type_scope *scope)
{
	return g_hash_table_lookup(scope->type_declarations,
				   (gconstpointer) (unsigned long) type_name);
}

struct declaration *lookup_type_declaration(GQuark type_name, struct type_scope *scope)
{
	struct declaration *declaration;

	while (scope) {
		declaration = lookup_type_declaration_scope(type_name, scope);
		if (declaration)
			return declaration;
		scope = scope->parent_scope;
	}
	return NULL;
}

int register_type_declaration(GQuark name, struct declaration *declaration,
			      struct type_scope *scope)
{
	if (!name)
		return -EPERM;

	/* Only lookup in local scope */
	if (lookup_type_declaration_scope(name, scope))
		return -EEXIST;

	g_hash_table_insert(scope->type_declarations,
			    (gpointer) (unsigned long) name,
			    declaration);
	declaration_ref(declaration);
	return 0;
}

static
struct declaration *
	lookup_field_declaration_scope(GQuark field_name, struct declaration_scope *scope)
{
	return g_hash_table_lookup(scope->declarations,
				   (gconstpointer) (unsigned long) field_name);
}

struct declaration *
	lookup_field_declaration(GQuark field_name, struct declaration_scope *scope)
{
	struct declaration *declaration;

	while (scope) {
		declaration = lookup_field_declaration_scope(field_name, scope);
		if (declaration)
			return declaration;
		scope = scope->parent_scope;
	}
	return NULL;
}

int register_field_declaration(GQuark field_name, struct declaration *declaration,
			       struct declaration_scope *scope)
{
	if (!field_name)
		return -EPERM;

	/* Only lookup in local scope */
	if (lookup_field_declaration_scope(field_name, scope))
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

struct type_scope *
	new_type_scope(struct type_scope *parent_scope)
{
	struct type_scope *scope = g_new(struct type_scope, 1);

	scope->type_declarations = g_hash_table_new_full(g_direct_hash,
					g_direct_equal, NULL,
					(GDestroyNotify) declaration_unref);
	scope->struct_types = g_hash_table_new_full(g_direct_hash,
					g_direct_equal, NULL,
					(GDestroyNotify) type_unref);
	scope->variant_types = g_hash_table_new_full(g_direct_hash,
					g_direct_equal, NULL,
					(GDestroyNotify) type_unref);
	scope->enum_types = g_hash_table_new_full(g_direct_hash,
					g_direct_equal, NULL,
					(GDestroyNotify) type_unref);
	scope->parent_scope = parent_scope;
	return scope;
}

void free_type_scope(struct type_scope *scope)
{
	g_hash_table_destroy(scope->enum_types);
	g_hash_table_destroy(scope->variant_types);
	g_hash_table_destroy(scope->struct_types);
	g_hash_table_destroy(scope->type_declarations);
	g_free(scope);
}

static
struct type_struct *lookup_struct_type_scope(GQuark struct_name,
					     struct type_scope *scope)
{
	return g_hash_table_lookup(scope->struct_types,
				   (gconstpointer) (unsigned long) struct_name);
}

struct type_struct *lookup_struct_type(GQuark struct_name,
				       struct type_scope *scope)
{
	struct type_struct *type;

	while (scope) {
		type = lookup_struct_type_scope(struct_name, scope);
		if (type)
			return type;
		scope = scope->parent_scope;
	}
	return NULL;
}

int register_struct_type(GQuark struct_name, struct type_struct *struct_type,
			 struct type_scope *scope)
{
	if (!struct_name)
		return -EPERM;

	/* Only lookup in local scope */
	if (lookup_struct_type_scope(struct_name, scope))
		return -EEXIST;

	g_hash_table_insert(scope->struct_types,
			    (gpointer) (unsigned long) struct_name,
			    struct_type);
	type_ref(&struct_type->p);
	return 0;
}

static
struct type_variant *lookup_variant_type_scope(GQuark variant_name,
					       struct type_scope *scope)
{
	return g_hash_table_lookup(scope->variant_types,
				   (gconstpointer) (unsigned long) variant_name);
}

struct type_variant *lookup_variant_type(GQuark variant_name,
					 struct type_scope *scope)
{
	struct type_variant *type;

	while (scope) {
		type = lookup_variant_type_scope(variant_name, scope);
		if (type)
			return type;
		scope = scope->parent_scope;
	}
	return NULL;
}

int register_variant_type(GQuark variant_name,
			  struct type_variant *variant_type,
		          struct type_scope *scope)
{
	if (!variant_name)
		return -EPERM;

	/* Only lookup in local scope */
	if (lookup_variant_type_scope(variant_name, scope))
		return -EEXIST;

	g_hash_table_insert(scope->variant_types,
			    (gpointer) (unsigned long) variant_name,
			    variant_type);
	type_ref(&variant_type->p);
	return 0;
}

static
struct type_enum *lookup_enum_type_scope(GQuark enum_name,
					 struct type_scope *scope)
{
	return g_hash_table_lookup(scope->enum_types,
				   (gconstpointer) (unsigned long) enum_name);
}

struct type_enum *lookup_enum_type(GQuark enum_name,
				   struct type_scope *scope)
{
	struct type_enum *type;

	while (scope) {
		type = lookup_enum_type_scope(enum_name, scope);
		if (type)
			return type;
		scope = scope->parent_scope;
	}
	return NULL;
}

int register_enum_type(GQuark enum_name, struct type_enum *enum_type,
		       struct type_scope *scope)
{
	if (!enum_name)
		return -EPERM;

	/* Only lookup in local scope */
	if (lookup_enum_type_scope(enum_name, scope))
		return -EEXIST;

	g_hash_table_insert(scope->enum_types,
			    (gpointer) (unsigned long) enum_name,
			    enum_type);
	type_ref(&enum_type->p);
	return 0;
}

struct declaration_scope *
	new_declaration_scope(struct declaration_scope *parent_scope)
{
	struct declaration_scope *scope = g_new(struct declaration_scope, 1);

	scope->declarations = g_hash_table_new_full(g_direct_hash,
					g_direct_equal, NULL,
					(GDestroyNotify) declaration_unref);
	scope->parent_scope = parent_scope;
	return scope;
}

void free_declaration_scope(struct declaration_scope *scope)
{
	g_hash_table_destroy(scope->declarations);
	g_free(scope);
}
