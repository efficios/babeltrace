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

#include <babeltrace/object.h>
#include <babeltrace/graph/connection.h>
#include <babeltrace/graph/private-component-class-sink.h>
#include <babeltrace/graph/self-component-sink.h>
#include <babeltrace/graph/self-component-port.h>
#include <babeltrace/graph/self-component-port-input-notification-iterator.h>
#include <babeltrace/graph/self-component.h>
#include <babeltrace/graph/component-class-sink-colander-internal.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>

static
struct bt_private_component_class_sink *colander_comp_cls;

struct colander_data {
	bt_notification_array notifs;
	uint64_t *count_addr;
	struct bt_self_component_port_input_notification_iterator *notif_iter;
};

static
enum bt_self_component_status colander_init(
		struct bt_self_component_sink *self_comp,
		struct bt_value *params, void *init_method_data)
{
	enum bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	struct colander_data *colander_data = NULL;
	struct bt_component_class_sink_colander_data *user_provided_data =
		init_method_data;

	if (!init_method_data) {
		BT_LOGW_STR("Component initialization method data is NULL.");
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	colander_data = g_new0(struct colander_data, 1);
	if (!colander_data) {
		BT_LOGE_STR("Failed to allocate colander data.");
		status = BT_SELF_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	colander_data->notifs = user_provided_data->notifs;
	colander_data->count_addr = user_provided_data->count_addr;
	status = bt_self_component_sink_add_input_port(self_comp, "in",
		NULL, NULL);
	if (status != BT_SELF_COMPONENT_STATUS_OK) {
		BT_LOGE_STR("Cannot add input port.");
		goto end;
	}

	bt_self_component_set_data(
		bt_self_component_sink_as_self_component(self_comp),
		colander_data);

end:
	return status;
}

static
void colander_finalize(struct bt_self_component_sink *self_comp)
{
	struct colander_data *colander_data =
		bt_self_component_get_data(
			bt_self_component_sink_as_self_component(self_comp));

	if (!colander_data) {
		return;
	}

	BT_OBJECT_PUT_REF_AND_RESET(colander_data->notif_iter);
	g_free(colander_data);
}

static
enum bt_self_component_status colander_input_port_connected(
		struct bt_self_component_sink *self_comp,
		struct bt_self_component_port_input *self_port,
		struct bt_port_output *other_port)
{
	enum bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	struct colander_data *colander_data =
		bt_self_component_get_data(
			bt_self_component_sink_as_self_component(self_comp));

	BT_ASSERT(colander_data);
	BT_OBJECT_PUT_REF_AND_RESET(colander_data->notif_iter);
	colander_data->notif_iter =
		bt_self_component_port_input_notification_iterator_create(
			self_port);
	if (!colander_data->notif_iter) {
		BT_LIB_LOGE("Cannot create notification iterator on "
			"self component input port: %![port-]+p",
			self_port);
		status = BT_SELF_COMPONENT_STATUS_NOMEM;
		goto end;
	}

end:
	return status;
}

static
enum bt_self_component_status colander_consume(
		struct bt_self_component_sink *self_comp)
{
	enum bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	enum bt_notification_iterator_status notif_iter_status;
	struct colander_data *colander_data =
		bt_self_component_get_data(
			bt_self_component_sink_as_self_component(self_comp));
	bt_notification_array notifs;

	BT_ASSERT(colander_data);

	if (!colander_data->notif_iter) {
		BT_LIB_LOGW("Trying to consume without an "
			"upstream notification iterator: %![comp-]+c",
			self_comp);
		goto end;
	}

	notif_iter_status =
		bt_self_component_port_input_notification_iterator_next(
			colander_data->notif_iter, &notifs,
			colander_data->count_addr);
	switch (notif_iter_status) {
	case BT_NOTIFICATION_ITERATOR_STATUS_CANCELED:
		status = BT_SELF_COMPONENT_STATUS_OK;
		goto end;
	case BT_NOTIFICATION_ITERATOR_STATUS_AGAIN:
		status = BT_SELF_COMPONENT_STATUS_AGAIN;
		goto end;
	case BT_NOTIFICATION_ITERATOR_STATUS_END:
		status = BT_SELF_COMPONENT_STATUS_END;
		goto end;
	case BT_NOTIFICATION_ITERATOR_STATUS_OK:
		/* Move notifications to user (count already set) */
		memcpy(colander_data->notifs, notifs,
			sizeof(*notifs) * *colander_data->count_addr);
		break;
	default:
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

end:
	return status;
}

struct bt_component_class_sink *bt_component_class_sink_colander_get(void)
{
	if (colander_comp_cls) {
		goto end;
	}

	colander_comp_cls = bt_private_component_class_sink_create(
		"colander", colander_consume);
	if (!colander_comp_cls) {
		BT_LOGE_STR("Cannot create sink colander component class.");
		goto end;
	}

	(void) bt_private_component_class_sink_set_init_method(
		colander_comp_cls, colander_init);
	(void) bt_private_component_class_sink_set_finalize_method(
		colander_comp_cls, colander_finalize);
	(void) bt_private_component_class_sink_set_input_port_connected_method(
		colander_comp_cls, colander_input_port_connected);

end:
	bt_object_get_ref(colander_comp_cls);
	return (void *) colander_comp_cls;
}

__attribute__((destructor)) static
void put_colander(void) {
	BT_OBJECT_PUT_REF_AND_RESET(colander_comp_cls);
}
