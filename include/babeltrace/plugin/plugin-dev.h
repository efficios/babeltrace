#ifndef BABELTRACE_PLUGIN_PLUGIN_DEV_H
#define BABELTRACE_PLUGIN_PLUGIN_DEV_H

/*
 * BabelTrace - Babeltrace Plug-in System Interface
 *
 * This is the header that you need to include for the development of
 * a Babeltrace plug-in.
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/plugin/plugin.h>
#include <babeltrace/component/component-class.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum bt_plugin_status (*bt_plugin_init_func)(
		struct bt_plugin *plugin);

typedef enum bt_plugin_status (*bt_plugin_exit_func)(void);

extern enum bt_plugin_status bt_plugin_add_component_class(
		struct bt_plugin *plugin,
		struct bt_component_class *component_class);

#ifdef BT_BUILT_IN_PLUGINS
/*
 * Statically-linked plug-in symbol types are stored in separate sections and
 * which are read using the bt_component_factory interface.
 */
# define BT_PLUGIN_INIT(_x)		static bt_plugin_init_func __attribute__((section("__bt_plugin_init_funcs"), used)) __bt_plugin_init = (_x)
# define BT_PLUGIN_EXIT(_x)		static bt_plugin_exit_func __attribute__((section("__bt_plugin_exit_funcs"), used)) __bt_plugin_exit = (_x)
# define BT_PLUGIN_NAME(_x)		static const char *__bt_plugin_name __attribute__((section("__bt_plugin_names"), used)) = (_x)
# define BT_PLUGIN_AUTHOR(_x)		static const char *__bt_plugin_author __attribute__((section("__bt_plugin_authors"), used)) = (_x)
# define BT_PLUGIN_LICENSE(_x)		static const char *__bt_plugin_license __attribute__((section("__bt_plugin_licenses"), used)) = (_x)
# define BT_PLUGIN_DESCRIPTION(_x)	static const char *__bt_plugin_description __attribute__((section("__bt_plugin_descriptions"), used)) = (_x)
#else /* BT_BUILT_IN_PLUGINS */
# define BT_PLUGIN_INIT(_x)		bt_plugin_init_func __bt_plugin_init = (_x)
# define BT_PLUGIN_EXIT(_x)		bt_plugin_exit_func __bt_plugin_exit = (_x)
# define BT_PLUGIN_NAME(_x)		const char __bt_plugin_name[] = (_x)
# define BT_PLUGIN_AUTHOR(_x)		const char __bt_plugin_author[] = (_x)
# define BT_PLUGIN_LICENSE(_x)		const char __bt_plugin_license[] = (_x)
# define BT_PLUGIN_DESCRIPTION(_x)	const char __bt_plugin_description[] = (_x)
#endif /* BT_BUILT_IN_PLUGINS */

#define BT_PLUGIN_COMPONENT_CLASSES_BEGIN					\
	static enum bt_plugin_status __bt_plugin_init_add_component_classes(	\
			struct bt_plugin *plugin)				\
	{									\
		enum bt_plugin_status status = BT_PLUGIN_STATUS_OK;		\
		struct bt_component_class *component_class = NULL;

#define __BT_PLUGIN_COMPONENT_CLASS_ENTRY_EPILOGUE				\
		if (!component_class) {						\
			status = BT_PLUGIN_STATUS_ERROR;			\
			goto end;						\
		}								\
		status = bt_plugin_add_component_class(plugin, component_class);\
		bt_put(component_class);					\
		component_class = NULL;						\
		if (status < 0) {						\
			goto end;						\
		}

#define BT_PLUGIN_COMPONENT_CLASS_SOURCE_ENTRY(_name, _description, _init_func)	\
		component_class = bt_component_class_create(			\
			BT_COMPONENT_TYPE_SOURCE, _name,			\
			_description, _init_func);				\
		__BT_PLUGIN_COMPONENT_CLASS_ENTRY_EPILOGUE

#define BT_PLUGIN_COMPONENT_CLASS_SINK_ENTRY(_name, _description, _init_func)	\
		component_class = bt_component_class_create(			\
			BT_COMPONENT_TYPE_SINK, _name,				\
			_description, _init_func);				\
		__BT_PLUGIN_COMPONENT_CLASS_ENTRY_EPILOGUE

#define BT_PLUGIN_COMPONENT_CLASS_FILTER_ENTRY(_name, _description, _init_func)	\
		component_class = bt_component_class_create(			\
			BT_COMPONENT_TYPE_FILTER, _name,			\
			_description, _init_func);				\
		__BT_PLUGIN_COMPONENT_CLASS_ENTRY_EPILOGUE

#define BT_PLUGIN_COMPONENT_CLASSES_END						\
	end:									\
		return status;							\
	}									\
										\
	static enum bt_plugin_status __bt_plugin_nop_exit(void) {		\
		return BT_PLUGIN_STATUS_OK;					\
	}									\
										\
	BT_PLUGIN_INIT(__bt_plugin_init_add_component_classes);			\
	BT_PLUGIN_EXIT(__bt_plugin_nop_exit);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PLUGIN_PLUGIN_DEV_H */
