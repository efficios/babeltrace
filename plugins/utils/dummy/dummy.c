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

#include <babeltrace/babeltrace.h>
#include <babeltrace/babeltrace-internal.h>
#include <plugins-common.h>
#include <babeltrace/assert-internal.h>
#include "dummy.h"

void destroy_private_dummy_data(struct dummy *dummy)
{
	bt_put(dummy->notif_iter);
	g_free(dummy);
}

void dummy_finalize(struct bt_private_component *component)
{
	struct dummy *dummy;

	BT_ASSERT(component);
	dummy = bt_private_component_get_user_data(component);
	BT_ASSERT(dummy);
	destroy_private_dummy_data(dummy);
}

enum bt_component_status dummy_init(struct bt_private_component *component,
		struct bt_value *params, UNUSED_VAR void *init_method_data)
{
	enum bt_component_status ret;
	struct dummy *dummy = g_new0(struct dummy, 1);

	if (!dummy) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	ret = bt_private_component_sink_add_input_private_port(component,
		"in", NULL, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	ret = bt_private_component_set_user_data(component, dummy);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	goto end;

error:
	destroy_private_dummy_data(dummy);

end:
	return ret;
}

void dummy_port_connected(
		struct bt_private_component *component,
		struct bt_private_port *self_port,
		struct bt_port *other_port)
{
	struct dummy *dummy;
	struct bt_notification_iterator *iterator;
	struct bt_private_connection *connection;
	enum bt_connection_status conn_status;

	dummy = bt_private_component_get_user_data(component);
	BT_ASSERT(dummy);
	connection = bt_private_port_get_private_connection(self_port);
	BT_ASSERT(connection);
	conn_status = bt_private_connection_create_notification_iterator(
		connection, NULL, &iterator);
	if (conn_status != BT_CONNECTION_STATUS_OK) {
		dummy->error = true;
		goto end;
	}

	BT_MOVE(dummy->notif_iter, iterator);

end:
	bt_put(connection);
}

enum bt_component_status dummy_consume(struct bt_private_component *component)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_notification *notif = NULL;
	struct dummy *dummy;
	enum bt_notification_iterator_status it_ret;

	dummy = bt_private_component_get_user_data(component);
	BT_ASSERT(dummy);

	if (unlikely(dummy->error)) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	if (unlikely(!dummy->notif_iter)) {
		ret = BT_COMPONENT_STATUS_END;
		goto end;
	}

	/* Consume one notification  */
	it_ret = bt_notification_iterator_next(dummy->notif_iter);
	switch (it_ret) {
	case BT_NOTIFICATION_ITERATOR_STATUS_ERROR:
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	case BT_NOTIFICATION_ITERATOR_STATUS_AGAIN:
		ret = BT_COMPONENT_STATUS_AGAIN;
		goto end;
	case BT_NOTIFICATION_ITERATOR_STATUS_END:
		ret = BT_COMPONENT_STATUS_END;
		goto end;
	default:
		break;
	}

end:
	bt_put(notif);
	return ret;
}
