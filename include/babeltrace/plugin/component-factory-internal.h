#ifndef BABELTRACE_PLUGIN_COMPONENT_FACTORY_INTERNAL_H
#define BABELTRACE_PLUGIN_COMPONENT_FACTORY_INTERNAL_H

/*
 * BabelTrace - Component Factory Internal
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/plugin/component-factory.h>
#include <babeltrace/plugin/component.h>
#include <babeltrace/plugin/component-class-internal.h>
#include <babeltrace/plugin/plugin-system.h>
#include <babeltrace/plugin/plugin.h>
#include <glib.h>

#define SECTION_BEGIN(_name)	&__start_##_name
#define SECTION_END(_name)	&__stop_##_name

#define SECTION_ELEMENT_COUNT(_name) (SECTION_END(_name) - SECTION_BEGIN(_name))

#define DECLARE_SECTION(_type, _name)				\
	extern _type const __start_##_name __attribute((weak));	\
	extern _type const __stop_##_name __attribute((weak))

#define DECLARE_PLUG_IN_SECTIONS						\
	DECLARE_SECTION(bt_plugin_register_func, __plugin_register_funcs);	\
	DECLARE_SECTION(const char *, __plugin_names);				\
	DECLARE_SECTION(const char *, __plugin_authors);			\
	DECLARE_SECTION(const char *, __plugin_licenses);			\
	DECLARE_SECTION(const char *, __plugin_descriptions)

#define PRINT_SECTION(_printer, _name)					\
	_printer("Section " #_name " [%p - %p], (%zu elements)\n",	\
	SECTION_BEGIN(_name), SECTION_END(_name), SECTION_ELEMENT_COUNT(_name))

#define PRINT_PLUG_IN_SECTIONS(_printer)					\
	PRINT_SECTION(_printer, __plugin_register_funcs);			\
	PRINT_SECTION(_printer, __plugin_names);				\
	PRINT_SECTION(_printer, __plugin_authors);				\
	PRINT_SECTION(_printer, __plugin_licenses);				\
	PRINT_SECTION(_printer, __plugin_descriptions)

struct bt_component_factory {
	struct bt_object base;
	/** Array of pointers to struct bt_component_class */
	GPtrArray *component_classes;
	/* Plug-in currently registering component classes. Weak ref. */
	struct bt_plugin *current_plugin;
};

#endif /* BABELTRACE_PLUGIN_COMPONENT_FACTORY_INTERNAL_H */
