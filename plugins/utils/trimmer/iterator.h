#ifndef BABELTRACE_PLUGIN_TRIMMER_ITERATOR_H
#define BABELTRACE_PLUGIN_TRIMMER_ITERATOR_H

/*
 * BabelTrace - Trace Trimmer Iterator
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

#include "trimmer.h"
#include <babeltrace/babeltrace.h>

struct trimmer_iterator {
	/* Input iterator associated with this output iterator. */
	struct bt_notification_iterator *input_iterator;
	struct bt_notification *current_notification;
	FILE *err;
	/* Map between reader and writer packets. */
	GHashTable *packet_map;
};

BT_HIDDEN
enum bt_notification_iterator_status trimmer_iterator_init(
		struct bt_self_notification_iterator *iterator,
		struct bt_private_port *port);

BT_HIDDEN
void trimmer_iterator_finalize(struct bt_self_notification_iterator *it);

BT_HIDDEN
struct bt_notification_iterator_next_method_return trimmer_iterator_next(
		struct bt_self_notification_iterator *iterator);

#endif /* BABELTRACE_PLUGIN_TRIMMER_ITERATOR_H */
