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

#include <babeltrace/babeltrace.h>
#include <babeltrace/assert-internal.h>

static enum bt_self_component_status sink_consume(
		struct bt_self_component_sink *self_comp)
{
	return BT_SELF_COMPONENT_STATUS_OK;
}

static enum bt_self_notification_iterator_status src_dummy_iterator_init_method(
		struct bt_self_notification_iterator *self_notif_iter,
		struct bt_self_component_source *self_comp,
		struct bt_self_component_port_output *self_port)
{
	return BT_SELF_NOTIFICATION_ITERATOR_STATUS_OK;
}

static enum bt_self_notification_iterator_status flt_dummy_iterator_init_method(
		struct bt_self_notification_iterator *self_notif_iter,
		struct bt_self_component_filter *self_comp,
		struct bt_self_component_port_output *self_port)
{
	return BT_SELF_NOTIFICATION_ITERATOR_STATUS_OK;
}

static void dummy_iterator_finalize_method(
		struct bt_self_notification_iterator *self_notif_iter)
{
}

static enum bt_self_notification_iterator_status dummy_iterator_next_method(
		struct bt_self_notification_iterator *self_notif_iter,
		bt_notification_array notifs, uint64_t capacity,
		uint64_t *count)
{
	return BT_SELF_NOTIFICATION_ITERATOR_STATUS_ERROR;
}

static enum bt_query_status flt_query_method(
		struct bt_self_component_class_filter *component_class,
		struct bt_query_executor *query_exec,
		const char *object, const struct bt_value *params,
		const struct bt_value **result)
{
	struct bt_value *res = bt_value_array_create();
	struct bt_value *val;
	*result = res;
	int iret;

	BT_ASSERT(*result);
	iret = bt_value_array_append_string_element(res, object);
	BT_ASSERT(iret == 0);
	iret = bt_value_copy(&val, params);
	BT_ASSERT(iret == 0);
	iret = bt_value_array_append_element(res, val);
	BT_ASSERT(iret == 0);
	bt_object_put_ref(val);
	return BT_QUERY_STATUS_OK;
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
	src_dummy_iterator_init_method);
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
	flt_dummy_iterator_init_method);
BT_PLUGIN_FILTER_COMPONENT_CLASS_NOTIFICATION_ITERATOR_FINALIZE_METHOD(filter,
	dummy_iterator_finalize_method);
BT_PLUGIN_FILTER_COMPONENT_CLASS_QUERY_METHOD(filter, flt_query_method);
