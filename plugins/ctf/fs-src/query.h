#ifndef BABELTRACE_PLUGIN_CTF_FS_QUERY_H
#define BABELTRACE_PLUGIN_CTF_FS_QUERY_H

/*
 * BabelTrace - CTF on File System Component
 *
 * Copyright 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/babeltrace.h>

BT_HIDDEN
struct bt_component_class_query_method_return metadata_info_query(
		struct bt_component_class *comp_class,
		struct bt_value *params);

BT_HIDDEN
struct bt_component_class_query_method_return trace_info_query(
		struct bt_component_class *comp_class,
		struct bt_value *params);

#endif /* BABELTRACE_PLUGIN_CTF_FS_QUERY_H */
