#ifndef BABELTRACE_PLUGIN_COMPONENT_CLASS_H
#define BABELTRACE_PLUGIN_COMPONENT_CLASS_H

/*
 * Babeltrace - Component Class Interface.
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Component type.
 */
enum bt_component_type {
	BT_COMPONENT_TYPE_UNKNOWN =	-1,

	/** A source component is a notification generator. */
	BT_COMPONENT_TYPE_SOURCE =	0,

	/** A sink component handles incoming notifications. */
	BT_COMPONENT_TYPE_SINK =	1,

	/** A filter component implements both Source and Sink interfaces. */
	BT_COMPONENT_TYPE_FILTER =	2,
};

struct bt_component_class;

const char *bt_component_class_get_name(
		struct bt_component_class *component_class);

enum bt_component_type bt_component_class_get_type(
		struct bt_component_class *component_class);

#endif /* BABELTRACE_PLUGIN_COMPONENT_CLASS_H */
