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

/* Type */
struct bt_notification_iterator;

/* Status */
enum bt_notification_iterator_status {
	BT_NOTIFICATION_ITERATOR_STATUS_CANCELED = 125,
	BT_NOTIFICATION_ITERATOR_STATUS_AGAIN = 11,
	BT_NOTIFICATION_ITERATOR_STATUS_END = 1,
	BT_NOTIFICATION_ITERATOR_STATUS_OK = 0,
	BT_NOTIFICATION_ITERATOR_STATUS_INVALID = -22,
	BT_NOTIFICATION_ITERATOR_STATUS_ERROR = -1,
	BT_NOTIFICATION_ITERATOR_STATUS_NOMEM = -12,
	BT_NOTIFICATION_ITERATOR_STATUS_UNSUPPORTED = -2,
};

/* Functions (base) */
struct bt_notification *bt_notification_iterator_get_notification(
		struct bt_notification_iterator *iterator);
enum bt_notification_iterator_status bt_notification_iterator_next(
		struct bt_notification_iterator *iterator);

/* Functions (private connection) */
struct bt_component *bt_private_connection_notification_iterator_get_component(
		struct bt_notification_iterator *iterator);

/* Functions (output port) */
struct bt_notification_iterator *bt_output_port_notification_iterator_create(
		struct bt_port *port, const char *colander_component_name,
		const enum bt_notification_type *notification_types);

/* Helper functions for Python */
%{
static PyObject *bt_py3_get_user_component_from_user_notif_iter(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter)
{
	struct bt_private_component *priv_comp =
		bt_private_connection_private_notification_iterator_get_private_component(
			priv_notif_iter);
	PyObject *py_comp;

	BT_ASSERT(priv_comp);
	py_comp = bt_private_component_get_user_data(priv_comp);
	bt_put(priv_comp);
	BT_ASSERT(py_comp);

	/* Return new reference */
	Py_INCREF(py_comp);
	return py_comp;
}
%}

PyObject *bt_py3_get_user_component_from_user_notif_iter(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter);
