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
#include <babeltrace/component/component-class.h>
#include <babeltrace/values.h>
#include <babeltrace/ref.h>
#include <assert.h>

static enum bt_component_status sink_consume(struct bt_component *component)
{
	return BT_COMPONENT_STATUS_OK;
}

static enum bt_notification_iterator_status dummy_iterator_init_method(
		struct bt_component *component,
		struct bt_notification_iterator *iterator,
		void *init_method_data)
{
	return BT_NOTIFICATION_ITERATOR_STATUS_OK;
}

static void dummy_iterator_destroy_method(
		struct bt_notification_iterator *iterator)
{
}

static struct bt_notification *dummy_iterator_get_method(
		struct bt_notification_iterator *iterator)
{
	return NULL;
}

static enum bt_notification_iterator_status dummy_iterator_next_method(
		struct bt_notification_iterator *iterator)
{
	return BT_NOTIFICATION_ITERATOR_STATUS_OK;
}

static enum bt_notification_iterator_status dummy_iterator_seek_time_method(
		struct bt_notification_iterator *iterator, int64_t time)
{
	return BT_NOTIFICATION_ITERATOR_STATUS_OK;
}

static struct bt_value *query_method(
		struct bt_component_class *component_class,
		const char *object, struct bt_value *params)
{
	int ret;
	struct bt_value *results = bt_value_array_create();

	assert(results);
	ret = bt_value_array_append_string(results, object);
	assert(ret == 0);
	ret = bt_value_array_append(results, params);
	assert(ret == 0);
	return results;
}

BT_PLUGIN(test_sfs);
BT_PLUGIN_DESCRIPTION("Babeltrace plugin with source, sink, and filter component classes");
BT_PLUGIN_AUTHOR("Janine Sutto");
BT_PLUGIN_LICENSE("Beerware");
BT_PLUGIN_VERSION(1, 2, 3, "yes");

BT_PLUGIN_SOURCE_COMPONENT_CLASS(source, dummy_iterator_get_method,
	dummy_iterator_next_method);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION(source, "A source.");
BT_PLUGIN_SOURCE_COMPONENT_CLASS_NOTIFICATION_ITERATOR_INIT_METHOD(source,
	dummy_iterator_init_method);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_NOTIFICATION_ITERATOR_DESTROY_METHOD(source,
	dummy_iterator_destroy_method);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_NOTIFICATION_ITERATOR_SEEK_TIME_METHOD(source,
	dummy_iterator_seek_time_method);

BT_PLUGIN_SINK_COMPONENT_CLASS(sink, sink_consume);
BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION(sink, "A sink.");
BT_PLUGIN_SINK_COMPONENT_CLASS_HELP(sink,
	"Bacon ipsum dolor amet strip steak cupim pastrami venison shoulder.\n"
	"Prosciutto beef ribs flank meatloaf pancetta brisket kielbasa drumstick\n"
	"venison tenderloin cow tail. Beef short loin shoulder meatball, sirloin\n"
	"ground round brisket salami cupim pork bresaola turkey bacon boudin.\n"
);

BT_PLUGIN_FILTER_COMPONENT_CLASS(filter, dummy_iterator_get_method,
	dummy_iterator_next_method);
BT_PLUGIN_FILTER_COMPONENT_CLASS_DESCRIPTION(filter, "A filter.");
BT_PLUGIN_FILTER_COMPONENT_CLASS_NOTIFICATION_ITERATOR_INIT_METHOD(filter,
	dummy_iterator_init_method);
BT_PLUGIN_FILTER_COMPONENT_CLASS_NOTIFICATION_ITERATOR_DESTROY_METHOD(filter,
	dummy_iterator_destroy_method);
BT_PLUGIN_FILTER_COMPONENT_CLASS_NOTIFICATION_ITERATOR_SEEK_TIME_METHOD(filter,
	dummy_iterator_seek_time_method);
BT_PLUGIN_FILTER_COMPONENT_CLASS_QUERY_METHOD(filter, query_method);
