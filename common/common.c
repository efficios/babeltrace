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

#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>
#include <glib.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/common-internal.h>

#define SYSTEM_PLUGIN_PATH	INSTALL_LIBDIR "/babeltrace/plugins"
#define HOME_ENV_VAR		"HOME"
#define HOME_PLUGIN_SUBPATH	"/.local/lib/babeltrace/plugins"

BT_HIDDEN
const char *bt_common_get_system_plugin_path(void)
{
	return SYSTEM_PLUGIN_PATH;
}

BT_HIDDEN
bool bt_common_is_setuid_setgid(void)
{
	return (geteuid() != getuid() || getegid() != getgid());
}

static char *bt_secure_getenv(const char *name)
{
	if (bt_common_is_setuid_setgid()) {
		printf_error("Disregarding %s environment variable for setuid/setgid binary",
			name);
		return NULL;
	}
	return getenv(name);
}

static const char *get_home_dir(void)
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

BT_HIDDEN
char *bt_common_get_home_plugin_path(void)
{
	char *path = NULL;
	const char *home_dir;

	home_dir = get_home_dir();
	if (!home_dir) {
		goto end;
	}

	if (strlen(home_dir) + strlen(HOME_PLUGIN_SUBPATH) + 1 >= PATH_MAX) {
		printf_error("Home directory path is too long: `%s`\n",
			home_dir);
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
		const char *next_colon;

		next_colon = strchr(at, ':');
		if (next_colon == at) {
			/*
			 * Empty path: try next character (supported
			 * to conform to the typical parsing of $PATH).
			 */
			at++;
			continue;
		} else if (!next_colon) {
			/* No more colon: use the remaining */
			next_colon = paths + strlen(paths);
		}

		path = g_string_new(NULL);
		if (!path) {
			goto error;
		}

		g_string_append_len(path, at, next_colon - at);
		at = next_colon + 1;
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

	if (supports_colors_set) {
		goto end;
	}

	supports_colors_set = true;

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
	return bt_common_colors_supported() ? BT_COMMON_COLOR_RESET : "";
}

BT_HIDDEN
const char *bt_common_color_bold(void)
{
	return bt_common_colors_supported() ? BT_COMMON_COLOR_BOLD : "";
}

BT_HIDDEN
const char *bt_common_color_fg_default(void)
{
	return bt_common_colors_supported() ? BT_COMMON_COLOR_FG_DEFAULT : "";
}

BT_HIDDEN
const char *bt_common_color_fg_red(void)
{
	return bt_common_colors_supported() ? BT_COMMON_COLOR_FG_RED : "";
}

BT_HIDDEN
const char *bt_common_color_fg_green(void)
{
	return bt_common_colors_supported() ? BT_COMMON_COLOR_FG_GREEN : "";
}

BT_HIDDEN
const char *bt_common_color_fg_yellow(void)
{
	return bt_common_colors_supported() ? BT_COMMON_COLOR_FG_YELLOW : "";
}

BT_HIDDEN
const char *bt_common_color_fg_blue(void)
{
	return bt_common_colors_supported() ? BT_COMMON_COLOR_FG_BLUE : "";
}

BT_HIDDEN
const char *bt_common_color_fg_magenta(void)
{
	return bt_common_colors_supported() ? BT_COMMON_COLOR_FG_MAGENTA : "";
}

BT_HIDDEN
const char *bt_common_color_fg_cyan(void)
{
	return bt_common_colors_supported() ? BT_COMMON_COLOR_FG_CYAN : "";
}

BT_HIDDEN
const char *bt_common_color_fg_light_gray(void)
{
	return bt_common_colors_supported() ?
		BT_COMMON_COLOR_FG_LIGHT_GRAY : "";
}

BT_HIDDEN
const char *bt_common_color_bg_default(void)
{
	return bt_common_colors_supported() ? BT_COMMON_COLOR_BG_DEFAULT : "";
}

BT_HIDDEN
const char *bt_common_color_bg_red(void)
{
	return bt_common_colors_supported() ? BT_COMMON_COLOR_BG_RED : "";
}

BT_HIDDEN
const char *bt_common_color_bg_green(void)
{
	return bt_common_colors_supported() ? BT_COMMON_COLOR_BG_GREEN : "";
}

BT_HIDDEN
const char *bt_common_color_bg_yellow(void)
{
	return bt_common_colors_supported() ? BT_COMMON_COLOR_BG_YELLOW : "";
}

BT_HIDDEN
const char *bt_common_color_bg_blue(void)
{
	return bt_common_colors_supported() ? BT_COMMON_COLOR_BG_BLUE : "";
}

BT_HIDDEN
const char *bt_common_color_bg_magenta(void)
{
	return bt_common_colors_supported() ? BT_COMMON_COLOR_BG_MAGENTA : "";
}

BT_HIDDEN
const char *bt_common_color_bg_cyan(void)
{
	return bt_common_colors_supported() ? BT_COMMON_COLOR_BG_CYAN : "";
}

BT_HIDDEN
const char *bt_common_color_bg_light_gray(void)
{
	return bt_common_colors_supported() ?
		BT_COMMON_COLOR_BG_LIGHT_GRAY : "";
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
