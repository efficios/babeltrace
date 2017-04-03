/*
 * default-cfg.c
 *
 * Babeltrace Trace Converter - Default Configuration
 *
 * Copyright 2016 - Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/values.h>
#include "babeltrace-cfg.h"
#include "default-cfg.h"
#include "config.h"

#ifdef BT_SET_DEFAULT_IN_TREE_CONFIGURATION

struct bt_config *bt_config_from_args_with_defaults(int argc,
		const char *argv[], int *retcode)
{
	struct bt_value *initial_plugin_paths;
	struct bt_config *cfg = NULL;
	int ret;

	initial_plugin_paths = bt_value_array_create();
	if (!initial_plugin_paths) {
		goto error;
	}

	ret = bt_config_append_plugin_paths(initial_plugin_paths,
		CONFIG_IN_TREE_PLUGIN_PATH);
	if (ret) {
		goto error;
	}

	cfg = bt_config_from_args(argc, argv, retcode, true, true,
		initial_plugin_paths);
	goto end;

error:
	*retcode = 1;
	BT_PUT(cfg);

end:
	bt_put(initial_plugin_paths);
	return cfg;
}

#else /* BT_SET_DEFAULT_IN_TREE_CONFIGURATION */

struct bt_config *bt_config_from_args_with_defaults(int argc,
		const char *argv[], int *retcode)
{
	return bt_config_from_args(argc, argv, retcode, false, false, NULL);
}

#endif /* BT_SET_DEFAULT_IN_TREE_CONFIGURATION */
