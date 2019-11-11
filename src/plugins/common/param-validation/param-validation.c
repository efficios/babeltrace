/*
 * Copyright 2019 EfficiOS Inc.
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

#include "param-validation.h"

#include <babeltrace2/babeltrace.h>
#include <glib.h>
#include <inttypes.h>
#include <stdbool.h>

#include "common/common.h"

struct bt_param_validation_context {
	gchar *error;
	GArray *scope_stack;
};

struct validate_ctx_stack_element {
	enum {
		VALIDATE_CTX_STACK_ELEMENT_MAP,
		VALIDATE_CTX_STACK_ELEMENT_ARRAY,
	} type;

	union {
		const char *map_key_name;
		uint64_t array_index;
	};
};

static
void validate_ctx_push_map_scope(
		struct bt_param_validation_context *ctx,
		const char *key)
{
	struct validate_ctx_stack_element stack_element = {
		.type = VALIDATE_CTX_STACK_ELEMENT_MAP,
		.map_key_name = key,
	};

	g_array_append_val(ctx->scope_stack, stack_element);
}

static
void validate_ctx_push_array_scope(
		struct bt_param_validation_context *ctx, uint64_t index)
{
	struct validate_ctx_stack_element stack_element = {
		.type = VALIDATE_CTX_STACK_ELEMENT_ARRAY,
		.array_index = index,
	};

	g_array_append_val(ctx->scope_stack, stack_element);
}

static
void validate_ctx_pop_scope(struct bt_param_validation_context *ctx)
{
	BT_ASSERT(ctx->scope_stack->len > 0);

	g_array_remove_index_fast(ctx->scope_stack, ctx->scope_stack->len - 1);
}

static
void append_scope_to_string(GString *str,
		const struct validate_ctx_stack_element *elem,
		bool first)
{
	switch (elem->type) {
	case VALIDATE_CTX_STACK_ELEMENT_MAP:
		if (!first) {
			g_string_append_c(str, '.');
		}

		g_string_append(str, elem->map_key_name);
		break;
	case VALIDATE_CTX_STACK_ELEMENT_ARRAY:
		g_string_append_printf(str, "[%" PRIu64 "]", elem->array_index);
		break;
	default:
		bt_common_abort();
	}
}

enum bt_param_validation_status bt_param_validation_error(
		struct bt_param_validation_context *ctx,
		const char *format, ...) {
	va_list ap;
	enum bt_param_validation_status status;

	GString *str = g_string_new(NULL);
	if (!str) {
		status = BT_PARAM_VALIDATION_STATUS_MEMORY_ERROR;
		goto end;
	}

	if (ctx->scope_stack->len > 0) {
		guint i;

		g_string_assign(str, "Error validating parameter `");

		append_scope_to_string(str, &g_array_index(ctx->scope_stack,
			struct validate_ctx_stack_element, 0), true);

		for (i = 1; i < ctx->scope_stack->len; i++) {
			append_scope_to_string(str,
				&g_array_index(ctx->scope_stack,
					struct validate_ctx_stack_element, i), false);
		}

		g_string_append(str, "`: ");
	} else {
		g_string_assign(str, "Error validating parameters: ");
	}

	va_start(ap, format);
	g_string_append_vprintf(str, format, ap);
	va_end(ap);

	ctx->error = g_string_free(str, FALSE);
	status = BT_PARAM_VALIDATION_STATUS_VALIDATION_ERROR;

end:
	return status;
}

struct validate_map_value_data
{
	GPtrArray *available_keys;
	enum bt_param_validation_status status;
	struct bt_param_validation_context *ctx;
};

static
enum bt_param_validation_status validate_value(
		const bt_value *value,
		const struct bt_param_validation_value_descr *descr,
		struct bt_param_validation_context *ctx);

static
bt_value_map_foreach_entry_const_func_status validate_map_value_entry(
		const char *key, const bt_value *value, void *v_data)
{
	struct validate_map_value_data *data = v_data;
	const struct bt_param_validation_map_value_entry_descr *entry = NULL;
	guint i;

	/* Check if this key is in the available keys. */
	for (i = 0; i < data->available_keys->len; i++) {
		const struct bt_param_validation_map_value_entry_descr *candidate =
			g_ptr_array_index(data->available_keys, i);

		if (g_str_equal(key, candidate->key)) {
			entry = candidate;
			break;
		}
	}

	if (entry) {
		/* Key was found in available keys. */
		g_ptr_array_remove_index_fast(data->available_keys, i);

		/* Push key name as the scope. */
		validate_ctx_push_map_scope(data->ctx, key);

		/* Validate the value of the entry. */
		data->status = validate_value(value, &entry->value_descr,
			data->ctx);

		validate_ctx_pop_scope(data->ctx);
	} else {
		data->status = bt_param_validation_error(data->ctx,
			"unexpected key `%s`.", key);
	}

	/* Continue iterating if everything is good so far. */
	return data->status == BT_PARAM_VALIDATION_STATUS_OK ?
		BT_VALUE_MAP_FOREACH_ENTRY_CONST_FUNC_STATUS_OK :
		BT_VALUE_MAP_FOREACH_ENTRY_CONST_FUNC_STATUS_INTERRUPT;
}

static
enum bt_param_validation_status validate_map_value(
		const struct bt_param_validation_map_value_descr *descr,
		const bt_value *map,
		struct bt_param_validation_context *ctx) {
	enum bt_param_validation_status status;
	struct validate_map_value_data data;
	bt_value_map_foreach_entry_const_status foreach_entry_status;
	GPtrArray *available_keys = NULL;
	const struct bt_param_validation_map_value_entry_descr *descr_iter;
	guint i;

	BT_ASSERT(bt_value_get_type(map) == BT_VALUE_TYPE_MAP);

	available_keys = g_ptr_array_new();
	if (!available_keys) {
		status = BT_PARAM_VALIDATION_STATUS_MEMORY_ERROR;
		goto end;
	}

	for (descr_iter = descr->entries; descr_iter->key; descr_iter++) {
		g_ptr_array_add(available_keys, (gpointer) descr_iter);
	}

	/* Initialize `status` to OK, in case the map is empty. */
	data.status = BT_PARAM_VALIDATION_STATUS_OK;
	data.available_keys = available_keys;
	data.ctx = ctx;

	foreach_entry_status = bt_value_map_foreach_entry_const(map,
		validate_map_value_entry, &data);
	if (foreach_entry_status == BT_VALUE_MAP_FOREACH_ENTRY_CONST_STATUS_INTERRUPTED) {
		BT_ASSERT(data.status != BT_PARAM_VALIDATION_STATUS_OK);
		status = data.status;
		goto end;
	}

	BT_ASSERT(data.status == BT_PARAM_VALIDATION_STATUS_OK);

	for (i = 0; i < data.available_keys->len; i++) {
		const struct bt_param_validation_map_value_entry_descr *entry =
			g_ptr_array_index(data.available_keys, i);

		if (!entry->is_optional) {
			status = bt_param_validation_error(ctx,
				"missing mandatory entry `%s`",
				entry->key);
			goto end;
		}
	}

	status = BT_PARAM_VALIDATION_STATUS_OK;

end:
	g_ptr_array_free(available_keys, TRUE);
	return status;
}

static
enum bt_param_validation_status validate_array_value(
		const struct bt_param_validation_array_value_descr *descr,
		const bt_value *array,
		struct bt_param_validation_context *ctx) {
	enum bt_param_validation_status status;
	uint64_t i;

	BT_ASSERT(bt_value_get_type(array) == BT_VALUE_TYPE_ARRAY);

	if (bt_value_array_get_length(array) < descr->min_length) {
		status = bt_param_validation_error(ctx,
			"array is smaller than the minimum length: "
			"array-length=%" PRIu64 ", min-length=%" PRIu64,
			bt_value_array_get_length(array),
			descr->min_length);
		goto end;
	}

	if (bt_value_array_get_length(array) > descr->max_length) {
		status = bt_param_validation_error(ctx,
			"array is larger than the maximum length: "
			"array-length=%" PRIu64 ", max-length=%" PRIu64,
			bt_value_array_get_length(array),
			descr->max_length);
		goto end;
	}

	for (i = 0; i < bt_value_array_get_length(array); i++) {
		const bt_value *element =
			bt_value_array_borrow_element_by_index_const(array, i);

		validate_ctx_push_array_scope(ctx, i);

		status = validate_value(element, descr->element_type, ctx);

		validate_ctx_pop_scope(ctx);

		if (status != BT_PARAM_VALIDATION_STATUS_OK) {
			goto end;
		}
	}

	status = BT_PARAM_VALIDATION_STATUS_OK;

end:
	return status;
}

static
enum bt_param_validation_status validate_string_value(
		const struct bt_param_validation_string_value_descr *descr,
		const bt_value *string,
		struct bt_param_validation_context *ctx) {
	enum bt_param_validation_status status;
	const char *s = bt_value_string_get(string);
	gchar *joined_choices = NULL;

	BT_ASSERT(bt_value_get_type(string) == BT_VALUE_TYPE_STRING);

	if (descr->choices) {
		const char **choice;

		for (choice = descr->choices; *choice; choice++) {
			if (strcmp(s, *choice) == 0) {
				break;
			}
		}

		if (!*choice) {
			/*
			 * g_strjoinv takes a gchar **, but it doesn't modify
			 * the array of the strings (yet).
			 */
			joined_choices = g_strjoinv(", ", (gchar **) descr->choices);
			if (!joined_choices) {
				status = BT_PARAM_VALIDATION_STATUS_MEMORY_ERROR;
				goto end;
			}

			status = bt_param_validation_error(ctx,
				"string is not amongst the available choices: "
				"string=%s, choices=[%s]", s, joined_choices);
			goto end;
		}
	}

	status = BT_PARAM_VALIDATION_STATUS_OK;
end:
	g_free(joined_choices);

	return status;
}

static
enum bt_param_validation_status validate_value(
		const bt_value *value,
		const struct bt_param_validation_value_descr *descr,
		struct bt_param_validation_context *ctx) {
	enum bt_param_validation_status status;

	/* If there is a custom validation func, we call it and ignore the rest. */
	if (descr->validation_func) {
		status = descr->validation_func(value, ctx);

		if (status == BT_PARAM_VALIDATION_STATUS_VALIDATION_ERROR) {
			BT_ASSERT(ctx->error);
		}

		goto end;
	}

	if (bt_value_get_type(value) != descr->type) {
		bt_param_validation_error(ctx,
			"unexpected type: expected-type=%s, actual-type=%s",
			bt_common_value_type_string(descr->type),
			bt_common_value_type_string(bt_value_get_type(value)));
		status = BT_PARAM_VALIDATION_STATUS_VALIDATION_ERROR;
		goto end;
	}

	switch (bt_value_get_type(value)) {
	case BT_VALUE_TYPE_MAP:
		status = validate_map_value(&descr->map, value, ctx);
		break;
	case BT_VALUE_TYPE_ARRAY:
		status = validate_array_value(&descr->array, value, ctx);
		break;
	case BT_VALUE_TYPE_STRING:
		status = validate_string_value(&descr->string, value, ctx);
		break;
	default:
		status = BT_PARAM_VALIDATION_STATUS_OK;
		break;
	}

end:
	return status;
}

enum bt_param_validation_status bt_param_validation_validate(
		const bt_value *params,
		const struct bt_param_validation_map_value_entry_descr *entries,
		gchar **error) {
	struct bt_param_validation_context ctx;
	struct bt_param_validation_map_value_descr map_value_descr;
	enum bt_param_validation_status status;

	memset(&ctx, '\0', sizeof(ctx));

	ctx.scope_stack = g_array_new(FALSE, FALSE,
		sizeof(struct validate_ctx_stack_element));
	if (!ctx.scope_stack) {
		status = BT_PARAM_VALIDATION_STATUS_MEMORY_ERROR;
		goto end;
	}

	map_value_descr.entries = entries;

	status = validate_map_value(&map_value_descr, params, &ctx);

end:
	*error = ctx.error;
	ctx.error = NULL;

	if (ctx.scope_stack) {
		g_array_free(ctx.scope_stack, TRUE);
	}

	return status;
}
