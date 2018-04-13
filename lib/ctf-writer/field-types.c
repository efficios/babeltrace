/*
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define BT_LOG_TAG "CTF-WRITER-FIELD-TYPES"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/utils-internal.h>
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/ctf-ir/field-path-internal.h>
#include <babeltrace/ctf-writer/field-types.h>
#include <babeltrace/ctf-writer/field-types-internal.h>
#include <babeltrace/ctf-writer/fields.h>
#include <babeltrace/ctf-writer/clock-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/endian-internal.h>
#include <babeltrace/assert-internal.h>
#include <float.h>
#include <inttypes.h>
#include <stdlib.h>

static
struct bt_ctf_field_type *bt_ctf_field_type_integer_copy(
		struct bt_ctf_field_type *ft);

static
struct bt_ctf_field_type *bt_ctf_field_type_enumeration_copy_recursive(
		struct bt_ctf_field_type *ft);

static
struct bt_ctf_field_type *bt_ctf_field_type_floating_point_copy(
		struct bt_ctf_field_type *ft);

static
struct bt_ctf_field_type *bt_ctf_field_type_structure_copy_recursive(
		struct bt_ctf_field_type *ft);

static
struct bt_ctf_field_type *bt_ctf_field_type_variant_copy_recursive(
		struct bt_ctf_field_type *ft);

static
struct bt_ctf_field_type *bt_ctf_field_type_array_copy_recursive(
		struct bt_ctf_field_type *ft);

static
struct bt_ctf_field_type *bt_ctf_field_type_sequence_copy_recursive(
		struct bt_ctf_field_type *type);

static
struct bt_ctf_field_type *bt_ctf_field_type_string_copy(
		struct bt_ctf_field_type *type);

static struct bt_field_type_common_methods bt_ctf_field_type_integer_methods = {
	.freeze = bt_field_type_common_generic_freeze,
	.validate = bt_field_type_common_integer_validate,
	.set_byte_order = bt_field_type_common_integer_set_byte_order,
	.copy = (bt_field_type_common_method_copy)
		bt_ctf_field_type_integer_copy,
	.compare = bt_field_type_common_integer_compare,
};

static struct bt_field_type_common_methods bt_ctf_field_type_floating_point_methods = {
	.freeze = bt_field_type_common_generic_freeze,
	.validate = NULL,
	.set_byte_order = bt_field_type_common_floating_point_set_byte_order,
	.copy = (bt_field_type_common_method_copy)
		bt_ctf_field_type_floating_point_copy,
	.compare = bt_field_type_common_floating_point_compare,
};

static struct bt_field_type_common_methods bt_ctf_field_type_enumeration_methods = {
	.freeze = bt_field_type_common_enumeration_freeze_recursive,
	.validate = bt_field_type_common_enumeration_validate_recursive,
	.set_byte_order = bt_field_type_common_enumeration_set_byte_order_recursive,
	.copy = (bt_field_type_common_method_copy)
		bt_ctf_field_type_enumeration_copy_recursive,
	.compare = bt_field_type_common_enumeration_compare_recursive,
};

static struct bt_field_type_common_methods bt_ctf_field_type_string_methods = {
	.freeze = bt_field_type_common_generic_freeze,
	.validate = NULL,
	.set_byte_order = NULL,
	.copy = (bt_field_type_common_method_copy)
		bt_ctf_field_type_string_copy,
	.compare = bt_field_type_common_string_compare,
};

static struct bt_field_type_common_methods bt_ctf_field_type_array_methods = {
	.freeze = bt_field_type_common_array_freeze_recursive,
	.validate = bt_field_type_common_array_validate_recursive,
	.set_byte_order = bt_field_type_common_array_set_byte_order_recursive,
	.copy = (bt_field_type_common_method_copy)
		bt_ctf_field_type_array_copy_recursive,
	.compare = bt_field_type_common_array_compare_recursive,
};

static struct bt_field_type_common_methods bt_ctf_field_type_sequence_methods = {
	.freeze = bt_field_type_common_sequence_freeze_recursive,
	.validate = bt_field_type_common_sequence_validate_recursive,
	.set_byte_order = bt_field_type_common_sequence_set_byte_order_recursive,
	.copy = (bt_field_type_common_method_copy)
		bt_ctf_field_type_sequence_copy_recursive,
	.compare = bt_field_type_common_sequence_compare_recursive,
};

static struct bt_field_type_common_methods bt_ctf_field_type_structure_methods = {
	.freeze = bt_field_type_common_structure_freeze_recursive,
	.validate = bt_field_type_common_structure_validate_recursive,
	.set_byte_order = bt_field_type_common_structure_set_byte_order_recursive,
	.copy = (bt_field_type_common_method_copy)
		bt_ctf_field_type_structure_copy_recursive,
	.compare = bt_field_type_common_structure_compare_recursive,
};

static struct bt_field_type_common_methods bt_ctf_field_type_variant_methods = {
	.freeze = bt_field_type_common_variant_freeze_recursive,
	.validate = bt_field_type_common_variant_validate_recursive,
	.set_byte_order = bt_field_type_common_variant_set_byte_order_recursive,
	.copy = (bt_field_type_common_method_copy)
		bt_ctf_field_type_variant_copy_recursive,
	.compare = bt_field_type_common_variant_compare_recursive,
};

typedef int (*bt_ctf_field_type_serialize_func)(struct bt_field_type_common *,
		struct metadata_context *);

BT_HIDDEN
int bt_ctf_field_type_serialize_recursive(struct bt_ctf_field_type *type,
		struct metadata_context *context)
{
	int ret;
	struct bt_field_type_common *type_common = (void *) type;
	bt_ctf_field_type_serialize_func serialize_func;

	BT_ASSERT(type);
	BT_ASSERT(context);

	/* Make sure field type is valid before serializing it */
	ret = bt_field_type_common_validate((void *) type);
	if (ret) {
		BT_LOGW("Cannot serialize field type's metadata: field type is invalid: "
			"addr=%p", type);
		goto end;
	}

	serialize_func = type_common->spec.writer.serialize_func;
	ret = serialize_func((void *) type, context);

end:
	return ret;
}

static
const char *get_encoding_string(enum bt_string_encoding encoding)
{
	const char *encoding_string;

	switch (encoding) {
	case BT_STRING_ENCODING_NONE:
		encoding_string = "none";
		break;
	case BT_STRING_ENCODING_ASCII:
		encoding_string = "ASCII";
		break;
	case BT_STRING_ENCODING_UTF8:
		encoding_string = "UTF8";
		break;
	default:
		encoding_string = "unknown";
		break;
	}

	return encoding_string;
}

static
const char *get_integer_base_string(enum bt_integer_base base)
{
	const char *base_string;

	switch (base) {
	case BT_INTEGER_BASE_DECIMAL:
	case BT_INTEGER_BASE_UNSPECIFIED:
		base_string = "decimal";
		break;
	case BT_INTEGER_BASE_HEXADECIMAL:
		base_string = "hexadecimal";
		break;
	case BT_INTEGER_BASE_OCTAL:
		base_string = "octal";
		break;
	case BT_INTEGER_BASE_BINARY:
		base_string = "binary";
		break;
	default:
		base_string = "unknown";
		break;
	}

	return base_string;
}

static
void append_field_name(struct metadata_context *context,
		const char *name)
{
	g_string_append_c(context->string, ' ');

	if (!bt_identifier_is_valid(name) || *name == '_') {
		g_string_append_c(context->string, '_');
	}

	g_string_append(context->string, name);
}

static
int bt_ctf_field_type_integer_serialize(struct bt_field_type_common *type,
		struct metadata_context *context)
{
	struct bt_field_type_common_integer *integer = BT_FROM_COMMON(type);
	int ret = 0;

	BT_LOGD("Serializing CTF writer integer field type's metadata: "
		"ft-addr=%p, metadata-context-addr=%p", type, context);
	g_string_append_printf(context->string,
		"integer { size = %u; align = %u; signed = %s; encoding = %s; base = %s; byte_order = %s",
		integer->size, type->alignment,
		(integer->is_signed ? "true" : "false"),
		get_encoding_string(integer->encoding),
		get_integer_base_string(integer->base),
		get_byte_order_string(integer->user_byte_order));
	if (integer->mapped_clock_class) {
		const char *clock_name = bt_clock_class_get_name(
			integer->mapped_clock_class);

		BT_ASSERT(clock_name);
		g_string_append_printf(context->string,
			"; map = clock.%s.value", clock_name);
	}

	g_string_append(context->string, "; }");
	return ret;
}

static
int bt_ctf_field_type_enumeration_serialize_recursive(
		struct bt_field_type_common *type,
		struct metadata_context *context)
{
	size_t entry;
	int ret;
	struct bt_field_type_common_enumeration *enumeration =
		BT_FROM_COMMON(type);
	struct bt_field_type_common *container_type;
	int container_signed;

	BT_LOGD("Serializing CTF writer enumeration field type's metadata: "
		"ft-addr=%p, metadata-context-addr=%p", type, context);
	container_type =
		bt_field_type_common_enumeration_borrow_container_field_type(type);
	BT_ASSERT(container_type);
	container_signed = bt_field_type_common_integer_is_signed(
		container_type);
	BT_ASSERT(container_signed >= 0);
	g_string_append(context->string, "enum : ");
	BT_LOGD_STR("Serializing CTF writer enumeration field type's container field type's metadata.");
	ret = bt_ctf_field_type_serialize_recursive(
		(void *) enumeration->container_ft, context);
	if (ret) {
		BT_LOGW("Cannot serialize CTF writer enumeration field type's container field type's metadata: "
			"container-ft-addr=%p", enumeration->container_ft);
		goto end;
	}

	g_string_append(context->string, " { ");
	for (entry = 0; entry < enumeration->entries->len; entry++) {
		struct enumeration_mapping *mapping =
			enumeration->entries->pdata[entry];
		const char *label = g_quark_to_string(mapping->string);

		g_string_append(context->string, "\"");

		if (!bt_identifier_is_valid(label) || label[0] == '_') {
			g_string_append(context->string, "_");
		}

		g_string_append_printf(context->string, "%s\" = ", label);

		if (container_signed) {
			if (mapping->range_start._signed ==
				mapping->range_end._signed) {
				g_string_append_printf(context->string,
					"%" PRId64,
					mapping->range_start._signed);
			} else {
				g_string_append_printf(context->string,
					"%" PRId64 " ... %" PRId64,
					mapping->range_start._signed,
					mapping->range_end._signed);
			}
		} else {
			if (mapping->range_start._unsigned ==
				mapping->range_end._unsigned) {
				g_string_append_printf(context->string,
					"%" PRIu64,
					mapping->range_start._unsigned);
			} else {
				g_string_append_printf(context->string,
					"%" PRIu64 " ... %" PRIu64,
					mapping->range_start._unsigned,
					mapping->range_end._unsigned);
			}
		}

		g_string_append(context->string,
			((entry != (enumeration->entries->len - 1)) ?
			", " : " }"));
	}

	if (context->field_name->len) {
		append_field_name(context,
			context->field_name->str);
		g_string_assign(context->field_name, "");
	}

end:
	return ret;
}

static
int bt_ctf_field_type_floating_point_serialize(struct bt_field_type_common *type,
		struct metadata_context *context)
{
	struct bt_field_type_common_floating_point *floating_point =
		BT_FROM_COMMON(type);

	BT_LOGD("Serializing CTF writer floating point number field type's metadata: "
		"ft-addr=%p, metadata-context-addr=%p", type, context);
	g_string_append_printf(context->string,
		"floating_point { exp_dig = %u; mant_dig = %u; byte_order = %s; align = %u; }",
		floating_point->exp_dig,
		floating_point->mant_dig,
		get_byte_order_string(floating_point->user_byte_order),
		type->alignment);
	return 0;
}

static
int bt_ctf_field_type_structure_serialize_recursive(
		struct bt_field_type_common *type,
		struct metadata_context *context)
{
	size_t i;
	unsigned int indent;
	int ret = 0;
	struct bt_field_type_common_structure *structure = BT_FROM_COMMON(type);
	GString *structure_field_name = context->field_name;

	BT_LOGD("Serializing CTF writer structure field type's metadata: "
		"ft-addr=%p, metadata-context-addr=%p", type, context);
	context->field_name = g_string_new("");

	context->current_indentation_level++;
	g_string_append(context->string, "struct {\n");

	for (i = 0; i < structure->fields->len; i++) {
		struct structure_field_common *field = structure->fields->pdata[i];

		BT_LOGD("Serializing CTF writer structure field type's field metadata: "
			"index=%zu, "
			"field-ft-addr=%p, field-name=\"%s\"",
			i, field, g_quark_to_string(field->name));

		for (indent = 0; indent < context->current_indentation_level;
			indent++) {
			g_string_append_c(context->string, '\t');
		}

		g_string_assign(context->field_name,
			g_quark_to_string(field->name));
		ret = bt_ctf_field_type_serialize_recursive(
			(void *) field->type, context);
		if (ret) {
			BT_LOGW("Cannot serialize CTF writer structure field type's field's metadata: "
				"index=%zu, "
				"field-ft-addr=%p, field-name=\"%s\"",
				i, field->type,
				g_quark_to_string(field->name));
			goto end;
		}

		if (context->field_name->len) {
			append_field_name(context,
				context->field_name->str);
		}
		g_string_append(context->string, ";\n");
	}

	context->current_indentation_level--;
	for (indent = 0; indent < context->current_indentation_level;
		indent++) {
		g_string_append_c(context->string, '\t');
	}

	g_string_append_printf(context->string, "} align(%u)",
		 type->alignment);

end:
	g_string_free(context->field_name, TRUE);
	context->field_name = structure_field_name;
	return ret;
}

static
int bt_ctf_field_type_variant_serialize_recursive(
		struct bt_field_type_common *type,
		struct metadata_context *context)
{
	size_t i;
	unsigned int indent;
	int ret = 0;
	struct bt_field_type_common_variant *variant = BT_FROM_COMMON(type);
	GString *variant_field_name = context->field_name;

	BT_LOGD("Serializing CTF writer variant field type's metadata: "
		"ft-addr=%p, metadata-context-addr=%p", type, context);
	context->field_name = g_string_new("");
	if (variant->tag_name->len > 0) {
		g_string_append(context->string, "variant <");
	        append_field_name(context, variant->tag_name->str);
		g_string_append(context->string, "> {\n");
	} else {
		g_string_append(context->string, "variant {\n");
	}

	context->current_indentation_level++;
	for (i = 0; i < variant->fields->len; i++) {
		struct structure_field_common *field =
			variant->fields->pdata[i];

		BT_LOGD("Serializing CTF writer variant field type's field metadata: "
			"index=%zu, "
			"field-ft-addr=%p, field-name=\"%s\"",
			i, field, g_quark_to_string(field->name));

		g_string_assign(context->field_name,
			g_quark_to_string(field->name));
		for (indent = 0; indent < context->current_indentation_level;
			indent++) {
			g_string_append_c(context->string, '\t');
		}

		g_string_assign(context->field_name,
			g_quark_to_string(field->name));
		ret = bt_ctf_field_type_serialize_recursive(
			(void *) field->type, context);
		if (ret) {
			BT_LOGW("Cannot serialize CTF writer variant field type's field's metadata: "
				"index=%zu, "
				"field-ft-addr=%p, field-name=\"%s\"",
				i, field->type,
				g_quark_to_string(field->name));
			goto end;
		}

		if (context->field_name->len) {
			append_field_name(context,
				context->field_name->str);
			g_string_append_c(context->string, ';');
		}

		g_string_append_c(context->string, '\n');
	}

	context->current_indentation_level--;
	for (indent = 0; indent < context->current_indentation_level;
		indent++) {
		g_string_append_c(context->string, '\t');
	}

	g_string_append(context->string, "}");

end:
	g_string_free(context->field_name, TRUE);
	context->field_name = variant_field_name;
	return ret;
}

static
int bt_ctf_field_type_array_serialize_recursive(
		struct bt_field_type_common *type,
		struct metadata_context *context)
{
	int ret = 0;
	struct bt_field_type_common_array *array = BT_FROM_COMMON(type);

	BT_LOGD("Serializing CTF writer array field type's metadata: "
		"ft-addr=%p, metadata-context-addr=%p", type, context);
	BT_LOGD_STR("Serializing CTF writer array field type's element field type's metadata.");
	ret = bt_ctf_field_type_serialize_recursive(
		(void *) array->element_ft, context);
	if (ret) {
		BT_LOGW("Cannot serialize CTF writer array field type's element field type's metadata: "
			"element-ft-addr=%p", array->element_ft);
		goto end;
	}

	if (context->field_name->len) {
		append_field_name(context,
			context->field_name->str);

		g_string_append_printf(context->string, "[%u]", array->length);
		g_string_assign(context->field_name, "");
	} else {
		g_string_append_printf(context->string, "[%u]", array->length);
	}

end:
	return ret;
}

static
int bt_ctf_field_type_sequence_serialize_recursive(
		struct bt_field_type_common *type,
		struct metadata_context *context)
{
	int ret = 0;
	struct bt_field_type_common_sequence *sequence = BT_FROM_COMMON(type);

	BT_LOGD("Serializing CTF writer sequence field type's metadata: "
		"ft-addr=%p, metadata-context-addr=%p", type, context);
	BT_LOGD_STR("Serializing CTF writer sequence field type's element field type's metadata.");
	ret = bt_ctf_field_type_serialize_recursive(
		(void *) sequence->element_ft, context);
	if (ret) {
		BT_LOGW("Cannot serialize CTF writer sequence field type's element field type's metadata: "
			"element-ft-addr=%p", sequence->element_ft);
		goto end;
	}

	if (context->field_name->len) {
		append_field_name(context, context->field_name->str);
		g_string_assign(context->field_name, "");
	}
	g_string_append(context->string, "[");
	append_field_name(context, sequence->length_field_name->str);
	g_string_append(context->string, "]");

end:
	return ret;
}

static
int bt_ctf_field_type_string_serialize(struct bt_field_type_common *type,
		struct metadata_context *context)
{
	struct bt_field_type_common_string *string = BT_FROM_COMMON(type);

	BT_LOGD("Serializing CTF writer string field type's metadata: "
		"ft-addr=%p, metadata-context-addr=%p", type, context);
	g_string_append_printf(context->string,
		"string { encoding = %s; }",
		get_encoding_string(string->encoding));
	return 0;
}

struct bt_ctf_field_type *bt_ctf_field_type_integer_create(unsigned int size)
{
	struct bt_field_type_common_integer *integer = NULL;

	BT_LOGD("Creating CTF writer integer field type object: size=%u", size);

	if (size == 0 || size > 64) {
		BT_LOGW("Invalid parameter: size must be between 1 and 64: "
			"size=%u", size);
		goto error;
	}

	integer = g_new0(struct bt_field_type_common_integer, 1);
	if (!integer) {
		BT_LOGE_STR("Failed to allocate one integer field type.");
		goto error;
	}

	bt_field_type_common_integer_initialize(BT_TO_COMMON(integer),
		size, bt_field_type_common_integer_destroy,
		&bt_ctf_field_type_integer_methods);
	integer->common.spec.writer.serialize_func =
		bt_ctf_field_type_integer_serialize;
	BT_LOGD("Created CTF writer integer field type object: addr=%p, size=%u",
		integer, size);
	goto end;

error:
	BT_PUT(integer);

end:
	return (void *) integer;
}

int bt_ctf_field_type_integer_get_size(struct bt_ctf_field_type *ft)
{
	return bt_field_type_common_integer_get_size((void *) ft);
}

bt_bool bt_ctf_field_type_integer_is_signed(struct bt_ctf_field_type *ft)
{
	return bt_field_type_common_integer_is_signed((void *) ft);
}

int bt_ctf_field_type_integer_set_is_signed(struct bt_ctf_field_type *ft,
		bt_bool is_signed)
{
	return bt_field_type_common_integer_set_is_signed((void *) ft,
		is_signed);
}

int bt_ctf_field_type_integer_set_size(struct bt_ctf_field_type *ft,
		unsigned int size)
{
	return bt_field_type_common_integer_set_size((void *) ft, size);
}

enum bt_ctf_integer_base bt_ctf_field_type_integer_get_base(
		struct bt_ctf_field_type *ft)
{
	return (int) bt_field_type_common_integer_get_base((void *) ft);
}

int bt_ctf_field_type_integer_set_base(struct bt_ctf_field_type *ft,
		enum bt_ctf_integer_base base)
{
	return bt_field_type_common_integer_set_base((void *) ft,
		(int) base);
}

enum bt_ctf_string_encoding bt_ctf_field_type_integer_get_encoding(
		struct bt_ctf_field_type *ft)
{
	return (int) bt_field_type_common_integer_get_encoding((void *) ft);
}

int bt_ctf_field_type_integer_set_encoding(struct bt_ctf_field_type *ft,
		enum bt_ctf_string_encoding encoding)
{
	return bt_field_type_common_integer_set_encoding((void *) ft,
		(int) encoding);
}

struct bt_ctf_clock_class *bt_ctf_field_type_integer_get_mapped_clock_class(
		struct bt_ctf_field_type *ft)
{
	return bt_get(bt_field_type_common_integer_borrow_mapped_clock_class(
		(void *) ft));
}

int bt_ctf_field_type_integer_set_mapped_clock_class(
		struct bt_ctf_field_type *ft,
		struct bt_ctf_clock_class *clock_class)
{
	return bt_field_type_common_integer_set_mapped_clock_class((void *) ft,
		BT_TO_COMMON(clock_class));
}

int bt_ctf_field_type_enumeration_signed_get_mapping_by_index(
		struct bt_ctf_field_type *ft, uint64_t index,
		const char **mapping_name, int64_t *range_begin,
		int64_t *range_end)
{
	return bt_field_type_common_enumeration_signed_get_mapping_by_index(
		(void *) ft, index, mapping_name, range_begin, range_end);
}

int bt_ctf_field_type_enumeration_unsigned_get_mapping_by_index(
		struct bt_ctf_field_type *ft, uint64_t index,
		const char **mapping_name, uint64_t *range_begin,
		uint64_t *range_end)
{
	return bt_field_type_common_enumeration_unsigned_get_mapping_by_index(
		(void *) ft, index, mapping_name, range_begin, range_end);
}

struct bt_ctf_field_type *bt_ctf_field_type_enumeration_create(
		struct bt_ctf_field_type *container_ft)
{
	struct bt_field_type_common_enumeration *enumeration = NULL;
	struct bt_field_type_common *int_ft = (void *) container_ft;

	BT_LOGD("Creating CTF writer enumeration field type object: int-ft-addr=%p",
		container_ft);

	if (!container_ft) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		goto error;
	}

	if (int_ft->id != BT_FIELD_TYPE_ID_INTEGER) {
		BT_LOGW("Invalid parameter: container field type is not an integer field type: "
			"container-ft-addr=%p, container-ft-id=%s",
			container_ft, bt_common_field_type_id_string(int_ft->id));
		goto error;
	}

	enumeration = g_new0(struct bt_field_type_common_enumeration, 1);
	if (!enumeration) {
		BT_LOGE_STR("Failed to allocate one enumeration field type.");
		goto error;
	}

	bt_field_type_common_enumeration_initialize(BT_TO_COMMON(enumeration),
		int_ft, bt_field_type_common_enumeration_destroy_recursive,
		&bt_ctf_field_type_enumeration_methods);
	enumeration->common.spec.writer.serialize_func =
		bt_ctf_field_type_enumeration_serialize_recursive;
	BT_LOGD("Created CTF writer enumeration field type object: addr=%p, "
		"int-ft-addr=%p, int-ft-size=%u",
		enumeration, container_ft,
		bt_ctf_field_type_integer_get_size(container_ft));
	goto end;

error:
	BT_PUT(enumeration);

end:
	return (void *) enumeration;
}

struct bt_ctf_field_type *bt_ctf_field_type_enumeration_get_container_field_type(
		struct bt_ctf_field_type *ft)
{
	return bt_get(
		bt_field_type_common_enumeration_borrow_container_field_type(
			(void *) ft));
}

int bt_ctf_field_type_enumeration_signed_add_mapping(
		struct bt_ctf_field_type *ft, const char *string,
		int64_t range_start, int64_t range_end)
{
	return bt_field_type_common_enumeration_signed_add_mapping(
		(void *) ft, string, range_start, range_end);
}

int bt_ctf_field_type_enumeration_unsigned_add_mapping(
		struct bt_ctf_field_type *ft, const char *string,
		uint64_t range_start, uint64_t range_end)
{
	return bt_field_type_common_enumeration_unsigned_add_mapping(
		(void *) ft, string, range_start, range_end);
}

int64_t bt_ctf_field_type_enumeration_get_mapping_count(
		struct bt_ctf_field_type *ft)
{
	return bt_field_type_common_enumeration_get_mapping_count((void *) ft);
}

struct bt_ctf_field_type *bt_ctf_field_type_floating_point_create(void)
{
	struct bt_field_type_common_floating_point *floating_point =
		g_new0(struct bt_field_type_common_floating_point, 1);

	BT_LOGD_STR("Creating CTF writer floating point number field type object.");

	if (!floating_point) {
		BT_LOGE_STR("Failed to allocate one floating point number field type.");
		goto end;
	}

	bt_field_type_common_floating_point_initialize(
		BT_TO_COMMON(floating_point),
		bt_field_type_common_floating_point_destroy,
		&bt_ctf_field_type_floating_point_methods);
	floating_point->common.spec.writer.serialize_func =
		bt_ctf_field_type_floating_point_serialize;
	BT_LOGD("Created CTF writer floating point number field type object: addr=%p, "
		"exp-size=%u, mant-size=%u", floating_point,
		floating_point->exp_dig, floating_point->mant_dig);

end:
	return (void *) floating_point;
}

int bt_ctf_field_type_floating_point_get_exponent_digits(
		struct bt_ctf_field_type *ft)
{
	return bt_field_type_common_floating_point_get_exponent_digits(
		(void *) ft);
}

int bt_ctf_field_type_floating_point_set_exponent_digits(
		struct bt_ctf_field_type *ft, unsigned int exponent_digits)
{
	return bt_field_type_common_floating_point_set_exponent_digits(
		(void *) ft, exponent_digits);
}

int bt_ctf_field_type_floating_point_get_mantissa_digits(
		struct bt_ctf_field_type *ft)
{
	return bt_field_type_common_floating_point_get_mantissa_digits(
		(void *) ft);
}

int bt_ctf_field_type_floating_point_set_mantissa_digits(
		struct bt_ctf_field_type *ft, unsigned int mantissa_digits)
{
	return bt_field_type_common_floating_point_set_mantissa_digits(
		(void *) ft, mantissa_digits);
}

struct bt_ctf_field_type *bt_ctf_field_type_structure_create(void)
{
	struct bt_field_type_common_structure *structure =
		g_new0(struct bt_field_type_common_structure, 1);

	BT_LOGD_STR("Creating CTF writer structure field type object.");

	if (!structure) {
		BT_LOGE_STR("Failed to allocate one structure field type.");
		goto error;
	}

	bt_field_type_common_structure_initialize(BT_TO_COMMON(structure),
		bt_field_type_common_structure_destroy_recursive,
		&bt_ctf_field_type_structure_methods);
	structure->common.spec.writer.serialize_func =
		bt_ctf_field_type_structure_serialize_recursive;
	BT_LOGD("Created CTF writer structure field type object: addr=%p",
		structure);
	goto end;

error:
	BT_PUT(structure);

end:
	return (void *) structure;
}

int bt_ctf_field_type_structure_add_field(struct bt_ctf_field_type *ft,
		struct bt_ctf_field_type *field_type,
		const char *field_name)
{
	return bt_field_type_common_structure_add_field((void *) ft,
		(void *) field_type, field_name);
}

int64_t bt_ctf_field_type_structure_get_field_count(struct bt_ctf_field_type *ft)
{
	return bt_field_type_common_structure_get_field_count((void *) ft);
}

int bt_ctf_field_type_structure_get_field_by_index(
		struct bt_ctf_field_type *ft,
		const char **field_name,
		struct bt_ctf_field_type **field_type, uint64_t index)
{
	int ret = bt_field_type_common_structure_borrow_field_by_index(
		(void *) ft, field_name, (void *) field_type, index);

	if (ret == 0 && field_type) {
		bt_get(*field_type);
	}

	return ret;
}

struct bt_ctf_field_type *bt_ctf_field_type_structure_get_field_type_by_name(
		struct bt_ctf_field_type *ft, const char *name)
{
	return bt_get(bt_field_type_common_structure_borrow_field_type_by_name(
		(void *) ft, name));
}

struct bt_ctf_field_type *bt_ctf_field_type_variant_create(
	struct bt_ctf_field_type *tag_ft, const char *tag_name)
{
	struct bt_field_type_common_variant *var_ft = NULL;

	BT_LOGD("Creating CTF writer variant field type object: "
		"tag-ft-addr=%p, tag-field-name=\"%s\"",
		tag_ft, tag_name);

	if (tag_name && !bt_identifier_is_valid(tag_name)) {
		BT_LOGW("Invalid parameter: tag field name is not a valid CTF identifier: "
			"tag-ft-addr=%p, tag-field-name=\"%s\"",
			tag_ft, tag_name);
		goto error;
	}

	var_ft = g_new0(struct bt_field_type_common_variant, 1);
	if (!var_ft) {
		BT_LOGE_STR("Failed to allocate one variant field type.");
		goto error;
	}

	bt_field_type_common_variant_initialize(BT_TO_COMMON(var_ft),
		(void *) tag_ft, tag_name,
		bt_field_type_common_variant_destroy_recursive,
		&bt_ctf_field_type_variant_methods);
	var_ft->common.spec.writer.serialize_func =
		bt_ctf_field_type_variant_serialize_recursive;
	BT_LOGD("Created CTF writer variant field type object: addr=%p, "
		"tag-ft-addr=%p, tag-field-name=\"%s\"",
		var_ft, tag_ft, tag_name);
	goto end;

error:
	BT_PUT(var_ft);

end:
	return (void *) var_ft;
}

struct bt_ctf_field_type *bt_ctf_field_type_variant_get_tag_field_type(
		struct bt_ctf_field_type *ft)
{
	return bt_get(bt_field_type_common_variant_borrow_tag_field_type(
		(void *) ft));
}

const char *bt_ctf_field_type_variant_get_tag_name(struct bt_ctf_field_type *ft)
{
	return bt_field_type_common_variant_get_tag_name((void *) ft);
}

int bt_ctf_field_type_variant_set_tag_name(
		struct bt_ctf_field_type *ft, const char *name)
{
	return bt_field_type_common_variant_set_tag_name((void *) ft, name);
}

int bt_ctf_field_type_variant_add_field(struct bt_ctf_field_type *ft,
		struct bt_ctf_field_type *field_type,
		const char *field_name)
{
	return bt_field_type_common_variant_add_field((void *) ft,
		(void *) field_type, field_name);
}

struct bt_ctf_field_type *bt_ctf_field_type_variant_get_field_type_by_name(
		struct bt_ctf_field_type *ft,
		const char *field_name)
{
	return bt_get(bt_field_type_common_variant_borrow_field_type_by_name(
		(void *) ft, field_name));
}

struct bt_ctf_field_type *bt_ctf_field_type_variant_get_field_type_from_tag(
		struct bt_ctf_field_type *ft,
		struct bt_ctf_field *tag_field)
{
	return bt_get(bt_field_type_common_variant_borrow_field_type_from_tag(
		(void *) ft, (void *) tag_field,
		(bt_field_common_create_func) bt_field_create));
}

int64_t bt_ctf_field_type_variant_get_field_count(struct bt_ctf_field_type *ft)
{
	return bt_field_type_common_variant_get_field_count((void *) ft);
}

int bt_ctf_field_type_variant_get_field_by_index(struct bt_ctf_field_type *ft,
		const char **field_name, struct bt_ctf_field_type **field_type,
		uint64_t index)
{
	int ret = bt_field_type_common_variant_borrow_field_by_index(
		(void *) ft, field_name, (void *) field_type, index);

	if (ret == 0 && field_type) {
		bt_get(*field_type);
	}

	return ret;
}

struct bt_ctf_field_type *bt_ctf_field_type_array_create(
		struct bt_ctf_field_type *element_ft, unsigned int length)
{
	struct bt_field_type_common_array *array = NULL;

	BT_LOGD("Creating CTF writer array field type object: element-ft-addr=%p, "
		"length=%u", element_ft, length);

	if (!element_ft) {
		BT_LOGW_STR("Invalid parameter: element field type is NULL.");
		goto error;
	}

	if (length == 0) {
		BT_LOGW_STR("Invalid parameter: length is zero.");
		goto error;
	}

	array = g_new0(struct bt_field_type_common_array, 1);
	if (!array) {
		BT_LOGE_STR("Failed to allocate one array field type.");
		goto error;
	}

	bt_field_type_common_array_initialize(BT_TO_COMMON(array),
		(void *) element_ft, length,
		bt_field_type_common_array_destroy_recursive,
		&bt_ctf_field_type_array_methods);
	array->common.spec.writer.serialize_func =
		bt_ctf_field_type_array_serialize_recursive;
	BT_LOGD("Created CTF writer array field type object: addr=%p, "
		"element-ft-addr=%p, length=%u",
		array, element_ft, length);
	goto end;

error:
	BT_PUT(array);

end:
	return (void *) array;
}

struct bt_ctf_field_type *bt_ctf_field_type_array_get_element_field_type(
		struct bt_ctf_field_type *ft)
{
	return bt_get(bt_field_type_common_array_borrow_element_field_type(
		(void *) ft));
}

int64_t bt_ctf_field_type_array_get_length(struct bt_ctf_field_type *ft)
{
	return bt_field_type_common_array_get_length((void *) ft);
}

struct bt_ctf_field_type *bt_ctf_field_type_sequence_create(
		struct bt_ctf_field_type *element_ft,
		const char *length_field_name)
{
	struct bt_field_type_common_sequence *sequence = NULL;

	BT_LOGD("Creating CTF writer sequence field type object: element-ft-addr=%p, "
		"length-field-name=\"%s\"", element_ft, length_field_name);

	if (!element_ft) {
		BT_LOGW_STR("Invalid parameter: element field type is NULL.");
		goto error;
	}

	if (!bt_identifier_is_valid(length_field_name)) {
		BT_LOGW("Invalid parameter: length field name is not a valid CTF identifier: "
			"length-field-name=\"%s\"", length_field_name);
		goto error;
	}

	sequence = g_new0(struct bt_field_type_common_sequence, 1);
	if (!sequence) {
		BT_LOGE_STR("Failed to allocate one sequence field type.");
		goto error;
	}

	bt_field_type_common_sequence_initialize(BT_TO_COMMON(sequence),
		(void *) element_ft, length_field_name,
		bt_field_type_common_sequence_destroy_recursive,
		&bt_ctf_field_type_sequence_methods);
	sequence->common.spec.writer.serialize_func =
		bt_ctf_field_type_sequence_serialize_recursive;
	BT_LOGD("Created CTF writer sequence field type object: addr=%p, "
		"element-ft-addr=%p, length-field-name=\"%s\"",
		sequence, element_ft, length_field_name);
	goto end;

error:
	BT_PUT(sequence);

end:
	return (void *) sequence;
}

struct bt_ctf_field_type *bt_ctf_field_type_sequence_get_element_field_type(
		struct bt_ctf_field_type *ft)
{
	return bt_get(bt_field_type_common_sequence_borrow_element_field_type(
		(void *) ft));
}

const char *bt_ctf_field_type_sequence_get_length_field_name(
		struct bt_ctf_field_type *ft)
{
	return bt_field_type_common_sequence_get_length_field_name((void *) ft);
}

struct bt_ctf_field_type *bt_ctf_field_type_string_create(void)
{
	struct bt_field_type_common_string *string =
		g_new0(struct bt_field_type_common_string, 1);

	BT_LOGD_STR("Creating CTF writer string field type object.");

	if (!string) {
		BT_LOGE_STR("Failed to allocate one string field type.");
		return NULL;
	}

	bt_field_type_common_string_initialize(BT_TO_COMMON(string),
		bt_field_type_common_string_destroy,
		&bt_ctf_field_type_string_methods);
	string->common.spec.writer.serialize_func =
		bt_ctf_field_type_string_serialize;
	BT_LOGD("Created CTF writer string field type object: addr=%p", string);
	return (void *) string;
}

enum bt_ctf_string_encoding bt_ctf_field_type_string_get_encoding(
		struct bt_ctf_field_type *ft)
{
	return (int) bt_field_type_common_string_get_encoding((void *) ft);
}

int bt_ctf_field_type_string_set_encoding(struct bt_ctf_field_type *ft,
		enum bt_ctf_string_encoding encoding)
{
	return bt_field_type_common_string_set_encoding((void *) ft,
		(int) encoding);
}

int bt_ctf_field_type_get_alignment(struct bt_ctf_field_type *ft)
{
	return bt_field_type_common_get_alignment((void *) ft);
}

int bt_ctf_field_type_set_alignment(struct bt_ctf_field_type *ft,
		unsigned int alignment)
{
	return bt_field_type_common_set_alignment((void *) ft, alignment);
}

enum bt_ctf_byte_order bt_ctf_field_type_get_byte_order(
		struct bt_ctf_field_type *ft)
{
	return (int) bt_field_type_common_get_byte_order((void *) ft);
}

int bt_ctf_field_type_set_byte_order(struct bt_ctf_field_type *ft,
		enum bt_ctf_byte_order byte_order)
{
	return bt_field_type_common_set_byte_order((void *) ft,
		(int) byte_order);
}

enum bt_ctf_field_type_id bt_ctf_field_type_get_type_id(
		struct bt_ctf_field_type *ft)
{
	return (int) bt_field_type_common_get_type_id((void *) ft);
}

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_copy(struct bt_ctf_field_type *ft)
{
	return (void *) bt_field_type_common_copy((void *) ft);
}

static
struct bt_ctf_field_type *bt_ctf_field_type_integer_copy(
		struct bt_ctf_field_type *ft)
{
	struct bt_field_type_common_integer *int_ft = (void *) ft;
	struct bt_field_type_common_integer *copy_ft;

	BT_LOGD("Copying CTF writer integer field type's: addr=%p", ft);
	copy_ft = (void *) bt_ctf_field_type_integer_create(int_ft->size);
	if (!copy_ft) {
		BT_LOGE_STR("Cannot create CTF writer integer field type.");
		goto end;
	}

	copy_ft->mapped_clock_class = bt_get(int_ft->mapped_clock_class);
	copy_ft->user_byte_order = int_ft->user_byte_order;
	copy_ft->is_signed = int_ft->is_signed;
	copy_ft->size = int_ft->size;
	copy_ft->base = int_ft->base;
	copy_ft->encoding = int_ft->encoding;
	BT_LOGD("Copied CTF writer integer field type: original-ft-addr=%p, copy-ft-addr=%p",
		ft, copy_ft);

end:
	return (void *) copy_ft;
}

static
struct bt_ctf_field_type *bt_ctf_field_type_enumeration_copy_recursive(
		struct bt_ctf_field_type *ft)
{
	size_t i;
	struct bt_field_type_common_enumeration *enum_ft = (void *) ft;
	struct bt_field_type_common_enumeration *copy_ft = NULL;
	struct bt_field_type_common_enumeration *container_copy_ft;

	BT_LOGD("Copying CTF writer enumeration field type's: addr=%p", ft);

	/* Copy the source enumeration's container */
	BT_LOGD_STR("Copying CTF writer enumeration field type's container field type.");
	container_copy_ft = BT_FROM_COMMON(bt_field_type_common_copy(
		BT_TO_COMMON(enum_ft->container_ft)));
	if (!container_copy_ft) {
		BT_LOGE_STR("Cannot copy CTF writer enumeration field type's container field type.");
		goto end;
	}

	copy_ft = (void *) bt_ctf_field_type_enumeration_create(
		(void *) container_copy_ft);
	if (!copy_ft) {
		BT_LOGE_STR("Cannot create CTF writer enumeration field type.");
		goto end;
	}

	/* Copy all enumaration entries */
	for (i = 0; i < enum_ft->entries->len; i++) {
		struct enumeration_mapping *mapping = g_ptr_array_index(
			enum_ft->entries, i);
		struct enumeration_mapping *copy_mapping = g_new0(
			struct enumeration_mapping, 1);

		if (!copy_mapping) {
			BT_LOGE_STR("Failed to allocate one enumeration mapping.");
			goto error;
		}

		*copy_mapping = *mapping;
		g_ptr_array_add(copy_ft->entries, copy_mapping);
	}

	BT_LOGD("Copied CTF writer enumeration field type: original-ft-addr=%p, copy-ft-addr=%p",
		ft, copy_ft);

end:
	bt_put(container_copy_ft);
	return (void *) copy_ft;

error:
	bt_put(container_copy_ft);
        BT_PUT(copy_ft);
	return (void *) copy_ft;
}

static
struct bt_ctf_field_type *bt_ctf_field_type_floating_point_copy(
		struct bt_ctf_field_type *ft)
{
	struct bt_field_type_common_floating_point *flt_ft = BT_FROM_COMMON(ft);
	struct bt_field_type_common_floating_point *copy_ft;

	BT_LOGD("Copying CTF writer floating point number field type's: addr=%p", ft);
	copy_ft = (void *) bt_ctf_field_type_floating_point_create();
	if (!copy_ft) {
		BT_LOGE_STR("Cannot create CTF writer floating point number field type.");
		goto end;
	}

	copy_ft->user_byte_order = flt_ft->user_byte_order;
	copy_ft->exp_dig = flt_ft->exp_dig;
	copy_ft->mant_dig = flt_ft->mant_dig;
	BT_LOGD("Copied CTF writer floating point number field type: original-ft-addr=%p, copy-ft-addr=%p",
		ft, copy_ft);

end:
	return (void *) copy_ft;
}

static
struct bt_ctf_field_type *bt_ctf_field_type_structure_copy_recursive(
		struct bt_ctf_field_type *ft)
{
	int64_t i;
	GHashTableIter iter;
	gpointer key, value;
	struct bt_field_type_common_structure *struct_ft = (void *) ft;
	struct bt_field_type_common_structure *copy_ft;

	BT_LOGD("Copying CTF writer structure field type's: addr=%p", ft);
	copy_ft = (void *) bt_ctf_field_type_structure_create();
	if (!copy_ft) {
		BT_LOGE_STR("Cannot create CTF writer structure field type.");
		goto end;
	}

	/* Copy field_name_to_index */
	g_hash_table_iter_init(&iter, struct_ft->field_name_to_index);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		g_hash_table_insert(copy_ft->field_name_to_index,
			key, value);
	}

	for (i = 0; i < struct_ft->fields->len; i++) {
		struct structure_field_common *entry, *copy_entry;
		struct bt_field_type_common *field_ft_copy;

		entry = g_ptr_array_index(struct_ft->fields, i);
		BT_LOGD("Copying CTF writer structure field type's field: "
			"index=%" PRId64 ", "
			"field-ft-addr=%p, field-name=\"%s\"",
			i, entry, g_quark_to_string(entry->name));
		copy_entry = g_new0(struct structure_field_common, 1);
		if (!copy_entry) {
			BT_LOGE_STR("Failed to allocate one structure field type field.");
			goto error;
		}

		field_ft_copy = (void *) bt_ctf_field_type_copy(
			(void *) entry->type);
		if (!field_ft_copy) {
			BT_LOGE("Cannot copy CTF writer structure field type's field: "
				"index=%" PRId64 ", "
				"field-ft-addr=%p, field-name=\"%s\"",
				i, entry, g_quark_to_string(entry->name));
			g_free(copy_entry);
			goto error;
		}

		copy_entry->name = entry->name;
		copy_entry->type = field_ft_copy;
		g_ptr_array_add(copy_ft->fields, copy_entry);
	}

	BT_LOGD("Copied CTF writer structure field type: original-ft-addr=%p, copy-ft-addr=%p",
		ft, copy_ft);

end:
	return (void *) copy_ft;

error:
        BT_PUT(copy_ft);
	return NULL;
}

static
struct bt_ctf_field_type *bt_ctf_field_type_variant_copy_recursive(
		struct bt_ctf_field_type *ft)
{
	int64_t i;
	GHashTableIter iter;
	gpointer key, value;
	struct bt_field_type_common *tag_ft_copy = NULL;
	struct bt_field_type_common_variant *var_ft = (void *) ft;
	struct bt_field_type_common_variant *copy_ft = NULL;

	BT_LOGD("Copying CTF writer variant field type's: addr=%p", ft);
	if (var_ft->tag_ft) {
		BT_LOGD_STR("Copying CTF writer variant field type's tag field type.");
		tag_ft_copy = bt_field_type_common_copy(
			BT_TO_COMMON(var_ft->tag_ft));
		if (!tag_ft_copy) {
			BT_LOGE_STR("Cannot copy CTF writer variant field type's tag field type.");
			goto end;
		}
	}

	copy_ft = (void *) bt_ctf_field_type_variant_create(
		(void *) tag_ft_copy,
		var_ft->tag_name->len ? var_ft->tag_name->str : NULL);
	if (!copy_ft) {
		BT_LOGE_STR("Cannot create CTF writer variant field type.");
		goto end;
	}

	/* Copy field_name_to_index */
	g_hash_table_iter_init(&iter, var_ft->field_name_to_index);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		g_hash_table_insert(copy_ft->field_name_to_index,
			key, value);
	}

	for (i = 0; i < var_ft->fields->len; i++) {
		struct structure_field_common *entry, *copy_entry;
		struct bt_field_type_common *field_ft_copy;

		entry = g_ptr_array_index(var_ft->fields, i);
		BT_LOGD("Copying CTF writer variant field type's field: "
			"index=%" PRId64 ", "
			"field-ft-addr=%p, field-name=\"%s\"",
			i, entry, g_quark_to_string(entry->name));
		copy_entry = g_new0(struct structure_field_common, 1);
		if (!copy_entry) {
			BT_LOGE_STR("Failed to allocate one variant field type field.");
			goto error;
		}

		field_ft_copy = (void *) bt_ctf_field_type_copy(
			(void *) entry->type);
		if (!field_ft_copy) {
			BT_LOGE("Cannot copy CTF writer variant field type's field: "
				"index=%" PRId64 ", "
				"field-ft-addr=%p, field-name=\"%s\"",
				i, entry, g_quark_to_string(entry->name));
			g_free(copy_entry);
			goto error;
		}

		copy_entry->name = entry->name;
		copy_entry->type = field_ft_copy;
		g_ptr_array_add(copy_ft->fields, copy_entry);
	}

	if (var_ft->tag_field_path) {
		BT_LOGD_STR("Copying CTF writer variant field type's tag field path.");
		copy_ft->tag_field_path = bt_field_path_copy(
			var_ft->tag_field_path);
		if (!copy_ft->tag_field_path) {
			BT_LOGE_STR("Cannot copy CTF writer variant field type's tag field path.");
			goto error;
		}
	}

	BT_LOGD("Copied variant field type: original-ft-addr=%p, copy-ft-addr=%p",
		ft, copy_ft);

end:
	bt_put(tag_ft_copy);
	return (void *) copy_ft;

error:
	bt_put(tag_ft_copy);
        BT_PUT(copy_ft);
	return NULL;
}

static
struct bt_ctf_field_type *bt_ctf_field_type_array_copy_recursive(
		struct bt_ctf_field_type *ft)
{
	struct bt_field_type_common *container_ft_copy = NULL;
	struct bt_field_type_common_array *array_ft = (void *) ft;
	struct bt_field_type_common_array *copy_ft = NULL;

	BT_LOGD("Copying CTF writer array field type's: addr=%p", ft);
	BT_LOGD_STR("Copying CTF writer array field type's element field type.");
	container_ft_copy = bt_field_type_common_copy(array_ft->element_ft);
	if (!container_ft_copy) {
		BT_LOGE_STR("Cannot copy CTF writer array field type's element field type.");
		goto end;
	}

	copy_ft = (void *) bt_ctf_field_type_array_create(
		(void *) container_ft_copy, array_ft->length);
	if (!copy_ft) {
		BT_LOGE_STR("Cannot create CTF writer array field type.");
		goto end;
	}

	BT_LOGD("Copied CTF writer array field type: original-ft-addr=%p, copy-ft-addr=%p",
		ft, copy_ft);

end:
	bt_put(container_ft_copy);
	return (void *) copy_ft;
}

static
struct bt_ctf_field_type *bt_ctf_field_type_sequence_copy_recursive(
		struct bt_ctf_field_type *ft)
{
	struct bt_field_type_common *container_ft_copy = NULL;
	struct bt_field_type_common_sequence *seq_ft = (void *) ft;
	struct bt_field_type_common_sequence *copy_ft = NULL;

	BT_LOGD("Copying CTF writer sequence field type's: addr=%p", ft);
	BT_LOGD_STR("Copying CTF writer sequence field type's element field type.");
	container_ft_copy = bt_field_type_common_copy(seq_ft->element_ft);
	if (!container_ft_copy) {
		BT_LOGE_STR("Cannot copy CTF writer sequence field type's element field type.");
		goto end;
	}

	copy_ft = (void *) bt_ctf_field_type_sequence_create(
		(void *) container_ft_copy,
		seq_ft->length_field_name->len ?
			seq_ft->length_field_name->str : NULL);
	if (!copy_ft) {
		BT_LOGE_STR("Cannot create CTF writer sequence field type.");
		goto end;
	}

	if (seq_ft->length_field_path) {
		BT_LOGD_STR("Copying CTF writer sequence field type's length field path.");
		copy_ft->length_field_path = bt_field_path_copy(
			seq_ft->length_field_path);
		if (!copy_ft->length_field_path) {
			BT_LOGE_STR("Cannot copy CTF writer sequence field type's length field path.");
			goto error;
		}
	}

	BT_LOGD("Copied CTF writer sequence field type: original-ft-addr=%p, copy-ft-addr=%p",
		ft, copy_ft);

end:
	bt_put(container_ft_copy);
	return (void *) copy_ft;
error:
	bt_put(container_ft_copy);
	BT_PUT(copy_ft);
	return NULL;
}

static
struct bt_ctf_field_type *bt_ctf_field_type_string_copy(struct bt_ctf_field_type *ft)
{
	struct bt_field_type_common_string *string_ft = (void *) ft;
	struct bt_field_type_common_string *copy_ft = NULL;

	BT_LOGD("Copying CTF writer string field type's: addr=%p", ft);
	copy_ft = (void *) bt_ctf_field_type_string_create();
	if (!copy_ft) {
		BT_LOGE_STR("Cannot create CTF writer string field type.");
		goto end;
	}

	copy_ft->encoding = string_ft->encoding;
	BT_LOGD("Copied CTF writer string field type: original-ft-addr=%p, copy-ft-addr=%p",
		ft, copy_ft);

end:
	return (void *) copy_ft;
}
