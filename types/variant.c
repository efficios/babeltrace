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


static
struct type *variant_type_new(struct type_class *type_class,
			      struct declaration_scope *parent_scope);
static
void variant_type_free(struct type *type);

void variant_copy(struct stream_pos *dest, const struct format *fdest, 
		  struct stream_pos *src, const struct format *fsrc,
		  struct type *type)
{
	struct type_variant *variant = container_of(type, struct type_variant,
						    p);
	struct type_class_variant *variant_class = variant->_class;
	struct field *field;
	struct type_class *field_class;
	unsigned long i;

	fsrc->variant_begin(src, variant_class);
	fdest->variant_begin(dest, variant_class);

	field = variant_type_get_current_field(variant);
	field_class = field->type->p._class;
	field_class->copy(dest, fdest, src, fsrc, &field->type->p);

	fsrc->variant_end(src, variant_class);
	fdest->variant_end(dest, variant_class);
}

static
void variant_type_class_free(struct type_class *type_class)
{
	struct type_class_variant *variant_class =
		container_of(type_class, struct type_class_variant, p);
	unsigned long i;

	g_hash_table_destroy(struct_class->fields_by_tag);

	for (i = 0; i < variant_class->fields->len; i++) {
		struct field *type_class_field =
			&g_array_index(variant_class->fields,
				       struct type_class_field, i);
		type_class_unref(field->type_class);
	}
	g_array_free(variant_class->fields, true);
	g_free(variant_class);
}

struct type_class_variant *
variant_type_class_new(const char *name)
{
	struct type_class_variant *variant_class;
	struct type_class *type_class;
	int ret;

	variant_class = g_new(struct type_class_variant, 1);
	type_class = &variant_class->p;
	variant_class->fields_by_tag = g_hash_table_new(g_direct_hash,
							g_direct_equal);
	variant_class->fields = g_array_sized_new(FALSE, TRUE,
						  sizeof(struct type_class_field),
						  DEFAULT_NR_STRUCT_FIELDS);
	type_class->name = g_quark_from_string(name);
	type_class->alignment = 1;
	type_class->copy = variant_copy;
	type_class->class_free = _variant_type_class_free;
	type_class->type_new = _variant_type_new;
	type_class->type_free = _variant_type_free;
	type_class->ref = 1;

	if (type_class->name) {
		ret = register_type(type_class);
		if (ret)
			goto error_register;
	}
	return struct_class;

error_register:
	g_hash_table_destroy(variant_class->fields_by_tag);
	g_array_free(variant_class->fields, true);
	g_free(variant_class);
	return NULL;
}

static
struct type_variant *_variant_type_new(struct type_class *type_class,
				       struct declaration_scope *parent_scope)
{
	struct type_class_variant *variant_class =
		container_of(type_class, struct type_class_variant, p);
	struct type_struct *variant;

	variant = g_new(struct type_variant, 1);
	type_class_ref(&variant_class->p);
	variant->p._class = variant_class;
	variant->p.ref = 1;
	variant->scope = new_declaration_scope(parent_scope);
	variant->fields = g_array_sized_new(FALSE, TRUE,
					    sizeof(struct field),
					    DEFAULT_NR_STRUCT_FIELDS);
	variant->current_field = NULL;
	return &variant->p;
}

static
void variant_type_free(struct type *type)
{
	struct type_variant *variant = container_of(type, struct type_variant,
						    p);
	unsigned long i;

	for (i = 0; i < variant->fields->len; i++) {
		struct field *field = &g_array_index(variant->fields,
						     struct field, i);
		type_unref(field->type);
	}
	free_declaration_scope(variant->scope);
	type_class_unref(variant->p._class);
	g_free(variant);
}

void variant_type_class_add_field(struct type_class_variant *variant_class,
				  const char *tag_name,
				  struct type_class *type_class)
{
	struct type_class_field *field;
	unsigned long index;

	g_array_set_size(variant_class->fields, variant_class->fields->len + 1);
	index = variant_class->fields->len - 1;	/* last field (new) */
	field = &g_array_index(variant_class->fields, struct type_class_field, index);
	field->name = g_quark_from_string(tag_name);
	type_ref(type_class);
	field->type_class = type_class;
	/* Keep index in hash rather than pointer, because array can relocate */
	g_hash_table_insert(variant_class->fields_by_name,
			    (gpointer) (unsigned long) field->name,
			    (gpointer) index);
	/*
	 * Alignment of variant is based on the alignment of its currently
	 * selected choice, so we leave variant alignment as-is (statically
	 * speaking).
	 */
}

struct type_class_field *
struct_type_class_get_field_from_tag(struct type_class_variant *variant_class,
				     GQuark tag)
{
	unsigned long index;

	index = (unsigned long) g_hash_table_lookup(variant_class->fields_by_tag,
						    (gconstpointer) (unsigned long) tag);
	return &g_array_index(variant_class->fields, struct type_class_field, index);
}

/*
 * tag_instance is assumed to be an enumeration.
 */
int variant_type_set_tag(struct type_variant *variant,
			 struct type *enum_tag_instance)
{
	struct type_enum *_enum =
		container_of(struct type_enum, variant->enum_tag, p);
	struct type_class_enum *enum_class = _enum->_class;
	int missing_field = 0;
	unsigned long i;

	/*
	 * Strictly speaking, each enumerator must map to a field of the
	 * variant. However, we are even stricter here by requiring that each
	 * variant choice map to an enumerator too. We then validate that the
	 * number of enumerators equals the number of variant choices.
	 */
	if (variant->_class->fields->len != enum_get_nr_enumerators(enum_class))
		return -EPERM;

	for (i = 0; i < variant->_class->fields->len; i++) {
		struct type_class_field *field_class =
			&g_array_index(variant->_class->fields,
				       struct type_class_field, i);
		if (!enum_quark_to_range_set(enum_class, field_class->name)) {
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
	variant->enum_tag = enum_tag_instance;
	return 0;
}

/*
 * field returned only valid as long as the field structure is not appended to.
 */
struct field *
variant_type_get_current_field(struct type_variant *variant)
{
	struct type_enum *_enum =
		container_of(struct type_enum, variant->enum_tag, p);
	struct variat_type_class *variant_class = variant->_class;
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
	index = (unsigned long) g_hash_table_lookup(variant_class->fields_by_tag,
						    (gconstpointer) (unsigned long) tag);
	variant->current_field = &g_array_index(variant_class->fields, struct field, index);
	return variant->current_field;
}
