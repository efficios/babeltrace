/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2020 Philippe Proulx <pproulx@efficios.com>
 */

#define BT_LOG_TAG "LIB/ASSERT-COND"
#include "lib/logging.h"

#include <string.h>
#include <stdarg.h>
#include <glib.h>
#include "common/assert.h"
#include "common/macros.h"
#include "assert-cond-base.h"

static
GString *format_cond_id(const char *cond_type, const char *func,
		const char *id_suffix)
{
	static const char func_prefix[] = "bt_";
	GString *id = g_string_new(NULL);
	const char *src_ch;

	BT_ASSERT(id);

	/* Condition type */
	BT_ASSERT(cond_type);
	g_string_append_printf(id, "%s:", cond_type);

	/* Function name: no prefix */
	BT_ASSERT(func);
	BT_ASSERT(strstr(func, func_prefix) == func);
	src_ch = &func[strlen(func_prefix)];

	/* Function name: `_` replaced with `-` */
	for (; *src_ch; src_ch++) {
		char dst_ch;

		if (*src_ch == '_') {
			dst_ch = '-';
		} else {
			dst_ch = *src_ch;
		}

		g_string_append_c(id, dst_ch);
	}

	/* Suffix */
	BT_ASSERT(id_suffix);
	g_string_append_printf(id, ":%s", id_suffix);

	return id;
}

BT_HIDDEN
void bt_lib_assert_cond_failed(const char *cond_type, const char *func,
		const char *id_suffix, const char *fmt, ...)
{
	va_list args;
	GString *cond_id = format_cond_id(cond_type, func, id_suffix);

	BT_ASSERT(cond_id);
	BT_ASSERT_COND_MSG("Babeltrace 2 library %scondition not satisfied.",
		cond_type);
	BT_ASSERT_COND_MSG("------------------------------------------------------------------------");
	BT_ASSERT_COND_MSG("Condition ID: `%s`.", cond_id->str);
	g_string_free(cond_id, TRUE);
	BT_ASSERT_COND_MSG("Function: %s().", func);
	BT_ASSERT_COND_MSG("------------------------------------------------------------------------");
	BT_ASSERT_COND_MSG("Error is:");
	va_start(args, fmt);
	bt_lib_log_v(_BT_LOG_SRCLOC_FUNCTION, __FILE__, __LINE__, BT_LOG_FATAL,
		BT_LOG_TAG, fmt, &args);
	va_end(args);
	BT_ASSERT_COND_MSG("Aborting...");
	bt_common_abort();
}
