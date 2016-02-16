#ifndef BABELTRACE_PLUGIN_H
#define BABELTRACE_PLUGIN_H

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

/* A plugin must define the __bt_plugin_init symbol */
#define BT_PLUGIN_NAME(_x)	const char __bt_plugin_name[] = (_x)
#define BT_PLUGIN_AUTHOR(_x)	const char __bt_plugin_author[] = (_x)
#define BT_PLUGIN_LICENSE(_x)	const char __bt_plugin_license[] = (_x)
#define BT_PLUGIN_INIT(_x)	bt_plugin_init_func __bt_plugin_init = (_x)
#define BT_PLUGIN_EXIT(_x)	bt_plugin_exit_func __bt_plugin_exit = (_x)

#define BT_PLUGIN_COMPONENT_CLASSES_BEGIN			\
	enum bt_component_status __bt_plugin_register_component_classes(\
		struct bt_component_factory *factory)\
	{

#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_ENTRY(_name, _init) \
	bt_component_factory_register_source_component_class(factory, \
		_name, _init);

#define BT_PLUGIN_SINK_COMPONENT_CLASS_ENTRY(_name, _init) \
	bt_component_factory_register_sink_component_class(factory, \
		_name, _init);

#define BT_PLUGIN_COMPONENT_CLASSES_END\
	\
	return BT_COMPONENT_STATUS_OK;\
}\
	\
	BT_PLUGIN_INIT(__bt_plugin_register_component_classes);\

#endif /* BABELTRACE_PLUGIN_MACROS_H */
