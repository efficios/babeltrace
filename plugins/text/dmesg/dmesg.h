#ifndef BABELTRACE_PLUGIN_TEXT_DMESG_DMESG_H
#define BABELTRACE_PLUGIN_TEXT_DMESG_DMESG_H

/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

BT_HIDDEN
enum bt_self_component_status dmesg_init(
		bt_self_component_source *self_comp,
		const bt_value *params, void *init_method_data);

BT_HIDDEN
void dmesg_finalize(bt_self_component_source *self_comp);

BT_HIDDEN
enum bt_self_message_iterator_status dmesg_msg_iter_init(
		bt_self_message_iterator *self_msg_iter,
		bt_self_component_source *self_comp,
		bt_self_component_port_output *self_port);

BT_HIDDEN
void dmesg_msg_iter_finalize(
		bt_self_message_iterator *self_msg_iter);

BT_HIDDEN
enum bt_self_message_iterator_status dmesg_msg_iter_next(
		bt_self_message_iterator *self_msg_iter,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count);

#endif /* BABELTRACE_PLUGIN_TEXT_DMESG_DMESG_H */
