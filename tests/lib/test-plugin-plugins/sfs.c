/*
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <babeltrace/plugin/plugin-dev.h>
#include <babeltrace/graph/component-class.h>
#include <babeltrace/values.h>
#include <babeltrace/ref.h>
#include <assert.h>

static enum bt_component_status sink_consume(
		struct bt_private_component *private_component)
{
	return BT_COMPONENT_STATUS_OK;
}

static enum bt_notification_iterator_status dummy_iterator_init_method(
		struct bt_private_connection_private_notification_iterator *private_iterator,
		struct bt_private_port *private_port)
{
	return BT_NOTIFICATION_ITERATOR_STATUS_OK;
}

static void dummy_iterator_finalize_method(
		struct bt_private_connection_private_notification_iterator *private_iterator)
{
}

static struct bt_notification_iterator_next_method_return dummy_iterator_next_method(
		struct bt_private_connection_private_notification_iterator *private_iterator)
{
	struct bt_notification_iterator_next_method_return next_return = {
		.notification = NULL,
		.status = BT_NOTIFICATION_ITERATOR_STATUS_OK,
	};

	return next_return;
}

static struct bt_component_class_query_method_return query_method(
		struct bt_component_class *component_class,
		struct bt_query_executor *query_exec,
		const char *object, struct bt_value *params)
{
	struct bt_component_class_query_method_return ret = {
		.status = BT_QUERY_STATUS_OK,
		.result = bt_value_array_create(),
	};
	int iret;

	assert(ret.result);
	iret = bt_value_array_append_string(ret.result, object);
	assert(iret == 0);
	iret = bt_value_array_append(ret.result, params);
	assert(iret == 0);
	return ret;
}

BT_PLUGIN_MODULE();
BT_PLUGIN(test_sfs);
BT_PLUGIN_DESCRIPTION("Babeltrace plugin with source, sink, and filter component classes");
BT_PLUGIN_AUTHOR("Janine Sutto");
BT_PLUGIN_LICENSE("Beerware");
BT_PLUGIN_VERSION(1, 2, 3, "yes");

BT_PLUGIN_SOURCE_COMPONENT_CLASS(source, dummy_iterator_next_method);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION(source, "A source.");
BT_PLUGIN_SOURCE_COMPONENT_CLASS_NOTIFICATION_ITERATOR_INIT_METHOD(source,
	dummy_iterator_init_method);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_NOTIFICATION_ITERATOR_FINALIZE_METHOD(source,
	dummy_iterator_finalize_method);

BT_PLUGIN_SINK_COMPONENT_CLASS(sink, sink_consume);
BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION(sink, "A sink.");
BT_PLUGIN_SINK_COMPONENT_CLASS_HELP(sink,
	"Bacon ipsum dolor amet strip steak cupim pastrami venison shoulder.\n"
	"Prosciutto beef ribs flank meatloaf pancetta brisket kielbasa drumstick\n"
	"venison tenderloin cow tail. Beef short loin shoulder meatball, sirloin\n"
	"ground round brisket salami cupim pork bresaola turkey bacon boudin.\n"
);

BT_PLUGIN_FILTER_COMPONENT_CLASS(filter, dummy_iterator_next_method);
BT_PLUGIN_FILTER_COMPONENT_CLASS_DESCRIPTION(filter, "A filter.");
BT_PLUGIN_FILTER_COMPONENT_CLASS_NOTIFICATION_ITERATOR_INIT_METHOD(filter,
	dummy_iterator_init_method);
BT_PLUGIN_FILTER_COMPONENT_CLASS_NOTIFICATION_ITERATOR_FINALIZE_METHOD(filter,
	dummy_iterator_finalize_method);
BT_PLUGIN_FILTER_COMPONENT_CLASS_QUERY_METHOD(filter, query_method);
