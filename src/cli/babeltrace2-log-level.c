/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "CLI"
#include "logging.h"

#include "babeltrace2-log-level.h"

#include <stdlib.h>

#include "common/assert.h"

/*
 * Known environment variable names for the log levels of the project's
 * modules.
 */
static const char* log_level_env_var_names[] = {
	"BABELTRACE_PLUGIN_CTF_METADATA_LOG_LEVEL",
	"BABELTRACE_PYTHON_BT2_LOG_LEVEL",
	NULL,
};

static const int babeltrace2_default_log_level = BT_LOG_WARNING;

void set_auto_log_levels(int *logging_level)
{
	const char **env_var_name;

	/* Setting this is equivalent to passing --debug. */
	if (getenv("BABELTRACE_DEBUG") &&
			strcmp(getenv("BABELTRACE_DEBUG"), "1") == 0) {
		*logging_level = logging_level_min(*logging_level, BT_LOG_TRACE);
	}

	/* Setting this is equivalent to passing --verbose. */
	if (getenv("BABELTRACE_VERBOSE") &&
			strcmp(getenv("BABELTRACE_VERBOSE"), "1") == 0) {
		*logging_level = logging_level_min(*logging_level, BT_LOG_INFO);
	}

	/*
	 * logging_level is BT_LOG_NONE at this point if no log level was
	 * specified at all by the user.
	 */
	if (*logging_level == -1) {
		*logging_level = babeltrace2_default_log_level;
	}

	/*
	 * If the user hasn't requested a specific log level for the lib
	 * (through LIBBABELTRACE2_INIT_LOG_LEVEL), set it.
	 */
	if (!getenv("LIBBABELTRACE2_INIT_LOG_LEVEL")) {
		bt_logging_set_global_level(*logging_level);
	}

	/*
	 * If the user hasn't requested a specific log level for the CLI,
	 * (through BABELTRACE_CLI_LOG_LEVEL), set it.
	 */
	if (!getenv(ENV_BABELTRACE_CLI_LOG_LEVEL)) {
		bt_cli_log_level = *logging_level;
	}

	for (env_var_name = log_level_env_var_names; *env_var_name; env_var_name++) {
		if (!getenv(*env_var_name)) {
			char val[2] = { 0 };

			/*
			 * Set module's default log level if not
			 * explicitly specified.
			 */
			val[0] = bt_log_get_letter_from_level(*logging_level);
			g_setenv(*env_var_name, val, 1);
		}
	}
}
