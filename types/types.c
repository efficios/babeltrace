/*
 * declarations.c
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
#include <limits.h>
#include <glib.h>
#include <errno.h>

static
GQuark prefix_quark(const char *prefix, GQuark quark)
{
	GQuark nq;
	GString *str;

	str = g_string_new(prefix);
	g_string_append(str, g_quark_to_string(quark));
	nq = g_quark_from_string(g_string_free(str, FALSE));
	return nq;
}

static
struct declaration *
	lookup_declaration_scope(GQuark declaration_name,
		struct declaration_scope *scope)
{
	return g_hash_table_lookup(scope->typedef_declarations,
				   (gconstpointer) (unsigned long) declaration_name);
}

struct declaration *lookup_declaration(GQuark declaration_name,
		struct declaration_scope *scope)
{
	struct declaration *declaration;

	while (scope) {
		declaration = lookup_declaration_scope(declaration_name,
						       scope);
		if (declaration)
			return declaration;
		scope = scope->parent_scope;
	}
	return NULL;
}

int register_declaration(GQuark name, struct declaration *declaration,
		struct declaration_scope *scope)
{
	if (!name)
		return -EPERM;

	/* Only lookup in local scope */
	if (lookup_declaration_scope(name, scope))
		return -EEXIST;

	g_hash_table_insert(scope->typedef_declarations,
			    (gpointer) (unsigned long) name,
			    declaration);
	declaration_ref(declaration);
	return 0;
}

static
struct definition *
	lookup_field_definition_scope(GQuark field_name,
		struct definition_scope *scope)
{
	return g_hash_table_lookup(scope->definitions,
				   (gconstpointer) (unsigned long) field_name);
}

/*
 * Returns the index at which the paths differ.
 * If the value returned equals len, it means the paths are identical
 * from index 0 to len-1.
 */
static int compare_paths(GArray *a, GArray *b, int len)
{
	int i;

	assert(len <= a->len);
	assert(len <= b->len);

	for (i = 0; i < len; i++) {
		GQuark qa, qb;

		qa = g_array_index(a, GQuark, i);
		qb = g_array_index(b, GQuark, i);
		if (qa != qb)
			return i;
	}
	return i;
}

static int is_path_child_of(GArray *path, GArray *maybe_parent)
{
	if (path->len <= maybe_parent->len)
		return 0;
	if (compare_paths(path, maybe_parent, maybe_parent->len)
			== maybe_parent->len)
		return 1;
	else
		return 0;
}

static struct definition_scope *
	get_definition_scope(struct definition *definition)
{
	switch (definition->declaration->id) {
	case CTF_TYPE_STRUCT:
	{
		struct definition_struct *def =
			container_of(definition, struct definition_struct, p);
		return def->scope;
	}
	case CTF_TYPE_VARIANT:
	{
		struct definition_variant *def =
			container_of(definition, struct definition_variant, p);
		return def->scope;
	}
	case CTF_TYPE_ARRAY:
	{
		struct definition_array *def =
			container_of(definition, struct definition_array, p);
		return def->scope;
	}
	case CTF_TYPE_SEQUENCE:
	{
		struct definition_sequence *def =
			container_of(definition, struct definition_sequence, p);
		return def->scope;
	}

	case CTF_TYPE_INTEGER:
	case CTF_TYPE_FLOAT:
	case CTF_TYPE_ENUM:
	case CTF_TYPE_STRING:
	case CTF_TYPE_UNKNOWN:
	default:
		return NULL;
	}
}

/*
 * OK, here is the fun. We want to lookup a field that is:
 * - either in the same dynamic scope:
 *   - either in the current scope, but prior to the current field.
 *   - or in a parent scope (or parent of parent ...) still in a field
 *     prior to the current field position within the parents.
 * - or in a different dynamic scope:
 *   - either in a upper dynamic scope (walk down a targeted scope from
 *     the dynamic scope root)
 *   - or in a lower dynamic scope (failure)
 * The dynamic scope roots are linked together, so we can access the
 * parent dynamic scope from the child dynamic scope by walking up to
 * the parent.
 * If we cannot find such a field that is prior to our current path, we
 * return NULL.
 *
 * cur_path: the path leading to the variant definition.
 * lookup_path: the path leading to the enum we want to look for.
 * scope: the definition scope containing the variant definition.
 */
struct definition *
	lookup_definition(GArray *cur_path,
			  GArray *lookup_path,
			  struct definition_scope *scope)
{
	struct definition *definition, *lookup_definition;
	GQuark last;
	int index;

	while (scope) {
		/* going up in the hierarchy. Check where we come from. */
		assert(is_path_child_of(cur_path, scope->scope_path));
		assert(cur_path->len - scope->scope_path->len == 1);
		last = g_array_index(cur_path, GQuark, cur_path->len - 1);
		definition = lookup_field_definition_scope(last, scope);
		assert(definition);
		index = definition->index;
lookup:
		if (is_path_child_of(lookup_path, scope->scope_path)) {
			/* Means we can lookup the field in this scope */
			last = g_array_index(lookup_path, GQuark,
					     scope->scope_path->len);
			lookup_definition = lookup_field_definition_scope(last, scope);
			if (!lookup_definition || ((index != -1) && lookup_definition->index >= index))
				return NULL;
			/* Found it! And it is prior to the current field. */
			if (lookup_path->len - scope->scope_path->len == 1) {
				/* Direct child */
				return lookup_definition;
			} else {
				scope = get_definition_scope(lookup_definition);
				/* Check if the definition has a sub-scope */
				if (!scope)
					return NULL;
				/*
				 * Don't compare index anymore, because we are
				 * going within a scope that has been validated
				 * to be entirely prior to the current scope.
				 */
				cur_path = NULL;
				index = -1;
				goto lookup;
			}
		} else {
			assert(index != -1);
			/* lookup_path is within an upper scope */
			cur_path = scope->scope_path;
			scope = scope->parent_scope;
		}
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

void declaration_ref(struct declaration *declaration)
{
	declaration->ref++;
}

void declaration_unref(struct declaration *declaration)
{
	if (!declaration)
		return;
	if (!--declaration->ref)
		declaration->declaration_free(declaration);
}

void definition_ref(struct definition *definition)
{
	definition->ref++;
}

void definition_unref(struct definition *definition)
{
	if (!definition)
		return;
	if (!--definition->ref)
		definition->declaration->definition_free(definition);
}

struct declaration_scope *
	new_declaration_scope(struct declaration_scope *parent_scope)
{
	struct declaration_scope *scope = g_new(struct declaration_scope, 1);

	scope->typedef_declarations = g_hash_table_new_full(g_direct_hash,
					g_direct_equal, NULL,
					(GDestroyNotify) definition_unref);
	scope->struct_declarations = g_hash_table_new_full(g_direct_hash,
					g_direct_equal, NULL,
					(GDestroyNotify) declaration_unref);
	scope->variant_declarations = g_hash_table_new_full(g_direct_hash,
					g_direct_equal, NULL,
					(GDestroyNotify) declaration_unref);
	scope->enum_declarations = g_hash_table_new_full(g_direct_hash,
					g_direct_equal, NULL,
					(GDestroyNotify) declaration_unref);
	scope->parent_scope = parent_scope;
	return scope;
}

void free_declaration_scope(struct declaration_scope *scope)
{
	g_hash_table_destroy(scope->enum_declarations);
	g_hash_table_destroy(scope->variant_declarations);
	g_hash_table_destroy(scope->struct_declarations);
	g_hash_table_destroy(scope->typedef_declarations);
	g_free(scope);
}

static
struct declaration_struct *lookup_struct_declaration_scope(GQuark struct_name,
					     struct declaration_scope *scope)
{
	return g_hash_table_lookup(scope->struct_declarations,
				   (gconstpointer) (unsigned long) struct_name);
}

struct declaration_struct *lookup_struct_declaration(GQuark struct_name,
				       struct declaration_scope *scope)
{
	struct declaration_struct *declaration;

	while (scope) {
		declaration = lookup_struct_declaration_scope(struct_name, scope);
		if (declaration)
			return declaration;
		scope = scope->parent_scope;
	}
	return NULL;
}

int register_struct_declaration(GQuark struct_name,
	struct declaration_struct *struct_declaration,
	struct declaration_scope *scope)
{
	GQuark prefix_name;
	int ret;

	if (!struct_name)
		return -EPERM;

	/* Only lookup in local scope */
	if (lookup_struct_declaration_scope(struct_name, scope))
		return -EEXIST;

	g_hash_table_insert(scope->struct_declarations,
			    (gpointer) (unsigned long) struct_name,
			    struct_declaration);
	declaration_ref(&struct_declaration->p);

	/* Also add in typedef/typealias scopes */
	prefix_name = prefix_quark("struct ", struct_name);
	ret = register_declaration(prefix_name, &struct_declaration->p, scope);
	assert(!ret);
	return 0;
}

static
struct declaration_untagged_variant *
	lookup_variant_declaration_scope(GQuark variant_name,
		struct declaration_scope *scope)
{
	return g_hash_table_lookup(scope->variant_declarations,
				   (gconstpointer) (unsigned long) variant_name);
}

struct declaration_untagged_variant *
	lookup_variant_declaration(GQuark variant_name,
		struct declaration_scope *scope)
{
	struct declaration_untagged_variant *declaration;

	while (scope) {
		declaration = lookup_variant_declaration_scope(variant_name, scope);
		if (declaration)
			return declaration;
		scope = scope->parent_scope;
	}
	return NULL;
}

int register_variant_declaration(GQuark variant_name,
		struct declaration_untagged_variant *untagged_variant_declaration,
		struct declaration_scope *scope)
{
	GQuark prefix_name;
	int ret;

	if (!variant_name)
		return -EPERM;

	/* Only lookup in local scope */
	if (lookup_variant_declaration_scope(variant_name, scope))
		return -EEXIST;

	g_hash_table_insert(scope->variant_declarations,
			    (gpointer) (unsigned long) variant_name,
			    untagged_variant_declaration);
	declaration_ref(&untagged_variant_declaration->p);

	/* Also add in typedef/typealias scopes */
	prefix_name = prefix_quark("variant ", variant_name);
	ret = register_declaration(prefix_name,
			&untagged_variant_declaration->p, scope);
	assert(!ret);
	return 0;
}

static
struct declaration_enum *
	lookup_enum_declaration_scope(GQuark enum_name,
		struct declaration_scope *scope)
{
	return g_hash_table_lookup(scope->enum_declarations,
				   (gconstpointer) (unsigned long) enum_name);
}

struct declaration_enum *
	lookup_enum_declaration(GQuark enum_name,
		struct declaration_scope *scope)
{
	struct declaration_enum *declaration;

	while (scope) {
		declaration = lookup_enum_declaration_scope(enum_name, scope);
		if (declaration)
			return declaration;
		scope = scope->parent_scope;
	}
	return NULL;
}

int register_enum_declaration(GQuark enum_name,
		struct declaration_enum *enum_declaration,
		struct declaration_scope *scope)
{
	GQuark prefix_name;
	int ret;

	if (!enum_name)
		return -EPERM;

	/* Only lookup in local scope */
	if (lookup_enum_declaration_scope(enum_name, scope))
		return -EEXIST;

	g_hash_table_insert(scope->enum_declarations,
			    (gpointer) (unsigned long) enum_name,
			    enum_declaration);
	declaration_ref(&enum_declaration->p);

	/* Also add in typedef/typealias scopes */
	prefix_name = prefix_quark("enum ", enum_name);
	ret = register_declaration(prefix_name, &enum_declaration->p, scope);
	assert(!ret);
	return 0;
}

static struct definition_scope *
	_new_definition_scope(struct definition_scope *parent_scope,
			      int scope_path_len)
{
	struct definition_scope *scope = g_new(struct definition_scope, 1);

	scope->definitions = g_hash_table_new_full(g_direct_hash,
					g_direct_equal, NULL,
					(GDestroyNotify) definition_unref);
	scope->parent_scope = parent_scope;
	scope->scope_path = g_array_sized_new(FALSE, TRUE, sizeof(GQuark),
					      scope_path_len);
	g_array_set_size(scope->scope_path, scope_path_len);
	return scope;
}

struct definition_scope *
	new_definition_scope(struct definition_scope *parent_scope,
			     GQuark field_name)
{
	struct definition_scope *scope;
	int scope_path_len = 1;

	if (parent_scope)
		scope_path_len += parent_scope->scope_path->len;
	scope = _new_definition_scope(parent_scope, scope_path_len);
	if (parent_scope)
		memcpy(scope->scope_path, parent_scope->scope_path,
		       sizeof(GQuark) * (scope_path_len - 1));
	g_array_index(scope->scope_path, GQuark, scope_path_len - 1) =
		field_name;
	return scope;
}

/*
 * in: path (dot separated), out: q (GArray of GQuark)
 */
void append_scope_path(const char *path, GArray *q)
{
	const char *ptrbegin, *ptrend = path;
	GQuark quark;

	for (;;) {
		char *str;
		size_t len;

		ptrbegin = ptrend;
		ptrend = strchr(ptrbegin, '.');
		if (!ptrend)
			break;
		len = ptrend - ptrbegin;
		/* Don't accept two consecutive dots */
		assert(len != 0);
		str = g_new(char, len + 1);	/* include \0 */
		memcpy(str, ptrbegin, len);
		str[len] = '\0';
		quark = g_quark_from_string(str);
		g_array_append_val(q, quark);
		g_free(str);
		ptrend++;	/* skip current dot */
	}
	/* last. Check for trailing dot (and discard). */
	if (ptrbegin[0] != '\0') {
		quark = g_quark_from_string(ptrbegin);
		g_array_append_val(q, quark);
	}
}

void set_dynamic_definition_scope(struct definition *definition,
				  struct definition_scope *scope,
				  const char *root_name)
{
	g_array_set_size(scope->scope_path, 0);
	append_scope_path(root_name, scope->scope_path);
	/*
	 * Use INT_MAX order to ensure that all fields of the parent
	 * scope are seen as being prior to this scope.
	 */
	definition->index = INT_MAX;
}

void free_definition_scope(struct definition_scope *scope)
{
	g_array_free(scope->scope_path, TRUE);
	g_hash_table_destroy(scope->definitions);
	g_free(scope);
}
