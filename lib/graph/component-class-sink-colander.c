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

#define BT_LOG_TAG "COLANDER"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/ref.h>
#include <babeltrace/graph/connection.h>
#include <babeltrace/graph/component-class-sink.h>
#include <babeltrace/graph/private-component-sink.h>
#include <babeltrace/graph/private-port.h>
#include <babeltrace/graph/private-connection.h>
#include <babeltrace/graph/private-component.h>
#include <babeltrace/graph/component-class-sink-colander-internal.h>
#include <glib.h>
#include <assert.h>

static
struct bt_component_class *colander_comp_cls;

struct colander_data {
	struct bt_notification **user_notif;
	enum bt_notification_type *notif_types;
	struct bt_notification_iterator *notif_iter;
};

static
enum bt_component_status colander_init(
		struct bt_private_component *priv_comp,
		struct bt_value *params, void *init_method_data)
{
	enum bt_component_status status = BT_COMPONENT_STATUS_OK;
	struct colander_data *colander_data = NULL;
	struct bt_component_class_sink_colander_data *user_provided_data =
		init_method_data;
	const enum bt_notification_type *notif_type;

	if (!init_method_data) {
		BT_LOGW_STR("Component initialization method data is NULL.");
		status = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	colander_data = g_new0(struct colander_data, 1);
	if (!colander_data) {
		BT_LOGE_STR("Failed to allocate colander data.");
		status = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	colander_data->user_notif = user_provided_data->notification;

	if (user_provided_data->notification_types) {
		notif_type = user_provided_data->notification_types;
		unsigned long count;

		while (*notif_type != BT_NOTIFICATION_TYPE_SENTINEL) {
			notif_type++;
		}

		count = notif_type - user_provided_data->notification_types + 1;

		colander_data->notif_types =
			g_new0(enum bt_notification_type, count);
		if (!colander_data->notif_types) {
			BT_LOGE_STR("Failed to allocate an array of notification types.");
			status = BT_COMPONENT_STATUS_NOMEM;
			goto end;
		}

		memcpy(colander_data->notif_types,
			user_provided_data->notification_types,
			count * sizeof(enum bt_notification_type));
	}

	status = bt_private_component_sink_add_input_private_port(
		priv_comp, "in", NULL, NULL);
	if (status != BT_COMPONENT_STATUS_OK) {
		BT_LOGE_STR("Cannot add input port.");
		goto end;
	}

	(void) bt_private_component_set_user_data(priv_comp, colander_data);

end:
	return status;
}

static
void colander_finalize(struct bt_private_component *priv_comp)
{
	struct colander_data *colander_data =
		bt_private_component_get_user_data(priv_comp);

	if (!colander_data) {
		return;
	}

	if (colander_data->notif_iter) {
		bt_put(colander_data->notif_iter);
	}

	g_free(colander_data->notif_types);
	g_free(colander_data);
}

static
void colander_port_connected(struct bt_private_component *priv_comp,
		struct bt_private_port *self_priv_port,
		struct bt_port *other_port)
{
	enum bt_connection_status conn_status;
	struct bt_private_connection *priv_conn =
		bt_private_port_get_private_connection(self_priv_port);
	struct colander_data *colander_data =
		bt_private_component_get_user_data(priv_comp);

	assert(priv_conn);
	assert(colander_data);
	BT_PUT(colander_data->notif_iter);
	conn_status = bt_private_connection_create_notification_iterator(
		priv_conn, colander_data->notif_types,
		&colander_data->notif_iter);
	if (conn_status) {
		BT_LOGE("Cannot create notification iterator from connection: "
			"comp-addr=%p, conn-addr=%p", priv_comp, priv_conn);
		goto end;
	}

end:
	bt_put(priv_conn);
}

static
enum bt_component_status colander_consume(
		struct bt_private_component *priv_comp)
{
	enum bt_component_status status = BT_COMPONENT_STATUS_OK;
	enum bt_notification_iterator_status notif_iter_status;
	struct bt_notification *notif = NULL;
	struct colander_data *colander_data =
		bt_private_component_get_user_data(priv_comp);

	assert(colander_data);

	if (!colander_data->notif_iter) {
		BT_LOGW("Trying to consume without an upstream notification iterator: "
			"comp-addr=%p", priv_comp);
		goto end;
	}

	notif_iter_status = bt_notification_iterator_next(
		colander_data->notif_iter);
	switch (notif_iter_status) {
	case BT_NOTIFICATION_ITERATOR_STATUS_CANCELED:
		status = BT_COMPONENT_STATUS_OK;
		goto end;
	case BT_NOTIFICATION_ITERATOR_STATUS_AGAIN:
		status = BT_COMPONENT_STATUS_AGAIN;
		goto end;
	case BT_NOTIFICATION_ITERATOR_STATUS_END:
		status = BT_COMPONENT_STATUS_END;
		goto end;
	case BT_NOTIFICATION_ITERATOR_STATUS_OK:
		break;
	default:
		status = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	notif = bt_notification_iterator_get_notification(
		colander_data->notif_iter);
	assert(notif);

end:
	/* Move notification to user's pointer, even if NULL. */
	*colander_data->user_notif = notif;
	return status;
}

struct bt_component_class *bt_component_class_sink_colander_get(void)
{
	if (colander_comp_cls) {
		goto end;
	}

	colander_comp_cls = bt_component_class_sink_create("colander",
		colander_consume);
	if (!colander_comp_cls) {
		BT_LOGE_STR("Cannot create sink colander component class.");
		goto end;
	}

	(void) bt_component_class_set_init_method(colander_comp_cls,
		colander_init);
	(void) bt_component_class_set_finalize_method(colander_comp_cls,
		colander_finalize);
	(void) bt_component_class_set_port_connected_method(colander_comp_cls,
		colander_port_connected);
	(void) bt_component_class_freeze(colander_comp_cls);

end:
	return bt_get(colander_comp_cls);
}

__attribute__((destructor)) static
void put_colander(void) {
	BT_PUT(colander_comp_cls);
}
