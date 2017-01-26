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
#include <babeltrace/component/component.h>

static enum bt_component_status sink_consume(struct bt_component *component)
{
	return BT_COMPONENT_STATUS_OK;
}

enum bt_component_status dummy_init_iterator_method(
		struct bt_component *component, struct bt_notification_iterator *iter)
{
	return BT_COMPONENT_STATUS_OK;
}

BT_PLUGIN(test_sfs);
BT_PLUGIN_DESCRIPTION("Babeltrace plugin with source, sink, and filter component classes");
BT_PLUGIN_AUTHOR("Janine Sutto");
BT_PLUGIN_LICENSE("Beerware");
BT_PLUGIN_VERSION(1, 2, 3, "yes");

BT_PLUGIN_SOURCE_COMPONENT_CLASS(source, dummy_init_iterator_method);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION(source, "A source.");

BT_PLUGIN_SINK_COMPONENT_CLASS(sink, sink_consume);
BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION(sink, "A sink.");

BT_PLUGIN_FILTER_COMPONENT_CLASS(filter, dummy_init_iterator_method);
BT_PLUGIN_FILTER_COMPONENT_CLASS_DESCRIPTION(filter, "A filter.");
