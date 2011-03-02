/*
 * variant.c
 *
 * BabelTrace - Variant Type Converter
 *
 * Copyright 2011 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
#include <errno.h>

static
struct declaration *_variant_declaration_new(struct type *type,
				struct declaration_scope *parent_scope);
static
void _variant_declaration_free(struct declaration *declaration);

void variant_copy(struct stream_pos *dest, const struct format *fdest, 
		  struct stream_pos *src, const struct format *fsrc,
		  struct declaration *declaration)
{
	struct declaration_variant *variant =
		container_of(declaration, struct declaration_variant, p);
	struct type_variant *variant_type = variant->type;
	struct field *field;
	struct type *field_type;

	fsrc->variant_begin(src, variant_type);
	fdest->variant_begin(dest, variant_type);

	field = variant_get_current_field(variant);
	field_type = field->declaration->type;
	field_type->copy(dest, fdest, src, fsrc, field->declaration);

	fsrc->variant_end(src, variant_type);
	fdest->variant_end(dest, variant_type);
}

static
void _variant_type_free(struct type *type)
{
	struct type_variant *variant_type =
		container_of(type, struct type_variant, p);
	unsigned long i;

	free_type_scope(variant_type->scope);
	g_hash_table_destroy(variant_type->fields_by_tag);

	for (i = 0; i < variant_type->fields->len; i++) {
		struct type_field *type_field =
			&g_array_index(variant_type->fields,
				       struct type_field, i);
		type_unref(type_field->type);
	}
	g_array_free(variant_type->fields, true);
	g_free(variant_type);
}

struct type_variant *variant_type_new(const char *name,
				      struct type_scope *parent_scope)
{
	struct type_variant *variant_type;
	struct type *type;

	variant_type = g_new(struct type_variant, 1);
	type = &variant_type->p;
	variant_type->fields_by_tag = g_hash_table_new(g_direct_hash,
						       g_direct_equal);
	variant_type->fields = g_array_sized_new(FALSE, TRUE,
						 sizeof(struct type_field),
						 DEFAULT_NR_STRUCT_FIELDS);
	variant_type->scope = new_type_scope(parent_scope);
	type->name = g_quark_from_string(name);
	type->alignment = 1;
	type->copy = variant_copy;
	type->type_free = _variant_type_free;
	type->declaration_new = _variant_declaration_new;
	type->declaration_free = _variant_declaration_free;
	type->ref = 1;
	return variant_type;
}

static
struct declaration *
	_variant_declaration_new(struct type *type,
				 struct declaration_scope *parent_scope)
{
	struct type_variant *variant_type =
		container_of(type, struct type_variant, p);
	struct declaration_variant *variant;
	unsigned long i;

	variant = g_new(struct declaration_variant, 1);
	type_ref(&variant_type->p);
	variant->p.type = type;
	variant->type = variant_type;
	variant->p.ref = 1;
	variant->scope = new_declaration_scope(parent_scope);
	variant->fields = g_array_sized_new(FALSE, TRUE,
					    sizeof(struct field),
					    DEFAULT_NR_STRUCT_FIELDS);
	g_array_set_size(variant->fields, variant_type->fields->len);
	for (i = 0; i < variant_type->fields->len; i++) {
		struct type_field *type_field =
			&g_array_index(variant_type->fields,
				       struct type_field, i);
		struct field *field = &g_array_index(variant->fields,
						     struct field, i);

		field->name = type_field->name;
		field->declaration =
			type_field->type->declaration_new(type_field->type,
							  variant->scope);
	}
	variant->current_field = NULL;
	return &variant->p;
}

static
void _variant_declaration_free(struct declaration *declaration)
{
	struct declaration_variant *variant =
		container_of(declaration, struct declaration_variant, p);
	unsigned long i;

	assert(variant->fields->len == variant->type->fields->len);
	for (i = 0; i < variant->fields->len; i++) {
		struct field *field = &g_array_index(variant->fields,
						     struct field, i);
		declaration_unref(field->declaration);
	}
	free_declaration_scope(variant->scope);
	type_unref(variant->p.type);
	g_free(variant);
}

void variant_type_add_field(struct type_variant *variant_type,
			    const char *tag_name,
			    struct type *tag_type)
{
	struct type_field *field;
	unsigned long index;

	g_array_set_size(variant_type->fields, variant_type->fields->len + 1);
	index = variant_type->fields->len - 1;	/* last field (new) */
	field = &g_array_index(variant_type->fields, struct type_field, index);
	field->name = g_quark_from_string(tag_name);
	type_ref(tag_type);
	field->type = tag_type;
	/* Keep index in hash rather than pointer, because array can relocate */
	g_hash_table_insert(variant_type->fields_by_tag,
			    (gpointer) (unsigned long) field->name,
			    (gpointer) index);
	/*
	 * Alignment of variant is based on the alignment of its currently
	 * selected choice, so we leave variant alignment as-is (statically
	 * speaking).
	 */
}

struct type_field *
struct_type_get_field_from_tag(struct type_variant *variant_type, GQuark tag)
{
	unsigned long index;

	index = (unsigned long) g_hash_table_lookup(variant_type->fields_by_tag,
						    (gconstpointer) (unsigned long) tag);
	return &g_array_index(variant_type->fields, struct type_field, index);
}

/*
 * tag_instance is assumed to be an enumeration.
 */
int variant_declaration_set_tag(struct declaration_variant *variant,
				struct declaration *enum_tag)
{
	struct declaration_enum *_enum =
		container_of(variant->enum_tag, struct declaration_enum, p);
	struct type_enum *enum_type = _enum->type;
	int missing_field = 0;
	unsigned long i;

	/*
	 * Strictly speaking, each enumerator must map to a field of the
	 * variant. However, we are even stricter here by requiring that each
	 * variant choice map to an enumerator too. We then validate that the
	 * number of enumerators equals the number of variant choices.
	 */
	if (variant->type->fields->len != enum_get_nr_enumerators(enum_type))
		return -EPERM;

	for (i = 0; i < variant->type->fields->len; i++) {
		struct type_field *field_type =
			&g_array_index(variant->type->fields,
				       struct type_field, i);
		if (!enum_quark_to_range_set(enum_type, field_type->name)) {
			missing_field = 1;
			break;
		}
	}
	if (missing_field)
		return -EPERM;

	/*
	 * Check the enumeration: it must map each value to one and only one
	 * enumerator tag.
	 * TODO: we should also check that each range map to one and only one
	 * tag. For the moment, we will simply check this dynamically in
	 * variant_type_get_current_field().
	 */

	/* Set the enum tag field */
	variant->enum_tag = enum_tag;
	return 0;
}

/*
 * field returned only valid as long as the field structure is not appended to.
 */
struct field *variant_get_current_field(struct declaration_variant *variant)
{
	struct declaration_enum *_enum =
		container_of(variant->enum_tag, struct declaration_enum, p);
	struct type_variant *variant_type = variant->type;
	unsigned long index;
	GArray *tag_array;
	GQuark tag;

	tag_array = _enum->value;
	/*
	 * The 1 to 1 mapping from enumeration to value should have been already
	 * checked. (see TODO above)
	 */
	assert(tag_array->len == 1);
	tag = g_array_index(tag_array, GQuark, 0);
	index = (unsigned long) g_hash_table_lookup(variant_type->fields_by_tag,
						    (gconstpointer) (unsigned long) tag);
	variant->current_field = &g_array_index(variant->fields, struct field, index);
	return variant->current_field;
}
