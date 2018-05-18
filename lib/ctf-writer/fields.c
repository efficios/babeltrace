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

#define BT_LOG_TAG "CTF-WRITER-FIELDS"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/compat/fcntl-internal.h>
#include <babeltrace/ctf-writer/fields-internal.h>
#include <babeltrace/ctf-writer/field-types-internal.h>
#include <babeltrace/ctf-writer/serialize-internal.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/endian-internal.h>
#include <babeltrace/assert-internal.h>
#include <float.h>
#include <inttypes.h>
#include <stdlib.h>

static struct bt_field_common_methods bt_ctf_field_integer_methods = {
	.set_is_frozen = bt_field_common_generic_set_is_frozen,
	.validate = bt_field_common_generic_validate,
	.copy = NULL,
	.is_set = bt_field_common_generic_is_set,
	.reset = bt_field_common_generic_reset,
};

static struct bt_field_common_methods bt_ctf_field_floating_point_methods = {
	.set_is_frozen = bt_field_common_generic_set_is_frozen,
	.validate = bt_field_common_generic_validate,
	.copy = NULL,
	.is_set = bt_field_common_generic_is_set,
	.reset = bt_field_common_generic_reset,
};

static
void bt_ctf_field_enumeration_set_is_frozen_recursive(
		struct bt_field_common *field, bool is_frozen);

static
int bt_ctf_field_enumeration_validate_recursive(struct bt_field_common *field);

static
bt_bool bt_ctf_field_enumeration_is_set_recursive(
		struct bt_field_common *field);

static
void bt_ctf_field_enumeration_reset_recursive(struct bt_field_common *field);

static struct bt_field_common_methods bt_ctf_field_enumeration_methods = {
	.set_is_frozen = bt_ctf_field_enumeration_set_is_frozen_recursive,
	.validate = bt_ctf_field_enumeration_validate_recursive,
	.copy = NULL,
	.is_set = bt_ctf_field_enumeration_is_set_recursive,
	.reset = bt_ctf_field_enumeration_reset_recursive,
};

static struct bt_field_common_methods bt_ctf_field_string_methods = {
	.set_is_frozen = bt_field_common_generic_set_is_frozen,
	.validate = bt_field_common_generic_validate,
	.copy = NULL,
	.is_set = bt_field_common_generic_is_set,
	.reset = bt_field_common_generic_reset,
};

static struct bt_field_common_methods bt_ctf_field_structure_methods = {
	.set_is_frozen = bt_field_common_structure_set_is_frozen_recursive,
	.validate = bt_field_common_structure_validate_recursive,
	.copy = NULL,
	.is_set = bt_field_common_structure_is_set_recursive,
	.reset = bt_field_common_structure_reset_recursive,
};

static struct bt_field_common_methods bt_ctf_field_sequence_methods = {
	.set_is_frozen = bt_field_common_sequence_set_is_frozen_recursive,
	.validate = bt_field_common_sequence_validate_recursive,
	.copy = NULL,
	.is_set = bt_field_common_sequence_is_set_recursive,
	.reset = bt_field_common_sequence_reset_recursive,
};

static struct bt_field_common_methods bt_ctf_field_array_methods = {
	.set_is_frozen = bt_field_common_array_set_is_frozen_recursive,
	.validate = bt_field_common_array_validate_recursive,
	.copy = NULL,
	.is_set = bt_field_common_array_is_set_recursive,
	.reset = bt_field_common_array_reset_recursive,
};

static
void bt_ctf_field_variant_set_is_frozen_recursive(struct bt_field_common *field,
		bool is_frozen);

static
int bt_ctf_field_variant_validate_recursive(struct bt_field_common *field);

static
bt_bool bt_ctf_field_variant_is_set_recursive(struct bt_field_common *field);

static
void bt_ctf_field_variant_reset_recursive(struct bt_field_common *field);

static struct bt_field_common_methods bt_ctf_field_variant_methods = {
	.set_is_frozen = bt_ctf_field_variant_set_is_frozen_recursive,
	.validate = bt_ctf_field_variant_validate_recursive,
	.copy = NULL,
	.is_set = bt_ctf_field_variant_is_set_recursive,
	.reset = bt_ctf_field_variant_reset_recursive,
};

static
struct bt_ctf_field *bt_ctf_field_integer_create(struct bt_ctf_field_type *);

static
struct bt_ctf_field *bt_ctf_field_enumeration_create(struct bt_ctf_field_type *);

static
struct bt_ctf_field *bt_ctf_field_floating_point_create(struct bt_ctf_field_type *);

static
struct bt_ctf_field *bt_ctf_field_structure_create(struct bt_ctf_field_type *);

static
struct bt_ctf_field *bt_ctf_field_variant_create(struct bt_ctf_field_type *);

static
struct bt_ctf_field *bt_ctf_field_array_create(struct bt_ctf_field_type *);

static
struct bt_ctf_field *bt_ctf_field_sequence_create(struct bt_ctf_field_type *);

static
struct bt_ctf_field *bt_ctf_field_string_create(struct bt_ctf_field_type *);

static
struct bt_ctf_field *(* const field_create_funcs[])(struct bt_ctf_field_type *) = {
	[BT_FIELD_TYPE_ID_INTEGER] =	bt_ctf_field_integer_create,
	[BT_FIELD_TYPE_ID_ENUM] =	bt_ctf_field_enumeration_create,
	[BT_FIELD_TYPE_ID_FLOAT] =	bt_ctf_field_floating_point_create,
	[BT_FIELD_TYPE_ID_STRUCT] =	bt_ctf_field_structure_create,
	[BT_FIELD_TYPE_ID_VARIANT] =	bt_ctf_field_variant_create,
	[BT_FIELD_TYPE_ID_ARRAY] =	bt_ctf_field_array_create,
	[BT_FIELD_TYPE_ID_SEQUENCE] =	bt_ctf_field_sequence_create,
	[BT_FIELD_TYPE_ID_STRING] =	bt_ctf_field_string_create,
};

typedef int (*bt_ctf_field_serialize_recursive_func)(
		struct bt_field_common *, struct bt_ctf_stream_pos *,
		enum bt_ctf_byte_order);

static
void bt_ctf_field_integer_destroy(struct bt_ctf_field *field)
{
	BT_LOGD("Destroying CTF writer integer field object: addr=%p", field);
	bt_field_common_integer_finalize((void *) field);
	g_free(field);
}

static
void bt_ctf_field_floating_point_destroy(struct bt_ctf_field *field)
{
	BT_LOGD("Destroying CTF writer floating point field object: addr=%p",
		field);
	bt_field_common_floating_point_finalize((void *) field);
	g_free(field);
}

static
void bt_ctf_field_enumeration_destroy_recursive(struct bt_ctf_field *field)
{
	struct bt_ctf_field_enumeration *enumeration = BT_FROM_COMMON(field);

	BT_LOGD("Destroying CTF writer enumeration field object: addr=%p",
		field);
	BT_LOGD_STR("Putting container field.");
	bt_put(enumeration->container);
	bt_field_common_finalize((void *) field);
	g_free(field);
}

static
void bt_ctf_field_structure_destroy_recursive(struct bt_ctf_field *field)
{
	BT_LOGD("Destroying CTF writer structure field object: addr=%p", field);
	bt_field_common_structure_finalize_recursive((void *) field);
	g_free(field);
}

static
void bt_ctf_field_variant_destroy_recursive(struct bt_ctf_field *field)
{
	struct bt_ctf_field_variant *variant = BT_FROM_COMMON(field);

	BT_LOGD("Destroying CTF writer variant field object: addr=%p", field);
	BT_LOGD_STR("Putting tag field.");
	bt_put(variant->tag);
	bt_field_common_variant_finalize_recursive((void *) field);
	g_free(field);
}

static
void bt_ctf_field_array_destroy_recursive(struct bt_ctf_field *field)
{
	BT_LOGD("Destroying CTF writer array field object: addr=%p", field);
	bt_field_common_array_finalize_recursive((void *) field);
	g_free(field);
}

static
void bt_ctf_field_sequence_destroy_recursive(struct bt_ctf_field *field)
{
	BT_LOGD("Destroying CTF writer sequence field object: addr=%p", field);
	bt_field_common_sequence_finalize_recursive((void *) field);
	g_free(field);
}

static
void bt_ctf_field_string_destroy(struct bt_ctf_field *field)
{
	BT_LOGD("Destroying CTF writer string field object: addr=%p", field);
	bt_field_common_string_finalize((void *) field);
	g_free(field);
}

BT_HIDDEN
int bt_ctf_field_serialize_recursive(struct bt_ctf_field *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	struct bt_field_common *field_common = (void *) field;
	bt_ctf_field_serialize_recursive_func serialize_func;

	BT_ASSERT(pos);
	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT(field_common->spec.writer.serialize_func);
	serialize_func = field_common->spec.writer.serialize_func;
	return serialize_func(field_common, pos,
		native_byte_order);
}

static
int increase_packet_size(struct bt_ctf_stream_pos *pos)
{
	int ret;

	BT_ASSERT(pos);
	BT_LOGV("Increasing packet size: pos-offset=%" PRId64 ", "
		"cur-packet-size=%" PRIu64,
		pos->offset, pos->packet_size);
	ret = munmap_align(pos->base_mma);
	if (ret) {
		BT_LOGE_ERRNO("Failed to perform an aligned memory unmapping",
			": ret=%d", ret);
		goto end;
	}

	pos->packet_size += PACKET_LEN_INCREMENT;
	do {
		ret = bt_posix_fallocate(pos->fd, pos->mmap_offset,
			pos->packet_size / CHAR_BIT);
	} while (ret == EINTR);
	if (ret) {
		BT_LOGE_ERRNO("Failed to preallocate memory space",
			": ret=%d", ret);
		errno = EINTR;
		ret = -1;
		goto end;
	}

	pos->base_mma = mmap_align(pos->packet_size / CHAR_BIT, pos->prot,
		pos->flags, pos->fd, pos->mmap_offset);
	if (pos->base_mma == MAP_FAILED) {
		BT_LOGE_ERRNO("Failed to perform an aligned memory mapping",
			": ret=%d", ret);
		ret = -1;
	}

	BT_LOGV("Increased packet size: pos-offset=%" PRId64 ", "
		"new-packet-size=%" PRIu64,
		pos->offset, pos->packet_size);
	BT_ASSERT(pos->packet_size % 8 == 0);

end:
	return ret;
}

static
int bt_ctf_field_integer_serialize(struct bt_field_common *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	int ret = 0;

	BT_ASSERT_PRE_FIELD_COMMON_IS_SET(field, "Integer field");
	BT_LOGV("Serializing CTF writer integer field: addr=%p, pos-offset=%" PRId64 ", "
		"native-bo=%s", field, pos->offset,
		bt_common_byte_order_string((int) native_byte_order));

retry:
	ret = bt_ctf_field_integer_write(field, pos, native_byte_order);
	if (ret == -EFAULT) {
		/*
		 * The field is too large to fit in the current packet's
		 * remaining space. Bump the packet size and retry.
		 */
		ret = increase_packet_size(pos);
		if (ret) {
			BT_LOGE("Cannot increase packet size: ret=%d", ret);
			goto end;
		}
		goto retry;
	}

end:
	return ret;
}

static
int bt_ctf_field_enumeration_serialize_recursive(struct bt_field_common *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	struct bt_ctf_field_enumeration *enumeration = (void *) field;

	BT_LOGV("Serializing enumeration field: addr=%p, pos-offset=%" PRId64 ", "
		"native-bo=%s", field, pos->offset,
		bt_common_byte_order_string((int) native_byte_order));
	BT_LOGV_STR("Serializing enumeration field's payload field.");
	return bt_ctf_field_serialize_recursive(
		(void *) enumeration->container, pos, native_byte_order);
}

static
int bt_ctf_field_floating_point_serialize(struct bt_field_common *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	int ret = 0;

	BT_ASSERT_PRE_FIELD_COMMON_IS_SET(field, "Floating point number field");
	BT_LOGV("Serializing floating point number field: addr=%p, pos-offset=%" PRId64 ", "
		"native-bo=%s", field, pos->offset,
		bt_common_byte_order_string((int) native_byte_order));

retry:
	ret = bt_ctf_field_floating_point_write(field, pos,
		native_byte_order);
	if (ret == -EFAULT) {
		/*
		 * The field is too large to fit in the current packet's
		 * remaining space. Bump the packet size and retry.
		 */
		ret = increase_packet_size(pos);
		if (ret) {
			BT_LOGE("Cannot increase packet size: ret=%d", ret);
			goto end;
		}
		goto retry;
	}

end:
	return ret;
}

static
int bt_ctf_field_structure_serialize_recursive(struct bt_field_common *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	int64_t i;
	int ret = 0;
	struct bt_field_common_structure *structure = BT_FROM_COMMON(field);

	BT_LOGV("Serializing structure field: addr=%p, pos-offset=%" PRId64 ", "
		"native-bo=%s", field, pos->offset,
		bt_common_byte_order_string((int) native_byte_order));

	while (!bt_ctf_stream_pos_access_ok(pos,
		offset_align(pos->offset, field->type->alignment))) {
		ret = increase_packet_size(pos);
		if (ret) {
			BT_LOGE("Cannot increase packet size: ret=%d", ret);
			goto end;
		}
	}

	if (!bt_ctf_stream_pos_align(pos, field->type->alignment)) {
		BT_LOGE("Cannot align packet's position: pos-offset=%" PRId64 ", "
			"align=%u", pos->offset, field->type->alignment);
		ret = -1;
		goto end;
	}

	for (i = 0; i < structure->fields->len; i++) {
		struct bt_field_common *member = g_ptr_array_index(
			structure->fields, i);
		const char *field_name = NULL;

		BT_LOGV("Serializing structure field's field: pos-offset=%" PRId64 ", "
			"field-addr=%p, index=%" PRId64,
			pos->offset, member, i);

		if (!member) {
			ret = bt_field_type_common_structure_borrow_field_by_index(
				field->type, &field_name, NULL, i);
			BT_ASSERT(ret == 0);
			BT_LOGW("Cannot serialize structure field's field: field is not set: "
				"struct-field-addr=%p, "
				"field-name=\"%s\", index=%" PRId64,
				field, field_name, i);
			ret = -1;
			goto end;
		}

		ret = bt_ctf_field_serialize_recursive((void *) member, pos,
			native_byte_order);
		if (ret) {
			ret = bt_field_type_common_structure_borrow_field_by_index(
				field->type, &field_name, NULL, i);
			BT_ASSERT(ret == 0);
			BT_LOGW("Cannot serialize structure field's field: "
				"struct-field-addr=%p, field-addr=%p, "
				"field-name=\"%s\", index=%" PRId64,
				field->type, member, field_name, i);
			break;
		}
	}

end:
	return ret;
}

static
int bt_ctf_field_variant_serialize_recursive(struct bt_field_common *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	struct bt_field_common_variant *variant = BT_FROM_COMMON(field);

	BT_LOGV("Serializing variant field: addr=%p, pos-offset=%" PRId64 ", "
		"native-bo=%s", field, pos->offset,
		bt_common_byte_order_string((int) native_byte_order));
	BT_LOGV_STR("Serializing variant field's payload field.");
	return bt_ctf_field_serialize_recursive(
		(void *) variant->current_field, pos, native_byte_order);
}

static
int bt_ctf_field_array_serialize_recursive(struct bt_field_common *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	int64_t i;
	int ret = 0;
	struct bt_field_common_array *array = BT_FROM_COMMON(field);

	BT_LOGV("Serializing array field: addr=%p, pos-offset=%" PRId64 ", "
		"native-bo=%s", field, pos->offset,
		bt_common_byte_order_string((int) native_byte_order));

	for (i = 0; i < array->elements->len; i++) {
		struct bt_field_common *elem_field =
			g_ptr_array_index(array->elements, i);

		BT_LOGV("Serializing array field's element field: "
			"pos-offset=%" PRId64 ", field-addr=%p, index=%" PRId64,
			pos->offset, elem_field, i);
		ret = bt_ctf_field_serialize_recursive(
			(void *) elem_field, pos, native_byte_order);
		if (ret) {
			BT_LOGW("Cannot serialize array field's element field: "
				"array-field-addr=%p, field-addr=%p, "
				"index=%" PRId64, field, elem_field, i);
			goto end;
		}
	}

end:
	return ret;
}

static
int bt_ctf_field_sequence_serialize_recursive(struct bt_field_common *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	int64_t i;
	int ret = 0;
	struct bt_field_common_sequence *sequence = BT_FROM_COMMON(field);

	BT_LOGV("Serializing sequence field: addr=%p, pos-offset=%" PRId64 ", "
		"native-bo=%s", field, pos->offset,
		bt_common_byte_order_string((int) native_byte_order));

	for (i = 0; i < sequence->elements->len; i++) {
		struct bt_field_common *elem_field =
			g_ptr_array_index(sequence->elements, i);

		BT_LOGV("Serializing sequence field's element field: "
			"pos-offset=%" PRId64 ", field-addr=%p, index=%" PRId64,
			pos->offset, elem_field, i);
		ret = bt_ctf_field_serialize_recursive(
			(void *) elem_field, pos, native_byte_order);
		if (ret) {
			BT_LOGW("Cannot serialize sequence field's element field: "
				"sequence-field-addr=%p, field-addr=%p, "
				"index=%" PRId64, field, elem_field, i);
			goto end;
		}
	}

end:
	return ret;
}

static
int bt_ctf_field_string_serialize(struct bt_field_common *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	int64_t i;
	int ret = 0;
	struct bt_field_common_string *string = BT_FROM_COMMON(field);
	struct bt_ctf_field_type *character_type =
		get_field_type(FIELD_TYPE_ALIAS_UINT8_T);
	struct bt_ctf_field *character;

	BT_ASSERT_PRE_FIELD_COMMON_IS_SET(field, "String field");
	BT_LOGV("Serializing string field: addr=%p, pos-offset=%" PRId64 ", "
		"native-bo=%s", field, pos->offset,
		bt_common_byte_order_string((int) native_byte_order));

	BT_LOGV_STR("Creating character field from string field's character field type.");
	character = bt_ctf_field_create(character_type);

	for (i = 0; i < string->payload->len + 1; i++) {
		const uint64_t chr = (uint64_t) string->payload->str[i];

		ret = bt_ctf_field_integer_unsigned_set_value(character, chr);
		BT_ASSERT(ret == 0);
		BT_LOGV("Serializing string field's character field: "
			"pos-offset=%" PRId64 ", field-addr=%p, "
			"index=%" PRId64 ", char-int=%" PRIu64,
			pos->offset, character, i, chr);
		ret = bt_ctf_field_integer_serialize(
			(void *) character, pos, native_byte_order);
		if (ret) {
			BT_LOGW_STR("Cannot serialize character field.");
			goto end;
		}
	}

end:
	bt_put(character);
	bt_put(character_type);
	return ret;
}

struct bt_ctf_field *bt_ctf_field_create(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field *field = NULL;
	enum bt_ctf_field_type_id type_id;

	BT_ASSERT_PRE_NON_NULL(type, "Field type");
	BT_ASSERT(field_type_common_has_known_id((void *) type));
	BT_ASSERT_PRE(bt_field_type_common_validate((void *) type) == 0,
		"Field type is invalid: %!+wF", type);
	type_id = bt_ctf_field_type_get_type_id(type);
	field = field_create_funcs[type_id](type);
	if (!field) {
		goto end;
	}

	bt_field_type_common_freeze((void *) type);

end:
	return field;
}

struct bt_ctf_field_type *bt_ctf_field_get_type(struct bt_ctf_field *field)
{
	return bt_get(bt_field_common_borrow_type((void *) field));
}

enum bt_ctf_field_type_id bt_ctf_field_get_type_id(struct bt_ctf_field *field)
{
	struct bt_field_common *field_common = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	return (int) field_common->type->id;
}

int bt_ctf_field_sequence_set_length(struct bt_ctf_field *field,
		struct bt_ctf_field *length_field)
{
	int ret;
	struct bt_field_common *common_length_field = (void *) length_field;
	uint64_t length;

	BT_ASSERT_PRE_NON_NULL(length_field, "Length field");
	BT_ASSERT_PRE_FIELD_COMMON_IS_SET((void *) length_field, "Length field");
	BT_ASSERT_PRE(common_length_field->type->id == BT_CTF_FIELD_TYPE_ID_INTEGER ||
			common_length_field->type->id == BT_CTF_FIELD_TYPE_ID_ENUM,
		"Length field must be an integer or enumeration field: %!+wf",
		length_field);

	if (common_length_field->type->id == BT_CTF_FIELD_TYPE_ID_ENUM) {
		struct bt_ctf_field_enumeration *enumeration = (void *)
			length_field;

		length_field = (void *) enumeration->container;
	}

	ret = bt_ctf_field_integer_unsigned_get_value(length_field, &length);
	BT_ASSERT(ret == 0);
	return bt_field_common_sequence_set_length((void *) field,
		length, (bt_field_common_create_func) bt_ctf_field_create);
}

struct bt_ctf_field *bt_ctf_field_structure_get_field_by_index(
		struct bt_ctf_field *field, uint64_t index)
{
	return bt_get(bt_field_common_structure_borrow_field_by_index(
		(void *) field, index));
}

struct bt_ctf_field *bt_ctf_field_structure_get_field_by_name(
		struct bt_ctf_field *field, const char *name)
{
	return bt_get(bt_field_common_structure_borrow_field_by_name(
		(void *) field, name));
}

struct bt_ctf_field *bt_ctf_field_array_get_field(
		struct bt_ctf_field *field, uint64_t index)
{
	return bt_get(
		bt_field_common_array_borrow_field((void *) field, index));
}

struct bt_ctf_field *bt_ctf_field_sequence_get_field(
		struct bt_ctf_field *field, uint64_t index)
{
	return bt_get(
		bt_field_common_sequence_borrow_field((void *) field, index));
}

struct bt_ctf_field *bt_ctf_field_variant_get_field(struct bt_ctf_field *field,
		struct bt_ctf_field *tag_field)
{
	struct bt_ctf_field_variant *variant_field = (void *) field;
	struct bt_ctf_field_enumeration *enum_field = (void *) tag_field;
	struct bt_field_type_common_variant *variant_ft;
	struct bt_field_type_common_enumeration *tag_ft;
	struct bt_ctf_field *current_field = NULL;
	bt_bool is_signed;
	uint64_t tag_uval;
	int ret;

	BT_ASSERT_PRE_NON_NULL(field, "Variant field");
	BT_ASSERT_PRE_NON_NULL(tag_field, "Tag field");
	BT_ASSERT_PRE_FIELD_COMMON_IS_SET((void *) tag_field, "Tag field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(
		(struct bt_field_common *) tag_field,
		BT_FIELD_TYPE_ID_ENUM, "Tag field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(
		(struct bt_field_common *) field,
		BT_FIELD_TYPE_ID_VARIANT, "Field");
	BT_ASSERT_PRE(
		bt_field_common_validate_recursive((void *) tag_field) == 0,
		"Tag field is invalid: %!+wf", tag_field);
	variant_ft = BT_FROM_COMMON(variant_field->common.common.type);
	BT_ASSERT_PRE(bt_field_type_common_compare(
		BT_TO_COMMON(variant_ft->tag_ft), enum_field->common.type) == 0,
		"Unexpected tag field's type: %![expected-ft-]+wF, "
		"%![tag-ft-]+wF", variant_ft->tag_ft,
		enum_field->common.type);
	tag_ft = BT_FROM_COMMON(enum_field->common.type);
	is_signed = tag_ft->container_ft->is_signed;

	if (is_signed) {
		int64_t tag_ival;

		ret = bt_ctf_field_integer_signed_get_value(
			(void *) enum_field->container, &tag_ival);
		tag_uval = (uint64_t) tag_ival;
	} else {
		ret = bt_ctf_field_integer_unsigned_get_value(
			(void *) enum_field->container, &tag_uval);
	}

	BT_ASSERT(ret == 0);
	ret = bt_field_variant_common_set_tag((void *) field, tag_uval,
		is_signed);
	if (ret) {
		goto end;
	}

	bt_put(variant_field->tag);
	variant_field->tag = bt_get(tag_field);
	current_field = bt_ctf_field_variant_get_current_field(field);
	BT_ASSERT(current_field);

end:
	return current_field;
}

struct bt_ctf_field *bt_ctf_field_variant_get_current_field(
		struct bt_ctf_field *variant_field)
{
	return bt_get(bt_field_common_variant_borrow_current_field(
		(void *) variant_field));
}

BT_HIDDEN
struct bt_ctf_field *bt_ctf_field_enumeration_borrow_container(
		struct bt_ctf_field *field)
{
	struct bt_ctf_field_enumeration *enumeration = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Enumeration field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID((struct bt_field_common *) field,
		BT_CTF_FIELD_TYPE_ID_ENUM, "Field");
	BT_ASSERT(enumeration->container);
	return (void *) enumeration->container;
}

struct bt_ctf_field *bt_ctf_field_enumeration_get_container(
		struct bt_ctf_field *field)
{
	return bt_get(bt_ctf_field_enumeration_borrow_container(field));
}

int bt_ctf_field_integer_signed_get_value(struct bt_ctf_field *field,
		int64_t *value)
{
	struct bt_field_common_integer *integer = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Integer field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_COMMON_IS_SET(BT_TO_COMMON(integer), "Integer field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(BT_TO_COMMON(integer),
		BT_FIELD_TYPE_ID_INTEGER, "Field");
	BT_ASSERT_PRE(bt_field_type_common_integer_is_signed(
		integer->common.type),
		"Field's type is unsigned: %!+_f", field);
	*value = integer->payload.signd;
	return 0;
}

int bt_ctf_field_integer_signed_set_value(struct bt_ctf_field *field,
		int64_t value)
{
	int ret = 0;
	struct bt_field_common_integer *integer = (void *) field;
	struct bt_field_type_common_integer *integer_type;

	BT_ASSERT_PRE_NON_NULL(field, "Integer field");
	BT_ASSERT_PRE_FIELD_COMMON_HOT(BT_TO_COMMON(integer), "Integer field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(BT_TO_COMMON(integer),
		BT_FIELD_TYPE_ID_INTEGER, "Field");
	integer_type = BT_FROM_COMMON(integer->common.type);
	BT_ASSERT_PRE(
		bt_field_type_common_integer_is_signed(integer->common.type),
		"Field's type is unsigned: %!+wf", field);
	BT_ASSERT_PRE(value_is_in_range_signed(integer_type->size, value),
		"Value is out of bounds: value=%" PRId64 ", %![field-]+wf",
		value, field);
	integer->payload.signd = value;
	bt_field_common_set(BT_TO_COMMON(integer), true);
	return ret;
}

int bt_ctf_field_integer_unsigned_get_value(struct bt_ctf_field *field,
		uint64_t *value)
{
	struct bt_field_common_integer *integer = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Integer field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_COMMON_IS_SET(BT_TO_COMMON(integer), "Integer field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(BT_TO_COMMON(integer),
		BT_FIELD_TYPE_ID_INTEGER, "Field");
	BT_ASSERT_PRE(
		!bt_field_type_common_integer_is_signed(integer->common.type),
		"Field's type is signed: %!+wf", field);
	*value = integer->payload.unsignd;
	return 0;
}

int bt_ctf_field_integer_unsigned_set_value(struct bt_ctf_field *field,
		uint64_t value)
{
	struct bt_field_common_integer *integer = (void *) field;
	struct bt_field_type_common_integer *integer_type;

	BT_ASSERT_PRE_NON_NULL(field, "Integer field");
	BT_ASSERT_PRE_FIELD_COMMON_HOT(BT_TO_COMMON(integer), "Integer field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(BT_TO_COMMON(integer),
		BT_FIELD_TYPE_ID_INTEGER, "Field");
	integer_type = BT_FROM_COMMON(integer->common.type);
	BT_ASSERT_PRE(
		!bt_field_type_common_integer_is_signed(integer->common.type),
		"Field's type is signed: %!+wf", field);
	BT_ASSERT_PRE(value_is_in_range_unsigned(integer_type->size, value),
		"Value is out of bounds: value=%" PRIu64 ", %![field-]+wf",
		value, field);
	integer->payload.unsignd = value;
	bt_field_common_set(BT_TO_COMMON(integer), true);
	return 0;
}

int bt_ctf_field_floating_point_get_value(struct bt_ctf_field *field,
		double *value)
{
	return bt_field_common_floating_point_get_value((void *) field, value);
}

int bt_ctf_field_floating_point_set_value(struct bt_ctf_field *field,
		double value)
{
	return bt_field_common_floating_point_set_value((void *) field, value);
}

const char *bt_ctf_field_string_get_value(struct bt_ctf_field *field)
{
	return bt_field_common_string_get_value((void *) field);
}

int bt_ctf_field_string_set_value(struct bt_ctf_field *field, const char *value)
{
	return bt_field_common_string_set_value((void *) field, value);
}

int bt_ctf_field_string_append(struct bt_ctf_field *field, const char *value)
{
	return bt_field_common_string_append((void *) field, value);
}

int bt_ctf_field_string_append_len(struct bt_ctf_field *field,
		const char *value, unsigned int length)
{
	return bt_field_common_string_append_len((void *) field, value, length);
}

struct bt_ctf_field *bt_ctf_field_copy(struct bt_ctf_field *field)
{
	return (void *) bt_field_common_copy((void *) field);
}

static
struct bt_ctf_field *bt_ctf_field_integer_create(struct bt_ctf_field_type *type)
{
	struct bt_field_common_integer *integer =
		g_new0(struct bt_field_common_integer, 1);

	BT_LOGD("Creating CTF writer integer field object: ft-addr=%p", type);

	if (integer) {
		bt_field_common_initialize(BT_TO_COMMON(integer), (void *) type,
			(bt_object_release_func) bt_ctf_field_integer_destroy,
			&bt_ctf_field_integer_methods);
		integer->common.spec.writer.serialize_func =
			(bt_ctf_field_serialize_recursive_func) bt_ctf_field_integer_serialize;
		BT_LOGD("Created CTF writer integer field object: addr=%p, ft-addr=%p",
			integer, type);
	} else {
		BT_LOGE_STR("Failed to allocate one integer field.");
	}

	return (void *) integer;
}

static
struct bt_ctf_field *bt_ctf_field_enumeration_create(
		struct bt_ctf_field_type *type)
{
	struct bt_field_type_common_enumeration *enum_ft = (void *) type;
	struct bt_ctf_field_enumeration *enumeration = g_new0(
		struct bt_ctf_field_enumeration, 1);

	BT_LOGD("Creating CTF writer enumeration field object: ft-addr=%p", type);

	if (!enumeration) {
		BT_LOGE_STR("Failed to allocate one enumeration field.");
		goto end;
	}

	bt_field_common_initialize(BT_TO_COMMON(enumeration),
		(void *) type,
		(bt_object_release_func)
			bt_ctf_field_enumeration_destroy_recursive,
		&bt_ctf_field_enumeration_methods);
	enumeration->container = (void *) bt_ctf_field_create(
		BT_FROM_COMMON(enum_ft->container_ft));
	if (!enumeration->container) {
		BT_PUT(enumeration);
		goto end;
	}

	enumeration->common.spec.writer.serialize_func =
		(bt_ctf_field_serialize_recursive_func)
			bt_ctf_field_enumeration_serialize_recursive;
	BT_LOGD("Created CTF writer enumeration field object: addr=%p, ft-addr=%p",
		enumeration, type);

end:
	return (void *) enumeration;
}

static
struct bt_ctf_field *bt_ctf_field_floating_point_create(
		struct bt_ctf_field_type *type)
{
	struct bt_field_common_floating_point *floating_point;

	BT_LOGD("Creating CTF writer floating point number field object: ft-addr=%p", type);
	floating_point = g_new0(struct bt_field_common_floating_point, 1);

	if (floating_point) {
		bt_field_common_initialize(BT_TO_COMMON(floating_point),
			(void *) type,
			(bt_object_release_func)
				bt_ctf_field_floating_point_destroy,
			&bt_ctf_field_floating_point_methods);
		floating_point->common.spec.writer.serialize_func =
			(bt_ctf_field_serialize_recursive_func) bt_ctf_field_floating_point_serialize;
		BT_LOGD("Created CTF writer floating point number field object: addr=%p, ft-addr=%p",
			floating_point, type);
	} else {
		BT_LOGE_STR("Failed to allocate one floating point number field.");
	}

	return (void *) floating_point;
}

static
struct bt_ctf_field *bt_ctf_field_structure_create(
		struct bt_ctf_field_type *type)
{
	struct bt_field_common_structure *structure = g_new0(
		struct bt_field_common_structure, 1);
	int iret;

	BT_LOGD("Creating CTF writer structure field object: ft-addr=%p", type);

	if (!structure) {
		BT_LOGE_STR("Failed to allocate one structure field.");
		goto end;
	}

	iret = bt_field_common_structure_initialize(BT_TO_COMMON(structure),
		(void *) type,
		(bt_object_release_func)
			bt_ctf_field_structure_destroy_recursive,
		&bt_ctf_field_structure_methods,
		(bt_field_common_create_func) bt_ctf_field_create,
		(GDestroyNotify) bt_put);
	structure->common.spec.writer.serialize_func =
		(bt_ctf_field_serialize_recursive_func) bt_ctf_field_structure_serialize_recursive;
	if (iret) {
		BT_PUT(structure);
		goto end;
	}

	BT_LOGD("Created CTF writer structure field object: addr=%p, ft-addr=%p",
		structure, type);

end:
	return (void *) structure;
}

static
struct bt_ctf_field *bt_ctf_field_variant_create(struct bt_ctf_field_type *type)
{
	struct bt_field_type_common_variant *var_ft = (void *) type;
	struct bt_ctf_field_variant *variant = g_new0(
		struct bt_ctf_field_variant, 1);

	BT_LOGD("Creating CTF writer variant field object: ft-addr=%p", type);

	if (!variant) {
		BT_LOGE_STR("Failed to allocate one variant field.");
		goto end;
	}

	bt_field_common_variant_initialize(BT_TO_COMMON(BT_TO_COMMON(variant)),
		(void *) type,
		(bt_object_release_func)
			bt_ctf_field_variant_destroy_recursive,
		&bt_ctf_field_variant_methods,
		(bt_field_common_create_func) bt_ctf_field_create,
		(GDestroyNotify) bt_put);
	variant->tag = (void *) bt_ctf_field_create(
		BT_FROM_COMMON(var_ft->tag_ft));
	variant->common.common.spec.writer.serialize_func =
		(bt_ctf_field_serialize_recursive_func) bt_ctf_field_variant_serialize_recursive;
	BT_LOGD("Created CTF writer variant field object: addr=%p, ft-addr=%p",
		variant, type);

end:
	return (void *) variant;
}

static
struct bt_ctf_field *bt_ctf_field_array_create(struct bt_ctf_field_type *type)
{
	struct bt_field_common_array *array =
		g_new0(struct bt_field_common_array, 1);
	int ret;

	BT_LOGD("Creating CTF writer array field object: ft-addr=%p", type);
	BT_ASSERT(type);

	if (!array) {
		BT_LOGE_STR("Failed to allocate one array field.");
		goto end;
	}

	ret = bt_field_common_array_initialize(BT_TO_COMMON(array),
		(void *) type,
		(bt_object_release_func)
			bt_ctf_field_array_destroy_recursive,
		&bt_ctf_field_array_methods,
		(bt_field_common_create_func) bt_ctf_field_create,
		(GDestroyNotify) bt_put);
	array->common.spec.writer.serialize_func =
		(bt_ctf_field_serialize_recursive_func) bt_ctf_field_array_serialize_recursive;
	if (ret) {
		BT_PUT(array);
		goto end;
	}

	BT_LOGD("Created CTF writer array field object: addr=%p, ft-addr=%p",
		array, type);

end:
	return (void *) array;
}

static
struct bt_ctf_field *bt_ctf_field_sequence_create(struct bt_ctf_field_type *type)
{
	struct bt_field_common_sequence *sequence = g_new0(
		struct bt_field_common_sequence, 1);

	BT_LOGD("Creating CTF writer sequence field object: ft-addr=%p", type);

	if (sequence) {
		bt_field_common_sequence_initialize(BT_TO_COMMON(sequence),
			(void *) type,
			(bt_object_release_func)
				bt_ctf_field_sequence_destroy_recursive,
			&bt_ctf_field_sequence_methods,
			(GDestroyNotify) bt_put);
		sequence->common.spec.writer.serialize_func =
			(bt_ctf_field_serialize_recursive_func) bt_ctf_field_sequence_serialize_recursive;
		BT_LOGD("Created CTF writer sequence field object: addr=%p, ft-addr=%p",
			sequence, type);
	} else {
		BT_LOGE_STR("Failed to allocate one sequence field.");
	}

	return (void *) sequence;
}

static
struct bt_ctf_field *bt_ctf_field_string_create(struct bt_ctf_field_type *type)
{
	struct bt_field_common_string *string = g_new0(
		struct bt_field_common_string, 1);

	BT_LOGD("Creating CTF writer string field object: ft-addr=%p", type);

	if (string) {
		bt_field_common_initialize(BT_TO_COMMON(string),
			(void *) type,
			(bt_object_release_func)
				bt_ctf_field_string_destroy,
			&bt_ctf_field_string_methods);
		string->common.spec.writer.serialize_func =
			(bt_ctf_field_serialize_recursive_func) bt_ctf_field_string_serialize;
		BT_LOGD("Created CTF writer string field object: addr=%p, ft-addr=%p",
			string, type);
	} else {
		BT_LOGE_STR("Failed to allocate one string field.");
	}

	return (void *) string;
}

static
void bt_ctf_field_enumeration_set_is_frozen_recursive(
		struct bt_field_common *field, bool is_frozen)
{
	struct bt_ctf_field_enumeration *enumeration = (void *) field;

	if (enumeration->container) {
		bt_field_common_set_is_frozen_recursive(
			(void *) enumeration->container, is_frozen);
	}

	bt_field_common_generic_set_is_frozen((void *) field, is_frozen);
}

static
int bt_ctf_field_enumeration_validate_recursive(struct bt_field_common *field)
{
	int ret = -1;
	struct bt_ctf_field_enumeration *enumeration = (void *) field;

	if (enumeration->container) {
		ret = bt_field_common_validate_recursive(
			(void *) enumeration->container);
	}

	return ret;
}

static
bt_bool bt_ctf_field_enumeration_is_set_recursive(struct bt_field_common *field)
{
	bt_bool is_set = BT_FALSE;
	struct bt_ctf_field_enumeration *enumeration = (void *) field;

	if (enumeration->container) {
		is_set = bt_field_common_is_set_recursive(
			(void *) enumeration->container);
	}

	return is_set;
}

static
void bt_ctf_field_enumeration_reset_recursive(struct bt_field_common *field)
{
	struct bt_ctf_field_enumeration *enumeration = (void *) field;

	if (enumeration->container) {
		bt_field_common_reset_recursive(
			(void *) enumeration->container);
	}

	bt_field_common_generic_reset((void *) field);
}

static
void bt_ctf_field_variant_set_is_frozen_recursive(
		struct bt_field_common *field, bool is_frozen)
{
	struct bt_ctf_field_variant *variant = (void *) field;

	if (variant->tag) {
		bt_field_common_set_is_frozen_recursive(
			(void *) variant->tag, is_frozen);
	}

	bt_field_common_variant_set_is_frozen_recursive((void *) field,
		is_frozen);
}

static
int bt_ctf_field_variant_validate_recursive(struct bt_field_common *field)
{
	int ret;
	struct bt_ctf_field_variant *variant = (void *) field;

	if (variant->tag) {
		ret = bt_field_common_validate_recursive(
			(void *) variant->tag);
		if (ret) {
			goto end;
		}
	}

	ret = bt_field_common_variant_validate_recursive((void *) field);

end:
	return ret;
}

static
bt_bool bt_ctf_field_variant_is_set_recursive(struct bt_field_common *field)
{
	bt_bool is_set;
	struct bt_ctf_field_variant *variant = (void *) field;

	if (variant->tag) {
		is_set = bt_field_common_is_set_recursive(
			(void *) variant->tag);
		if (is_set) {
			goto end;
		}
	}

	is_set = bt_field_common_variant_is_set_recursive((void *) field);

end:
	return is_set;
}

static
void bt_ctf_field_variant_reset_recursive(struct bt_field_common *field)
{
	struct bt_ctf_field_variant *variant = (void *) field;

	if (variant->tag) {
		bt_field_common_reset_recursive(
			(void *) variant->tag);
	}

	bt_field_common_variant_reset_recursive((void *) field);
}

BT_ASSERT_PRE_FUNC
static inline bool field_to_set_has_expected_type(
		struct bt_field_common *struct_field,
		const char *name, struct bt_field_common *value)
{
	bool ret = true;
	struct bt_field_type_common *expected_field_type = NULL;

	expected_field_type =
		bt_field_type_common_structure_borrow_field_type_by_name(
			struct_field->type, name);

	if (bt_field_type_common_compare(expected_field_type, value->type)) {
		BT_ASSERT_PRE_MSG("Value field's type is different from the expected field type: "
			"%![value-ft-]+_F, %![expected-ft-]+_F", value->type,
			expected_field_type);
		ret = false;
		goto end;
	}

end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_structure_set_field_by_name(struct bt_ctf_field *field,
		const char *name, struct bt_ctf_field *value)
{
	int ret = 0;
	GQuark field_quark;
	struct bt_field_common *common_field = (void *) field;
	struct bt_field_common_structure *structure =
		BT_FROM_COMMON(common_field);
	struct bt_field_common *common_value = (void *) value;
	size_t index;
	GHashTable *field_name_to_index;
	struct bt_field_type_common_structure *structure_ft;

	BT_ASSERT_PRE_NON_NULL(field, "Parent field");
	BT_ASSERT_PRE_NON_NULL(name, "Field name");
	BT_ASSERT_PRE_NON_NULL(value, "Value field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(common_field,
		BT_FIELD_TYPE_ID_STRUCT, "Parent field");
	BT_ASSERT_PRE(field_to_set_has_expected_type(common_field,
		name, common_value),
		"Value field's type is different from the expected field type.");
	field_quark = g_quark_from_string(name);
	structure_ft = BT_FROM_COMMON(common_field->type);
	field_name_to_index = structure_ft->field_name_to_index;
	if (!g_hash_table_lookup_extended(field_name_to_index,
			GUINT_TO_POINTER(field_quark), NULL,
			(gpointer *) &index)) {
		BT_LOGV("Invalid parameter: no such field in structure field's type: "
			"struct-field-addr=%p, struct-ft-addr=%p, "
			"field-ft-addr=%p, name=\"%s\"",
			field, common_field->type, common_value->type, name);
		ret = -1;
		goto end;
	}
	bt_get(value);
	BT_MOVE(structure->fields->pdata[index], value);

end:
	return ret;
}
