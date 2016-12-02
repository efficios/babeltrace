/*
 * iterator.c
 *
 * Babeltrace Trace Trimmer Iterator
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
#include "iterator.h"
#include <babeltrace/plugin/notification/iterator.h>

BT_HIDDEN
enum bt_component_status trimmer_iterator_init(struct bt_component *component,
		struct bt_notification_iterator *iterator)
{
	enum bt_component_status ret;
	enum bt_notification_iterator_status it_ret;

	it_ret = bt_notification_iterator_set_next_cb(iterator,
			trimmer_iterator_next);
	if (it_ret) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	it_ret = bt_notification_iterator_set_get_cb(iterator,
			trimmer_iterator_get);
	if (it_ret) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	it_ret = bt_notification_iterator_set_seek_time_cb(iterator,
			trimmer_iterator_seek_time);
	if (it_ret) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
end:
	return ret;
}

BT_HIDDEN
struct bt_notification *trimmer_iterator_get(
		struct bt_notification_iterator *iterator)
{
	return NULL;
}

BT_HIDDEN
enum bt_notification_iterator_status trimmer_iterator_next(
		struct bt_notification_iterator *iterator)
{
	enum bt_notification_iterator_status ret;

	ret = BT_NOTIFICATION_ITERATOR_STATUS_OK;
end:
	return ret;
}

BT_HIDDEN
enum bt_notification_iterator_status trimmer_iterator_seek_time(
		struct bt_notification_iterator *iterator, int64_t time)
{
	enum bt_notification_iterator_status ret;

	ret = BT_NOTIFICATION_ITERATOR_STATUS_OK;
end:
	return ret;
}
