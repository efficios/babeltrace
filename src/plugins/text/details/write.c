/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace2/babeltrace.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "common/assert.h"
#include "common/common.h"
#include "common/uuid.h"
#include "details.h"
#include "write.h"
#include "obj-lifetime-mgmt.h"
#include "colors.h"

static inline
const char *plural(uint64_t value)
{
	return value == 1 ? "" : "s";
}

static inline
void incr_indent_by(struct details_write_ctx *ctx, unsigned int value)
{
	BT_ASSERT_DBG(ctx);
	ctx->indent_level += value;
}

static inline
void incr_indent(struct details_write_ctx *ctx)
{
	incr_indent_by(ctx, 2);
}

static inline
void decr_indent_by(struct details_write_ctx *ctx, unsigned int value)
{
	BT_ASSERT_DBG(ctx);
	BT_ASSERT_DBG(ctx->indent_level >= value);
	ctx->indent_level -= value;
}

static inline
void decr_indent(struct details_write_ctx *ctx)
{
	decr_indent_by(ctx, 2);
}

static inline
void format_uint(char *buf, uint64_t value, unsigned int base)
{
	const char *spec = "%" PRIu64;
	char *buf_start = buf;
	unsigned int digits_per_group = 3;
	char sep = ',';
	bool sep_digits = true;

	switch (base) {
	case 2:
	case 16:
		/* TODO: Support binary format */
		spec = "%" PRIx64;
		strcpy(buf, "0x");
		buf_start = buf + 2;
		digits_per_group = 4;
		sep = ':';
		break;
	case 8:
		spec = "%" PRIo64;
		strcpy(buf, "0");
		buf_start = buf + 1;
		sep = ':';
		break;
	case 10:
		if (value <= 9999) {
			/*
			 * Do not insert digit separators for numbers
			 * under 10,000 as it looks weird.
			 */
			sep_digits = false;
		}

		break;
	default:
		bt_common_abort();
	}

	sprintf(buf_start, spec, value);

	if (sep_digits) {
		bt_common_sep_digits(buf_start, digits_per_group, sep);
	}
}

static inline
void format_int(char *buf, int64_t value, unsigned int base)
{
	const char *spec = "%" PRIu64;
	char *buf_start = buf;
	unsigned int digits_per_group = 3;
	char sep = ',';
	bool sep_digits = true;
	uint64_t abs_value = value < 0 ? (uint64_t) -value : (uint64_t) value;

	if (value < 0) {
		buf[0] = '-';
		buf_start++;
	}

	switch (base) {
	case 2:
	case 16:
		/* TODO: Support binary format */
		spec = "%" PRIx64;
		strcpy(buf_start, "0x");
		buf_start += 2;
		digits_per_group = 4;
		sep = ':';
		break;
	case 8:
		spec = "%" PRIo64;
		strcpy(buf_start, "0");
		buf_start++;
		sep = ':';
		break;
	case 10:
		if (value >= -9999 && value <= 9999) {
			/*
			 * Do not insert digit separators for numbers
			 * over -10,000 and under 10,000 as it looks
			 * weird.
			 */
			sep_digits = false;
		}

		break;
	default:
		bt_common_abort();
	}

	sprintf(buf_start, spec, abs_value);

	if (sep_digits) {
		bt_common_sep_digits(buf_start, digits_per_group, sep);
	}
}

static inline
void write_nl(struct details_write_ctx *ctx)
{
	BT_ASSERT_DBG(ctx);
	g_string_append_c(ctx->str, '\n');
}

static inline
void write_sp(struct details_write_ctx *ctx)
{
	BT_ASSERT_DBG(ctx);
	g_string_append_c(ctx->str, ' ');
}

static inline
void write_indent(struct details_write_ctx *ctx)
{
	uint64_t i;

	BT_ASSERT_DBG(ctx);

	for (i = 0; i < ctx->indent_level; i++) {
		write_sp(ctx);
	}
}

static inline
void write_compound_member_name(struct details_write_ctx *ctx, const char *name)
{
	write_indent(ctx);
	g_string_append_printf(ctx->str, "%s%s%s:",
		color_fg_cyan(ctx), name, color_reset(ctx));
}

static inline
void write_array_index(struct details_write_ctx *ctx, uint64_t index,
		const char *color)
{
	char buf[32];

	write_indent(ctx);
	format_uint(buf, index, 10);
	g_string_append_printf(ctx->str, "%s[%s]%s:",
		color, buf, color_reset(ctx));
}

static inline
void write_obj_type_name(struct details_write_ctx *ctx, const char *name)
{
	g_string_append_printf(ctx->str, "%s%s%s%s",
		color_bold(ctx), color_fg_bright_yellow(ctx), name,
		color_reset(ctx));
}

static inline
void write_prop_name(struct details_write_ctx *ctx, const char *prop_name)
{
	g_string_append_printf(ctx->str, "%s%s%s",
		color_fg_magenta(ctx), prop_name, color_reset(ctx));
}

static inline
void write_prop_name_line(struct details_write_ctx *ctx, const char *prop_name)
{
	write_indent(ctx);
	g_string_append_printf(ctx->str, "%s%s%s:",
		color_fg_magenta(ctx), prop_name, color_reset(ctx));
}

static inline
void write_str_prop_value(struct details_write_ctx *ctx, const char *value)
{
	g_string_append_printf(ctx->str, "%s%s%s",
		color_bold(ctx), value, color_reset(ctx));
}

static inline
void write_none_prop_value(struct details_write_ctx *ctx, const char *value)
{
	g_string_append_printf(ctx->str, "%s%s%s%s",
		color_bold(ctx), color_fg_bright_magenta(ctx),
		value, color_reset(ctx));
}

static inline
void write_uint_str_prop_value(struct details_write_ctx *ctx, const char *value)
{
	write_str_prop_value(ctx, value);
}

static inline
void write_uint_prop_value(struct details_write_ctx *ctx, uint64_t value)
{
	char buf[32];

	format_uint(buf, value, 10);
	write_uint_str_prop_value(ctx, buf);
}

static inline
void write_int_prop_value(struct details_write_ctx *ctx, int64_t value)
{
	char buf[32];

	format_int(buf, value, 10);
	write_uint_str_prop_value(ctx, buf);
}

static inline
void write_float_prop_value(struct details_write_ctx *ctx, double value)
{
	g_string_append_printf(ctx->str, "%s%f%s",
		color_bold(ctx), value, color_reset(ctx));
}

static inline
void write_str_prop_line(struct details_write_ctx *ctx, const char *prop_name,
		const char *prop_value)
{
	BT_ASSERT_DBG(prop_value);
	write_indent(ctx);
	write_prop_name(ctx, prop_name);
	g_string_append(ctx->str, ": ");
	write_str_prop_value(ctx, prop_value);
	write_nl(ctx);
}

static inline
void write_uint_prop_line(struct details_write_ctx *ctx, const char *prop_name,
		uint64_t prop_value)
{
	write_indent(ctx);
	write_prop_name(ctx, prop_name);
	g_string_append(ctx->str, ": ");
	write_uint_prop_value(ctx, prop_value);
	write_nl(ctx);
}

static inline
void write_int_prop_line(struct details_write_ctx *ctx, const char *prop_name,
		int64_t prop_value)
{
	write_indent(ctx);
	write_prop_name(ctx, prop_name);
	g_string_append(ctx->str, ": ");
	write_int_prop_value(ctx, prop_value);
	write_nl(ctx);
}

static inline
void write_int_str_prop_value(struct details_write_ctx *ctx, const char *value)
{
	write_str_prop_value(ctx, value);
}

static inline
void write_bool_prop_value(struct details_write_ctx *ctx, bt_bool prop_value)
{
	const char *str;

	g_string_append(ctx->str, color_bold(ctx));

	if (prop_value) {
		g_string_append(ctx->str, color_fg_bright_green(ctx));
		str = "Yes";
	} else {
		g_string_append(ctx->str, color_fg_bright_red(ctx));
		str = "No";
	}

	g_string_append_printf(ctx->str, "%s%s", str, color_reset(ctx));
}

static inline
void write_bool_prop_line(struct details_write_ctx *ctx, const char *prop_name,
		bt_bool prop_value)
{
	write_indent(ctx);
	write_prop_name(ctx, prop_name);
	g_string_append(ctx->str, ": ");
	write_bool_prop_value(ctx, prop_value);
	write_nl(ctx);
}

static inline
void write_uuid_prop_line(struct details_write_ctx *ctx, const char *prop_name,
		bt_uuid uuid)
{
	BT_ASSERT_DBG(uuid);
	write_indent(ctx);
	write_prop_name(ctx, prop_name);
	g_string_append_printf(ctx->str,
		": %s" BT_UUID_FMT "%s\n",
		color_bold(ctx),
		BT_UUID_FMT_VALUES(uuid),
		color_reset(ctx));
}

static
gint compare_strings(const char **a, const char **b)
{
	return strcmp(*a, *b);
}

static
bt_value_map_foreach_entry_const_func_status map_value_foreach_add_key_to_array(
		const char *key, const bt_value *object, void *data)
{
	GPtrArray *keys = data;

	BT_ASSERT_DBG(keys);
	g_ptr_array_add(keys, (void *) key);
	return BT_VALUE_MAP_FOREACH_ENTRY_CONST_FUNC_STATUS_OK;
}

static
void write_value(struct details_write_ctx *ctx, const bt_value *value,
		const char *name)
{
	uint64_t i;
	bt_value_type value_type = bt_value_get_type(value);
	GPtrArray *keys = g_ptr_array_new();
	char buf[64];

	BT_ASSERT_DBG(keys);

	/* Write field's name */
	if (name) {
		write_prop_name_line(ctx, name);
	}

	/* Write field's value */
	switch (value_type) {
	case BT_VALUE_TYPE_NULL:
		write_sp(ctx);
		write_none_prop_value(ctx, "Null");
		break;
	case BT_VALUE_TYPE_BOOL:
		write_sp(ctx);
		write_bool_prop_value(ctx, bt_value_bool_get(value));
		break;
	case BT_VALUE_TYPE_UNSIGNED_INTEGER:
		format_uint(buf, bt_value_integer_unsigned_get(value), 10);
		write_sp(ctx);
		write_uint_str_prop_value(ctx, buf);
		break;
	case BT_VALUE_TYPE_SIGNED_INTEGER:
		format_int(buf, bt_value_integer_signed_get(value), 10);
		write_sp(ctx);
		write_int_str_prop_value(ctx, buf);
		break;
	case BT_VALUE_TYPE_REAL:
		write_sp(ctx);
		write_float_prop_value(ctx, bt_value_real_get(value));
		break;
	case BT_VALUE_TYPE_STRING:
		write_sp(ctx);
		write_str_prop_value(ctx, bt_value_string_get(value));
		break;
	case BT_VALUE_TYPE_ARRAY:
	{
		uint64_t length = bt_value_array_get_length(value);

		if (length == 0) {
			write_sp(ctx);
			write_none_prop_value(ctx, "Empty");
		} else {
			g_string_append(ctx->str, " Length ");
			write_uint_prop_value(ctx, length);
			g_string_append_c(ctx->str, ':');
		}

		incr_indent(ctx);

		for (i = 0; i < length; i++) {
			const bt_value *elem_value =
				bt_value_array_borrow_element_by_index_const(
					value, i);

			write_nl(ctx);
			write_array_index(ctx, i, color_fg_magenta(ctx));
			write_value(ctx, elem_value, NULL);
		}

		decr_indent(ctx);
		break;
	}
	case BT_VALUE_TYPE_MAP:
	{
		bt_value_map_foreach_entry_const_status foreach_status =
			bt_value_map_foreach_entry_const(value,
				map_value_foreach_add_key_to_array, keys);

		BT_ASSERT_DBG(foreach_status ==
			BT_VALUE_MAP_FOREACH_ENTRY_CONST_STATUS_OK);
		g_ptr_array_sort(keys, (GCompareFunc) compare_strings);

		if (keys->len > 0) {
			incr_indent(ctx);

			for (i = 0; i < keys->len; i++) {
				const char *key = keys->pdata[i];
				const bt_value *entry_value =
					bt_value_map_borrow_entry_value_const(
						value, key);

				write_nl(ctx);
				write_value(ctx, entry_value, key);
			}

			decr_indent(ctx);
		} else {
			write_sp(ctx);
			write_none_prop_value(ctx, "Empty");
		}

		break;
	}
	default:
		bt_common_abort();
	}

	g_ptr_array_free(keys, TRUE);
}

static
void write_user_attributes(struct details_write_ctx *ctx,
		const bt_value *user_attrs, bool write_newline, bool *written)
{
	BT_ASSERT_DBG(user_attrs);

	if (!bt_value_map_is_empty(user_attrs)) {
		write_value(ctx, user_attrs, "User attributes");

		if (write_newline) {
			write_nl(ctx);
		}

		if (written) {
			*written = true;
		}
	}
}

static
void write_int_field_class_props(struct details_write_ctx *ctx,
		const bt_field_class *fc, bool close)
{
	g_string_append_printf(ctx->str, "(%s%" PRIu64 "-bit%s, Base ",
		color_bold(ctx),
		bt_field_class_integer_get_field_value_range(fc),
		color_reset(ctx));

	switch (bt_field_class_integer_get_preferred_display_base(fc)) {
	case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_BINARY:
		write_uint_prop_value(ctx, 2);
		break;
	case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL:
		write_uint_prop_value(ctx, 8);
		break;
	case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL:
		write_uint_prop_value(ctx, 10);
		break;
	case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL:
		write_uint_prop_value(ctx, 16);
		break;
	default:
		bt_common_abort();
	}

	if (close) {
		g_string_append(ctx->str, ")");
	}
}

struct int_range {
	union {
		uint64_t u;
		int64_t i;
	} lower;

	union {
		uint64_t u;
		int64_t i;
	} upper;
};

struct enum_field_class_mapping {
	/* Weak */
	const char *label;

	/* Array of `struct int_range` */
	GArray *ranges;
};

static
gint compare_enum_field_class_mappings(struct enum_field_class_mapping **a,
		struct enum_field_class_mapping **b)
{
	return strcmp((*a)->label, (*b)->label);
}

static
gint compare_int_ranges_signed(struct int_range *a, struct int_range *b)
{

	if (a->lower.i < b->lower.i) {
		return -1;
	} else if (a->lower.i > b->lower.i) {
		return 1;
	} else {
		if (a->upper.i < b->upper.i) {
			return -1;
		} else if (a->upper.i > b->upper.i) {
			return 1;
		} else {
			return 0;
		}
	}
}

static
gint compare_int_ranges_unsigned(struct int_range *a, struct int_range *b)
{
	if (a->lower.u < b->lower.u) {
		return -1;
	} else if (a->lower.u > b->lower.u) {
		return 1;
	} else {
		if (a->upper.u < b->upper.u) {
			return -1;
		} else if (a->upper.u > b->upper.u) {
			return 1;
		} else {
			return 0;
		}
	}
}

static
GArray *range_set_to_int_ranges(const void *spec_range_set, bool is_signed)
{
	uint64_t i;
	const bt_integer_range_set *range_set;
	GArray *ranges = g_array_new(FALSE, TRUE, sizeof(struct int_range));

	if (!ranges) {
		goto end;
	}

	if (is_signed) {
		range_set = bt_integer_range_set_signed_as_range_set_const(
			spec_range_set);
	} else {
		range_set = bt_integer_range_set_unsigned_as_range_set_const(
			spec_range_set);
	}

	for (i = 0; i < bt_integer_range_set_get_range_count(range_set); i++) {
		struct int_range range;

		if (is_signed) {
			const bt_integer_range_signed *orig_range =
				bt_integer_range_set_signed_borrow_range_by_index_const(
					spec_range_set, i);

			range.lower.i = bt_integer_range_signed_get_lower(orig_range);
			range.upper.i = bt_integer_range_signed_get_upper(orig_range);
		} else {
			const bt_integer_range_unsigned *orig_range =
				bt_integer_range_set_unsigned_borrow_range_by_index_const(
					spec_range_set, i);

			range.lower.u = bt_integer_range_unsigned_get_lower(orig_range);
			range.upper.u = bt_integer_range_unsigned_get_upper(orig_range);
		}

		g_array_append_val(ranges, range);
	}

	if (is_signed) {
		g_array_sort(ranges, (GCompareFunc) compare_int_ranges_signed);
	} else {
		g_array_sort(ranges,
			(GCompareFunc) compare_int_ranges_unsigned);
	}

end:
	return ranges;
}

static
void destroy_enum_field_class_mapping(struct enum_field_class_mapping *mapping)
{
	if (mapping->ranges) {
		g_array_free(mapping->ranges, TRUE);
		mapping->ranges = NULL;
	}

	g_free(mapping);
}

static
struct int_range *int_range_at(GArray *ranges, uint64_t index)
{
	return &g_array_index(ranges, struct int_range, index);
}

static
void write_int_range(struct details_write_ctx *ctx,
		struct int_range *range, bool is_signed)
{
	g_string_append(ctx->str, "[");

	if (is_signed) {
		write_int_prop_value(ctx, range->lower.i);
	} else {
		write_int_prop_value(ctx, range->lower.u);
	}

	if (range->lower.u != range->upper.u) {
		g_string_append(ctx->str, ", ");

		if (is_signed) {
			write_int_prop_value(ctx, range->upper.i);
		} else {
			write_int_prop_value(ctx, range->upper.u);
		}
	}

	g_string_append(ctx->str, "]");
}

static
void write_enum_field_class_mappings(struct details_write_ctx *ctx,
		const bt_field_class *fc)
{
	GPtrArray *mappings;
	uint64_t i;
	uint64_t range_i;
	bool is_signed = bt_field_class_get_type(fc) ==
		BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION;

	mappings = g_ptr_array_new_with_free_func(
		(GDestroyNotify) destroy_enum_field_class_mapping);
	BT_ASSERT_DBG(mappings);

	/*
	 * Copy field class's mappings to our own arrays and structures
	 * to sort them.
	 */
	for (i = 0; i < bt_field_class_enumeration_get_mapping_count(fc); i++) {
		const void *fc_mapping;
		const void *fc_range_set;
		struct enum_field_class_mapping *mapping = g_new0(
			struct enum_field_class_mapping, 1);

		BT_ASSERT_DBG(mapping);

		if (is_signed) {
			fc_mapping = bt_field_class_enumeration_signed_borrow_mapping_by_index_const(
				fc, i);
			fc_range_set = bt_field_class_enumeration_signed_mapping_borrow_ranges_const(
				fc_mapping);
		} else {
			fc_mapping = bt_field_class_enumeration_unsigned_borrow_mapping_by_index_const(
				fc, i);
			fc_range_set = bt_field_class_enumeration_unsigned_mapping_borrow_ranges_const(
				fc_mapping);
		}

		mapping->label = bt_field_class_enumeration_mapping_get_label(
			bt_field_class_enumeration_signed_mapping_as_mapping_const(
				fc_mapping));
		mapping->ranges = range_set_to_int_ranges(fc_range_set,
			is_signed);
		BT_ASSERT_DBG(mapping->ranges);
		g_ptr_array_add(mappings, mapping);
	}

	/* Sort mappings (ranges are already sorted within mappings) */
	g_ptr_array_sort(mappings,
		(GCompareFunc) compare_enum_field_class_mappings);

	/* Write mappings */
	for (i = 0; i < mappings->len; i++) {
		struct enum_field_class_mapping *mapping = mappings->pdata[i];

		write_nl(ctx);
		write_prop_name_line(ctx, mapping->label);

		for (range_i = 0; range_i < mapping->ranges->len; range_i++) {
			write_sp(ctx);
			write_int_range(ctx,
				int_range_at(mapping->ranges, range_i),
				is_signed);
		}
	}

	g_ptr_array_free(mappings, TRUE);
}

static
void write_field_path(struct details_write_ctx *ctx,
		const bt_field_path *field_path)
{
	uint64_t i;

	g_string_append_c(ctx->str, '[');

	switch (bt_field_path_get_root_scope(field_path)) {
	case BT_FIELD_PATH_SCOPE_PACKET_CONTEXT:
		write_str_prop_value(ctx, "Packet context");
		break;
	case BT_FIELD_PATH_SCOPE_EVENT_COMMON_CONTEXT:
		write_str_prop_value(ctx, "Event common context");
		break;
	case BT_FIELD_PATH_SCOPE_EVENT_SPECIFIC_CONTEXT:
		write_str_prop_value(ctx, "Event specific context");
		break;
	case BT_FIELD_PATH_SCOPE_EVENT_PAYLOAD:
		write_str_prop_value(ctx, "Event payload");
		break;
	default:
		bt_common_abort();
	}

	g_string_append(ctx->str, ": ");

	for (i = 0; i < bt_field_path_get_item_count(field_path); i++) {
		const bt_field_path_item *fp_item =
			bt_field_path_borrow_item_by_index_const(field_path, i);

		if (i != 0) {
			g_string_append(ctx->str, ", ");
		}

		switch (bt_field_path_item_get_type(fp_item)) {
		case BT_FIELD_PATH_ITEM_TYPE_INDEX:
			write_uint_prop_value(ctx,
				bt_field_path_item_index_get_index(fp_item));
			break;
		case BT_FIELD_PATH_ITEM_TYPE_CURRENT_ARRAY_ELEMENT:
			write_str_prop_value(ctx, "<current>");
			break;
		default:
			bt_common_abort();
		}
	}

	g_string_append_c(ctx->str, ']');
}

static
void write_field_class(struct details_write_ctx *ctx, const bt_field_class *fc);

static
void write_variant_field_class_option(struct details_write_ctx *ctx,
		const bt_field_class *fc, uint64_t index)
{
	bt_field_class_type fc_type = bt_field_class_get_type(fc);
	const bt_field_class_variant_option *option =
		bt_field_class_variant_borrow_option_by_index_const(
			fc, index);
	const void *orig_ranges = NULL;
	GArray *int_ranges = NULL;
	bool is_signed;
	const bt_value *user_attrs =
		bt_field_class_variant_option_borrow_user_attributes_const(
			option);
	const bt_field_class *option_fc =
		bt_field_class_variant_option_borrow_field_class_const(option);

	write_nl(ctx);
	write_compound_member_name(ctx,
		bt_field_class_variant_option_get_name(option));

	if (fc_type == BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD) {
		const bt_field_class_variant_with_selector_field_integer_unsigned_option *spec_opt =
			bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_index_const(
				fc, index);

		orig_ranges =
			bt_field_class_variant_with_selector_field_integer_unsigned_option_borrow_ranges_const(
				spec_opt);
		is_signed = false;
	} else if (fc_type == BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD) {
		const bt_field_class_variant_with_selector_field_integer_signed_option *spec_opt =
			bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_index_const(
				fc, index);

		orig_ranges =
			bt_field_class_variant_with_selector_field_integer_signed_option_borrow_ranges_const(
				spec_opt);
		is_signed = true;
	}

	if (orig_ranges) {
		uint64_t i;

		int_ranges = range_set_to_int_ranges(orig_ranges, is_signed);
		BT_ASSERT_DBG(int_ranges);

		for (i = 0; i < int_ranges->len; i++) {
			struct int_range *range = int_range_at(int_ranges, i);

			write_sp(ctx);
			write_int_range(ctx, range, is_signed);
		}

		g_string_append(ctx->str, ": ");
	} else {
		write_sp(ctx);
	}

	if (bt_value_map_is_empty(user_attrs)) {
		write_field_class(ctx, option_fc);
	} else {
		write_nl(ctx);
		incr_indent(ctx);

		/* Field class */
		write_prop_name_line(ctx, "Field class");
		write_sp(ctx);
		write_field_class(ctx, option_fc);
		write_nl(ctx);

		/* User attributes */
		write_user_attributes(ctx, user_attrs,
			false, NULL);

		decr_indent(ctx);
	}

	if (int_ranges) {
		g_array_free(int_ranges, TRUE);
	}
}

static
void write_field_class(struct details_write_ctx *ctx, const bt_field_class *fc)
{
	uint64_t i;
	const char *type;
	bt_field_class_type fc_type = bt_field_class_get_type(fc);
	const bt_value *user_attrs;
	bool wrote_user_attrs = false;

	/* Write field class's type */
	switch (fc_type) {
	case BT_FIELD_CLASS_TYPE_BOOL:
		type = "Boolean";
		break;
	case BT_FIELD_CLASS_TYPE_BIT_ARRAY:
		type = "Bit array";
		break;
	case BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER:
		type = "Unsigned integer";
		break;
	case BT_FIELD_CLASS_TYPE_SIGNED_INTEGER:
		type = "Signed integer";
		break;
	case BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION:
		type = "Unsigned enumeration";
		break;
	case BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION:
		type = "Signed enumeration";
		break;
	case BT_FIELD_CLASS_TYPE_SINGLE_PRECISION_REAL:
		type = "Single-precision real";
		break;
	case BT_FIELD_CLASS_TYPE_DOUBLE_PRECISION_REAL:
		type = "Double-precision real";
		break;
	case BT_FIELD_CLASS_TYPE_STRING:
		type = "String";
		break;
	case BT_FIELD_CLASS_TYPE_STRUCTURE:
		type = "Structure";
		break;
	case BT_FIELD_CLASS_TYPE_STATIC_ARRAY:
		type = "Static array";
		break;
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITHOUT_LENGTH_FIELD:
		type = "Dynamic array (no length field)";
		break;
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD:
		type = "Dynamic array (with length field)";
		break;
	case BT_FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD:
		type = "Option (no selector)";
		break;
	case BT_FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD:
		type = "Option (boolean selector)";
		break;
	case BT_FIELD_CLASS_TYPE_OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD:
		type = "Option (unsigned integer selector)";
		break;
	case BT_FIELD_CLASS_TYPE_OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD:
		type = "Option (signed integer selector)";
		break;
	case BT_FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD:
		type = "Variant (no selector)";
		break;
	case BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD:
		type = "Variant (unsigned integer selector)";
		break;
	case BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD:
		type = "Variant (signed integer selector)";
		break;
	default:
		bt_common_abort();
	}

	g_string_append_printf(ctx->str, "%s%s%s",
		color_fg_blue(ctx), type, color_reset(ctx));

	/* Write field class's single-line properties */
	if (bt_field_class_type_is(fc_type, BT_FIELD_CLASS_TYPE_ENUMERATION)) {
		uint64_t mapping_count =
			bt_field_class_enumeration_get_mapping_count(fc);

		write_sp(ctx);
		write_int_field_class_props(ctx, fc, false);
		g_string_append(ctx->str, ", ");
		write_uint_prop_value(ctx, mapping_count);
		g_string_append_printf(ctx->str, " mapping%s)",
			plural(mapping_count));
	} else if (bt_field_class_type_is(fc_type,
			BT_FIELD_CLASS_TYPE_INTEGER)) {
		write_sp(ctx);
		write_int_field_class_props(ctx, fc, true);
	} else if (fc_type == BT_FIELD_CLASS_TYPE_STRUCTURE) {
		uint64_t member_count =
			bt_field_class_structure_get_member_count(fc);

		g_string_append(ctx->str, " (");
		write_uint_prop_value(ctx, member_count);
		g_string_append_printf(ctx->str, " member%s)",
			plural(member_count));
	} else if (fc_type == BT_FIELD_CLASS_TYPE_STATIC_ARRAY) {
		g_string_append(ctx->str, " (Length ");
		write_uint_prop_value(ctx,
			bt_field_class_array_static_get_length(fc));
		g_string_append_c(ctx->str, ')');
	} else if (fc_type == BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD) {
		const bt_field_path *length_field_path =
			bt_field_class_array_dynamic_with_length_field_borrow_length_field_path_const(
				fc);

		g_string_append(ctx->str, " (Length field path ");
		write_field_path(ctx, length_field_path);
		g_string_append_c(ctx->str, ')');
	} else if (bt_field_class_type_is(fc_type,
			BT_FIELD_CLASS_TYPE_OPTION_WITH_SELECTOR_FIELD)) {
		const bt_field_path *selector_field_path =
			bt_field_class_option_with_selector_field_borrow_selector_field_path_const(
				fc);

		g_string_append(ctx->str, " (Selector field path ");
		write_field_path(ctx, selector_field_path);
		g_string_append_c(ctx->str, ')');
	} else if (bt_field_class_type_is(fc_type,
			BT_FIELD_CLASS_TYPE_VARIANT)) {
		uint64_t option_count =
			bt_field_class_variant_get_option_count(fc);
		const bt_field_path *sel_field_path = NULL;

		if (bt_field_class_type_is(fc_type,
				BT_FIELD_CLASS_TYPE_VARIANT_WITH_SELECTOR_FIELD)) {
			sel_field_path =
				bt_field_class_variant_with_selector_field_borrow_selector_field_path_const(
					fc);
			BT_ASSERT_DBG(sel_field_path);
		}

		g_string_append(ctx->str, " (");
		write_uint_prop_value(ctx, option_count);
		g_string_append_printf(ctx->str, " option%s",
			plural(option_count));

		if (sel_field_path) {
			g_string_append(ctx->str, ", Selector field path ");
			write_field_path(ctx, sel_field_path);
		}

		g_string_append_c(ctx->str, ')');
	}

	incr_indent(ctx);
	user_attrs = bt_field_class_borrow_user_attributes_const(fc);
	if (!bt_value_map_is_empty(user_attrs)) {
		g_string_append(ctx->str, ":\n");
		write_user_attributes(ctx, user_attrs, false, NULL);
		wrote_user_attrs = true;
	}

	/* Write field class's complex properties */
	if (bt_field_class_type_is(fc_type, BT_FIELD_CLASS_TYPE_ENUMERATION)) {
		uint64_t mapping_count =
			bt_field_class_enumeration_get_mapping_count(fc);

		if (mapping_count > 0) {
			if (wrote_user_attrs) {
				write_nl(ctx);
				write_indent(ctx);
				write_prop_name(ctx, "Mappings");
				g_string_append_c(ctx->str, ':');
				incr_indent(ctx);
			} else {
				/* Each mapping starts with its own newline */
				g_string_append_c(ctx->str, ':');
			}

			write_enum_field_class_mappings(ctx, fc);

			if (wrote_user_attrs) {
				decr_indent(ctx);
			}
		}
	} else if (fc_type == BT_FIELD_CLASS_TYPE_STRUCTURE) {
		uint64_t member_count =
			bt_field_class_structure_get_member_count(fc);

		if (member_count > 0) {
			if (wrote_user_attrs) {
				write_nl(ctx);
				write_indent(ctx);
				write_prop_name(ctx, "Members");
				g_string_append_c(ctx->str, ':');
				incr_indent(ctx);
			} else {
				/* Each member starts with its own newline */
				g_string_append_c(ctx->str, ':');
			}

			for (i = 0; i < member_count; i++) {
				const bt_field_class_structure_member *member =
					bt_field_class_structure_borrow_member_by_index_const(
						fc, i);
				const bt_value *member_user_attrs;
				const bt_field_class *member_fc =
					bt_field_class_structure_member_borrow_field_class_const(member);

				write_nl(ctx);
				write_compound_member_name(ctx,
					bt_field_class_structure_member_get_name(member));
				member_user_attrs = bt_field_class_structure_member_borrow_user_attributes_const(
					member);

				if (bt_value_map_is_empty(member_user_attrs)) {
					write_sp(ctx);
					write_field_class(ctx, member_fc);
				} else {
					write_nl(ctx);
					incr_indent(ctx);

					/* Field class */
					write_prop_name_line(ctx, "Field class");
					write_sp(ctx);
					write_field_class(ctx, member_fc);
					write_nl(ctx);

					/* User attributes */
					write_user_attributes(ctx, member_user_attrs,
						false, NULL);

					decr_indent(ctx);
				}
			}

			if (wrote_user_attrs) {
				decr_indent(ctx);
			}
		}
	} else if (bt_field_class_type_is(fc_type, BT_FIELD_CLASS_TYPE_ARRAY)) {
		if (wrote_user_attrs) {
			write_nl(ctx);
		} else {
			g_string_append(ctx->str, ":\n");
		}

		write_prop_name_line(ctx, "Element");
		write_sp(ctx);
		write_field_class(ctx,
			bt_field_class_array_borrow_element_field_class_const(fc));
	} else if (bt_field_class_type_is(fc_type,
			BT_FIELD_CLASS_TYPE_OPTION)) {
		const void *ranges = NULL;
		bool selector_is_signed = false;

		if (wrote_user_attrs) {
			write_nl(ctx);
		} else {
			g_string_append(ctx->str, ":\n");
		}

		if (fc_type == BT_FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD) {
			write_bool_prop_line(ctx, "Selector is reversed",
				bt_field_class_option_with_selector_field_bool_selector_is_reversed(fc));
		} else if (fc_type == BT_FIELD_CLASS_TYPE_OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD) {
			ranges = bt_field_class_option_with_selector_field_integer_unsigned_borrow_selector_ranges_const(fc);
		} else if (fc_type == BT_FIELD_CLASS_TYPE_OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD) {
			ranges = bt_field_class_option_with_selector_field_integer_signed_borrow_selector_ranges_const(fc);
			selector_is_signed = true;
		}

		if (ranges) {
			GArray *sorted_ranges = range_set_to_int_ranges(
				ranges, selector_is_signed);

			BT_ASSERT_DBG(sorted_ranges);
			BT_ASSERT_DBG(sorted_ranges->len > 0);
			write_prop_name_line(ctx, "Selector ranges");

			for (i = 0; i < sorted_ranges->len; i++) {
				write_sp(ctx);
				write_int_range(ctx,
					int_range_at(sorted_ranges, i),
					selector_is_signed);
			}

			write_nl(ctx);
			g_array_free(sorted_ranges, TRUE);
		}

		write_prop_name_line(ctx, "Content");
		write_sp(ctx);
		write_field_class(ctx,
			bt_field_class_option_borrow_field_class_const(fc));
	} else if (bt_field_class_type_is(fc_type,
			BT_FIELD_CLASS_TYPE_VARIANT)) {
		uint64_t option_count =
			bt_field_class_variant_get_option_count(fc);

		if (option_count > 0) {
			if (wrote_user_attrs) {
				write_nl(ctx);
				write_indent(ctx);
				write_prop_name(ctx, "Options");
				g_string_append_c(ctx->str, ':');
				incr_indent(ctx);
			} else {
				/* Each option starts with its own newline */
				g_string_append_c(ctx->str, ':');
			}

			for (i = 0; i < option_count; i++) {
				write_variant_field_class_option(ctx, fc, i);
			}

			if (wrote_user_attrs) {
				decr_indent(ctx);
			}
		}
	}

	decr_indent(ctx);
}

static
void write_root_field_class(struct details_write_ctx *ctx, const char *name,
		const bt_field_class *fc)
{
	BT_ASSERT_DBG(name);
	BT_ASSERT_DBG(fc);
	write_indent(ctx);
	write_prop_name(ctx, name);
	g_string_append(ctx->str, ": ");
	write_field_class(ctx, fc);
	write_nl(ctx);
}

static
void write_event_class(struct details_write_ctx *ctx, const bt_event_class *ec)
{
	const char *name = bt_event_class_get_name(ec);
	const char *emf_uri;
	const bt_field_class *fc;
	bt_event_class_log_level log_level;

	write_indent(ctx);
	write_obj_type_name(ctx, "Event class");

	/* Write name and ID */
	if (name) {
		g_string_append_printf(ctx->str, " `%s%s%s`",
			color_fg_green(ctx), name, color_reset(ctx));
	}

	g_string_append(ctx->str, " (ID ");
	write_uint_prop_value(ctx, bt_event_class_get_id(ec));
	g_string_append(ctx->str, "):\n");

	/* Write properties */
	incr_indent(ctx);

	/* Write user attributes */
	write_user_attributes(ctx,
		bt_event_class_borrow_user_attributes_const(ec), true, NULL);

	/* Write log level */
	if (bt_event_class_get_log_level(ec, &log_level) ==
			BT_PROPERTY_AVAILABILITY_AVAILABLE) {
		const char *ll_str = NULL;

		switch (log_level) {
		case BT_EVENT_CLASS_LOG_LEVEL_EMERGENCY:
			ll_str = "Emergency";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_ALERT:
			ll_str = "Alert";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_CRITICAL:
			ll_str = "Critical";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_ERROR:
			ll_str = "Error";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_WARNING:
			ll_str = "Warning";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_NOTICE:
			ll_str = "Notice";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_INFO:
			ll_str = "Info";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM:
			ll_str = "Debug (system)";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM:
			ll_str = "Debug (program)";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS:
			ll_str = "Debug (process)";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE:
			ll_str = "Debug (module)";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT:
			ll_str = "Debug (unit)";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION:
			ll_str = "Debug (function)";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE:
			ll_str = "Debug (line)";
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG:
			ll_str = "Debug";
			break;
		default:
			bt_common_abort();
		}

		write_str_prop_line(ctx, "Log level", ll_str);
	}

	/* Write EMF URI */
	emf_uri = bt_event_class_get_emf_uri(ec);
	if (emf_uri) {
		write_str_prop_line(ctx, "EMF URI", emf_uri);
	}

	/* Write specific context field class */
	fc = bt_event_class_borrow_specific_context_field_class_const(ec);
	if (fc) {
		write_root_field_class(ctx, "Specific context field class", fc);
	}

	/* Write payload field class */
	fc = bt_event_class_borrow_payload_field_class_const(ec);
	if (fc) {
		write_root_field_class(ctx, "Payload field class", fc);
	}

	decr_indent(ctx);
}

static
void write_clock_class_prop_lines(struct details_write_ctx *ctx,
		const bt_clock_class *cc)
{
	int64_t offset_seconds;
	uint64_t offset_cycles;
	const char *str;

	str = bt_clock_class_get_name(cc);
	if (str) {
		write_str_prop_line(ctx, "Name", str);
	}

	write_user_attributes(ctx,
		bt_clock_class_borrow_user_attributes_const(cc), true, NULL);
	str = bt_clock_class_get_description(cc);
	if (str) {
		write_str_prop_line(ctx, "Description", str);
	}

	write_uint_prop_line(ctx, "Frequency (Hz)",
		bt_clock_class_get_frequency(cc));
	write_uint_prop_line(ctx, "Precision (cycles)",
		bt_clock_class_get_precision(cc));
	bt_clock_class_get_offset(cc, &offset_seconds, &offset_cycles);
	write_int_prop_line(ctx, "Offset (s)", offset_seconds);
	write_uint_prop_line(ctx, "Offset (cycles)", offset_cycles);
	write_bool_prop_line(ctx, "Origin is Unix epoch",
		bt_clock_class_origin_is_unix_epoch(cc));

	if (ctx->details_comp->cfg.with_uuid) {
		bt_uuid uuid = bt_clock_class_get_uuid(cc);

		if (uuid) {
			write_uuid_prop_line(ctx, "UUID", uuid);
		}
	}
}

static
gint compare_event_classes(const bt_event_class **a, const bt_event_class **b)
{
	uint64_t id_a = bt_event_class_get_id(*a);
	uint64_t id_b = bt_event_class_get_id(*b);

	if (id_a < id_b) {
		return -1;
	} else if (id_a > id_b) {
		return 1;
	} else {
		return 0;
	}
}

static
void write_stream_class(struct details_write_ctx *ctx,
		const bt_stream_class *sc)
{
	const bt_field_class *fc;
	GPtrArray *event_classes = g_ptr_array_new();
	uint64_t i;

	write_indent(ctx);
	write_obj_type_name(ctx, "Stream class");

	/* Write name and ID */
	if (ctx->details_comp->cfg.with_stream_class_name) {
		const char *name = bt_stream_class_get_name(sc);

		if (name) {
			g_string_append(ctx->str, " `");
			write_str_prop_value(ctx, name);
			g_string_append(ctx->str, "`");
		}
	}

	g_string_append(ctx->str, " (ID ");
	write_uint_prop_value(ctx, bt_stream_class_get_id(sc));
	g_string_append(ctx->str, "):\n");

	/* Write properties */
	incr_indent(ctx);

	/* Write user attributes */
	write_user_attributes(ctx,
		bt_stream_class_borrow_user_attributes_const(sc), true, NULL);

	/* Write configuration */
	write_bool_prop_line(ctx,
		"Supports packets", bt_stream_class_supports_packets(sc));

	if (bt_stream_class_supports_packets(sc)) {
		write_bool_prop_line(ctx,
			"Packets have beginning default clock snapshot",
			bt_stream_class_packets_have_beginning_default_clock_snapshot(sc));
		write_bool_prop_line(ctx,
			"Packets have end default clock snapshot",
			bt_stream_class_packets_have_end_default_clock_snapshot(sc));
	}

	write_bool_prop_line(ctx,
		"Supports discarded events",
		bt_stream_class_supports_discarded_events(sc));

	if (bt_stream_class_supports_discarded_events(sc)) {
		write_bool_prop_line(ctx,
			"Discarded events have default clock snapshots",
			bt_stream_class_discarded_events_have_default_clock_snapshots(sc));
	}

	write_bool_prop_line(ctx,
		"Supports discarded packets",
		bt_stream_class_supports_discarded_packets(sc));

	if (bt_stream_class_supports_discarded_packets(sc)) {
		write_bool_prop_line(ctx,
			"Discarded packets have default clock snapshots",
			bt_stream_class_discarded_packets_have_default_clock_snapshots(sc));
	}

	/* Write default clock class */
	if (bt_stream_class_borrow_default_clock_class_const(sc)) {
		write_indent(ctx);
		write_prop_name(ctx, "Default clock class");
		g_string_append_c(ctx->str, ':');
		write_nl(ctx);
		incr_indent(ctx);
		write_clock_class_prop_lines(ctx,
			bt_stream_class_borrow_default_clock_class_const(sc));
		decr_indent(ctx);
	}

	fc = bt_stream_class_borrow_packet_context_field_class_const(sc);
	if (fc) {
		write_root_field_class(ctx, "Packet context field class", fc);
	}

	fc = bt_stream_class_borrow_event_common_context_field_class_const(sc);
	if (fc) {
		write_root_field_class(ctx, "Event common context field class",
			fc);
	}

	for (i = 0; i < bt_stream_class_get_event_class_count(sc); i++) {
		g_ptr_array_add(event_classes,
			(gpointer) bt_stream_class_borrow_event_class_by_index_const(
				sc, i));
	}

	g_ptr_array_sort(event_classes, (GCompareFunc) compare_event_classes);

	for (i = 0; i < event_classes->len; i++) {
		write_event_class(ctx, event_classes->pdata[i]);
	}

	decr_indent(ctx);
	g_ptr_array_free(event_classes, TRUE);
}

static
gint compare_stream_classes(const bt_stream_class **a, const bt_stream_class **b)
{
	uint64_t id_a = bt_stream_class_get_id(*a);
	uint64_t id_b = bt_stream_class_get_id(*b);

	if (id_a < id_b) {
		return -1;
	} else if (id_a > id_b) {
		return 1;
	} else {
		return 0;
	}
}

static
void write_trace_class(struct details_write_ctx *ctx, const bt_trace_class *tc)
{
	GPtrArray *stream_classes = g_ptr_array_new();
	uint64_t i;
	bool printed_prop = false;

	write_indent(ctx);
	write_obj_type_name(ctx, "Trace class");


	for (i = 0; i < bt_trace_class_get_stream_class_count(tc); i++) {
		g_ptr_array_add(stream_classes,
			(gpointer) bt_trace_class_borrow_stream_class_by_index_const(
				tc, i));
	}

	g_ptr_array_sort(stream_classes, (GCompareFunc) compare_stream_classes);

	if (stream_classes->len > 0) {
		if (!printed_prop) {
			g_string_append(ctx->str, ":\n");
			printed_prop = true;
		}
	}

	incr_indent(ctx);

	/* Write user attributes */
	write_user_attributes(ctx,
		bt_trace_class_borrow_user_attributes_const(tc), true,
		&printed_prop);

	/* Write stream classes */
	for (i = 0; i < stream_classes->len; i++) {
		write_stream_class(ctx, stream_classes->pdata[i]);
	}

	if (!printed_prop) {
		write_nl(ctx);
	}

	decr_indent(ctx);
	g_ptr_array_free(stream_classes, TRUE);
}

static
int try_write_meta(struct details_write_ctx *ctx, const bt_trace_class *tc,
		const bt_stream_class *sc, const bt_event_class *ec)
{
	int ret = 0;

	BT_ASSERT_DBG(tc);

	if (details_need_to_write_trace_class(ctx, tc)) {
		uint64_t sc_i;

		if (ctx->details_comp->cfg.compact &&
				ctx->details_comp->printed_something) {
			/*
			 * There are no empty line between messages in
			 * compact mode, so write one here to decouple
			 * the trace class from the next message.
			 */
			write_nl(ctx);
		}

		/*
		 * write_trace_class() also writes all its stream
		 * classes their event classes, so we don't need to
		 * rewrite `sc`.
		 */
		write_trace_class(ctx, tc);

		/*
		 * Mark this trace class as written, as well as all
		 * its stream classes and their event classes.
		 */
		ret = details_did_write_trace_class(ctx, tc);
		if (ret) {
			goto end;
		}

		for (sc_i = 0; sc_i < bt_trace_class_get_stream_class_count(tc);
				sc_i++) {
			uint64_t ec_i;
			const bt_stream_class *tc_sc =
				bt_trace_class_borrow_stream_class_by_index_const(
					tc, sc_i);

			details_did_write_meta_object(ctx, tc, tc_sc);

			for (ec_i = 0; ec_i <
					bt_stream_class_get_event_class_count(tc_sc);
					ec_i++) {
				details_did_write_meta_object(ctx, tc,
					bt_stream_class_borrow_event_class_by_index_const(
						tc_sc, ec_i));
			}
		}

		goto end;
	}

	if (sc && details_need_to_write_meta_object(ctx, tc, sc)) {
		uint64_t ec_i;

		BT_ASSERT_DBG(tc);

		if (ctx->details_comp->cfg.compact &&
				ctx->details_comp->printed_something) {
			/*
			 * There are no empty line between messages in
			 * compact mode, so write one here to decouple
			 * the stream class from the next message.
			 */
			write_nl(ctx);
		}

		/*
		 * write_stream_class() also writes all its event
		 * classes, so we don't need to rewrite `ec`.
		 */
		write_stream_class(ctx, sc);

		/*
		 * Mark this stream class as written, as well as all its
		 * event classes.
		 */
		details_did_write_meta_object(ctx, tc, sc);

		for (ec_i = 0; ec_i <
				bt_stream_class_get_event_class_count(sc);
				ec_i++) {
			details_did_write_meta_object(ctx, tc,
				bt_stream_class_borrow_event_class_by_index_const(
					sc, ec_i));
		}

		goto end;
	}

	if (ec && details_need_to_write_meta_object(ctx, tc, ec)) {
		BT_ASSERT_DBG(sc);

		if (ctx->details_comp->cfg.compact &&
				ctx->details_comp->printed_something) {
			/*
			 * There are no empty line between messages in
			 * compact mode, so write one here to decouple
			 * the event class from the next message.
			 */
			write_nl(ctx);
		}

		write_event_class(ctx, ec);
		details_did_write_meta_object(ctx, tc, ec);
		goto end;
	}

end:
	return ret;
}

static
void write_time_str(struct details_write_ctx *ctx, const char *str)
{
	if (!ctx->details_comp->cfg.with_time) {
		goto end;
	}

	g_string_append_printf(ctx->str, "[%s%s%s%s]",
		color_bold(ctx), color_fg_bright_blue(ctx), str,
		color_reset(ctx));

	if (ctx->details_comp->cfg.compact) {
		write_sp(ctx);
	} else {
		write_nl(ctx);
	}

end:
	return;
}

static
void write_time(struct details_write_ctx *ctx, const bt_clock_snapshot *cs)
{
	bt_clock_snapshot_get_ns_from_origin_status cs_status;
	int64_t ns_from_origin;
	char buf[32];

	if (!ctx->details_comp->cfg.with_time) {
		goto end;
	}

	format_uint(buf, bt_clock_snapshot_get_value(cs), 10);
	g_string_append_printf(ctx->str, "[%s%s%s%s%s",
		color_bold(ctx), color_fg_bright_blue(ctx), buf,
		color_reset(ctx),
		ctx->details_comp->cfg.compact ? "" : " cycles");
	cs_status = bt_clock_snapshot_get_ns_from_origin(cs, &ns_from_origin);
	if (cs_status == BT_CLOCK_SNAPSHOT_GET_NS_FROM_ORIGIN_STATUS_OK) {
		format_int(buf, ns_from_origin, 10);
		g_string_append_printf(ctx->str, "%s %s%s%s%s%s",
			ctx->details_comp->cfg.compact ? "" : ",",
			color_bold(ctx), color_fg_bright_blue(ctx), buf,
			color_reset(ctx),
			ctx->details_comp->cfg.compact ? "" : " ns from origin");
	}

	g_string_append(ctx->str, "]");

	if (ctx->details_comp->cfg.compact) {
		write_sp(ctx);
	} else {
		write_nl(ctx);
	}

end:
	return;
}

static
int write_message_follow_tag(struct details_write_ctx *ctx,
		const bt_stream *stream)
{
	int ret;
	uint64_t unique_trace_id;
	const bt_stream_class *sc = bt_stream_borrow_class_const(stream);
	const bt_trace *trace = bt_stream_borrow_trace_const(stream);

	ret = details_trace_unique_id(ctx, trace, &unique_trace_id);
	if (ret) {
		goto end;
	}

	if (ctx->details_comp->cfg.compact) {
		g_string_append_printf(ctx->str,
			"%s{%s%s%" PRIu64 " %" PRIu64 " %" PRIu64 "%s%s}%s ",
			color_fg_cyan(ctx), color_bold(ctx),
			color_fg_bright_cyan(ctx),
			unique_trace_id, bt_stream_class_get_id(sc),
			bt_stream_get_id(stream),
			color_reset(ctx), color_fg_cyan(ctx), color_reset(ctx));
	} else {
		g_string_append_printf(ctx->str,
			"%s{Trace %s%s%" PRIu64 "%s%s, Stream class ID %s%s%" PRIu64 "%s%s, Stream ID %s%s%" PRIu64 "%s%s}%s\n",
			color_fg_cyan(ctx),
			color_bold(ctx), color_fg_bright_cyan(ctx),
			unique_trace_id, color_reset(ctx),
			color_fg_cyan(ctx),
			color_bold(ctx), color_fg_bright_cyan(ctx),
			bt_stream_class_get_id(sc), color_reset(ctx),
			color_fg_cyan(ctx),
			color_bold(ctx), color_fg_bright_cyan(ctx),
			bt_stream_get_id(stream), color_reset(ctx),
			color_fg_cyan(ctx), color_reset(ctx));
	}

end:
	return ret;
}

static
void write_field(struct details_write_ctx *ctx, const bt_field *field,
		const char *name)
{
	uint64_t i;
	bt_field_class_type fc_type = bt_field_get_class_type(field);
	const bt_field_class *fc;
	char buf[64];

	/* Write field's name */
	if (name) {
		write_compound_member_name(ctx, name);
	}

	/* Write field's value */
	if (fc_type == BT_FIELD_CLASS_TYPE_BOOL) {
		write_sp(ctx);
		write_bool_prop_value(ctx, bt_field_bool_get_value(field));
	} else if (fc_type == BT_FIELD_CLASS_TYPE_BIT_ARRAY) {
		format_uint(buf, bt_field_bit_array_get_value_as_integer(field),
			16);
		write_sp(ctx);
		write_uint_str_prop_value(ctx, buf);
	} else if (bt_field_class_type_is(fc_type,
			BT_FIELD_CLASS_TYPE_INTEGER)) {
		unsigned int fmt_base;
		bt_field_class_integer_preferred_display_base base;

		fc = bt_field_borrow_class_const(field);
		base = bt_field_class_integer_get_preferred_display_base(fc);

		switch (base) {
		case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL:
			fmt_base = 10;
			break;
		case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL:
			fmt_base = 8;
			break;
		case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_BINARY:
			fmt_base = 2;
			break;
		case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL:
			fmt_base = 16;
			break;
		default:
			bt_common_abort();
		}

		if (bt_field_class_type_is(fc_type,
				BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER)) {
			format_uint(buf,
				bt_field_integer_unsigned_get_value(field),
				fmt_base);
			write_sp(ctx);
			write_uint_str_prop_value(ctx, buf);
		} else {
			format_int(buf,
				bt_field_integer_signed_get_value(field),
				fmt_base);
			write_sp(ctx);
			write_int_str_prop_value(ctx, buf);
		}
	} else if (fc_type == BT_FIELD_CLASS_TYPE_SINGLE_PRECISION_REAL) {
		write_sp(ctx);
		write_float_prop_value(ctx, bt_field_real_single_precision_get_value(field));
	} else if (fc_type == BT_FIELD_CLASS_TYPE_DOUBLE_PRECISION_REAL) {
		write_sp(ctx);
		write_float_prop_value(ctx, bt_field_real_double_precision_get_value(field));
	} else if (fc_type == BT_FIELD_CLASS_TYPE_STRING) {
		write_sp(ctx);
		write_str_prop_value(ctx, bt_field_string_get_value(field));
	} else if (fc_type == BT_FIELD_CLASS_TYPE_STRUCTURE) {
		uint64_t member_count;

		fc = bt_field_borrow_class_const(field);
		member_count = bt_field_class_structure_get_member_count(fc);

		if (member_count > 0) {
			incr_indent(ctx);

			for (i = 0; i < member_count; i++) {
				const bt_field_class_structure_member *member =
					bt_field_class_structure_borrow_member_by_index_const(
						fc, i);
				const bt_field *member_field =
					bt_field_structure_borrow_member_field_by_index_const(
						field, i);

				write_nl(ctx);
				write_field(ctx, member_field,
					bt_field_class_structure_member_get_name(member));
			}

			decr_indent(ctx);
		} else {
			write_sp(ctx);
			write_none_prop_value(ctx, "Empty");
		}
	} else if (bt_field_class_type_is(fc_type, BT_FIELD_CLASS_TYPE_ARRAY)) {
		uint64_t length = bt_field_array_get_length(field);

		if (length == 0) {
			write_sp(ctx);
			write_none_prop_value(ctx, "Empty");
		} else {
			g_string_append(ctx->str, " Length ");
			write_uint_prop_value(ctx, length);
			g_string_append_c(ctx->str, ':');
		}

		incr_indent(ctx);

		for (i = 0; i < length; i++) {
			const bt_field *elem_field =
				bt_field_array_borrow_element_field_by_index_const(
					field, i);

			write_nl(ctx);
			write_array_index(ctx, i, color_fg_cyan(ctx));
			write_field(ctx, elem_field, NULL);
		}

		decr_indent(ctx);
	} else if (bt_field_class_type_is(fc_type,
			BT_FIELD_CLASS_TYPE_OPTION)) {
		const bt_field *content_field =
			bt_field_option_borrow_field_const(field);

		if (!content_field) {
			write_sp(ctx);
			write_none_prop_value(ctx, "None");
		} else {
			write_field(ctx, content_field, NULL);
		}
	} else if (bt_field_class_type_is(fc_type,
			BT_FIELD_CLASS_TYPE_VARIANT)) {
		write_field(ctx,
			bt_field_variant_borrow_selected_option_field_const(
				field), NULL);
	} else {
		bt_common_abort();
	}
}

static
void write_root_field(struct details_write_ctx *ctx, const char *name,
		const bt_field *field)
{
	BT_ASSERT_DBG(name);
	BT_ASSERT_DBG(field);
	write_indent(ctx);
	write_prop_name(ctx, name);
	g_string_append(ctx->str, ":");
	write_field(ctx, field, NULL);
	write_nl(ctx);
}

static
int write_event_message(struct details_write_ctx *ctx,
		const bt_message *msg)
{
	int ret = 0;
	const bt_event *event = bt_message_event_borrow_event_const(msg);
	const bt_stream *stream = bt_event_borrow_stream_const(event);
	const bt_event_class *ec = bt_event_borrow_class_const(event);
	const bt_stream_class *sc = bt_event_class_borrow_stream_class_const(ec);
	const bt_trace_class *tc = bt_stream_class_borrow_trace_class_const(sc);
	const char *ec_name;
	const bt_field *field;

	ret = try_write_meta(ctx, tc, sc, ec);
	if (ret) {
		goto end;
	}

	if (!ctx->details_comp->cfg.with_data) {
		goto end;
	}

	if (ctx->str->len > 0) {
		/*
		 * Output buffer contains metadata: separate blocks with
		 * newline.
		 */
		write_nl(ctx);
	}

	/* Write time */
	if (bt_stream_class_borrow_default_clock_class_const(sc)) {
		write_time(ctx,
			bt_message_event_borrow_default_clock_snapshot_const(
				msg));
	}

	/* Write follow tag for message */
	ret = write_message_follow_tag(ctx, stream);
	if (ret) {
		goto end;
	}

	/* Write object's basic properties */
	write_obj_type_name(ctx, "Event");
	ec_name = bt_event_class_get_name(ec);
	if (ec_name) {
		g_string_append_printf(ctx->str, " `%s%s%s`",
			color_fg_green(ctx), ec_name, color_reset(ctx));
	}

	g_string_append(ctx->str, " (");

	if (!ctx->details_comp->cfg.compact) {
		g_string_append(ctx->str, "Class ID ");
	}

	write_uint_prop_value(ctx, bt_event_class_get_id(ec));
	g_string_append(ctx->str, ")");

	if (ctx->details_comp->cfg.compact) {
		write_nl(ctx);
		goto end;
	}

	/* Write fields */
	g_string_append(ctx->str, ":\n");
	incr_indent(ctx);
	field = bt_event_borrow_common_context_field_const(event);
	if (field) {
		write_root_field(ctx, "Common context", field);
	}

	field = bt_event_borrow_specific_context_field_const(event);
	if (field) {
		write_root_field(ctx, "Specific context", field);
	}

	field = bt_event_borrow_payload_field_const(event);
	if (field) {
		write_root_field(ctx, "Payload", field);
	}

	decr_indent(ctx);

end:
	return ret;
}

static
gint compare_streams(const bt_stream **a, const bt_stream **b)
{
	uint64_t id_a = bt_stream_get_id(*a);
	uint64_t id_b = bt_stream_get_id(*b);

	if (id_a < id_b) {
		return -1;
	} else if (id_a > id_b) {
		return 1;
	} else {
		const bt_stream_class *a_sc = bt_stream_borrow_class_const(*a);
		const bt_stream_class *b_sc = bt_stream_borrow_class_const(*b);
		uint64_t a_sc_id = bt_stream_class_get_id(a_sc);
		uint64_t b_sc_id = bt_stream_class_get_id(b_sc);

		if (a_sc_id < b_sc_id) {
			return -1;
		} else if (a_sc_id > b_sc_id) {
			return 1;
		} else {
			return 0;
		}
	}
}

static
void write_trace(struct details_write_ctx *ctx, const bt_trace *trace)
{
	GPtrArray *streams = g_ptr_array_new();
	uint64_t i;
	bool printed_prop = false;
	GPtrArray *env_names = g_ptr_array_new();
	uint64_t env_count;

	write_indent(ctx);
	write_obj_type_name(ctx, "Trace");

	/* Write name */
	if (ctx->details_comp->cfg.with_trace_name) {
		const char *name = bt_trace_get_name(trace);
		if (name) {
			g_string_append(ctx->str, " `");
			write_str_prop_value(ctx, name);
			g_string_append(ctx->str, "`");
		}
	}

	/* Write properties */
	incr_indent(ctx);

	/* Write UUID */
	if (ctx->details_comp->cfg.with_uuid) {
		bt_uuid uuid = bt_trace_get_uuid(trace);

		if (uuid) {
			if (!printed_prop) {
				g_string_append(ctx->str, ":\n");
				printed_prop = true;
			}

			write_uuid_prop_line(ctx, "UUID", uuid);
		}
	}

	/* Write environment */
	env_count = bt_trace_get_environment_entry_count(trace);
	if (env_count > 0) {
		if (!printed_prop) {
			g_string_append(ctx->str, ":\n");
			printed_prop = true;
		}

		write_indent(ctx);
		write_prop_name(ctx, "Environment");
		g_string_append(ctx->str, " (");
		write_uint_prop_value(ctx, env_count);
		g_string_append_printf(ctx->str, " entr%s):",
			env_count == 1 ? "y" : "ies");
		write_nl(ctx);
		incr_indent(ctx);

		for (i = 0; i < env_count; i++) {
			const char *name;
			const bt_value *value;

			bt_trace_borrow_environment_entry_by_index_const(
				trace, i, &name, &value);
			g_ptr_array_add(env_names, (gpointer) name);
		}

		g_ptr_array_sort(env_names, (GCompareFunc) compare_strings);

		for (i = 0; i < env_names->len; i++) {
			const char *name = env_names->pdata[i];
			const bt_value *value =
				bt_trace_borrow_environment_entry_value_by_name_const(
					trace, name);

			BT_ASSERT_DBG(value);
			write_compound_member_name(ctx, name);
			write_sp(ctx);

			if (bt_value_get_type(value) ==
					BT_VALUE_TYPE_SIGNED_INTEGER) {
				write_int_prop_value(ctx,
					bt_value_integer_signed_get(value));
			} else if (bt_value_get_type(value) ==
					BT_VALUE_TYPE_STRING) {
				write_str_prop_value(ctx,
					bt_value_string_get(value));
			} else {
				bt_common_abort();
			}

			write_nl(ctx);
		}

		decr_indent(ctx);
	}

	for (i = 0; i < bt_trace_get_stream_count(trace); i++) {
		g_ptr_array_add(streams,
			(gpointer) bt_trace_borrow_stream_by_index_const(
				trace, i));
	}

	g_ptr_array_sort(streams, (GCompareFunc) compare_streams);

	if (streams->len > 0 && !printed_prop) {
		g_string_append(ctx->str, ":\n");
		printed_prop = true;
	}

	for (i = 0; i < streams->len; i++) {
		const bt_stream *stream = streams->pdata[i];

		write_indent(ctx);
		write_obj_type_name(ctx, "Stream");
		g_string_append(ctx->str, " (ID ");
		write_uint_prop_value(ctx, bt_stream_get_id(stream));
		g_string_append(ctx->str, ", Class ID ");
		write_uint_prop_value(ctx, bt_stream_class_get_id(
			bt_stream_borrow_class_const(stream)));
		g_string_append(ctx->str, ")");
		write_nl(ctx);
	}

	decr_indent(ctx);

	if (!printed_prop) {
		write_nl(ctx);
	}

	g_ptr_array_free(streams, TRUE);
	g_ptr_array_free(env_names, TRUE);
}

static
int write_stream_beginning_message(struct details_write_ctx *ctx,
		const bt_message *msg)
{
	int ret = 0;
	const bt_stream *stream =
		bt_message_stream_beginning_borrow_stream_const(msg);
	const bt_trace *trace = bt_stream_borrow_trace_const(stream);
	const bt_stream_class *sc = bt_stream_borrow_class_const(stream);
	const bt_clock_class *cc = bt_stream_class_borrow_default_clock_class_const(sc);
	const bt_trace_class *tc = bt_stream_class_borrow_trace_class_const(sc);
	const char *name;

	ret = try_write_meta(ctx, tc, sc, NULL);
	if (ret) {
		goto end;
	}

	if (!ctx->details_comp->cfg.with_data) {
		goto end;
	}

	if (ctx->str->len > 0) {
		/*
		 * Output buffer contains metadata: separate blocks with
		 * newline.
		 */
		write_nl(ctx);
	}

	/* Write time */
	if (cc) {
		const bt_clock_snapshot *cs;
		bt_message_stream_clock_snapshot_state cs_state =
			bt_message_stream_beginning_borrow_default_clock_snapshot_const(msg, &cs);

		if (cs_state == BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN) {
			write_time(ctx, cs);
		} else {
			write_time_str(ctx, "Unknown");
		}
	}

	/* Write follow tag for message */
	ret = write_message_follow_tag(ctx, stream);
	if (ret) {
		goto end;
	}

	/* Write stream properties */
	write_obj_type_name(ctx, "Stream beginning");

	if (ctx->details_comp->cfg.compact) {
		write_nl(ctx);
		goto end;
	}

	g_string_append(ctx->str, ":\n");
	incr_indent(ctx);

	if (ctx->details_comp->cfg.with_stream_name) {
		name = bt_stream_get_name(stream);
		if (name) {
			write_str_prop_line(ctx, "Name", name);
		}
	}

	if (ctx->details_comp->cfg.with_stream_class_name) {
		name = bt_stream_class_get_name(sc);
		if (name) {
			write_str_prop_line(ctx, "Class name", name);
		}
	}

	write_trace(ctx, trace);
	decr_indent(ctx);

end:
	return ret;
}

static
int write_stream_end_message(struct details_write_ctx *ctx,
		const bt_message *msg)
{
	int ret = 0;
	const bt_stream *stream =
		bt_message_stream_end_borrow_stream_const(msg);
	const bt_stream_class *sc =
		bt_stream_borrow_class_const(stream);
	const bt_clock_class *cc =
		bt_stream_class_borrow_default_clock_class_const(sc);

	if (!ctx->details_comp->cfg.with_data) {
		goto end;
	}

	/* Write time */
	if (cc) {
		const bt_clock_snapshot *cs;
		bt_message_stream_clock_snapshot_state cs_state =
			bt_message_stream_end_borrow_default_clock_snapshot_const(msg, &cs);

		if (cs_state == BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN) {
			write_time(ctx, cs);
		} else {
			write_time_str(ctx, "Unknown");
		}
	}

	/* Write follow tag for message */
	ret = write_message_follow_tag(ctx, stream);
	if (ret) {
		goto end;
	}

	/* Write stream properties */
	write_obj_type_name(ctx, "Stream end\n");

end:
	return ret;
}

static
int write_packet_beginning_message(struct details_write_ctx *ctx,
		const bt_message *msg)
{
	int ret = 0;
	const bt_packet *packet =
		bt_message_packet_beginning_borrow_packet_const(msg);
	const bt_stream *stream = bt_packet_borrow_stream_const(packet);
	const bt_stream_class *sc = bt_stream_borrow_class_const(stream);
	const bt_field *field;

	if (!ctx->details_comp->cfg.with_data) {
		goto end;
	}

	/* Write time */
	if (bt_stream_class_packets_have_beginning_default_clock_snapshot(sc)) {
		write_time(ctx,
			bt_message_packet_beginning_borrow_default_clock_snapshot_const(
				msg));
	}

	/* Write follow tag for message */
	ret = write_message_follow_tag(ctx, stream);
	if (ret) {
		goto end;
	}

	write_obj_type_name(ctx, "Packet beginning");

	if (ctx->details_comp->cfg.compact) {
		write_nl(ctx);
		goto end;
	}

	/* Write field */
	field = bt_packet_borrow_context_field_const(packet);
	if (field) {
		g_string_append(ctx->str, ":\n");
		incr_indent(ctx);
		write_root_field(ctx, "Context", field);
		decr_indent(ctx);
	} else {
		write_nl(ctx);
	}

end:
	return ret;
}

static
int write_discarded_items_message(struct details_write_ctx *ctx,
		const char *name, const bt_stream *stream,
		const bt_clock_snapshot *beginning_cs,
		const bt_clock_snapshot *end_cs, uint64_t count)
{
	int ret = 0;

	/* Write times */
	if (beginning_cs) {
		write_time(ctx, beginning_cs);
		BT_ASSERT_DBG(end_cs);
		write_time(ctx, end_cs);
	}

	/* Write follow tag for message */
	ret = write_message_follow_tag(ctx, stream);
	if (ret) {
		goto end;
	}

	write_obj_type_name(ctx, "Discarded ");
	write_obj_type_name(ctx, name);

	/* Write count */
	if (count == UINT64_C(-1)) {
		write_nl(ctx);
		goto end;
	}

	g_string_append(ctx->str, " (");
	write_uint_prop_value(ctx, count);
	g_string_append_printf(ctx->str, " %s)\n", name);

end:
	return ret;
}

static
int write_discarded_events_message(struct details_write_ctx *ctx,
		const bt_message *msg)
{
	int ret = 0;
	const bt_stream *stream = bt_message_discarded_events_borrow_stream_const(
		msg);
	const bt_stream_class *sc = bt_stream_borrow_class_const(stream);
	const bt_clock_snapshot *beginning_cs = NULL;
	const bt_clock_snapshot *end_cs = NULL;
	uint64_t count;

	if (!ctx->details_comp->cfg.with_data) {
		goto end;
	}

	if (bt_stream_class_discarded_events_have_default_clock_snapshots(sc)) {
		beginning_cs =
			bt_message_discarded_events_borrow_beginning_default_clock_snapshot_const(
				msg);
		end_cs =
			bt_message_discarded_events_borrow_end_default_clock_snapshot_const(
				msg);
	}

	if (bt_message_discarded_events_get_count(msg, &count) !=
			BT_PROPERTY_AVAILABILITY_AVAILABLE) {
		count = UINT64_C(-1);
	}

	ret = write_discarded_items_message(ctx, "events", stream,
		beginning_cs, end_cs, count);

end:
	return ret;
}

static
int write_discarded_packets_message(struct details_write_ctx *ctx,
		const bt_message *msg)
{
	int ret = 0;
	const bt_stream *stream = bt_message_discarded_packets_borrow_stream_const(
		msg);
	const bt_stream_class *sc = bt_stream_borrow_class_const(stream);
	const bt_clock_snapshot *beginning_cs = NULL;
	const bt_clock_snapshot *end_cs = NULL;
	uint64_t count;

	if (!ctx->details_comp->cfg.with_data) {
		goto end;
	}

	if (bt_stream_class_discarded_packets_have_default_clock_snapshots(sc)) {
		beginning_cs =
			bt_message_discarded_packets_borrow_beginning_default_clock_snapshot_const(
				msg);
		end_cs =
			bt_message_discarded_packets_borrow_end_default_clock_snapshot_const(
				msg);
	}

	if (bt_message_discarded_packets_get_count(msg, &count) !=
			BT_PROPERTY_AVAILABILITY_AVAILABLE) {
		count = UINT64_C(-1);
	}

	ret = write_discarded_items_message(ctx, "packets", stream,
		beginning_cs, end_cs, count);

end:
	return ret;
}

static
int write_packet_end_message(struct details_write_ctx *ctx,
		const bt_message *msg)
{
	int ret = 0;
	const bt_packet *packet =
		bt_message_packet_end_borrow_packet_const(msg);
	const bt_stream *stream = bt_packet_borrow_stream_const(packet);
	const bt_stream_class *sc = bt_stream_borrow_class_const(stream);

	if (!ctx->details_comp->cfg.with_data) {
		goto end;
	}

	/* Write time */
	if (bt_stream_class_packets_have_end_default_clock_snapshot(sc)) {
		write_time(ctx,
			bt_message_packet_end_borrow_default_clock_snapshot_const(
				msg));
	}

	/* Write follow tag for message */
	ret = write_message_follow_tag(ctx, stream);
	if (ret) {
		goto end;
	}

	write_obj_type_name(ctx, "Packet end");
	write_nl(ctx);

end:
	return ret;
}

static
int write_message_iterator_inactivity_message(struct details_write_ctx *ctx,
		const bt_message *msg)
{
	int ret = 0;
	const bt_clock_snapshot *cs =
		bt_message_message_iterator_inactivity_borrow_clock_snapshot_const(
			msg);

	/* Write time */
	write_time(ctx, cs);
	write_obj_type_name(ctx, "Message iterator inactivity");

	if (ctx->details_comp->cfg.compact) {
		write_nl(ctx);
		goto end;
	}

	/* Write clock class properties */
	g_string_append(ctx->str, ":\n");
	incr_indent(ctx);
	write_indent(ctx);
	write_prop_name(ctx, "Clock class");
	g_string_append_c(ctx->str, ':');
	write_nl(ctx);
	incr_indent(ctx);
	write_clock_class_prop_lines(ctx,
		bt_clock_snapshot_borrow_clock_class_const(cs));
	decr_indent(ctx);

end:
	return ret;
}

BT_HIDDEN
int details_write_message(struct details_comp *details_comp,
		const bt_message *msg)
{
	int ret = 0;
	struct details_write_ctx ctx = {
		.details_comp = details_comp,
		.str = details_comp->str,
		.indent_level = 0,
	};

	/* Reset output buffer */
	g_string_assign(details_comp->str, "");

	switch (bt_message_get_type(msg)) {
	case BT_MESSAGE_TYPE_EVENT:
		ret = write_event_message(&ctx, msg);
		break;
	case BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY:
		ret = write_message_iterator_inactivity_message(&ctx, msg);
		break;
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
		ret = write_stream_beginning_message(&ctx, msg);
		break;
	case BT_MESSAGE_TYPE_STREAM_END:
		ret = write_stream_end_message(&ctx, msg);
		break;
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		ret = write_packet_beginning_message(&ctx, msg);
		break;
	case BT_MESSAGE_TYPE_PACKET_END:
		ret = write_packet_end_message(&ctx, msg);
		break;
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
		ret = write_discarded_events_message(&ctx, msg);
		break;
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
		ret = write_discarded_packets_message(&ctx, msg);
		break;
	default:
		bt_common_abort();
	}

	/*
	 * If this component printed at least one character so far, and
	 * we're not in compact mode, and there's something in the
	 * output buffer for this message, then prepend a newline to the
	 * output buffer to visually separate message blocks.
	 */
	if (details_comp->printed_something && !details_comp->cfg.compact &&
			details_comp->str->len > 0) {
		/* TODO: Optimize this */
		g_string_prepend_c(details_comp->str, '\n');
	}

	return ret;
}
