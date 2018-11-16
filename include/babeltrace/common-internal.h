#ifndef BABELTRACE_COMMON_INTERNAL_H
#define BABELTRACE_COMMON_INTERNAL_H

#include <stdbool.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/trace-ir/field-classes.h>
#include <babeltrace/trace-ir/field-path.h>
#include <babeltrace/trace-ir/event-class.h>
#include <babeltrace/values.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdint.h>
#include <glib.h>

#define BT_COMMON_COLOR_RESET              "\033[0m"
#define BT_COMMON_COLOR_BOLD               "\033[1m"
#define BT_COMMON_COLOR_FG_DEFAULT         "\033[39m"
#define BT_COMMON_COLOR_FG_RED             "\033[31m"
#define BT_COMMON_COLOR_FG_GREEN           "\033[32m"
#define BT_COMMON_COLOR_FG_YELLOW          "\033[33m"
#define BT_COMMON_COLOR_FG_BLUE            "\033[34m"
#define BT_COMMON_COLOR_FG_MAGENTA         "\033[35m"
#define BT_COMMON_COLOR_FG_CYAN            "\033[36m"
#define BT_COMMON_COLOR_FG_LIGHT_GRAY      "\033[37m"
#define BT_COMMON_COLOR_BG_DEFAULT         "\033[49m"
#define BT_COMMON_COLOR_BG_RED             "\033[41m"
#define BT_COMMON_COLOR_BG_GREEN           "\033[42m"
#define BT_COMMON_COLOR_BG_YELLOW          "\033[43m"
#define BT_COMMON_COLOR_BG_BLUE            "\033[44m"
#define BT_COMMON_COLOR_BG_MAGENTA         "\033[45m"
#define BT_COMMON_COLOR_BG_CYAN            "\033[46m"
#define BT_COMMON_COLOR_BG_LIGHT_GRAY      "\033[47m"

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
 * `/usr/lib/babeltrace/plugins`. Do not free the return value.
 */
BT_HIDDEN
const char *bt_common_get_system_plugin_path(void);

/*
 * Returns the user plugin path, e.g.
 * `/home/user/.local/lib/babeltrace/plugins`. You need to free the
 * return value.
 */
BT_HIDDEN
char *bt_common_get_home_plugin_path(void);

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
size_t bt_common_get_page_size(void);

static inline
const char *bt_common_field_class_type_string(enum bt_field_class_type class_type)
{
	switch (class_type) {
	case BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER:
		return "BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER";
	case BT_FIELD_CLASS_TYPE_SIGNED_INTEGER:
		return "BT_FIELD_CLASS_TYPE_SIGNED_INTEGER";
	case BT_FIELD_CLASS_TYPE_REAL:
		return "BT_FIELD_CLASS_TYPE_REAL";
	case BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION:
		return "BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION";
	case BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION:
		return "BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION";
	case BT_FIELD_CLASS_TYPE_STRING:
		return "BT_FIELD_CLASS_TYPE_STRING";
	case BT_FIELD_CLASS_TYPE_STRUCTURE:
		return "BT_FIELD_CLASS_TYPE_STRUCTURE";
	case BT_FIELD_CLASS_TYPE_STATIC_ARRAY:
		return "BT_FIELD_CLASS_TYPE_STATIC_ARRAY";
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY:
		return "BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY";
	case BT_FIELD_CLASS_TYPE_VARIANT:
		return "BT_FIELD_CLASS_TYPE_VARIANT";
	default:
		return "(unknown)";
	}
};

static inline
const char *bt_common_field_class_integer_preferred_display_base_string(enum bt_field_class_integer_preferred_display_base base)
{
	switch (base) {
	case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_BINARY:
		return "BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_BINARY";
	case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL:
		return "BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL";
	case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL:
		return "BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL";
	case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL:
		return "BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL";
	default:
		return "(unknown)";
	}
}

static inline
const char *bt_common_scope_string(enum bt_scope scope)
{
	switch (scope) {
	case BT_SCOPE_PACKET_HEADER:
		return "BT_SCOPE_PACKET_HEADER";
	case BT_SCOPE_PACKET_CONTEXT:
		return "BT_SCOPE_PACKET_CONTEXT";
	case BT_SCOPE_EVENT_HEADER:
		return "BT_SCOPE_EVENT_HEADER";
	case BT_SCOPE_EVENT_COMMON_CONTEXT:
		return "BT_SCOPE_EVENT_COMMON_CONTEXT";
	case BT_SCOPE_EVENT_SPECIFIC_CONTEXT:
		return "BT_SCOPE_EVENT_SPECIFIC_CONTEXT";
	case BT_SCOPE_EVENT_PAYLOAD:
		return "BT_SCOPE_EVENT_PAYLOAD";
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
		return "BT_EVENT_CLASS_LOG_LEVEL_EMERGENCY";
	case BT_EVENT_CLASS_LOG_LEVEL_ALERT:
		return "BT_EVENT_CLASS_LOG_LEVEL_ALERT";
	case BT_EVENT_CLASS_LOG_LEVEL_CRITICAL:
		return "BT_EVENT_CLASS_LOG_LEVEL_CRITICAL";
	case BT_EVENT_CLASS_LOG_LEVEL_ERROR:
		return "BT_EVENT_CLASS_LOG_LEVEL_ERROR";
	case BT_EVENT_CLASS_LOG_LEVEL_WARNING:
		return "BT_EVENT_CLASS_LOG_LEVEL_WARNING";
	case BT_EVENT_CLASS_LOG_LEVEL_NOTICE:
		return "BT_EVENT_CLASS_LOG_LEVEL_NOTICE";
	case BT_EVENT_CLASS_LOG_LEVEL_INFO:
		return "BT_EVENT_CLASS_LOG_LEVEL_INFO";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM:
		return "BT_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM:
		return "BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS:
		return "BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE:
		return "BT_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT:
		return "BT_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION:
		return "BT_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE:
		return "BT_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE";
	case BT_EVENT_CLASS_LOG_LEVEL_DEBUG:
		return "BT_EVENT_CLASS_LOG_LEVEL_DEBUG";
	default:
		return "(unknown)";
	}
};

static inline
const char *bt_common_value_type_string(enum bt_value_type type)
{
	switch (type) {
	case BT_VALUE_TYPE_NULL:
		return "BT_VALUE_TYPE_NULL";
	case BT_VALUE_TYPE_BOOL:
		return "BT_VALUE_TYPE_BOOL";
	case BT_VALUE_TYPE_INTEGER:
		return "BT_VALUE_TYPE_INTEGER";
	case BT_VALUE_TYPE_REAL:
		return "BT_VALUE_TYPE_REAL";
	case BT_VALUE_TYPE_STRING:
		return "BT_VALUE_TYPE_STRING";
	case BT_VALUE_TYPE_ARRAY:
		return "BT_VALUE_TYPE_ARRAY";
	case BT_VALUE_TYPE_MAP:
		return "BT_VALUE_TYPE_MAP";
	default:
		return "(unknown)";
	}
};

static inline
GString *bt_field_path_string(struct bt_field_path *path)
{
	GString *str = g_string_new(NULL);
	uint64_t i;

	BT_ASSERT(path);

	if (!str) {
		goto end;
	}

	g_string_append_printf(str, "[%s", bt_common_scope_string(
		bt_field_path_get_root_scope(path)));

	for (i = 0; i < bt_field_path_get_index_count(path); i++) {
		g_string_append_printf(str, ", %" PRIu64,
			bt_field_path_get_index_by_index(path, i));
	}

	g_string_append(str, "]");

end:
	return str;
}

#endif /* BABELTRACE_COMMON_INTERNAL_H */
