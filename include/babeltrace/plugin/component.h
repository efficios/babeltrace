#ifndef BABELTRACE_PLUGIN_COMPONENT_H
#define BABELTRACE_PLUGIN_COMPONENT_H

/*
 * BabelTrace - Babeltrace Component Interface
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

#include <babeltrace/plugin/component-class.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Status code. Errors are always negative.
 */
enum bt_component_status {
	/** Memory allocation failure. */
	BT_COMPONENT_STATUS_NOMEM =		-4,

	/** Invalid arguments. */
	BT_COMPONENT_STATUS_INVAL =		-3,

	/** Unsupported component feature. */
	BT_COMPONENT_STATUS_UNSUPPORTED =	-2,

	/** General error. */
	BT_COMPONENT_STATUS_ERROR =		-1,

	/** No error, okay. */
	BT_COMPONENT_STATUS_OK =		0,
};

struct bt_component;


/**
 * Create an instance of a component from a component class.
 *
 * @param component_class	Component class of which to create an instance
 * @param name			Name of the new component instance, optional
 * @returns			Returns a pointer to a new component instance
 */
extern struct bt_component *bt_component_create(
		struct bt_component_class *component_class, const char *name);

/**
 * Get component's name.
 *
 * @param component	Component instance of which to get the name
 * @returns		Returns a pointer to the component's name
 */
extern const char *bt_component_get_name(struct bt_component *component);

/**
 * Set component's name.
 *
 * @param component	Component instance of which to set the name
 * @param name		New component name (will be copied)
 * @returns		One of #bt_component_status values
 */
extern enum bt_component_status bt_component_set_name(
		struct bt_component *component, const char *name);

/**
 * Get component's class.
 *
 * @param component	Component instance of which to get the class
 * @returns		The component's class
 */
extern struct bt_component_class *bt_component_get_class(
		struct bt_component *component);

/**
 * Set component instance's error stream.
 *
 * @param component	Component instance
 * @param error_stream	Error stream
 * @returns		One of #bt_component_status values
 */
extern enum bt_component_status bt_component_set_error_stream(
		struct bt_component *component, FILE *error_stream);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PLUGIN_COMPONENT_H */
