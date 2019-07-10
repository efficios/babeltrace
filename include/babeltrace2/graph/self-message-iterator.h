#ifndef BABELTRACE2_GRAPH_SELF_MESSAGE_ITERATOR_H
#define BABELTRACE2_GRAPH_SELF_MESSAGE_ITERATOR_H

/*
 * Copyright (c) 2010-2019 EfficiOS Inc. and Linux Foundation
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

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bt_self_component *
bt_self_message_iterator_borrow_component(
		bt_self_message_iterator *message_iterator);

extern bt_self_port_output *
bt_self_message_iterator_borrow_port(
		bt_self_message_iterator *message_iterator);

extern void bt_self_message_iterator_set_data(
		bt_self_message_iterator *message_iterator,
		void *user_data);

extern void *bt_self_message_iterator_get_data(
		const bt_self_message_iterator *message_iterator);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_SELF_MESSAGE_ITERATOR_H */
