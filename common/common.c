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
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>
#include <glib.h>
#include <stdlib.h>
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

	assert(dirs);
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

BT_HIDDEN
bool bt_common_colors_supported(void)
{
	static bool supports_colors = false;
	static bool supports_colors_set = false;
	const char *term;
	const char *force;

	if (supports_colors_set) {
		goto end;
	}

	supports_colors_set = true;

	force = getenv("BABELTRACE_FORCE_COLORS");
	if (force && strcmp(force, "1") == 0) {
		supports_colors = true;
		goto end;
	}

	term = getenv("TERM");
	if (!term) {
		goto end;
	}

	if (strncmp(term, "xterm", 5) != 0 &&
			strncmp(term, "rxvt", 4) != 0 &&
			strncmp(term, "konsole", 7) != 0 &&
			strncmp(term, "gnome", 5) != 0 &&
			strncmp(term, "screen", 5) != 0 &&
			strncmp(term, "tmux", 4) != 0 &&
			strncmp(term, "putty", 5) != 0) {
		goto end;
	}

	if (!isatty(1)) {
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
	assert(input);

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

	assert(url);
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

	assert(pattern);

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
		assert(*c);

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

				assert(part);
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
	GString *norm_path;
	char *tmp;

	assert(path);

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

	assert(path);
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
