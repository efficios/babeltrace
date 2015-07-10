#ifndef BABELTRACE_PLUGIN_COMPONENT_FACTORY_H
#define BABELTRACE_PLUGIN_COMPONENT_FACTORY_H

/*
 * Babeltrace - Component Factory Class Interface.
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/plugin/component.h>
#include <babeltrace/plugin/source.h>
#include <babeltrace/plugin/sink.h>
#include <babeltrace/plugin/filter.h>
#include <babeltrace/plugin/plugin-system.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_component_factory;

enum bt_component_status bt_component_factory_create(const char *path);

enum bt_component_status bt_component_factory_register_source_component_class(
	struct bt_component_factory *factory, const char *name,
	bt_component_init_cb init, bt_component_fini_cb fini,
	bt_component_source_iterator_create_cb iterator_create_cb);

enum bt_component_status bt_component_factory_register_sink_component_class(
	struct bt_component_factory *factory, const char *name,
	bt_component_init_cb init, bt_component_fini_cb fini,
	bt_component_sink_handle_notification_cb handle_notification_cb);

void bt_component_factory_destroy(struct bt_component_factory *factory);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PLUGIN_COMPONENT_FACTORY_H */
