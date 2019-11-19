#ifndef BABELTRACE_PLUGINS_COMMON_PARAM_VALIDATION_PARAM_VALIDATION_H
#define BABELTRACE_PLUGINS_COMMON_PARAM_VALIDATION_PARAM_VALIDATION_H

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

#include <babeltrace2/babeltrace.h>
#include <glib.h>
#include <stdbool.h>

#include <stdio.h> /* For __MINGW_PRINTF_FORMAT. */

#include <common/macros.h>

#ifdef __MINGW_PRINTF_FORMAT
# define BT_PRINTF_FORMAT __MINGW_PRINTF_FORMAT
#else
# define BT_PRINTF_FORMAT printf
#endif

struct bt_param_validation_context;
struct bt_param_validation_value_descr;

#define BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END { NULL, 0, {} }

struct bt_param_validation_map_value_descr {
	const struct bt_param_validation_map_value_entry_descr *entries;
};

#define BT_PARAM_VALIDATION_INFINITE UINT64_MAX

struct bt_param_validation_array_value_descr {
	uint64_t min_length;
	uint64_t max_length;  /* Use BT_PARAM_VALIDATION_INFINITE if there's no max. */
	const struct bt_param_validation_value_descr *element_type;
};

struct bt_param_validation_string_value_descr {
	/* NULL-terminated array of choices. Unused if NULL. */
	const char **choices;
};

enum bt_param_validation_status {
	BT_PARAM_VALIDATION_STATUS_OK = 0,
	BT_PARAM_VALIDATION_STATUS_MEMORY_ERROR = -1,
	BT_PARAM_VALIDATION_STATUS_VALIDATION_ERROR = -2,
};

typedef enum bt_param_validation_status
	(bt_param_validation_func)(const bt_value *value,
			struct bt_param_validation_context *);

struct bt_param_validation_value_descr {
	bt_value_type type;

	/* Additional checks dependent on the type. */
	union {
		struct bt_param_validation_array_value_descr array;
		struct bt_param_validation_map_value_descr map;
		struct bt_param_validation_string_value_descr string;
	};

	/*
	 * If set, call this function, which is responsible of validating the
	 * value. The other fields are ignored.
	 *
	 * If validation fails, this function must call
	 * `bt_param_validation_error` with the provided context
	 * to set the error string.
	 */
	bt_param_validation_func *validation_func;
};

#define BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL true
#define BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY false

struct bt_param_validation_map_value_entry_descr {
	const char *key;
	bool is_optional;

	const struct bt_param_validation_value_descr value_descr;
};

BT_HIDDEN
enum bt_param_validation_status bt_param_validation_validate(
		const bt_value *params,
		const struct bt_param_validation_map_value_entry_descr *entries,
		gchar **error);

BT_HIDDEN
__attribute__((format(BT_PRINTF_FORMAT, 2, 3)))
enum bt_param_validation_status bt_param_validation_error(
		struct bt_param_validation_context *ctx,
		const char *format, ...);

#endif /* BABELTRACE_PLUGINS_COMMON_PARAM_VALIDATION_PARAM_VALIDATION_H */
