/*
 * Babeltrace common functions
 *
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "COMMON"
#include "logging.h"

#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <babeltrace/assert-internal.h>
#include <stdarg.h>
#include <ctype.h>
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <stdbool.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/common-internal.h>
#include <babeltrace/compat/unistd-internal.h>

#ifndef __MINGW32__
#include <pwd.h>
#endif

#define SYSTEM_PLUGIN_PATH	INSTALL_LIBDIR "/babeltrace/plugins"
#define HOME_ENV_VAR		"HOME"
#define HOME_PLUGIN_SUBPATH	"/.local/lib/babeltrace/plugins"

static const char *bt_common_color_code_reset = "";
static const char *bt_common_color_code_bold = "";
static const char *bt_common_color_code_fg_default = "";
static const char *bt_common_color_code_fg_red = "";
static const char *bt_common_color_code_fg_green = "";
static const char *bt_common_color_code_fg_yellow = "";
static const char *bt_common_color_code_fg_blue = "";
static const char *bt_common_color_code_fg_magenta = "";
static const char *bt_common_color_code_fg_cyan = "";
static const char *bt_common_color_code_fg_light_gray = "";
static const char *bt_common_color_code_bg_default = "";
static const char *bt_common_color_code_bg_red = "";
static const char *bt_common_color_code_bg_green = "";
static const char *bt_common_color_code_bg_yellow = "";
static const char *bt_common_color_code_bg_blue = "";
static const char *bt_common_color_code_bg_magenta = "";
static const char *bt_common_color_code_bg_cyan = "";
static const char *bt_common_color_code_bg_light_gray = "";

static
void __attribute__((constructor)) bt_common_color_ctor(void)
{
	if (bt_common_colors_supported()) {
		bt_common_color_code_reset = BT_COMMON_COLOR_RESET;
		bt_common_color_code_bold = BT_COMMON_COLOR_BOLD;
		bt_common_color_code_fg_default = BT_COMMON_COLOR_FG_DEFAULT;
		bt_common_color_code_fg_red = BT_COMMON_COLOR_FG_RED;
		bt_common_color_code_fg_green = BT_COMMON_COLOR_FG_GREEN;
		bt_common_color_code_fg_yellow = BT_COMMON_COLOR_FG_YELLOW;
		bt_common_color_code_fg_blue = BT_COMMON_COLOR_FG_BLUE;
		bt_common_color_code_fg_magenta = BT_COMMON_COLOR_FG_MAGENTA;
		bt_common_color_code_fg_cyan = BT_COMMON_COLOR_FG_CYAN;
		bt_common_color_code_fg_light_gray = BT_COMMON_COLOR_FG_LIGHT_GRAY;
		bt_common_color_code_bg_default = BT_COMMON_COLOR_BG_DEFAULT;
		bt_common_color_code_bg_red = BT_COMMON_COLOR_BG_RED;
		bt_common_color_code_bg_green = BT_COMMON_COLOR_BG_GREEN;
		bt_common_color_code_bg_yellow = BT_COMMON_COLOR_BG_YELLOW;
		bt_common_color_code_bg_blue = BT_COMMON_COLOR_BG_BLUE;
		bt_common_color_code_bg_magenta = BT_COMMON_COLOR_BG_MAGENTA;
		bt_common_color_code_bg_cyan = BT_COMMON_COLOR_BG_CYAN;
		bt_common_color_code_bg_light_gray = BT_COMMON_COLOR_BG_LIGHT_GRAY;
	}
}

BT_HIDDEN
const char *bt_common_get_system_plugin_path(void)
{
	return SYSTEM_PLUGIN_PATH;
}

#ifdef __MINGW32__
BT_HIDDEN
bool bt_common_is_setuid_setgid(void)
{
	return false;
}
#else /* __MINGW32__ */
BT_HIDDEN
bool bt_common_is_setuid_setgid(void)
{
	return (geteuid() != getuid() || getegid() != getgid());
}
#endif /* __MINGW32__ */

static
char *bt_secure_getenv(const char *name)
{
	if (bt_common_is_setuid_setgid()) {
		BT_LOGD("Disregarding environment variable for setuid/setgid binary: "
			"name=\"%s\"", name);
		return NULL;
	}
	return getenv(name);
}

#ifdef __MINGW32__
static
const char *bt_get_home_dir(void)
{
	return g_get_home_dir();
}
#else /* __MINGW32__ */
static
const char *bt_get_home_dir(void)
{
	char *val = NULL;
	struct passwd *pwd;

	val = bt_secure_getenv(HOME_ENV_VAR);
	if (val) {
		goto end;
	}
	/* Fallback on password file. */
	pwd = getpwuid(getuid());
	if (!pwd) {
		goto end;
	}
	val = pwd->pw_dir;
end:
	return val;
}
#endif /* __MINGW32__ */

BT_HIDDEN
char *bt_common_get_home_plugin_path(void)
{
	char *path = NULL;
	const char *home_dir;
	size_t length;

	home_dir = bt_get_home_dir();
	if (!home_dir) {
		goto end;
	}

	length = strlen(home_dir) + strlen(HOME_PLUGIN_SUBPATH) + 1;

	if (length >= PATH_MAX) {
		BT_LOGW("Home directory path is too long: length=%zu",
			length);
		goto end;
	}

	path = malloc(PATH_MAX);
	if (!path) {
		goto end;
	}

	strcpy(path, home_dir);
	strcat(path, HOME_PLUGIN_SUBPATH);

end:
	return path;
}

BT_HIDDEN
int bt_common_append_plugin_path_dirs(const char *paths, GPtrArray *dirs)
{
	int ret = 0;
	const char *at;
	const char *end;
	size_t init_dirs_len;

	BT_ASSERT(dirs);
	init_dirs_len = dirs->len;

	if (!paths) {
		/* Nothing to append */
		goto end;
	}

	at = paths;
	end = paths + strlen(paths);

	while (at < end) {
		GString *path;
		const char *next_sep;

		next_sep = strchr(at, G_SEARCHPATH_SEPARATOR);
		if (next_sep == at) {
			/*
			 * Empty path: try next character (supported
			 * to conform to the typical parsing of $PATH).
			 */
			at++;
			continue;
		} else if (!next_sep) {
			/* No more separator: use the remaining */
			next_sep = paths + strlen(paths);
		}

		path = g_string_new(NULL);
		if (!path) {
			goto error;
		}

		g_string_append_len(path, at, next_sep - at);
		at = next_sep + 1;
		g_ptr_array_add(dirs, path);
	}

	goto end;

error:
	ret = -1;

	/* Remove the new entries in dirs */
	while (dirs->len > init_dirs_len) {
		g_ptr_array_remove_index(dirs, init_dirs_len);
	}

end:
	return ret;
}

static
bool isarealtty(int fd)
{
	bool istty = false;
	struct stat tty_stats;

	if (!isatty(fd)) {
		/* Not a TTY */
		goto end;
	}

	if (fstat(fd, &tty_stats) == 0) {
		if (!S_ISCHR(tty_stats.st_mode)) {
			/* Not a character device: not a TTY */
			goto end;
		}
	}

	istty = true;

end:
	return istty;
}

BT_HIDDEN
bool bt_common_colors_supported(void)
{
	static bool supports_colors = false;
	static bool supports_colors_set = false;
	const char *term_env_var;
	const char *term_color_env_var;

	if (supports_colors_set) {
		goto end;
	}

	supports_colors_set = true;

	/*
	 * `BABELTRACE_TERM_COLOR` environment variable always overrides
	 * the automatic color support detection.
	 */
	term_color_env_var = getenv("BABELTRACE_TERM_COLOR");
	if (term_color_env_var) {
		if (g_ascii_strcasecmp(term_color_env_var, "always") == 0) {
			/* Force colors */
			supports_colors = true;
		} else if (g_ascii_strcasecmp(term_color_env_var, "never") == 0) {
			/* Force no colors */
			goto end;
		}
	}

	/* We need a compatible, known terminal */
	term_env_var = getenv("TERM");
	if (!term_env_var) {
		goto end;
	}

	if (strncmp(term_env_var, "xterm", 5) != 0 &&
			strncmp(term_env_var, "rxvt", 4) != 0 &&
			strncmp(term_env_var, "konsole", 7) != 0 &&
			strncmp(term_env_var, "gnome", 5) != 0 &&
			strncmp(term_env_var, "screen", 5) != 0 &&
			strncmp(term_env_var, "tmux", 4) != 0 &&
			strncmp(term_env_var, "putty", 5) != 0) {
		goto end;
	}

	/* Both standard output and error streams need to be TTYs */
	if (!isarealtty(STDOUT_FILENO) || !isarealtty(STDERR_FILENO)) {
		goto end;
	}

	supports_colors = true;

end:
	return supports_colors;
}

BT_HIDDEN
const char *bt_common_color_reset(void)
{
	return bt_common_color_code_reset;
}

BT_HIDDEN
const char *bt_common_color_bold(void)
{
	return bt_common_color_code_bold;
}

BT_HIDDEN
const char *bt_common_color_fg_default(void)
{
	return bt_common_color_code_fg_default;
}

BT_HIDDEN
const char *bt_common_color_fg_red(void)
{
	return bt_common_color_code_fg_red;
}

BT_HIDDEN
const char *bt_common_color_fg_green(void)
{
	return bt_common_color_code_fg_green;
}

BT_HIDDEN
const char *bt_common_color_fg_yellow(void)
{
	return bt_common_color_code_fg_yellow;
}

BT_HIDDEN
const char *bt_common_color_fg_blue(void)
{
	return bt_common_color_code_fg_blue;
}

BT_HIDDEN
const char *bt_common_color_fg_magenta(void)
{
	return bt_common_color_code_fg_magenta;
}

BT_HIDDEN
const char *bt_common_color_fg_cyan(void)
{
	return bt_common_color_code_fg_cyan;
}

BT_HIDDEN
const char *bt_common_color_fg_light_gray(void)
{
	return bt_common_color_code_fg_light_gray;
}

BT_HIDDEN
const char *bt_common_color_bg_default(void)
{
	return bt_common_color_code_bg_default;
}

BT_HIDDEN
const char *bt_common_color_bg_red(void)
{
	return bt_common_color_code_bg_red;
}

BT_HIDDEN
const char *bt_common_color_bg_green(void)
{
	return bt_common_color_code_bg_green;
}

BT_HIDDEN
const char *bt_common_color_bg_yellow(void)
{
	return bt_common_color_code_bg_yellow;
}

BT_HIDDEN
const char *bt_common_color_bg_blue(void)
{
	return bt_common_color_code_bg_blue;
}

BT_HIDDEN
const char *bt_common_color_bg_magenta(void)
{
	return bt_common_color_code_bg_magenta;
}

BT_HIDDEN
const char *bt_common_color_bg_cyan(void)
{
	return bt_common_color_code_bg_cyan;
}

BT_HIDDEN
const char *bt_common_color_bg_light_gray(void)
{
	return bt_common_color_code_bg_light_gray;
}

BT_HIDDEN
GString *bt_common_string_until(const char *input, const char *escapable_chars,
		const char *end_chars, size_t *end_pos)
{
	GString *output = g_string_new(NULL);
	const char *ch;
	const char *es_char;
	const char *end_char;

	if (!output) {
		goto error;
	}

	for (ch = input; *ch != '\0'; ch++) {
		if (*ch == '\\') {
			bool continue_loop = false;

			if (ch[1] == '\0') {
				/* `\` at the end of the string: append `\` */
				g_string_append_c(output, *ch);
				ch++;
				goto set_end_pos;
			}

			for (es_char = escapable_chars; *es_char != '\0'; es_char++) {
				if (ch[1] == *es_char) {
					/*
					 * `\` followed by an escapable
					 * character: append the escaped
					 * character only.
					 */
					g_string_append_c(output, ch[1]);
					ch++;
					continue_loop = true;
					break;
				}
			}

			if (continue_loop) {
				continue;
			}

			/*
			 * `\` followed by a non-escapable character:
			 * append `\` and the character.
			 */
			g_string_append_c(output, *ch);
			g_string_append_c(output, ch[1]);
			ch++;
			continue;
		} else {
			for (end_char = end_chars; *end_char != '\0'; end_char++) {
				if (*ch == *end_char) {
					/*
					 * End character found:
					 * terminate this loop.
					 */
					goto set_end_pos;
				}
			}

			/* Normal character: append */
			g_string_append_c(output, *ch);
		}
	}

set_end_pos:
	if (end_pos) {
		*end_pos = ch - input;
	}

	goto end;

error:
	if (output) {
		g_string_free(output, TRUE);
	}

end:
	return output;
}

BT_HIDDEN
GString *bt_common_shell_quote(const char *input, bool with_single_quotes)
{
	GString *output = g_string_new(NULL);
	const char *ch;
	bool no_quote = true;

	if (!output) {
		goto end;
	}

	if (strlen(input) == 0) {
		if (with_single_quotes) {
			g_string_assign(output, "''");
		}

		goto end;
	}

	for (ch = input; *ch != '\0'; ch++) {
		const char c = *ch;

		if (!g_ascii_isalpha(c) && !g_ascii_isdigit(c) && c != '_' &&
				c != '@' && c != '%' && c != '+' && c != '=' &&
				c != ':' && c != ',' && c != '.' && c != '/' &&
				c != '-') {
			no_quote = false;
			break;
		}
	}

	if (no_quote) {
		g_string_assign(output, input);
		goto end;
	}

	if (with_single_quotes) {
		g_string_assign(output, "'");
	}

	for (ch = input; *ch != '\0'; ch++) {
		if (*ch == '\'') {
			g_string_append(output, "'\"'\"'");
		} else {
			g_string_append_c(output, *ch);
		}
	}

	if (with_single_quotes) {
		g_string_append_c(output, '\'');
	}

end:
	return output;
}

BT_HIDDEN
bool bt_common_string_is_printable(const char *input)
{
	const char *ch;
	bool printable = true;
	BT_ASSERT(input);

	for (ch = input; *ch != '\0'; ch++) {
		if (!isprint(*ch) && *ch != '\n' && *ch != '\r' &&
				*ch != '\t' && *ch != '\v') {
			printable = false;
			goto end;
		}
	}

end:
	return printable;
}

BT_HIDDEN
void bt_common_destroy_lttng_live_url_parts(
		struct bt_common_lttng_live_url_parts *parts)
{
	if (!parts) {
		goto end;
	}

	if (parts->proto) {
		g_string_free(parts->proto, TRUE);
		parts->proto = NULL;
	}

	if (parts->hostname) {
		g_string_free(parts->hostname, TRUE);
		parts->hostname = NULL;
	}

	if (parts->target_hostname) {
		g_string_free(parts->target_hostname, TRUE);
		parts->target_hostname = NULL;
	}

	if (parts->session_name) {
		g_string_free(parts->session_name, TRUE);
		parts->session_name = NULL;
	}

end:
	return;
}

BT_HIDDEN
struct bt_common_lttng_live_url_parts bt_common_parse_lttng_live_url(
		const char *url, char *error_buf, size_t error_buf_size)
{
	struct bt_common_lttng_live_url_parts parts;
	const char *at = url;
	size_t end_pos;

	BT_ASSERT(url);
	memset(&parts, 0, sizeof(parts));
	parts.port = -1;

	/* Protocol */
	parts.proto = bt_common_string_until(at, "", ":", &end_pos);
	if (!parts.proto || parts.proto->len == 0) {
		if (error_buf) {
			snprintf(error_buf, error_buf_size, "Missing protocol");
		}

		goto error;
	}

	if (strcmp(parts.proto->str, "net") == 0) {
		g_string_assign(parts.proto, "net4");
	}

	if (strcmp(parts.proto->str, "net4") != 0 &&
			strcmp(parts.proto->str, "net6") != 0) {
		if (error_buf) {
			snprintf(error_buf, error_buf_size,
				"Unknown protocol: `%s`", parts.proto->str);
		}

		goto error;
	}

	if (at[end_pos] != ':') {
		if (error_buf) {
			snprintf(error_buf, error_buf_size,
				"Expecting `:` after `%s`", parts.proto->str);
		}

		goto error;
	}

	at += end_pos;

	/* :// */
	if (strncmp(at, "://", 3) != 0) {
		if (error_buf) {
			snprintf(error_buf, error_buf_size,
				"Expecting `://` after protocol");
		}

		goto error;
	}

	at += 3;

	/* Hostname */
	parts.hostname = bt_common_string_until(at, "", ":/", &end_pos);
	if (!parts.hostname || parts.hostname->len == 0) {
		if (error_buf) {
			snprintf(error_buf, error_buf_size, "Missing hostname");
		}

		goto error;
	}

	if (at[end_pos] == ':') {
		/* Port */
		GString *port;

		at += end_pos + 1;
		port = bt_common_string_until(at, "", "/", &end_pos);
		if (!port || port->len == 0) {
			if (error_buf) {
				snprintf(error_buf, error_buf_size, "Missing port");
			}

			goto error;
		}

		if (sscanf(port->str, "%d", &parts.port) != 1) {
			if (error_buf) {
				snprintf(error_buf, error_buf_size,
					"Invalid port: `%s`", port->str);
			}

			g_string_free(port, TRUE);
			goto error;
		}

		g_string_free(port, TRUE);

		if (parts.port < 0 || parts.port >= 65536) {
			if (error_buf) {
				snprintf(error_buf, error_buf_size,
					"Invalid port: %d", parts.port);
			}

			goto error;
		}
	}

	if (at[end_pos] == '\0') {
		goto end;
	}

	at += end_pos;

	/* /host/ */
	if (strncmp(at, "/host/", 6) != 0) {
		if (error_buf) {
			snprintf(error_buf, error_buf_size,
				"Expecting `/host/` after hostname or port");
		}

		goto error;
	}

	at += 6;

	/* Target hostname */
	parts.target_hostname = bt_common_string_until(at, "", "/", &end_pos);
	if (!parts.target_hostname || parts.target_hostname->len == 0) {
		if (error_buf) {
			snprintf(error_buf, error_buf_size,
				"Missing target hostname");
		}

		goto error;
	}

	if (at[end_pos] == '\0') {
		goto end;
	}

	at += end_pos + 1;

	/* Session name */
	parts.session_name = bt_common_string_until(at, "", "/", &end_pos);
	if (!parts.session_name || parts.session_name->len == 0) {
		if (error_buf) {
			snprintf(error_buf, error_buf_size,
				"Missing session name");
		}

		goto error;
	}

	if (at[end_pos] == '/') {
		if (error_buf) {
			snprintf(error_buf, error_buf_size,
				"Unexpected `/` after session name (`%s`)",
				parts.session_name->str);
		}

		goto error;
	}

	goto end;

error:
	bt_common_destroy_lttng_live_url_parts(&parts);

end:
	return parts;
}

BT_HIDDEN
void bt_common_normalize_star_glob_pattern(char *pattern)
{
	const char *p;
	char *np;
	bool got_star = false;

	BT_ASSERT(pattern);

	for (p = pattern, np = pattern; *p != '\0'; p++) {
		switch (*p) {
		case '*':
			if (got_star) {
				/* Avoid consecutive stars. */
				continue;
			}

			got_star = true;
			break;
		case '\\':
			/* Copy backslash character. */
			*np = *p;
			np++;
			p++;

			if (*p == '\0') {
				goto end;
			}

			/* Fall through default case. */
		default:
			got_star = false;
			break;
		}

		/* Copy single character. */
		*np = *p;
		np++;
	}

end:
	*np = '\0';
}

static inline
bool at_end_of_pattern(const char *p, const char *pattern, size_t pattern_len)
{
	return (p - pattern) == pattern_len || *p == '\0';
}

/*
 * Globbing matching function with the star feature only (`?` and
 * character sets are not supported). This matches `candidate` (plain
 * string) against `pattern`. A literal star can be escaped with `\` in
 * `pattern`.
 *
 * `pattern_len` or `candidate_len` can be greater than the actual
 * string length of `pattern` or `candidate` if the string is
 * null-terminated.
 */
BT_HIDDEN
bool bt_common_star_glob_match(const char *pattern, size_t pattern_len,
		const char *candidate, size_t candidate_len) {
	const char *retry_c = candidate, *retry_p = pattern, *c, *p;
	bool got_a_star = false;

retry:
	c = retry_c;
	p = retry_p;

	/*
	 * The concept here is to retry a match in the specific case
	 * where we already got a star. The retry position for the
	 * pattern is just after the most recent star, and the retry
	 * position for the candidate is the character following the
	 * last try's first character.
	 *
	 * Example:
	 *
	 *     candidate: hi ev every onyx one
	 *                ^
	 *     pattern:   hi*every*one
	 *                ^
	 *
	 *     candidate: hi ev every onyx one
	 *                 ^
	 *     pattern:   hi*every*one
	 *                 ^
	 *
	 *     candidate: hi ev every onyx one
	 *                  ^
	 *     pattern:   hi*every*one
	 *                  ^
	 *
	 *     candidate: hi ev every onyx one
	 *                  ^
	 *     pattern:   hi*every*one
	 *                   ^ MISMATCH
	 *
	 *     candidate: hi ev every onyx one
	 *                   ^
	 *     pattern:   hi*every*one
	 *                   ^
	 *
	 *     candidate: hi ev every onyx one
	 *                   ^^
	 *     pattern:   hi*every*one
	 *                   ^^
	 *
	 *     candidate: hi ev every onyx one
	 *                   ^ ^
	 *     pattern:   hi*every*one
	 *                   ^ ^ MISMATCH
	 *
	 *     candidate: hi ev every onyx one
	 *                    ^
	 *     pattern:   hi*every*one
	 *                   ^ MISMATCH
	 *
	 *     candidate: hi ev every onyx one
	 *                     ^
	 *     pattern:   hi*every*one
	 *                   ^ MISMATCH
	 *
	 *     candidate: hi ev every onyx one
	 *                      ^
	 *     pattern:   hi*every*one
	 *                   ^
	 *
	 *     candidate: hi ev every onyx one
	 *                      ^^
	 *     pattern:   hi*every*one
	 *                   ^^
	 *
	 *     candidate: hi ev every onyx one
	 *                      ^ ^
	 *     pattern:   hi*every*one
	 *                   ^ ^
	 *
	 *     candidate: hi ev every onyx one
	 *                      ^  ^
	 *     pattern:   hi*every*one
	 *                   ^  ^
	 *
	 *     candidate: hi ev every onyx one
	 *                      ^   ^
	 *     pattern:   hi*every*one
	 *                   ^   ^
	 *
	 *     candidate: hi ev every onyx one
	 *                           ^
	 *     pattern:   hi*every*one
	 *                        ^
	 *
	 *     candidate: hi ev every onyx one
	 *                           ^
	 *     pattern:   hi*every*one
	 *                         ^ MISMATCH
	 *
	 *     candidate: hi ev every onyx one
	 *                            ^
	 *     pattern:   hi*every*one
	 *                         ^
	 *
	 *     candidate: hi ev every onyx one
	 *                            ^^
	 *     pattern:   hi*every*one
	 *                         ^^
	 *
	 *     candidate: hi ev every onyx one
	 *                            ^ ^
	 *     pattern:   hi*every*one
	 *                         ^ ^ MISMATCH
	 *
	 *     candidate: hi ev every onyx one
	 *                             ^
	 *     pattern:   hi*every*one
	 *                         ^ MISMATCH
	 *
	 *     candidate: hi ev every onyx one
	 *                              ^
	 *     pattern:   hi*every*one
	 *                         ^ MISMATCH
	 *
	 *     candidate: hi ev every onyx one
	 *                               ^
	 *     pattern:   hi*every*one
	 *                         ^ MISMATCH
	 *
	 *     candidate: hi ev every onyx one
	 *                                ^
	 *     pattern:   hi*every*one
	 *                         ^ MISMATCH
	 *
	 *     candidate: hi ev every onyx one
	 *                                 ^
	 *     pattern:   hi*every*one
	 *                         ^
	 *
	 *     candidate: hi ev every onyx one
	 *                                 ^^
	 *     pattern:   hi*every*one
	 *                         ^^
	 *
	 *     candidate: hi ev every onyx one
	 *                                 ^ ^
	 *     pattern:   hi*every*one
	 *                         ^ ^
	 *
	 *     candidate: hi ev every onyx one
	 *                                 ^  ^
	 *     pattern:   hi*every*one
	 *                         ^  ^ SUCCESS
	 */
	while ((c - candidate) < candidate_len && *c != '\0') {
		BT_ASSERT(*c);

		if (at_end_of_pattern(p, pattern, pattern_len)) {
			goto end_of_pattern;
		}

		switch (*p) {
		case '*':
			got_a_star = true;

			/*
			 * Our first try starts at the current candidate
			 * character and after the star in the pattern.
			 */
			retry_c = c;
			retry_p = p + 1;

			if (at_end_of_pattern(retry_p, pattern, pattern_len)) {
				/*
				 * Star at the end of the pattern at
				 * this point: automatic match.
				 */
				return true;
			}

			goto retry;
		case '\\':
			/* Go to escaped character. */
			p++;

			/*
			 * Fall through the default case which compares
			 * the escaped character now.
			 */
		default:
			if (at_end_of_pattern(p, pattern, pattern_len) ||
					*c != *p) {
end_of_pattern:
				/* Character mismatch OR end of pattern. */
				if (!got_a_star) {
					/*
					 * We didn't get any star yet,
					 * so this first mismatch
					 * automatically makes the whole
					 * test fail.
					 */
					return false;
				}

				/*
				 * Next try: next candidate character,
				 * original pattern character (following
				 * the most recent star).
				 */
				retry_c++;
				goto retry;
			}
			break;
		}

		/* Next pattern and candidate characters. */
		c++;
		p++;
	}

	/*
	 * We checked every candidate character and we're still in a
	 * success state: the only pattern character allowed to remain
	 * is a star.
	 */
	if (at_end_of_pattern(p, pattern, pattern_len)) {
		return true;
	}

	p++;
	return p[-1] == '*' && at_end_of_pattern(p, pattern, pattern_len);
}

static
void append_path_parts(const char *path, GPtrArray *parts)
{
	const char *ch = path;
	const char *last = path;

	while (true) {
		if (*ch == G_DIR_SEPARATOR || *ch == '\0') {
			if (ch - last > 0) {
				GString *part = g_string_new(NULL);

				BT_ASSERT(part);
				g_string_append_len(part, last, ch - last);
				g_ptr_array_add(parts, part);
			}

			if (*ch == '\0') {
				break;
			}

			last = ch + 1;
		}

		ch++;
	}
}

static
void destroy_gstring(void *gstring)
{
	(void) g_string_free(gstring, TRUE);
}

#ifdef __MINGW32__
BT_HIDDEN
GString *bt_common_normalize_path(const char *path, const char *wd)
{
	char *tmp;
	GString *norm_path = NULL;

	BT_ASSERT(path);

	tmp = _fullpath(NULL, path, PATH_MAX);
	if (!tmp) {
		goto error;
	}

	norm_path = g_string_new(tmp);
	if (!norm_path) {
		goto error;
	}

	goto end;
error:
	if (norm_path) {
		g_string_free(norm_path, TRUE);
		norm_path = NULL;
	}
end:
	if (tmp) {
		free(tmp);
	}
	return norm_path;
}
#else
BT_HIDDEN
GString *bt_common_normalize_path(const char *path, const char *wd)
{
	size_t i;
	GString *norm_path;
	GPtrArray *parts = NULL;

	BT_ASSERT(path);
	norm_path = g_string_new(G_DIR_SEPARATOR_S);
	if (!norm_path) {
		goto error;
	}

	parts = g_ptr_array_new_with_free_func(destroy_gstring);
	if (!parts) {
		goto error;
	}

	if (path[0] != G_DIR_SEPARATOR) {
		/* Relative path: start with working directory */
		if (wd) {
			append_path_parts(wd, parts);
		} else {
			gchar *cd = g_get_current_dir();

			append_path_parts(cd, parts);
			g_free(cd);
		}
	}

	/* Append parts of the path parameter */
	append_path_parts(path, parts);

	/* Resolve special `..` and `.` parts */
	for (i = 0; i < parts->len; i++) {
		GString *part = g_ptr_array_index(parts, i);

		if (strcmp(part->str, "..") == 0) {
			if (i == 0) {
				/*
				 * First part of absolute path is `..`:
				 * this is invalid.
				 */
				goto error;
			}

			/* Remove `..` and previous part */
			g_ptr_array_remove_index(parts, i - 1);
			g_ptr_array_remove_index(parts, i - 1);
			i -= 2;
		} else if (strcmp(part->str, ".") == 0) {
			/* Remove `.` */
			g_ptr_array_remove_index(parts, i);
			i -= 1;
		}
	}

	/* Create normalized path with what's left */
	for (i = 0; i < parts->len; i++) {
		GString *part = g_ptr_array_index(parts, i);

		g_string_append(norm_path, part->str);

		if (i < parts->len - 1) {
			g_string_append_c(norm_path, G_DIR_SEPARATOR);
		}
	}

	goto end;

error:
	if (norm_path) {
		g_string_free(norm_path, TRUE);
		norm_path = NULL;
	}

end:
	if (parts) {
		g_ptr_array_free(parts, TRUE);
	}

	return norm_path;
}
#endif

BT_HIDDEN
size_t bt_common_get_page_size(void)
{
	int page_size;

	page_size = bt_sysconf(_SC_PAGESIZE);
	if (page_size < 0) {
		BT_LOGF("Cannot get system's page size: ret=%d",
			page_size);
		abort();
	}

	return page_size;
}

#define BUF_STD_APPEND(...)						\
	do {								\
		char _tmp_fmt[64];					\
		int _count;						\
		size_t _size = buf_size - (size_t) (*buf_ch - buf);	\
		size_t _tmp_fmt_size = (size_t) (fmt_ch - *out_fmt_ch);	\
		strncpy(_tmp_fmt, *out_fmt_ch, _tmp_fmt_size);		\
		_tmp_fmt[_tmp_fmt_size] = '\0';				\
		_count = snprintf(*buf_ch, _size, _tmp_fmt, __VA_ARGS__); \
		BT_ASSERT(_count >= 0);					\
		*buf_ch += MIN(_count, _size);				\
	} while (0)

#define BUF_STD_APPEND_SINGLE_ARG(_type)				\
	do {								\
		_type _arg = va_arg(*args, _type);			\
		BUF_STD_APPEND(_arg);					\
	} while (0)

static inline void handle_conversion_specifier_std(char *buf, char **buf_ch,
		size_t buf_size, const char **out_fmt_ch, va_list *args)
{
	const char *fmt_ch = *out_fmt_ch;
	enum LENGTH_MODIFIER {
		LENGTH_MOD_H,
		LENGTH_MOD_HH,
		LENGTH_MOD_NONE,
		LENGTH_MOD_LOW_L,
		LENGTH_MOD_LOW_LL,
		LENGTH_MOD_UP_L,
		LENGTH_MOD_Z,
	} length_mod = LENGTH_MOD_NONE;

	/* skip '%' */
	fmt_ch++;

	if (*fmt_ch == '%') {
		fmt_ch++;
		**buf_ch = '%';
		(*buf_ch)++;
		goto update_rw_fmt;
	}

	/* flags */
	for (;;) {
		switch (*fmt_ch) {
			case '-':
			case '+':
			case ' ':
			case '#':
			case '0':
			case '\'':
				fmt_ch++;
				continue;
			default:
				break;
		}
		break;
	}

	/* width */
	for (;;) {
		if (*fmt_ch < '0' || *fmt_ch > '9') {
			break;
		}

		fmt_ch++;
	}

	/* precision */
	if (*fmt_ch == '.') {
		fmt_ch++;

		for (;;) {
			if (*fmt_ch < '0' || *fmt_ch > '9') {
				break;
			}

			fmt_ch++;
		}
	}

	/* format (PRI*64) */
	if (strncmp(fmt_ch, PRId64, sizeof(PRId64)) == 0) {
		fmt_ch += sizeof(PRId64);
		BUF_STD_APPEND_SINGLE_ARG(int64_t);
		goto update_rw_fmt;
	} else if (strncmp(fmt_ch, PRIu64, sizeof(PRIu64)) == 0) {
		fmt_ch += sizeof(PRIu64);
		BUF_STD_APPEND_SINGLE_ARG(uint64_t);
		goto update_rw_fmt;
	} else if (strncmp(fmt_ch, PRIx64, sizeof(PRIx64)) == 0) {
		fmt_ch += sizeof(PRIx64);
		BUF_STD_APPEND_SINGLE_ARG(uint64_t);
		goto update_rw_fmt;
	} else if (strncmp(fmt_ch, PRIX64, sizeof(PRIX64)) == 0) {
		fmt_ch += sizeof(PRIX64);
		BUF_STD_APPEND_SINGLE_ARG(uint64_t);
		goto update_rw_fmt;
	} else if (strncmp(fmt_ch, PRIo64, sizeof(PRIo64)) == 0) {
		fmt_ch += sizeof(PRIo64);
		BUF_STD_APPEND_SINGLE_ARG(uint64_t);
		goto update_rw_fmt;
	} else if (strncmp(fmt_ch, PRIi64, sizeof(PRIi64)) == 0) {
		fmt_ch += sizeof(PRIi64);
		BUF_STD_APPEND_SINGLE_ARG(int64_t);
		goto update_rw_fmt;
	}

	// length modifier
	switch (*fmt_ch) {
		case 'h':
			length_mod = LENGTH_MOD_H;
			fmt_ch++;

			if (*fmt_ch == 'h') {
				length_mod = LENGTH_MOD_HH;
				fmt_ch++;
				break;
			}
			break;
		case 'l':
			length_mod = LENGTH_MOD_LOW_L;
			fmt_ch++;

			if (*fmt_ch == 'l') {
				length_mod = LENGTH_MOD_LOW_LL;
				fmt_ch++;
				break;
			}
			break;
		case 'L':
			length_mod = LENGTH_MOD_UP_L;
			fmt_ch++;
			break;
		case 'z':
			length_mod = LENGTH_MOD_Z;
			fmt_ch++;
			break;
		default:
			break;
	}

	// format
	switch (*fmt_ch) {
	case 'c':
	{
		fmt_ch++;

		switch (length_mod) {
		case LENGTH_MOD_NONE:
			BUF_STD_APPEND_SINGLE_ARG(int);
			break;
		case LENGTH_MOD_LOW_L:
			BUF_STD_APPEND_SINGLE_ARG(wint_t);
			break;
		default:
			abort();
		}
		break;
	}
	case 's':
		fmt_ch++;

		switch (length_mod) {
		case LENGTH_MOD_NONE:
			BUF_STD_APPEND_SINGLE_ARG(char *);
			break;
		case LENGTH_MOD_LOW_L:
			BUF_STD_APPEND_SINGLE_ARG(wchar_t *);
			break;
		default:
			abort();
		}
		break;
	case 'd':
	case 'i':
		fmt_ch++;

		switch (length_mod) {
		case LENGTH_MOD_NONE:
		case LENGTH_MOD_H:
		case LENGTH_MOD_HH:
			BUF_STD_APPEND_SINGLE_ARG(int);
			break;
		case LENGTH_MOD_LOW_L:
			BUF_STD_APPEND_SINGLE_ARG(long);
			break;
		case LENGTH_MOD_LOW_LL:
			BUF_STD_APPEND_SINGLE_ARG(long long);
			break;
		case LENGTH_MOD_Z:
			BUF_STD_APPEND_SINGLE_ARG(size_t);
			break;
		default:
			abort();
		}
		break;
	case 'o':
	case 'x':
	case 'X':
	case 'u':
		fmt_ch++;

		switch (length_mod) {
		case LENGTH_MOD_NONE:
		case LENGTH_MOD_H:
		case LENGTH_MOD_HH:
			BUF_STD_APPEND_SINGLE_ARG(unsigned int);
			break;
		case LENGTH_MOD_LOW_L:
			BUF_STD_APPEND_SINGLE_ARG(unsigned long);
			break;
		case LENGTH_MOD_LOW_LL:
			BUF_STD_APPEND_SINGLE_ARG(unsigned long long);
			break;
		case LENGTH_MOD_Z:
			BUF_STD_APPEND_SINGLE_ARG(size_t);
			break;
		default:
			abort();
		}
		break;
	case 'f':
	case 'F':
	case 'e':
	case 'E':
	case 'g':
	case 'G':
		fmt_ch++;

		switch (length_mod) {
		case LENGTH_MOD_NONE:
			BUF_STD_APPEND_SINGLE_ARG(double);
			break;
		case LENGTH_MOD_UP_L:
			BUF_STD_APPEND_SINGLE_ARG(long double);
			break;
		default:
			abort();
		}
		break;
	case 'p':
		fmt_ch++;

		if (length_mod == LENGTH_MOD_NONE) {
			BUF_STD_APPEND_SINGLE_ARG(void *);
		} else {
			abort();
		}
		break;
	default:
		abort();
	}

update_rw_fmt:
	*out_fmt_ch = fmt_ch;
}

BT_HIDDEN
void bt_common_custom_vsnprintf(char *buf, size_t buf_size,
		char intro,
		bt_common_handle_custom_specifier_func handle_specifier,
		void *priv_data, const char *fmt, va_list *args)
{
	const char *fmt_ch = fmt;
	char *buf_ch = buf;

	BT_ASSERT(buf);
	BT_ASSERT(fmt);
	BT_ASSERT(*args);

	while (*fmt_ch != '\0') {
		switch (*fmt_ch) {
		case '%':
			BT_ASSERT(fmt_ch[1] != '\0');

			if (fmt_ch[1] == intro) {
				handle_specifier(priv_data, &buf_ch,
					buf_size - (size_t) (buf_ch - buf),
					&fmt_ch, args);
			} else {
				handle_conversion_specifier_std(buf, &buf_ch,
					buf_size, &fmt_ch, args);
			}

			if (buf_ch >= buf + buf_size - 1) {
				fmt_ch = "";
			}
			break;
		default:
			*buf_ch = *fmt_ch;
			buf_ch++;
			if (buf_ch >= buf + buf_size - 1) {
				fmt_ch = "";
			}

			fmt_ch++;
		}
	}

	*buf_ch = '\0';
}

BT_HIDDEN
void bt_common_custom_snprintf(char *buf, size_t buf_size,
		char intro,
		bt_common_handle_custom_specifier_func handle_specifier,
		void *priv_data, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	bt_common_custom_vsnprintf(buf, buf_size, intro, handle_specifier,
		priv_data, fmt, &args);
	va_end(args);
}
