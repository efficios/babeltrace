#ifndef BABELTRACE_PLUGINS_UTILS_TRIMMER_H
#define BABELTRACE_PLUGINS_UTILS_TRIMMER_H

/*
 * BabelTrace - Trace Trimmer Plug-in
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <stdbool.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/babeltrace.h>

#define NSEC_PER_SEC	1000000000LL

struct trimmer_bound {
	int64_t value;
	bool set;
	bool lazy;
	struct {
		int hh, mm, ss, ns;
		bool gmt;
	} lazy_values;
};

struct trimmer {
	struct trimmer_bound begin, end;
	bool date;
	int year, month, day;
};

enum bt_component_status trimmer_component_init(
	bt_self_component *component,
	bt_value *params, void *init_method_data);

void finalize_trimmer(bt_self_component *component);

#endif /* BABELTRACE_PLUGINS_UTILS_TRIMMER_H */
