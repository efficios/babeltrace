/*
 * types.c
 *
 * BabelTrace - Converter
 *
 * Types registry.
 *
 * Copyright 2010-2011 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/types.h>
#include <limits.h>
#include <glib.h>
#include <errno.h>

static
GQuark prefix_quark(const char *prefix, GQuark quark)
{
	GQuark nq;
	GString *str;
	char *quark_str;

	str = g_string_new(prefix);
	g_string_append(str, g_quark_to_string(quark));
	quark_str = g_string_free(str, FALSE);
	nq = g_quark_from_string(quark_str);
	g_free(quark_str);
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
	int i, ret;

	if (babeltrace_debug) {
		int need_dot = 0;

		printf_debug("Is path \"");
		for (i = 0; i < path->len; need_dot = 1, i++)
			printf("%s%s", need_dot ? "." : "",
				g_quark_to_string(g_array_index(path, GQuark, i)));
		need_dot = 0;
		printf("\" child of \"");
		for (i = 0; i < maybe_parent->len; need_dot = 1, i++)
			printf("%s%s", need_dot ? "." : "",
				g_quark_to_string(g_array_index(maybe_parent, GQuark, i)));
		printf("\" ? ");
	}

	if (path->len <= maybe_parent->len) {
		ret = 0;
		goto end;
	}
	if (compare_paths(path, maybe_parent, maybe_parent->len)
			== maybe_parent->len)
		ret = 1;
	else
		ret = 0;
end:
	if (babeltrace_debug)
		printf("%s\n", ret ? "Yes" : "No");
	return ret;
}

static struct definition_scope *
	get_definition_scope(const struct definition *definition)
{
	return definition->scope;
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
	lookup_path_definition(GArray *cur_path,
			       GArray *lookup_path,
			       struct definition_scope *scope)
{
	struct definition *definition, *lookup_definition;
	GQuark last;
	int index;

	/* Going up in the hierarchy. Check where we come from. */
	assert(is_path_child_of(cur_path, scope->scope_path));
	assert(cur_path->len - scope->scope_path->len == 1);

	/*
	 * First, check if the target name is size one, present in
	 * our parent path, located prior to us.
	 */
	if (lookup_path->len == 1) {
		last = g_array_index(lookup_path, GQuark, 0);
		lookup_definition = lookup_field_definition_scope(last, scope);
		last = g_array_index(cur_path, GQuark, cur_path->len - 1);
		definition = lookup_field_definition_scope(last, scope);
		assert(definition);
		if (lookup_definition && lookup_definition->index < definition->index)
			return lookup_definition;
		else
			return NULL;
	}

	while (scope) {
		if (is_path_child_of(cur_path, scope->scope_path) &&
		    cur_path->len - scope->scope_path->len == 1) {
			last = g_array_index(cur_path, GQuark, cur_path->len - 1);
			definition = lookup_field_definition_scope(last, scope);
			assert(definition);
			index = definition->index;
		} else {
			/*
			 * Getting to a dynamic scope parent. We are
			 * guaranteed that the parent is entirely
			 * located before the child.
			 */
			index = -1;
		}
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
	if (!scope || !field_name)
		return -EPERM;

	/* Only lookup in local scope */
	if (lookup_field_definition_scope(field_name, scope))
		return -EEXIST;

	g_hash_table_insert(scope->definitions,
			    (gpointer) (unsigned long) field_name,
			    definition);
	/* Don't keep reference on definition */
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
					(GDestroyNotify) declaration_unref);
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

	scope->definitions = g_hash_table_new(g_direct_hash,
					g_direct_equal);
	scope->parent_scope = parent_scope;
	scope->scope_path = g_array_sized_new(FALSE, TRUE, sizeof(GQuark),
					      scope_path_len);
	g_array_set_size(scope->scope_path, scope_path_len);
	return scope;
}

GQuark new_definition_path(struct definition_scope *parent_scope,
			   GQuark field_name, const char *root_name)
{
	GQuark path;
	GString *str;
	gchar *c_str;
	int i;
	int need_dot = 0;

	str = g_string_new("");
	if (root_name) {
		g_string_append(str, root_name);
		need_dot = 1;
	} else if (parent_scope) {
		for (i = 0; i < parent_scope->scope_path->len; i++) {
			GQuark q = g_array_index(parent_scope->scope_path,
						 GQuark, i);
			if (!q)
				continue;
			if (need_dot)
				g_string_append(str, ".");
			g_string_append(str, g_quark_to_string(q));
			need_dot = 1;
		}
	}
	if (field_name) {
		if (need_dot)
			g_string_append(str, ".");
		g_string_append(str, g_quark_to_string(field_name));
	}
	c_str = g_string_free(str, FALSE);
	if (c_str[0] == '\0')
		return 0;
	path = g_quark_from_string(c_str);
	printf_debug("new definition path: %s\n", c_str);
	g_free(c_str);
	return path;
}

struct definition_scope *
	new_definition_scope(struct definition_scope *parent_scope,
			     GQuark field_name, const char *root_name)
{
	struct definition_scope *scope;

	if (root_name) {
		scope = _new_definition_scope(parent_scope, 0);
		append_scope_path(root_name, scope->scope_path);
	} else {
		int scope_path_len = 1;

		assert(parent_scope);
		scope_path_len += parent_scope->scope_path->len;
		scope = _new_definition_scope(parent_scope, scope_path_len);
		memcpy(scope->scope_path->data, parent_scope->scope_path->data,
		       sizeof(GQuark) * (scope_path_len - 1));
		g_array_index(scope->scope_path, GQuark, scope_path_len - 1) =
			field_name;
	}
	if (babeltrace_debug) {
		int i, need_dot = 0;

		printf_debug("new definition scope: ");
		for (i = 0; i < scope->scope_path->len; need_dot = 1, i++)
			printf("%s%s", need_dot ? "." : "",
				g_quark_to_string(g_array_index(scope->scope_path, GQuark, i)));
		printf("\n");
	}
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

void free_definition_scope(struct definition_scope *scope)
{
	g_array_free(scope->scope_path, TRUE);
	g_hash_table_destroy(scope->definitions);
	g_free(scope);
}

struct definition *lookup_definition(const struct definition *definition,
				     const char *field_name)
{
	struct definition_scope *scope = get_definition_scope(definition);

	if (!scope)
		return NULL;

	return lookup_field_definition_scope(g_quark_from_string(field_name),
					     scope);
}

struct definition_integer *lookup_integer(const struct definition *definition,
					  const char *field_name,
					  int signedness)
{
	struct definition *lookup;
	struct definition_integer *lookup_integer;

	lookup = lookup_definition(definition, field_name);
	if (!lookup)
		return NULL;
	if (lookup->declaration->id != CTF_TYPE_INTEGER)
		return NULL;
	lookup_integer = container_of(lookup, struct definition_integer, p);
	if (lookup_integer->declaration->signedness != signedness)
		return NULL;
	return lookup_integer;
}

struct definition_enum *lookup_enum(const struct definition *definition,
				    const char *field_name,
				    int signedness)
{
	struct definition *lookup;
	struct definition_enum *lookup_enum;

	lookup = lookup_definition(definition, field_name);
	if (!lookup)
		return NULL;
	if (lookup->declaration->id != CTF_TYPE_ENUM)
		return NULL;
	lookup_enum = container_of(lookup, struct definition_enum, p);
	if (lookup_enum->integer->declaration->signedness != signedness)
		return NULL;
	return lookup_enum;
}

struct definition *lookup_variant(const struct definition *definition,
				  const char *field_name)
{
	struct definition *lookup;
	struct definition_variant *lookup_variant;

	lookup = lookup_definition(definition, field_name);
	if (!lookup)
		return NULL;
	if (lookup->declaration->id != CTF_TYPE_VARIANT)
		return NULL;
	lookup_variant = container_of(lookup, struct definition_variant, p);
	lookup = variant_get_current_field(lookup_variant);
	assert(lookup);
	return lookup;
}
