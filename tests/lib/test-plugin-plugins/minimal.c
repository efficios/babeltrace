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

extern int test_plugin_init_called;
extern int test_plugin_exit_called;

static enum bt_plugin_status plugin_init(struct bt_plugin *plugin)
{
    test_plugin_init_called = 1;

	return BT_PLUGIN_STATUS_OK;
}

static enum bt_plugin_status plugin_exit(void)
{
    test_plugin_exit_called = 1;

	return BT_PLUGIN_STATUS_OK;
}

BT_PLUGIN_NAME("test-minimal");
BT_PLUGIN_DESCRIPTION("Minimal Babeltrace plugin with no component classes");
BT_PLUGIN_AUTHOR("Janine Sutto");
BT_PLUGIN_LICENSE("Beerware");
BT_PLUGIN_INIT(plugin_init);
BT_PLUGIN_EXIT(plugin_exit);
