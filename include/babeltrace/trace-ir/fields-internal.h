#ifndef BABELTRACE_TRACE_IR_FIELDS_INTERNAL_H
#define BABELTRACE_TRACE_IR_FIELDS_INTERNAL_H

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

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/common-internal.h>
#include <babeltrace/trace-ir/field-classes-internal.h>
#include <babeltrace/trace-ir/utils-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/types.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <glib.h>

#define BT_ASSERT_PRE_FIELD_HAS_CLASS_TYPE(_field, _cls_type, _name)	\
	BT_ASSERT_PRE(((struct bt_field *) (_field))->class->type == (_cls_type), \
		_name " has the wrong class type: expected-class-type=%s, " \
		"%![field-]+f",						\
		bt_common_field_class_type_string(_cls_type), (_field))

#define BT_ASSERT_PRE_FIELD_IS_UNSIGNED_INT(_field, _name)		\
	BT_ASSERT_PRE(							\
		((struct bt_field *) (_field))->class->type == BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER || \
		((struct bt_field *) (_field))->class->type == BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION, \
		_name " is not an unsigned integer field: %![field-]+f", \
		(_field))

#define BT_ASSERT_PRE_FIELD_IS_SIGNED_INT(_field, _name)		\
	BT_ASSERT_PRE(							\
		((struct bt_field *) (_field))->class->type == BT_FIELD_CLASS_TYPE_SIGNED_INTEGER || \
		((struct bt_field *) (_field))->class->type == BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION, \
		_name " is not a signed integer field: %![field-]+f", \
		(_field))

#define BT_ASSERT_PRE_FIELD_IS_ARRAY(_field, _name)			\
	BT_ASSERT_PRE(							\
		((struct bt_field *) (_field))->class->type == BT_FIELD_CLASS_TYPE_STATIC_ARRAY || \
		((struct bt_field *) (_field))->class->type == BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY, \
		_name " is not an array field: %![field-]+f", (_field))

#define BT_ASSERT_PRE_FIELD_IS_SET(_field, _name)			\
	BT_ASSERT_PRE(bt_field_is_set(_field),				\
		_name " is not set: %!+f", (_field))

#define BT_ASSERT_PRE_FIELD_HOT(_field, _name)				\
	BT_ASSERT_PRE_HOT((struct bt_field *) (_field), (_name),	\
		": %!+f", (_field))

struct bt_field;

typedef struct bt_field *(* bt_field_create_func)(struct bt_field_class *);
typedef void (*bt_field_method_set_is_frozen)(struct bt_field *, bool);
typedef bool (*bt_field_method_is_set)(struct bt_field *);
typedef void (*bt_field_method_reset)(struct bt_field *);

struct bt_field_methods {
	bt_field_method_set_is_frozen set_is_frozen;
	bt_field_method_is_set is_set;
	bt_field_method_reset reset;
};

struct bt_field {
	struct bt_object base;

	/* Owned by this */
	struct bt_field_class *class;

	/* Virtual table for slow path (dev mode) operations */
	struct bt_field_methods *methods;

	bool is_set;
	bool frozen;
};

struct bt_field_integer {
	struct bt_field common;

	union {
		uint64_t u;
		int64_t i;
	} value;
};

struct bt_field_real {
	struct bt_field common;
	double value;
};

struct bt_field_structure {
	struct bt_field common;

	/* Array of `struct bt_field *`, owned by this */
	GPtrArray *fields;
};

struct bt_field_variant {
	struct bt_field common;

	/* Weak: belongs to `fields` below */
	struct bt_field *selected_field;

	/* Index of currently selected field */
	uint64_t selected_index;

	/* Array of `struct bt_field *`, owned by this */
	GPtrArray *fields;
};

struct bt_field_array {
	struct bt_field common;

	/* Array of `struct bt_field *`, owned by this */
	GPtrArray *fields;

	/* Current effective length */
	uint64_t length;
};

struct bt_field_string {
	struct bt_field common;
	GArray *buf;
	uint64_t length;
};

#ifdef BT_DEV_MODE
# define bt_field_set_is_frozen		_bt_field_set_is_frozen
# define bt_field_is_set		_bt_field_is_set
# define bt_field_reset			_bt_field_reset
# define bt_field_set_single		_bt_field_set_single
#else
# define bt_field_set_is_frozen(_field, _is_frozen)
# define bt_field_is_set(_field)	(BT_FALSE)
# define bt_field_reset(_field)
# define bt_field_set_single(_field, _val)
#endif

BT_HIDDEN
void _bt_field_set_is_frozen(struct bt_field *field, bool is_frozen);

static inline
void _bt_field_reset(struct bt_field *field)
{
	BT_ASSERT(field);
	BT_ASSERT(field->methods->reset);
	field->methods->reset(field);
}

static inline
void _bt_field_set_single(struct bt_field *field, bool value)
{
	BT_ASSERT(field);
	field->is_set = value;
}

static inline
bt_bool _bt_field_is_set(struct bt_field *field)
{
	bt_bool is_set = BT_FALSE;

	if (!field) {
		goto end;
	}

	BT_ASSERT(bt_field_class_has_known_type(field->class));
	BT_ASSERT(field->methods->is_set);
	is_set = field->methods->is_set(field);

end:
	return is_set;
}

BT_HIDDEN
struct bt_field *bt_field_create(struct bt_field_class *class);

BT_HIDDEN
void bt_field_destroy(struct bt_field *field);

#endif /* BABELTRACE_TRACE_IR_FIELDS_INTERNAL_H */
