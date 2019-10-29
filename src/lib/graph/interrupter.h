#ifndef BABELTRACE_GRAPH_INTERRUPTER_INTERNAL_H
#define BABELTRACE_GRAPH_INTERRUPTER_INTERNAL_H

/*
 * Copyright (c) 2019 Philippe Proulx <pproulx@efficios.com>
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

#include <glib.h>
#include <babeltrace2/babeltrace.h>

#include "lib/object.h"

struct bt_interrupter {
	struct bt_object base;
	bool is_set;
};

static inline
bool bt_interrupter_array_any_is_set(const GPtrArray *interrupters)
{
	bool is_set = false;
	uint64_t i;

	BT_ASSERT_DBG(interrupters);

	for (i = 0; i < interrupters->len; i++) {
		const struct bt_interrupter *intr = interrupters->pdata[i];

		if (intr->is_set) {
			is_set = true;
			goto end;
		}
	}

end:
	return is_set;
}

#endif /* BABELTRACE_GRAPH_INTERRUPTER_INTERNAL_H */
