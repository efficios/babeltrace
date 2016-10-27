#ifndef BABELTRACE_PLUGIN_MACROS_H
#define BABELTRACE_PLUGIN_MACROS_H

/*
 * BabelTrace - Babeltrace Plug-in Helper Macros
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2015 Philippe Proulx <pproulx@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/plugin/component-factory.h>
#include <babeltrace/plugin/component.h>
#include <babeltrace/plugin/plugin.h>

#ifndef BT_BUILT_IN_PLUGINS

#define BT_PLUGIN_REGISTER(_x)		bt_plugin_register_func __bt_plugin_register = (_x)
#define BT_PLUGIN_NAME(_x)		const char __bt_plugin_name[] = (_x)
#define BT_PLUGIN_AUTHOR(_x)		const char __bt_plugin_author[] = (_x)
#define BT_PLUGIN_LICENSE(_x)		const char __bt_plugin_license[] = (_x)
#define BT_PLUGIN_DESCRIPTION(_x)	const char __bt_plugin_description[] = (_x)

#else /* BT_BUILT_IN_PLUGINS */

/*
 * Statically-linked plug-in symbol types are stored in separate sections and
 * which are read using the bt_component_factory interface.
 */
#define BT_PLUGIN_REGISTER(_x)		static bt_plugin_register_func __attribute__((section("__plugin_register_funcs"), used)) __plugin_register = (_x)
#define BT_PLUGIN_NAME(_x)		static const char *__plugin_name __attribute__((section("__plugin_names"), used)) = (_x)
#define BT_PLUGIN_AUTHOR(_x)		static const char *__plugin_author __attribute__((section("__plugin_authors"), used)) = (_x)
#define BT_PLUGIN_LICENSE(_x)		static const char *__plugin_license __attribute__((section("__plugin_licenses"), used)) = (_x)
#define BT_PLUGIN_DESCRIPTION(_x)	static const char *__plugin_description __attribute__((section("__plugin_descriptions"), used)) = (_x)

#endif /* BT_BUILT_IN_PLUGINS */

#define BT_PLUGIN_COMPONENT_CLASSES_BEGIN					\
	static enum bt_component_status __bt_plugin_register_component_classes(	\
		struct bt_component_factory *factory)				\
	{

#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_ENTRY(_name, description, _init)	\
	bt_component_factory_register_source_component_class(factory,		\
			_name, description, _init);

#define BT_PLUGIN_SINK_COMPONENT_CLASS_ENTRY(_name, _description, _init)	\
	bt_component_factory_register_sink_component_class(factory,		\
			_name, _description, _init);

#define BT_PLUGIN_FILTER_COMPONENT_CLASS_ENTRY(_name, _description, _init)	\
	bt_component_factory_register_filter_component_class(factory,		\
			_name, _description, _init);

#define BT_PLUGIN_COMPONENT_CLASSES_END						\
	return BT_COMPONENT_STATUS_OK;						\
}										\
										\
	BT_PLUGIN_REGISTER(__bt_plugin_register_component_classes);		\

#endif /* BABELTRACE_PLUGIN_MACROS_H */
