#ifndef BABELTRACE_GRAPH_MESSAGE_ITERATOR_CLASS_INTERNAL_H
#define BABELTRACE_GRAPH_MESSAGE_ITERATOR_CLASS_INTERNAL_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace2/graph/message-iterator-class.h>
#include <babeltrace2/types.h>
#include "common/macros.h"
#include "lib/object.h"
#include <stdbool.h>
#include <glib.h>

struct bt_message_iterator_class {
	struct bt_object base;
	bool frozen;

	struct {
		bt_message_iterator_class_initialize_method initialize;
		bt_message_iterator_class_finalize_method finalize;
		bt_message_iterator_class_next_method next;
		bt_message_iterator_class_seek_ns_from_origin_method seek_ns_from_origin;
		bt_message_iterator_class_seek_beginning_method seek_beginning;
		bt_message_iterator_class_can_seek_ns_from_origin_method can_seek_ns_from_origin;
		bt_message_iterator_class_can_seek_beginning_method can_seek_beginning;
	} methods;
};

BT_HIDDEN
void _bt_message_iterator_class_freeze(
		const struct bt_message_iterator_class *message_iterator_class);

#ifdef BT_DEV_MODE
# define bt_message_iterator_class_freeze	_bt_message_iterator_class_freeze
#else
# define bt_message_iterator_class_freeze(_cls)
#endif

#endif /* BABELTRACE_GRAPH_MESSAGE_ITERATOR_CLASS_INTERNAL_H */
