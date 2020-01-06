#ifndef CLI_BABELTRACE_PLUGINS_H
#define CLI_BABELTRACE_PLUGINS_H

/*
 * Babeltrace trace converter - CLI tool's configuration
 *
 * Copyright 2016-2019 EfficiOS Inc.
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

#include <babeltrace2/babeltrace.h>
#include "common/macros.h"

BT_HIDDEN void init_loaded_plugins(void);
BT_HIDDEN void fini_loaded_plugins(void);

BT_HIDDEN int require_loaded_plugins(const bt_value *plugin_paths);

BT_HIDDEN size_t get_loaded_plugins_count(void);
BT_HIDDEN const bt_plugin **borrow_loaded_plugins(void);
BT_HIDDEN const bt_plugin *borrow_loaded_plugin_by_index(size_t index);
BT_HIDDEN const bt_plugin *borrow_loaded_plugin_by_name(const char *name);


#endif /* CLI_BABELTRACE_PLUGINS_H */
