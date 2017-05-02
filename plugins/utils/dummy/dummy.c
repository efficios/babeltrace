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

#include <babeltrace/plugin/plugin-dev.h>
#include <babeltrace/graph/component.h>
#include <babeltrace/graph/private-component.h>
#include <babeltrace/graph/private-component-sink.h>
#include <babeltrace/graph/private-port.h>
#include <babeltrace/graph/port.h>
#include <babeltrace/graph/private-connection.h>
#include <babeltrace/graph/component-sink.h>
#include <babeltrace/graph/notification-iterator.h>
#include <babeltrace/graph/notification.h>
#include <babeltrace/babeltrace-internal.h>
#include <plugins-common.h>
#include <assert.h>
#include "dummy.h"

static
void destroy_private_dummy_data(struct dummy *dummy)
{
	if (dummy->iterators) {
		g_ptr_array_free(dummy->iterators, TRUE);
	}
	g_free(dummy);
}

void dummy_finalize(struct bt_private_component *component)
{
	struct dummy *dummy;

	assert(component);
	dummy = bt_private_component_get_user_data(component);
	assert(dummy);
	destroy_private_dummy_data(dummy);
}

enum bt_component_status dummy_init(struct bt_private_component *component,
		struct bt_value *params, UNUSED_VAR void *init_method_data)
{
	enum bt_component_status ret;
	struct dummy *dummy = g_new0(struct dummy, 1);
	void *priv_port;

	if (!dummy) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	priv_port = bt_private_component_sink_add_input_private_port(component,
		"in", NULL);
	if (!priv_port) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	bt_put(priv_port);
	dummy->iterators = g_ptr_array_new_with_free_func(
			(GDestroyNotify) bt_put);
	if (!dummy->iterators) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	ret = bt_private_component_set_user_data(component, dummy);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}
end:
	return ret;
error:
	destroy_private_dummy_data(dummy);
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

	dummy = bt_private_component_get_user_data(component);
	assert(dummy);
	connection = bt_private_port_get_private_connection(self_port);
	assert(connection);
	iterator = bt_private_connection_create_notification_iterator(
		connection, NULL);
	if (!iterator) {
		dummy->error = true;
		goto end;
	}

	g_ptr_array_add(dummy->iterators, iterator);

end:
	bt_put(connection);
}

enum bt_component_status dummy_consume(struct bt_private_component *component)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_notification *notif = NULL;
	size_t i;
	struct dummy *dummy;

	dummy = bt_private_component_get_user_data(component);
	assert(dummy);

	if (unlikely(dummy->error)) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	/* Consume one notification from each iterator. */
	for (i = 0; i < dummy->iterators->len; i++) {
		struct bt_notification_iterator *it;
		enum bt_notification_iterator_status it_ret;

		it = g_ptr_array_index(dummy->iterators, i);

		it_ret = bt_notification_iterator_next(it);
		switch (it_ret) {
		case BT_NOTIFICATION_ITERATOR_STATUS_ERROR:
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		case BT_NOTIFICATION_ITERATOR_STATUS_AGAIN:
			ret = BT_COMPONENT_STATUS_AGAIN;
			goto end;
		case BT_NOTIFICATION_ITERATOR_STATUS_END:
			g_ptr_array_remove_index(dummy->iterators, i);
			i--;
			continue;
		default:
			break;
		}
	}

	if (dummy->iterators->len == 0) {
		ret = BT_COMPONENT_STATUS_END;
	}
end:
	bt_put(notif);
	return ret;
}
