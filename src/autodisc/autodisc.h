#ifndef AUTODISC_AUTODISC_H
#define AUTODISC_AUTODISC_H

/*
 * Copyright (c) 2019 EfficiOS Inc. and Linux Foundation
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

#include <glib.h>

#include <babeltrace2/babeltrace.h>

#define __BT_IN_BABELTRACE_H
#include <babeltrace2/func-status.h>
#undef __BT_IN_BABELTRACE_H

struct auto_source_discovery {
	/* Array of `struct auto_source_discovery_result *`. */
	GPtrArray *results;
};

/* Value type of the `auto_source_discovery::results` array. */

struct auto_source_discovery_result {
	/*
	 * `plugin_name` and `source_cc_name` are borrowed from the plugin and source
	 * component class (which outlive this structure).
	 */
	const char *plugin_name;
	const char *source_cc_name;

	/*
	 * `group` is owned by this structure.
	 *
	 * May be NULL, to mean "no group".
	 */
	gchar *group;

	/* Array of input strings. */
	bt_value *inputs;

	/*
	 * Array of integers: indices of the original inputs that contributed
	 * to this result.
	 */
	bt_value *original_input_indices;
};

typedef enum auto_source_discovery_status {
	AUTO_SOURCE_DISCOVERY_STATUS_OK			= __BT_FUNC_STATUS_OK,
	AUTO_SOURCE_DISCOVERY_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
	AUTO_SOURCE_DISCOVERY_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	AUTO_SOURCE_DISCOVERY_STATUS_INTERRUPTED	= __BT_FUNC_STATUS_INTERRUPTED,
} auto_source_discovery_status;

int auto_source_discovery_init(struct auto_source_discovery *auto_disc);
void auto_source_discovery_fini(struct auto_source_discovery *auto_disc);

/*
 * Given `inputs` a list of strings, query source component classes to discover
 * which source components should be instantiated to deal with these inputs.
 */

auto_source_discovery_status auto_discover_source_components(
		const bt_value *inputs,
		const bt_plugin **plugins,
		size_t plugin_count,
		const char *component_class_restrict,
		enum bt_logging_level log_level,
		struct auto_source_discovery *auto_disc,
		const bt_interrupter *interrupter);

#endif /* AUTODISC_AUTODISC_H */
