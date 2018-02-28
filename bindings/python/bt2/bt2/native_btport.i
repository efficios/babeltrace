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
struct bt_port;
struct bt_private_port;

/* Status */
enum bt_port_status {
	BT_PORT_STATUS_OK = 0,
	BT_PORT_STATUS_ERROR = -1,
	BT_PORT_STATUS_INVALID = -2,
};

/* Port type */
enum bt_port_type {
	BT_PORT_TYPE_INPUT = 0,
	BT_PORT_TYPE_OUTPUT = 1,
	BT_PORT_TYPE_UNKOWN = -1,
};

/* Functions (public) */
const char *bt_port_get_name(struct bt_port *port);
enum bt_port_type bt_port_get_type(struct bt_port *port);
struct bt_connection *bt_port_get_connection(struct bt_port *port);
struct bt_component *bt_port_get_component(struct bt_port *port);
enum bt_port_status bt_port_disconnect(struct bt_port *port);
int bt_port_is_connected(struct bt_port *port);

/* Functions (private) */
struct bt_port *bt_port_from_private(struct bt_private_port *private_port);
struct bt_private_connection *bt_private_port_get_private_connection(
		struct bt_private_port *private_port);
struct bt_private_component *bt_private_port_get_private_component(
		struct bt_private_port *private_port);
enum bt_port_status bt_private_port_remove_from_component(
		struct bt_private_port *private_port);
void *bt_private_port_get_user_data(
		struct bt_private_port *private_port);

%{
static struct bt_notification_iterator *bt_py3_create_output_port_notif_iter(
		unsigned long long port_addr, const char *colander_name,
		PyObject *py_notif_types)
{
	struct bt_notification_iterator *notif_iter;
	struct bt_port *output_port;
	enum bt_notification_type *notification_types;

	output_port = (void *) port_addr;
	BT_ASSERT(!PyErr_Occurred());
	BT_ASSERT(output_port);

	notification_types = bt_py3_notif_types_from_py_list(py_notif_types);
	notif_iter = bt_output_port_notification_iterator_create(output_port,
		colander_name, notification_types);
	g_free(notification_types);
	return notif_iter;
}
%}

struct bt_notification_iterator *bt_py3_create_output_port_notif_iter(
		unsigned long long port_addr, const char *colander_name,
		PyObject *py_notif_types);
