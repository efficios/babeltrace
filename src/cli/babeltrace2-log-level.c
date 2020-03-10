/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
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
