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
struct definition *
	lookup_type_definition_scope(GQuark type_name, struct type_scope *scope)
{
	return g_hash_table_lookup(scope->type_definitions,
				   (gconstpointer) (unsigned long) type_name);
}

struct definition *lookup_type_definition(GQuark type_name, struct type_scope *scope)
{
	struct definition *definition;

	while (scope) {
		definition = lookup_type_definition_scope(type_name, scope);
		if (definition)
			return definition;
		scope = scope->parent_scope;
	}
	return NULL;
}

int register_type_definition(GQuark name, struct definition *definition,
			     struct type_scope *scope)
{
	if (!name)
		return -EPERM;

	/* Only lookup in local scope */
	if (lookup_type_definition_scope(name, scope))
		return -EEXIST;

	g_hash_table_insert(scope->type_definitions,
			    (gpointer) (unsigned long) name,
			    definition);
	definition_ref(definition);
	return 0;
}

static
struct definition *
	lookup_field_definition_scope(GQuark field_name, struct definition_scope *scope)
{
	return g_hash_table_lookup(scope->definitions,
				   (gconstpointer) (unsigned long) field_name);
}

struct definition *
	lookup_field_definition(GQuark field_name, struct definition_scope *scope)
{
	struct definition *definition;

	while (scope) {
		definition = lookup_field_definition_scope(field_name, scope);
		if (definition)
			return definition;
		scope = scope->parent_scope;
	}
	return NULL;
}

int register_field_definition(GQuark field_name, struct definition *definition,
			      struct definition_scope *scope)
{
	if (!field_name)
		return -EPERM;

	/* Only lookup in local scope */
	if (lookup_field_definition_scope(field_name, scope))
		return -EEXIST;

	g_hash_table_insert(scope->definitions,
			    (gpointer) (unsigned long) field_name,
			    definition);
	definition_ref(definition);
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

void definition_ref(struct definition *definition)
{
	definition->ref++;
}

void definition_unref(struct definition *definition)
{
	if (!--definition->ref)
		definition->type->definition_free(definition);
}

struct type_scope *
	new_type_scope(struct type_scope *parent_scope)
{
	struct type_scope *scope = g_new(struct type_scope, 1);

	scope->type_definitions = g_hash_table_new_full(g_direct_hash,
					g_direct_equal, NULL,
					(GDestroyNotify) definition_unref);
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
	g_hash_table_destroy(scope->type_definitions);
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

struct definition_scope *
	new_definition_scope(struct definition_scope *parent_scope)
{
	struct definition_scope *scope = g_new(struct definition_scope, 1);

	scope->definitions = g_hash_table_new_full(g_direct_hash,
					g_direct_equal, NULL,
					(GDestroyNotify) definition_unref);
	scope->parent_scope = parent_scope;
	return scope;
}

void free_definition_scope(struct definition_scope *scope)
{
	g_hash_table_destroy(scope->definitions);
	g_free(scope);
}
