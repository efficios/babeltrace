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

static int sink_value = 42;

static enum bt_component_status sink_run(struct bt_component *component)
{
	return BT_COMPONENT_STATUS_OK;
}

static enum bt_component_status comp_class_sink_init(
		struct bt_component *component, struct bt_value *params)
{
	enum bt_component_status ret;

	ret = bt_component_set_private_data(component, &sink_value);
	if (ret != BT_COMPONENT_STATUS_OK) {
		return BT_COMPONENT_STATUS_ERROR;
	}

	ret = bt_component_sink_set_consume_cb(component, sink_run);
	if (ret != BT_COMPONENT_STATUS_OK) {
		return BT_COMPONENT_STATUS_ERROR;
	}

	return BT_COMPONENT_STATUS_OK;
}

static enum bt_component_status comp_class_dummy_init(
		struct bt_component *component, struct bt_value *params)
{
	return BT_COMPONENT_STATUS_OK;
}

BT_PLUGIN(test_sfs);
BT_PLUGIN_DESCRIPTION("Babeltrace plugin with source, sink, and filter component classes");
BT_PLUGIN_AUTHOR("Janine Sutto");
BT_PLUGIN_LICENSE("Beerware");

BT_PLUGIN_COMPONENT_CLASS(BT_COMPONENT_TYPE_SOURCE, source, comp_class_dummy_init);
BT_PLUGIN_COMPONENT_CLASS_DESCRIPTION(BT_COMPONENT_TYPE_SOURCE, source, "A source.");

BT_PLUGIN_COMPONENT_CLASS(BT_COMPONENT_TYPE_SINK, sink, comp_class_sink_init);
BT_PLUGIN_COMPONENT_CLASS_DESCRIPTION(BT_COMPONENT_TYPE_SINK, sink, "A sink.");

BT_PLUGIN_COMPONENT_CLASS(BT_COMPONENT_TYPE_FILTER, filter, comp_class_dummy_init);
BT_PLUGIN_COMPONENT_CLASS_DESCRIPTION(BT_COMPONENT_TYPE_FILTER, filter, "A filter.");
