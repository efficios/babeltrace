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

#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <assert.h>
#include <glib.h>
#include <babeltrace/babeltrace-internal.h>

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
