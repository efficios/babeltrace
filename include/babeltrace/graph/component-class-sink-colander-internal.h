#ifndef BABELTRACE_GRAPH_COMPONENT_CLASS_SINK_COLANDER_H
#define BABELTRACE_GRAPH_COMPONENT_CLASS_SINK_COLANDER_H

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

#include <stdint.h>
#include <babeltrace/types.h>
#include <babeltrace/graph/message-const.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_component_class_sink_colander_priv_data {
	bt_message_array_const msgs;
	uint64_t *count_addr;
	struct bt_self_component_port_input_message_iterator *msg_iter;
};

struct bt_component_class_sink_colander_data {
	bt_message_array_const msgs;
	uint64_t *count_addr;
};

extern struct bt_component_class_sink *
bt_component_class_sink_colander_get(void);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_COMPONENT_CLASS_SINK_COLANDER_H */
