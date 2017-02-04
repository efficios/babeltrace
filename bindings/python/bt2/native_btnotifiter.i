/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

%{
#include <babeltrace/component/notification/iterator.h>
%}

/* Type */
struct bt_notification_iterator;

/* Status */
enum bt_notification_iterator_status {
	BT_NOTIFICATION_ITERATOR_STATUS_END = 1,
	BT_NOTIFICATION_ITERATOR_STATUS_OK = 0,
	BT_NOTIFICATION_ITERATOR_STATUS_INVAL = -1,
	BT_NOTIFICATION_ITERATOR_STATUS_ERROR = -2,
	BT_NOTIFICATION_ITERATOR_STATUS_NOMEM = -3,
	BT_NOTIFICATION_ITERATOR_STATUS_UNSUPPORTED = -4,
};

/* Seek reference */
enum bt_notification_iterator_seek_origin {
	BT_NOTIFICATION_ITERATOR_SEEK_ORIGIN_BEGIN = 0,
	BT_NOTIFICATION_ITERATOR_SEEK_ORIGIN_CURRENT = 1,
	BT_NOTIFICATION_ITERATOR_SEEK_ORIGIN_END = 2,
	BT_NOTIFICATION_ITERATOR_SEEK_ORIGIN_EPOCH = 3,
};

/* Functions */
struct bt_notification *bt_notification_iterator_get_notification(
		struct bt_notification_iterator *iterator);
enum bt_notification_iterator_status bt_notification_iterator_next(
		struct bt_notification_iterator *iterator);
enum bt_notification_iterator_status bt_notification_iterator_seek_time(
		struct bt_notification_iterator *iterator,
		enum bt_notification_iterator_seek_origin seek_origin,
		int64_t time);
struct bt_component *bt_notification_iterator_get_component(
		struct bt_notification_iterator *iterator);
enum bt_notification_iterator_status bt_notification_iterator_set_private_data(
		struct bt_notification_iterator *iterator, void *data);
void *bt_notification_iterator_get_private_data(
		struct bt_notification_iterator *iterator);

/* Helper functions for Python */
%{
static PyObject *bt_py3_get_component_from_notif_iter(
		struct bt_notification_iterator *iter)
{
	struct bt_component *component = bt_notification_iterator_get_component(iter);
	PyObject *py_comp;

	assert(component);
	py_comp = bt_component_get_private_data(component);
	BT_PUT(component);
	assert(py_comp);

	/* Return new reference */
	Py_INCREF(py_comp);
	return py_comp;
}
%}

PyObject *bt_py3_get_component_from_notif_iter(
		struct bt_notification_iterator *iter);
