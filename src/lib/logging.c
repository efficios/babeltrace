/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
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
#include <babeltrace2/babeltrace.h>

#define BT_LOG_TAG "LIB/LOGGING"
#include "lib/logging.h"

/*
 * This is exported because even though the Python plugin provider is a
 * different shared object for packaging purposes, it's still considered
 * part of the library and therefore needs the library's run-time log
 * level.
 *
 * The default log level is NONE: we don't print logging statements for
 * any executable which links with the library. The executable must call
 * bt_logging_set_global_level() or the executable's user must set the
 * `LIBBABELTRACE2_INIT_LOG_LEVEL` environment variable to enable
 * logging.
 */
int bt_lib_log_level = BT_LOG_NONE;

enum bt_logging_level bt_logging_get_minimal_level(void)
{
	return BT_MINIMAL_LOG_LEVEL;
}

enum bt_logging_level bt_logging_get_global_level(void)
{
	return bt_lib_log_level;
}

void bt_logging_set_global_level(enum bt_logging_level log_level)
{
	bt_lib_log_level = log_level;
}

static
void __attribute__((constructor)) bt_logging_ctor(void)
{
	const char *v_extra = bt_version_get_development_stage() ?
		bt_version_get_development_stage() : "";

	bt_logging_set_global_level(
		bt_log_get_level_from_env("LIBBABELTRACE2_INIT_LOG_LEVEL"));
	BT_LOGI("Babeltrace %u.%u.%u%s library loaded: "
		"major=%u, minor=%u, patch=%u, extra=\"%s\"",
		bt_version_get_major(), bt_version_get_minor(),
		bt_version_get_patch(), v_extra,
		bt_version_get_major(), bt_version_get_minor(),
		bt_version_get_patch(), v_extra);
}
