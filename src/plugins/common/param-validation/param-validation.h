/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 EfficiOS Inc.
 */
#ifndef BABELTRACE_PLUGINS_COMMON_PARAM_VALIDATION_PARAM_VALIDATION_H
#define BABELTRACE_PLUGINS_COMMON_PARAM_VALIDATION_PARAM_VALIDATION_H

#include <babeltrace2/babeltrace.h>
#include <glib.h>
#include <stdbool.h>

#include <stdio.h> /* For __MINGW_PRINTF_FORMAT. */

#include <common/macros.h>

struct bt_param_validation_context;
struct bt_param_validation_value_descr;

#if defined(__cplusplus)
#define BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END \
	{ bt_param_validation_map_value_entry_descr::end }
#else
#define BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END { NULL, 0, {} }
#endif

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
#if defined(__cplusplus)
	static struct {} array_t;
	static struct {} string_t;
	static struct {} signed_integer_t;
	static struct {} bool_t;

	bt_param_validation_value_descr(decltype(array_t),
			uint64_t min_length, uint64_t max_length,
			const bt_param_validation_value_descr &element_type)
		: type{BT_VALUE_TYPE_ARRAY}, array{min_length, max_length, &element_type}
	{}

	bt_param_validation_value_descr(decltype(string_t),
			const char **choices = nullptr)
		: type{BT_VALUE_TYPE_STRING}, string{choices}
        {}

	bt_param_validation_value_descr(decltype(signed_integer_t))
		: type{BT_VALUE_TYPE_SIGNED_INTEGER}
	{}

	bt_param_validation_value_descr(decltype(bool_t))
		: type{BT_VALUE_TYPE_BOOL}
	{}
#endif

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
	bt_param_validation_func *validation_func
#if defined(__cplusplus)
		= nullptr
#endif
	  ;
};

#define BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL true
#define BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY false

struct bt_param_validation_map_value_entry_descr {
#if defined(__cplusplus)
	static struct {} end;

	bt_param_validation_map_value_entry_descr(decltype(end))
		: key{nullptr},
		/* These values are not important. */
		is_optional{false}, value_descr(bt_param_validation_value_descr::bool_t)
	{}

	bt_param_validation_map_value_entry_descr(const char *key_, bool is_optional_,
			bt_param_validation_value_descr value_descr_)
		: key(key_), is_optional(is_optional_), value_descr(value_descr_)
	{}

#endif
	/* If NULL/nullptr, this entry represents the end of the list. */
	const char *key;
	bool is_optional;
	const struct bt_param_validation_value_descr value_descr;
};

BT_EXTERN_C BT_HIDDEN
enum bt_param_validation_status bt_param_validation_validate(
		const bt_value *params,
		const struct bt_param_validation_map_value_entry_descr *entries,
		gchar **error);

BT_EXTERN_C BT_HIDDEN __BT_ATTR_FORMAT_PRINTF(2, 3)
enum bt_param_validation_status bt_param_validation_error(
		struct bt_param_validation_context *ctx,
		const char *format, ...);

#endif /* BABELTRACE_PLUGINS_COMMON_PARAM_VALIDATION_PARAM_VALIDATION_H */
