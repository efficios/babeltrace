#ifndef BABELTRACE_COMMON_INTERNAL_H
#define BABELTRACE_COMMON_INTERNAL_H

#include <babeltrace/babeltrace-internal.h>

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

BT_HIDDEN
bool bt_common_is_setuid_setgid(void);

BT_HIDDEN
const char *bt_common_get_system_plugin_path(void);

BT_HIDDEN
char *bt_common_get_home_plugin_path(void);

BT_HIDDEN
int bt_common_append_plugin_path_dirs(const char *paths, GPtrArray *dirs);

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

BT_HIDDEN
GString *bt_common_string_until(const char *input, const char *escapable_chars,
                    const char *end_chars, size_t *end_pos);

BT_HIDDEN
GString *bt_common_shell_quote(const char *input, bool with_single_quotes);

BT_HIDDEN
bool bt_common_string_is_printable(const char *input);

BT_HIDDEN
void bt_common_destroy_lttng_live_url_parts(
		struct bt_common_lttng_live_url_parts *parts);

BT_HIDDEN
struct bt_common_lttng_live_url_parts bt_common_parse_lttng_live_url(
		const char *url, char *error_buf, size_t error_buf_size);

BT_HIDDEN
void bt_common_normalize_star_glob_pattern(char *pattern);

BT_HIDDEN
bool bt_common_star_glob_match(const char *pattern, size_t pattern_len,
                const char *candidate, size_t candidate_len);

#endif /* BABELTRACE_COMMON_INTERNAL_H */
