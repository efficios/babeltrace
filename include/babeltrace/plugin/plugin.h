#ifndef BABELTRACE_PLUGIN_H
#define BABELTRACE_PLUGIN_H

/*
 * BabelTrace - Babeltrace Plug-in Interface
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
 * Plug-in type.
 */
enum bt_plugin_type {
	BT_PLUGIN_TYPE_UNKNOWN =	-1,

	/** A source plug-in is a notification generator. */
	BT_PLUGIN_TYPE_SOURCE =		0,

	/** A sink plug-in handles incoming notifications. */
	BT_PLUGIN_TYPE_SINK =		1,

	/** A filter plug-in implements both Source and Sink interfaces. */
	BT_PLUGIN_TYPE_FILTER =		2,
};

/**
 * Status code. Errors are always negative.
 */
enum bt_plugin_status {
	/** Memory allocation failure. */
	/* -12 for compatibility with -ENOMEM */
	BT_PLUGIN_STATUS_NOMEM =	-12,

	/** Invalid arguments. */
	/* -22 for compatibility with -EINVAL */
	BT_PLUGIN_STATUS_INVAL =	-22,

	/** Unsupported plug-in feature. */
	BT_PLUGIN_STATUS_UNSUPPORTED =	-2,

	/** General error. */
	BT_PLUGIN_STATUS_ERROR =	-1,

	/** No error, okay. */
	BT_PLUGIN_STATUS_OK =		0,
};

struct bt_plugin;

/**
 * Get plug-in instance name.
 *
 * @param plugin	Plug-in instance of which to get the name
 * @returns		Returns a pointer to the plug-in's name
 */
extern const char *bt_plugin_get_name(struct bt_plugin *plugin);

/**
 * Set plug-in instance name.
 *
 * @param plugin	Plug-in instance of which to set the name
 * @param name		New plug-in name (will be copied)
 * @returns		One of #bt_plugin_status values
 */
extern enum bt_plugin_status bt_plugin_set_name(
		struct bt_plugin *plugin, const char *name);

/**
 * Get plug-in instance type.
 *
 * @param plugin	Plug-in instance of which to get the type
 * @returns		One of #bt_plugin_type values
 */
extern enum bt_plugin_type bt_plugin_get_type(struct bt_plugin *plugin);

/**
 * Set a plug-in instance's error stream.
 *
 * @param plugin	Plug-in instance
 * @param error_stream	Error stream
 * @returns		One of #bt_plugin_status values
 */
extern enum bt_plugin_status bt_plugin_set_error_stream(
		struct bt_plugin *plugin, FILE *error_stream);

/**
 * Increments the reference count of \p plugin.
 *
 * @param plugin	Plug-in of which to increment the reference count
 *
 * @see bt_plugin_put()
 */
extern void bt_plugin_get(struct bt_plugin *plugin);

/**
 * Decrements the reference count of \p plugin, destroying it when this
 * count reaches 0.
 *
 * @param plugin	Plug-in of which to decrement the reference count
 *
 * @see bt_plugin_get()
 */
extern void bt_plugin_put(struct bt_plugin *plugin);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PLUGIN_H */
