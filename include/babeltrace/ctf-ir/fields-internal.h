#ifndef BABELTRACE_CTF_IR_FIELDS_INTERNAL_H
#define BABELTRACE_CTF_IR_FIELDS_INTERNAL_H

/*
 * Babeltrace - CTF IR: Event Fields internal
 *
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

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/common-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/utils-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/types.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <glib.h>

#define BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(_field, _type_id, _name)	\
	BT_ASSERT_PRE((_field)->type->id == ((int) (_type_id)),		\
		_name " has the wrong type ID: expected-type-id=%s, "	\
		"%![field-]+f",					\
		bt_common_field_type_id_string((int) (_type_id)), (_field))

#define BT_ASSERT_PRE_FIELD_IS_SET(_field, _name)		\
	BT_ASSERT_PRE(bt_field_is_set_recursive(_field),		\
		_name " is not set: %!+f", (_field))

#define BT_ASSERT_PRE_FIELD_HOT(_field, _name)			\
	BT_ASSERT_PRE_HOT((_field), (_name), ": %!+f", (_field))

struct bt_field;

typedef void (*bt_field_method_set_is_frozen)(struct bt_field *,
		bool);
typedef int (*bt_field_method_validate)(struct bt_field *);
typedef bt_bool (*bt_field_method_is_set)(struct bt_field *);
typedef void (*bt_field_method_reset)(struct bt_field *);

struct bt_field_methods {
	bt_field_method_set_is_frozen set_is_frozen;
	bt_field_method_validate validate;
	bt_field_method_is_set is_set;
	bt_field_method_reset reset;
};

struct bt_field {
	struct bt_object base;
	struct bt_field_type *type;
	struct bt_field_methods *methods;
	bool payload_set;
	bool frozen;
};

struct bt_field_integer {
	struct bt_field common;
	union {
		int64_t signd;
		uint64_t unsignd;
	} payload;
};

struct bt_field_enumeration {
	struct bt_field_integer common;
};

struct bt_field_floating_point {
	struct bt_field common;
	double payload;
};

struct bt_field_structure {
	struct bt_field common;

	/* Array of `struct bt_field *`, owned by this */
	GPtrArray *fields;
};

struct bt_field_variant {
	struct bt_field common;

	union {
		uint64_t u;
		int64_t i;
	} tag_value;

	/* Weak: belongs to `choices` below */
	struct bt_field *current_field;

	/* Array of `struct bt_field *`, owned by this */
	GPtrArray *fields;
};

struct bt_field_array {
	struct bt_field common;

	/* Array of `struct bt_field *`, owned by this */
	GPtrArray *elements;
};

struct bt_field_sequence {
	struct bt_field common;

	/*
	 * This is the true sequence field's length: its value can be
	 * less than `elements->len` below because we never shrink the
	 * array of elements to avoid reallocation.
	 */
	uint64_t length;

	/* Array of `struct bt_field *`, owned by this */
	GPtrArray *elements;
};

struct bt_field_string {
	struct bt_field common;
	GArray *buf;
	size_t size;
};

#ifdef BT_DEV_MODE
# define bt_field_validate_recursive			_bt_field_validate_recursive
# define bt_field_set_is_frozen_recursive		_bt_field_set_is_frozen_recursive
# define bt_field_is_set_recursive			_bt_field_is_set_recursive
# define bt_field_reset_recursive			_bt_field_reset_recursive
# define bt_field_set					_bt_field_set
#else
# define bt_field_validate_recursive(_field)		(-1)
# define bt_field_set_is_frozen_recursive(_field, _is_frozen)
# define bt_field_is_set_recursive(_field)		(BT_FALSE)
# define bt_field_reset_recursive(_field)
# define bt_field_set(_field, _val)
#endif

BT_HIDDEN
void _bt_field_set_is_frozen_recursive(struct bt_field *field,
		bool is_frozen);

static inline
int _bt_field_validate_recursive(struct bt_field *field)
{
	int ret = 0;

	if (!field) {
		BT_ASSERT_PRE_MSG("%s", "Invalid field: field is NULL.");
		ret = -1;
		goto end;
	}

	BT_ASSERT(bt_field_type_has_known_id(field->type));

	if (field->methods->validate) {
		ret = field->methods->validate(field);
	}

end:
	return ret;
}

static inline
void _bt_field_reset_recursive(struct bt_field *field)
{
	BT_ASSERT(field);
	BT_ASSERT(field->methods->reset);
	field->methods->reset(field);
}

static inline
void _bt_field_set(struct bt_field *field, bool value)
{
	BT_ASSERT(field);
	field->payload_set = value;
}

static inline
bt_bool _bt_field_is_set_recursive(struct bt_field *field)
{
	bt_bool is_set = BT_FALSE;

	if (!field) {
		goto end;
	}

	BT_ASSERT(bt_field_type_has_known_id(field->type));
	BT_ASSERT(field->methods->is_set);
	is_set = field->methods->is_set(field);

end:
	return is_set;
}

BT_HIDDEN
struct bt_field *bt_field_create_recursive(struct bt_field_type *type);

BT_HIDDEN
void bt_field_destroy_recursive(struct bt_field *field);

#endif /* BABELTRACE_CTF_IR_FIELDS_INTERNAL_H */
