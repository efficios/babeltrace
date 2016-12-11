#ifndef BABELTRACE_CONVERTER_CFG_H
#define BABELTRACE_CONVERTER_CFG_H

/*
 * Babeltrace trace converter - configuration
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

#include <stdlib.h>
#include <stdint.h>
#include <babeltrace/values.h>
#include <babeltrace/ref.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/compiler.h>
#include <glib.h>

struct bt_config_component {
	struct bt_object base;
	GString *plugin_name;
	GString *component_name;
	struct bt_value *params;
	struct {
		bool set;
		int64_t value_ns;
	} begin;
	struct {
		bool set;
		int64_t value_ns;
	} end;
};

struct bt_config {
	struct bt_object base;
	struct bt_value *plugin_paths;

	/* Array of pointers to struct bt_config_component */
	GPtrArray *sources;

	/* Array of pointers to struct bt_config_component */
	GPtrArray *sinks;

	bool debug;
	bool verbose;
	bool do_list;
	bool force_correlate;
};

static inline
struct bt_config_component *bt_config_get_component(GPtrArray *array,
		size_t index)
{
	return bt_get(g_ptr_array_index(array, index));
}

struct bt_config *bt_config_from_args(int argc, char *argv[], int *exit_code);

#endif /* BABELTRACE_CONVERTER_CFG_H */
