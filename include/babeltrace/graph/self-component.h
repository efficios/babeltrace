#ifndef BABELTRACE_GRAPH_SELF_COMPONENT_H
#define BABELTRACE_GRAPH_SELF_COMPONENT_H

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

/* For bt_component, bt_self_component */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

enum bt_self_component_status {
	BT_SELF_COMPONENT_STATUS_OK = 0,
	BT_SELF_COMPONENT_STATUS_END = 1,
	BT_SELF_COMPONENT_STATUS_AGAIN = 11,
	BT_SELF_COMPONENT_STATUS_REFUSE_PORT_CONNECTION = 111,
	BT_SELF_COMPONENT_STATUS_ERROR = -1,
	BT_SELF_COMPONENT_STATUS_NOMEM = -12,
};

static inline
const bt_component *bt_self_component_as_component(
		bt_self_component *self_component)
{
	return (const void *) self_component;
}

extern void *bt_self_component_get_data(
		const bt_self_component *private_component);

extern void bt_self_component_set_data(
		bt_self_component *private_component, void *data);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_SELF_COMPONENT_H */
