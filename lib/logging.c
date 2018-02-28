/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#include <stdlib.h>
#include <babeltrace/logging.h>
#include <babeltrace/version.h>

#define BT_LOG_TAG "LIB"
#include <babeltrace/lib-logging-internal.h>

#ifdef BT_DEV_MODE
/*
 * Default log level is FATAL in developer mode because fatal logging is
 * our way to communicate an unsatisfied precondition and the details.
 */
# define DEFAULT_LOG_LEVEL	BT_LOG_FATAL
#else
/*
 * In non-developer mode, use NONE by default: we don't to print logging
 * statements for any executable which links with the library. The
 * executable should call bt_logging_set_global_level() or the
 * executable's user should set the BABELTRACE_LOGGING_GLOBAL_LEVEL
 * environment variable.
 */
# define DEFAULT_LOG_LEVEL	BT_LOG_NONE
#endif /* BT_DEV_MODE */

BT_HIDDEN
int bt_lib_log_level = DEFAULT_LOG_LEVEL;

enum bt_logging_level bt_logging_get_minimal_level(void)
{
	return BT_LOG_LEVEL;
}

enum bt_logging_level bt_logging_get_global_level(void)
{
	return bt_lib_log_level;
}

void bt_logging_set_global_level(enum bt_logging_level log_level)
{
#ifdef BT_DEV_MODE
	/*
	 * Do not allow the library's log level to fall to NONE when in
	 * developer mode because fatal logging is our way to
	 * communicate an unsatisfied precondition and the details.
	 */
	if (log_level == BT_LOG_NONE) {
		log_level = BT_LOG_FATAL;
	}
#endif

	bt_lib_log_level = log_level;
}

static
void __attribute__((constructor)) bt_logging_ctor(void)
{
	const char *v_extra = bt_version_get_extra() ? bt_version_get_extra() :
		"";

	bt_logging_set_global_level(
		bt_log_get_level_from_env("BABELTRACE_LOGGING_GLOBAL_LEVEL"));
	BT_LOGI("Babeltrace %d.%d.%d%s library loaded: "
		"major=%d, minor=%d, patch=%d, extra=\"%s\"",
		bt_version_get_major(), bt_version_get_minor(),
		bt_version_get_patch(), v_extra,
		bt_version_get_major(), bt_version_get_minor(),
		bt_version_get_patch(), v_extra);
}
