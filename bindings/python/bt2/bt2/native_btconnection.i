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
struct bt_connection;
struct bt_private_connection;

/* Status */
enum bt_connection_status {
	BT_CONNECTION_STATUS_GRAPH_IS_CANCELED = 125,
	BT_CONNECTION_STATUS_OK = 0,
	BT_CONNECTION_STATUS_INVALID = -22,
	BT_CONNECTION_STATUS_ERROR = -1,
	BT_CONNECTION_STATUS_NOMEM = -12,
	BT_CONNECTION_STATUS_IS_ENDED = 104,
};

/* Functions (public) */
struct bt_port *bt_connection_get_downstream_port(
		struct bt_connection *connection);
struct bt_port *bt_connection_get_upstream_port(
		struct bt_connection *connection);
int bt_connection_is_ended(struct bt_connection *connection);

/* Functions (private) */
struct bt_connection *bt_connection_from_private(
		struct bt_private_connection *private_connection);

/* Helper functions for Python */
%typemap(out) struct bt_py3_create_notif_iter_ret {
	$result = PyTuple_New(2);
	PyObject *py_notif_iter_ptr = SWIG_NewPointerObj(
		SWIG_as_voidptr($1.notif_iter),
		SWIGTYPE_p_bt_notification_iterator, 0);
	PyObject *py_status = SWIG_From_long_SS_long($1.status);
	PyTuple_SET_ITEM($result, 0, py_status);
	PyTuple_SET_ITEM($result, 1, py_notif_iter_ptr);
}

%{
struct bt_py3_create_notif_iter_ret {
	enum bt_connection_status status;
	struct bt_notification_iterator *notif_iter;
};

static struct bt_py3_create_notif_iter_ret bt_py3_create_priv_conn_notif_iter(
		unsigned long long priv_conn_addr, PyObject *py_notif_types)
{
	struct bt_private_connection *priv_conn;
	enum bt_notification_type *notification_types = NULL;
	struct bt_py3_create_notif_iter_ret ret;

	priv_conn = (void *) priv_conn_addr;
	BT_ASSERT(!PyErr_Occurred());
	BT_ASSERT(priv_conn);

	notification_types = bt_py3_notif_types_from_py_list(py_notif_types);
	ret.status = bt_private_connection_create_notification_iterator(
		priv_conn, notification_types, &ret.notif_iter);
	g_free(notification_types);

	return ret;
}
%}

struct bt_py3_create_notif_iter_ret bt_py3_create_priv_conn_notif_iter(
		unsigned long long priv_conn_addr, PyObject *notif_types);
