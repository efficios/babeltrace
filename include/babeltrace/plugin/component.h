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

#include <stdio.h>

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

/**
 * Status code. Errors are always negative.
 */
enum bt_component_status {
	/** Memory allocation failure. */
	/* -12 for compatibility with -ENOMEM */
	BT_COMPONENT_STATUS_NOMEM =		-12,

	/** Invalid arguments. */
	/* -22 for compatibility with -EINVAL */
	BT_COMPONENT_STATUS_INVAL =		-22,

	/** Unsupported component feature. */
	BT_COMPONENT_STATUS_UNSUPPORTED =	-2,

	/** General error. */
	BT_COMPONENT_STATUS_ERROR =		-1,

	/** No error, okay. */
	BT_COMPONENT_STATUS_OK =		0,
};

struct bt_component;

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
 * Get component instance type.
 *
 * @param component	Component instance of which to get the type
 * @returns		One of #bt_component_type values
 */
extern enum bt_component_type bt_component_get_type(
		struct bt_component *component);

/**
 * Set a component instance's error stream.
 *
 * @param component	Component instance
 * @param error_stream	Error stream
 * @returns		One of #bt_component_status values
 */
extern enum bt_component_status bt_component_set_error_stream(
		struct bt_component *component, FILE *error_stream);

/**
 * Increments the reference count of \p component.
 *
 * @param component	Component of which to increment the reference count
 *
 * @see bt_component_put()
 */
extern void bt_component_get(struct bt_component *component);

/**
 * Decrements the reference count of \p component, destroying it when this
 * count reaches 0.
 *
 * @param component	Component of which to decrement the reference count
 *
 * @see bt_component_get()
 */
extern void bt_component_put(struct bt_component *component);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PLUGIN_COMPONENT_H */
