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

struct bt_component_factory {
	struct bt_object base;
	/** Array of pointers to struct bt_plugin */
	GPtrArray *plugins;
	/** Array of pointers to struct bt_component_class */
	GPtrArray *component_classes;
};

#endif /* BABELTRACE_PLUGIN_COMPONENT_FACTORY_INTERNAL_H */
