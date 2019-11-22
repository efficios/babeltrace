#ifndef BABELTRACE_COMMON_INTERNAL_H
#define BABELTRACE_COMMON_INTERNAL_H

/*
 * Copyright (c) 2018 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2018 Philippe Proulx <pproulx@efficios.com>
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

#include <errno.h>
#include <glib.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <babeltrace2/babeltrace.h>

#define __BT_IN_BABELTRACE_H
#include <babeltrace2/func-status.h>

#include "common/assert.h"
#include "common/macros.h"
#include "common/safe.h"

#define BT_COMMON_COLOR_RESET			"\033[0m"
#define BT_COMMON_COLOR_BOLD			"\033[1m"
#define BT_COMMON_COLOR_FG_DEFAULT		"\033[39m"
#define BT_COMMON_COLOR_FG_RED			"\033[31m"
#define BT_COMMON_COLOR_FG_GREEN		"\033[32m"
#define BT_COMMON_COLOR_FG_YELLOW		"\033[33m"
#define BT_COMMON_COLOR_FG_BLUE			"\033[34m"
#define BT_COMMON_COLOR_FG_MAGENTA		"\033[35m"
#define BT_COMMON_COLOR_FG_CYAN			"\033[36m"
#define BT_COMMON_COLOR_FG_LIGHT_GRAY		"\033[37m"
#define BT_COMMON_COLOR_FG_BOLD_RED		"\033[1m\033[31m"
#define BT_COMMON_COLOR_FG_BOLD_GREEN		"\033[1m\033[32m"
#define BT_COMMON_COLOR_FG_BOLD_YELLOW		"\033[1m\033[33m"
#define BT_COMMON_COLOR_FG_BOLD_BLUE		"\033[1m\033[34m"
#define BT_COMMON_COLOR_FG_BOLD_MAGENTA		"\033[1m\033[35m"
#define BT_COMMON_COLOR_FG_BOLD_CYAN		"\033[1m\033[36m"
#define BT_COMMON_COLOR_FG_BOLD_LIGHT_GRAY	"\033[1m\033[37m"
#define BT_COMMON_COLOR_FG_BRIGHT_RED		"\033[91m"
#define BT_COMMON_COLOR_FG_BRIGHT_GREEN		"\033[92m"
#define BT_COMMON_COLOR_FG_BRIGHT_YELLOW	"\033[93m"
#define BT_COMMON_COLOR_FG_BRIGHT_BLUE		"\033[94m"
#define BT_COMMON_COLOR_FG_BRIGHT_MAGENTA	"\033[95m"
#define BT_COMMON_COLOR_FG_BRIGHT_CYAN		"\033[96m"
#define BT_COMMON_COLOR_FG_BRIGHT_LIGHT_GRAY	"\033[97m"
#define BT_COMMON_COLOR_BG_DEFAULT		"\033[49m"
#define BT_COMMON_COLOR_BG_RED			"\033[41m"
#define BT_COMMON_COLOR_BG_GREEN		"\033[42m"
#define BT_COMMON_COLOR_BG_YELLOW		"\033[43m"
#define BT_COMMON_COLOR_BG_BLUE			"\033[44m"
#define BT_COMMON_COLOR_BG_MAGENTA		"\033[45m"
#define BT_COMMON_COLOR_BG_CYAN			"\033[46m"
#define BT_COMMON_COLOR_BG_LIGHT_GRAY		"\033[47m"

enum bt_common_color_when {
	BT_COMMON_COLOR_WHEN_AUTO,
	BT_COMMON_COLOR_WHEN_ALWAYS,
	BT_COMMON_COLOR_WHEN_NEVER,
};

struct bt_common_color_codes {
	const char *reset;
	const char *bold;
	const char *fg_default;
	const char *fg_red;
	const char *fg_green;
	const char *fg_yellow;
	const char *fg_blue;
	const char *fg_magenta;
	const char *fg_cyan;
	const char *fg_light_gray;
	const char *fg_bright_red;
	const char *fg_bright_green;
	const char *fg_bright_yellow;
	const char *fg_bright_blue;
	const char *fg_bright_magenta;
	const char *fg_bright_cyan;
	const char *fg_bright_light_gray;
	const char *bg_default;
	const char *bg_red;
	const char *bg_green;
	const char *bg_yellow;
	const char *bg_blue;
	const char *bg_magenta;
	const char *bg_cyan;
	const char *bg_light_gray;
};

struct bt_common_lttng_live_url_parts {
	GString *proto;
	GString *hostname;
	GString *target_hostname;
	GString *session_name;

	/* -1 means default port */
	int port;
};

/*
 * Checks if the current process has setuid or setgid access rights.
 * Returns `true` if so.
 */
BT_HIDDEN
bool bt_common_is_setuid_setgid(void);

/*
 * Returns the system-wide plugin path, e.g.
 * `/usr/lib/babeltrace2/plugins`. Do not free the return value.
 */
BT_HIDDEN
const char *bt_common_get_system_plugin_path(void);

/*
 * Returns the user plugin path, e.g.
 * `/home/user/.local/lib/babeltrace2/plugins`. You need to free the
 * return value.
 */
BT_HIDDEN
char *bt_common_get_home_plugin_path(int log_level);

/*
 * Appends the list of directories in `paths` to the array `dirs`.
 * `paths` is a list of directories separated by `:`. Returns 0 on
 * success.
 */
BT_HIDDEN
int bt_common_append_plugin_path_dirs(const char *paths, GPtrArray *dirs);

/*
 * Returns `true` if terminal color codes are supported for this
 * process.
 */
BT_HIDDEN
bool bt_common_colors_supported(void);

BT_HIDDEN
const char *bt_common_color_reset(void);

BT_HIDDEN
const char *bt_common_color_bold(void);

BT_HIDDEN
const char *bt_common_color_fg_default(void);

BT_HIDDEN
const char *bt_common_color_fg_red(void);

BT_HIDDEN
const char *bt_common_color_fg_green(void);

BT_HIDDEN
const char *bt_common_color_fg_yellow(void);

BT_HIDDEN
const char *bt_common_color_fg_blue(void);

BT_HIDDEN
const char *bt_common_color_fg_magenta(void);

BT_HIDDEN
const char *bt_common_color_fg_cyan(void);

BT_HIDDEN
const char *bt_common_color_fg_light_gray(void);

BT_HIDDEN
const char *bt_common_color_fg_bright_red(void);

BT_HIDDEN
const char *bt_common_color_fg_bright_green(void);

BT_HIDDEN
const char *bt_common_color_fg_bright_yellow(void);

BT_HIDDEN
const char *bt_common_color_fg_bright_blue(void);

BT_HIDDEN
const char *bt_common_color_fg_bright_magenta(void);

BT_HIDDEN
const char *bt_common_color_fg_bright_cyan(void);

BT_HIDDEN
const char *bt_common_color_fg_bright_light_gray(void);

BT_HIDDEN
const char *bt_common_color_bg_default(void);

BT_HIDDEN
const char *bt_common_color_bg_red(void);

BT_HIDDEN
const char *bt_common_color_bg_green(void);

BT_HIDDEN
const char *bt_common_color_bg_yellow(void);

BT_HIDDEN
const char *bt_common_color_bg_blue(void);

BT_HIDDEN
const char *bt_common_color_bg_magenta(void);

BT_HIDDEN
const char *bt_common_color_bg_cyan(void);

BT_HIDDEN
const char *bt_common_color_bg_light_gray(void);

BT_HIDDEN
void bt_common_color_get_codes(struct bt_common_color_codes *codes,
		enum bt_common_color_when use_colors);

/*
 * Returns the substring from `input` to the first character found
 * in the list of characters `end_chars`, unescaping any character
 * found in `escapable_chars`, and sets `*end_pos` to the position of
 * the end (from `input`). The caller owns the returned GString.
 */
BT_HIDDEN
GString *bt_common_string_until(const char *input, const char *escapable_chars,
                    const char *end_chars, size_t *end_pos);

/*
 * Returns the quoted version of `input` for a shell. If
 * `with_single_quotes` is `true`, prepends and appends the `'` prefix
 * and suffix to the returned string; otherwise the caller should
 * prepend and append them manually, although they are not always
 * required. The caller owns the returned GString.
 */
BT_HIDDEN
GString *bt_common_shell_quote(const char *input, bool with_single_quotes);

/*
 * Returns `true` if `input` is a string made only of printable
 * characters.
 */
BT_HIDDEN
bool bt_common_string_is_printable(const char *input);

/*
 * Destroys the parts of an LTTng live URL as returned by
 * bt_common_parse_lttng_live_url().
 */
BT_HIDDEN
void bt_common_destroy_lttng_live_url_parts(
		struct bt_common_lttng_live_url_parts *parts);

/*
 * Parses the LTTng live URL `url` and returns its different parts.
 * If there's an error, writes the error message into `*error_buf`
 * up to `error_buf_size` bytes. You must destroy the returned value
 * with bt_common_destroy_lttng_live_url_parts().
 */
BT_HIDDEN
struct bt_common_lttng_live_url_parts bt_common_parse_lttng_live_url(
		const char *url, char *error_buf, size_t error_buf_size);

/*
 * Normalizes (in place) a star globbing pattern to be used with
 * bt_common_star_glob_match(). This function always succeeds.
 */
BT_HIDDEN
void bt_common_normalize_star_glob_pattern(char *pattern);

/*
 * Returns `true` if `candidate` (of size `candidate_len`) matches
 * the star globbing pattern `pattern` (of size `pattern_len`).
 */
BT_HIDDEN
bool bt_common_star_glob_match(const char *pattern, size_t pattern_len,
                const char *candidate, size_t candidate_len);

/*
 * Normalizes the path `path`:
 *
 * * If it's a relative path, converts it to an absolute path using
 *   `wd` as the working directory (or the current working directory
 *   if `wd` is NULL).
 * * Removes consecutive and trailing slashes.
 * * Resolves `..` and `.` in the path (both in `path` and in `wd`).
 * * Does NOT resolve symbolic links.
 *
 * The caller owns the returned GString.
 */
BT_HIDDEN
GString *bt_common_normalize_path(const char *path, const char *wd);

typedef void (* bt_common_handle_custom_specifier_func)(void *priv_data,
		char **buf, size_t avail_size, const char **fmt, va_list *args);

/*
 * This is a custom vsnprintf() which handles the standard conversion
 * specifier as well as custom ones.
 *
 * `fmt` is a typical printf()-style format string, with the following
 * limitations:
 *
 * * The `*` width specifier is not accepted.
 * * The `*` precision specifier is not accepted.
 * * The `j` and `t` length modifiers are not accepted.
 * * The `n` format specifier is not accepted.
 * * The format specifiers defined in <inttypes.h> are not accepted
 *   except for `PRId64`, `PRIu64`, `PRIx64`, `PRIX64`, `PRIo64`, and
 *   `PRIi64`.
 *
 * `intro` specifies which special character immediately following an
 * introductory `%` character in `fmt` is used to indicate a custom
 * conversion specifier. For example, if `intro` is '@', then any `%@`
 * sequence in `fmt` is the beginning of a custom conversion specifier.
 *
 * When a custom conversion specifier is encountered in `fmt`,
 * the function calls `handle_specifier`. This callback receives:
 *
 * `priv_data`:
 *     Custom, private data.
 *
 * `buf`:
 *     Address of the current buffer pointer. `*buf` is the position to
 *     append new data. The callback must update `*buf` when appending
 *     new data. The callback must ensure not to write passed the whole
 *     buffer passed to bt_common_custom_vsnprintf().
 *
 * `avail_size`:
 *     Number of bytes left in whole buffer from the `*buf` point.
 *
 * `fmt`:
 *     Address of the current format string pointer. `*fmt` points to
 *     the introductory `%` character, which is followed by the
 *     character `intro`. The callback must update `*fmt` so that it
 *     points after the whole custom conversion specifier.
 *
 * `args`:
 *     Variable argument list. Use va_arg() to get new arguments from
 *     this list and update it at the same time.
 *
 * Because this is an internal utility, this function and its callback
 * do not return error codes: they abort when there's any error (bad
 * format string, for example).
 */
BT_HIDDEN
void bt_common_custom_vsnprintf(char *buf, size_t buf_size,
		char intro,
		bt_common_handle_custom_specifier_func handle_specifier,
		void *priv_data, const char *fmt, va_list *args);

/*
 * Variadic form of bt_common_custom_vsnprintf().
 */
BT_HIDDEN
void bt_common_custom_snprintf(char *buf, size_t buf_size,
		char intro,
		bt_common_handle_custom_specifier_func handle_specifier,
		void *priv_data, const char *fmt, ...);

/*
 * Returns the system page size.
 */
BT_HIDDEN
size_t bt_common_get_page_size(int log_level);

/*
 * Adds the digit separator `sep` as many times as needed to form groups
 * of `digits_per_group` digits within `str`. `str` must have enough
 * room to accomodate the new separators, that is:
 *
 *     strlen(str) + (strlen(str) / digits_per_group) + 1
 *
 * Example: with `str` `1983198398213`, `digits_per_group` 3, and `sep`
 * `,`, `str` becomes `1,983,198,398,213`.
 *
 * `strlen(str)` must not be 0. `digits_per_group` must not be 0. `sep`
 * must not be `\0`.
 */
BT_HIDDEN
void bt_common_sep_digits(char *str, unsigned int digits_per_group, char sep);

/*
 * This is similar to what the command `fold --spaces` does: it wraps
 * the input lines of `str`, breaking at spaces, and indenting each line
 * with `indent` spaces so that each line fits the total length
 * `total_length`.
 *
 * If an original line in `str` contains a word which is >= the content
 * length (`total_length - indent`), then the corresponding folded line
 * is also larger than the content length. In other words, breaking at
 * spaces is a best effort, but it might not be possible.
 *
 * The returned string, on success, is owned by the caller.
 */
BT_HIDDEN
GString *bt_common_fold(const char *str, unsigned int total_length,
		unsigned int indent);

/*
 * Writes the terminal's width to `*width`, its height to `*height`,
 * and returns 0 on success, or returns -1 on error.
 */
BT_HIDDEN
int bt_common_get_term_size(unsigned int *width, unsigned int *height);

/*
 * Appends the textual content of `fp` to `str`, starting from its
 * current position to the end of the file.
 *
 * This function does NOT rewind `fp` once it's done or on error.
 */
BT_HIDDEN
int bt_common_append_file_content_to_g_string(GString *str, FILE *fp);

BT_HIDDEN
void bt_common_abort(void) __attribute__((noreturn));

/*
 * Wraps read() function to handle EINTR and partial reads.
 * On success, it returns `count` received as parameter. On error, it returns a
 * value smaller than the requested `count`.
 */
static inline
ssize_t bt_common_read(int fd, void *buf, size_t count, int log_level)
{
	size_t i = 0;
	ssize_t ret;

	BT_ASSERT_DBG(buf);

	/* Never return an overflow value. */
	BT_ASSERT_DBG(count <= SSIZE_MAX);

	do {
		ret = read(fd, buf + i, count - i);
		if (ret < 0) {
			if (errno == EINTR) {
#ifdef BT_LOG_WRITE_CUR_LVL
				BT_LOG_WRITE_CUR_LVL(BT_LOG_DEBUG, log_level,
					BT_LOG_TAG,
					"read() call interrupted; retrying...");
#endif
				/* retry operation */
				continue;
			} else {
#ifdef BT_LOG_WRITE_ERRNO_CUR_LVL
				BT_LOG_WRITE_ERRNO_CUR_LVL(BT_LOG_ERROR,
					log_level, BT_LOG_TAG,
					"Error while reading", ": fd=%d", fd);
#endif
				goto end;
			}
		}
		i += ret;
		BT_ASSERT_DBG(i <= count);
	} while (count - i > 0 && ret > 0);

end:
	if (ret >= 0) {
		if (i == 0) {
			ret = -1;
		} else {
			ret = i;
		}
	}

	return ret;
}

static inline
const char *bt_common_field_class_type_string(enum bt_field_class_type class_type)
{
	switch (class_type) {
	case BT_FIELD_CLASS_TYPE_BOOL:
		return "BOOL";
	case BT_FIELD_CLASS_TYPE_BIT_ARRAY:
		return "BIT_ARRAY";
	case BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER:
		return "UNSIGNED_INTEGER";
	case BT_FIELD_CLASS_TYPE_SIGNED_INTEGER:
		return "SIGNED_INTEGER";
	case BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION:
		return "UNSIGNED_ENUMERATION";
	case BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION:
		return "SIGNED_ENUMERATION";
	case BT_FIELD_CLASS_TYPE_SINGLE_PRECISION_REAL:
		return "SINGLE_PRECISION_REAL";
	case BT_FIELD_CLASS_TYPE_DOUBLE_PRECISION_REAL:
		return "DOUBLE_PRECISION_REAL";
	case BT_FIELD_CLASS_TYPE_STRING:
		return "STRING";
	case BT_FIELD_CLASS_TYPE_STRUCTURE:
		return "STRUCTURE";
	case BT_FIELD_CLASS_TYPE_STATIC_ARRAY:
		return "STATIC_ARRAY";
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITHOUT_LENGTH_FIELD:
		return "DYNAMIC_ARRAY_WITHOUT_LENGTH_FIELD";
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD:
		return "DYNAMIC_ARRAY_WITH_LENGTH_FIELD";
	case BT_FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD:
		return "OPTION_WITHOUT_SELECTOR_FIELD";
	case BT_FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD:
		return "OPTION_WITH_BOOL_SELECTOR_FIELD";
	case BT_FIELD_CLASS_TYPE_OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD:
		return "OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD";
	case BT_FIELD_CLASS_TYPE_OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD:
		return "OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD";
	case BT_FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD:
		return "VARIANT_WITHOUT_SELECTOR_FIELD";
	case BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD:
		return "VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD";
	case BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD:
		return "VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD";
	default:
		return "(unknown)";
	}
};

static inline
const char *bt_common_field_class_integer_preferred_display_base_string(enum bt_field_class_integer_preferred_display_base base)
{
	switch (base) {
	case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_BINARY:
		return "BINARY";
	case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL:
		return "OCTAL";
	case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL:
		return "DECIMAL";
	case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL:
		return "HEXADECIMAL";
	default:
		return "(unknown)";
	}
}

static inline
const char *bt_common_scope_string(enum bt_field_path_scope scope)
{
	switch (scope) {
	case BT_FIELD_PATH_SCOPE_PACKET_CONTEXT:
		return "PACKET_CONTEXT";
	case BT_FIELD_PATH_SCOPE_EVENT_COMMON_CONTEXT:
		return "EVENT_COMMON_CONTEXT";
	case BT_FIELD_PATH_SCOPE_EVENT_SPECIFIC_CONTEXT:
		return "EVENT_SPECIFIC_CONTEXT";
	case BT_FIELD_PATH_SCOPE_EVENT_PAYLOAD:
		return "EVENT_PAYLOAD";
	default:
		return "(unknown)";
	}
}

static inline
const char *bt_common_event_class_log_level_string(
		enum bt_event_class_log_level level)
{
	switch (level) {
	case BT_EVENT_CLASS_LOG_LEVEL_EMERGENCY:
		return "EMERGENCY";
	case BT_EVENT_CLASS_LOG_LEVEL_ALERT:
		return "ALERT";
	case BT_EVENT_CLASS_LOG_LEVEL_CRITICAL:
		return "CRITICAL";
	case BT_EVENT_CLASS_LOG_LEVEL_ERROR:
		return "ERROR";
	case BT_EVENT_CLASS_LOG_LEVEL_WARNING:
		return "WARNING";
	case BT_EVENT_CLASS_LOG_LEVEL_NOTICE:
		return "NOTICE";
	case BT_EVENT_CLASS_LOG_LEVEL_INFO:
		return "INFO";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM:
		return "DEBUG_SYSTEM";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM:
		return "DEBUG_PROGRAM";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS:
		return "DEBUG_PROCESS";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE:
		return "DEBUG_MODULE";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT:
		return "DEBUG_UNIT";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION:
		return "DEBUG_FUNCTION";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE:
		return "DEBUG_LINE";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG:
		return "DEBUG";
	default:
		return "(unknown)";
	}
};

static inline
const char *bt_common_value_type_string(enum bt_value_type type)
{
	switch (type) {
	case BT_VALUE_TYPE_NULL:
		return "NULL";
	case BT_VALUE_TYPE_BOOL:
		return "BOOL";
	case BT_VALUE_TYPE_UNSIGNED_INTEGER:
		return "UNSIGNED_INTEGER";
	case BT_VALUE_TYPE_SIGNED_INTEGER:
		return "SIGNED_INTEGER";
	case BT_VALUE_TYPE_REAL:
		return "REAL";
	case BT_VALUE_TYPE_STRING:
		return "STRING";
	case BT_VALUE_TYPE_ARRAY:
		return "ARRAY";
	case BT_VALUE_TYPE_MAP:
		return "MAP";
	default:
		return "(unknown)";
	}
};

static inline
GString *bt_common_field_path_string(struct bt_field_path *path)
{
	GString *str = g_string_new(NULL);
	uint64_t i;

	BT_ASSERT_DBG(path);

	if (!str) {
		goto end;
	}

	g_string_append_printf(str, "[%s", bt_common_scope_string(
		bt_field_path_get_root_scope(path)));

	for (i = 0; i < bt_field_path_get_item_count(path); i++) {
		const struct bt_field_path_item *fp_item =
			bt_field_path_borrow_item_by_index_const(path, i);

		switch (bt_field_path_item_get_type(fp_item)) {
		case BT_FIELD_PATH_ITEM_TYPE_INDEX:
			g_string_append_printf(str, ", %" PRIu64,
				bt_field_path_item_index_get_index(fp_item));
			break;
		case BT_FIELD_PATH_ITEM_TYPE_CURRENT_ARRAY_ELEMENT:
			g_string_append(str, ", <CUR>");
			break;
		default:
			bt_common_abort();
		}
	}

	g_string_append(str, "]");

end:
	return str;
}

static inline
const char *bt_common_logging_level_string(
		enum bt_logging_level level)
{
	switch (level) {
	case BT_LOGGING_LEVEL_TRACE:
		return "TRACE";
	case BT_LOGGING_LEVEL_DEBUG:
		return "DEBUG";
	case BT_LOGGING_LEVEL_INFO:
		return "INFO";
	case BT_LOGGING_LEVEL_WARNING:
		return "WARNING";
	case BT_LOGGING_LEVEL_ERROR:
		return "ERROR";
	case BT_LOGGING_LEVEL_FATAL:
		return "FATAL";
	case BT_LOGGING_LEVEL_NONE:
		return "NONE";
	default:
		return "(unknown)";
	}
};

static inline
const char *bt_common_func_status_string(int status)
{
	switch (status) {
	case __BT_FUNC_STATUS_OVERFLOW_ERROR:
		return "OVERFLOW";
	case __BT_FUNC_STATUS_UNKNOWN_OBJECT:
		return "UNKNOWN_OBJECT";
	case __BT_FUNC_STATUS_MEMORY_ERROR:
		return "MEMORY_ERROR";
	case __BT_FUNC_STATUS_USER_ERROR:
		return "USER_ERROR";
	case __BT_FUNC_STATUS_ERROR:
		return "ERROR";
	case __BT_FUNC_STATUS_OK:
		return "OK";
	case __BT_FUNC_STATUS_END:
		return "END";
	case __BT_FUNC_STATUS_NOT_FOUND:
		return "NOT_FOUND";
	case __BT_FUNC_STATUS_AGAIN:
		return "AGAIN";
	case __BT_FUNC_STATUS_INTERRUPTED:
		return "INTERRUPTED";
	default:
		return "(unknown)";
	}
}

#define NS_PER_S_I	INT64_C(1000000000)
#define NS_PER_S_U	UINT64_C(1000000000)

static inline
int bt_common_clock_value_from_ns_from_origin(
		int64_t cc_offset_seconds, uint64_t cc_offset_cycles,
		uint64_t cc_freq, int64_t ns_from_origin,
		uint64_t *raw_value)
{
	int ret = 0;
	int64_t offset_in_ns;
	uint64_t value_in_ns;
	uint64_t rem_value_in_ns;
	uint64_t value_periods;
	uint64_t value_period_cycles;
	int64_t ns_to_add;

	BT_ASSERT_DBG(raw_value);

	/* Compute offset part of requested value, in nanoseconds */
	if (!bt_safe_to_mul_int64(cc_offset_seconds, NS_PER_S_I)) {
		ret = -1;
		goto end;
	}

	offset_in_ns = cc_offset_seconds * NS_PER_S_I;

	if (cc_freq == NS_PER_S_U) {
		ns_to_add = (int64_t) cc_offset_cycles;
	} else {
		if (!bt_safe_to_mul_int64((int64_t) cc_offset_cycles,
				NS_PER_S_I)) {
			ret = -1;
			goto end;
		}

		ns_to_add = ((int64_t) cc_offset_cycles * NS_PER_S_I) /
			(int64_t) cc_freq;
	}

	if (!bt_safe_to_add_int64(offset_in_ns, ns_to_add)) {
		ret = -1;
		goto end;
	}

	offset_in_ns += ns_to_add;

	/* Value part in nanoseconds */
	if (ns_from_origin < offset_in_ns) {
		ret = -1;
		goto end;
	}

	value_in_ns = (uint64_t) (ns_from_origin - offset_in_ns);

	/* Number of whole clock periods in `value_in_ns` */
	value_periods = value_in_ns / NS_PER_S_U;

	/* Remaining nanoseconds in cycles + whole clock periods in cycles */
	rem_value_in_ns = value_in_ns - value_periods * NS_PER_S_U;

	if (value_periods > UINT64_MAX / cc_freq) {
		ret = -1;
		goto end;
	}

	if (!bt_safe_to_mul_uint64(value_periods, cc_freq)) {
		ret = -1;
		goto end;
	}

	value_period_cycles = value_periods * cc_freq;

	if (!bt_safe_to_mul_uint64(cc_freq, rem_value_in_ns)) {
		ret = -1;
		goto end;
	}

	if (!bt_safe_to_add_uint64(cc_freq * rem_value_in_ns / NS_PER_S_U,
			value_period_cycles)) {
		ret = -1;
		goto end;
	}

	*raw_value = cc_freq * rem_value_in_ns / NS_PER_S_U +
		value_period_cycles;

end:
	return ret;
}

/*
 * bt_g_string_append_printf cannot be inlined because it expects a
 * variadic argument list.
 */
BT_HIDDEN
int bt_common_g_string_append_printf(GString *str, const char *fmt, ...);

static inline
void bt_common_g_string_append(GString *str, const char *s)
{
	gsize len, allocated_len, s_len;

	/* str->len excludes \0. */
	len = str->len;
	/* Exclude \0. */
	allocated_len = str->allocated_len - 1;
	s_len = strlen(s);
	if (G_UNLIKELY(allocated_len < len + s_len)) {
		/* Resize. */
		g_string_set_size(str, len + s_len);
	} else {
		str->len = len + s_len;
	}
	memcpy(str->str + len, s, s_len + 1);
}

static inline
void bt_common_g_string_append_c(GString *str, char c)
{
	gsize len, allocated_len, s_len;

	/* str->len excludes \0. */
	len = str->len;
	/* Exclude \0. */
	allocated_len = str->allocated_len - 1;
	s_len = 1;
	if (G_UNLIKELY(allocated_len < len + s_len)) {
		/* Resize. */
		g_string_set_size(str, len + s_len);
	} else {
		str->len = len + s_len;
	}
	str->str[len] = c;
	str->str[len + 1] = '\0';
}

#endif /* BABELTRACE_COMMON_INTERNAL_H */
