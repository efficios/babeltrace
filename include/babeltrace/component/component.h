#ifndef BABELTRACE_COMPONENT_COMPONENT_H
#define BABELTRACE_COMPONENT_COMPONENT_H

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

#include <babeltrace/component/notification/iterator.h>
#include <babeltrace/values.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Status code. Errors are always negative.
 */
enum bt_component_status {
	/** No error, okay. */
	BT_COMPONENT_STATUS_OK =		0,
	/** No more work to be done by this component. **/
	BT_COMPONENT_STATUS_END =		1,
	/**
	 * Component can't process a notification at this time
	 * (e.g. would block), try again later.
	 */
	BT_COMPONENT_STATUS_AGAIN =		2,
	/** General error. */
	BT_COMPONENT_STATUS_ERROR =		-1,
	/** Unsupported component feature. */
	BT_COMPONENT_STATUS_UNSUPPORTED =	-2,
	/** Invalid arguments. */
	BT_COMPONENT_STATUS_INVALID =		-3,
	/** Memory allocation failure. */
	BT_COMPONENT_STATUS_NOMEM =		-4,
};

struct bt_component_class;
struct bt_component;
struct bt_value;

/**
 * Component private data deallocation function type.
 *
 * @param component	Component instance
 */
typedef void (*bt_component_destroy_cb)(struct bt_component *component);

/**
 * Component initialization function type.
 *
 * A component's private data and required callbacks must be set by this
 * function.
 *
 * @param component	Component instance
 * @param params	A dictionary of component parameters
 * @returns		One of #bt_component_status values
 */
typedef enum bt_component_status (*bt_component_init_cb)(
		struct bt_component *component, struct bt_value *params);


/**
 * Get a component's private data.
 *
 * @param component	Component of which to get the private data
 * @returns		Component's private data
 */
extern void *bt_component_get_private_data(struct bt_component *component);

/**
 * Set a component's private data.
 *
 * @param component	Component of which to set the private data
 * @param data		Component private data
 * @returns		One of #bt_component_status values
 */
extern enum bt_component_status bt_component_set_private_data(
		struct bt_component *component, void *data);

/**
 * Set a component's private data cleanup function.
 *
 * @param component	Component of which to set the private data destruction
 *			function
 * @param data		Component private data clean-up function
 * @returns		One of #bt_component_status values
 */
extern enum bt_component_status bt_component_set_destroy_cb(
		struct bt_component *component,
		bt_component_destroy_cb destroy);

/** bt_component_souce */
/**
 * Iterator initialization function type.
 *
 * A notification iterator's private data, deinitialization, next, and get
 * callbacks must be set by this function.
 *
 * @param source	Source component instance
 * @param iterator	Notification iterator instance
 */
typedef enum bt_component_status (*bt_component_source_init_iterator_cb)(
		struct bt_component *, struct bt_notification_iterator *);

/**
 * Set a source component's iterator initialization function.
 *
 * @param source	Source component instance
 * @param init_iterator	Notification iterator initialization callback
 */
extern enum bt_component_status
bt_component_source_set_iterator_init_cb(struct bt_component *source,
		bt_component_source_init_iterator_cb init_iterator);

/** bt_component_sink */
/**
 * Notification consumption function type.
 *
 * @param sink		Sink component instance
 * @returns		One of #bt_component_status values
 */
typedef enum bt_component_status (*bt_component_sink_consume_cb)(
		struct bt_component *);

/**
 * Iterator addition function type.
 *
 * A sink component may choose to refuse the addition of an iterator
 * by not returning BT_COMPONENT_STATUS_OK.
 *
 * @param sink		Sink component instance
 * @returns		One of #bt_component_status values
 */
typedef enum bt_component_status (*bt_component_sink_add_iterator_cb)(
		struct bt_component *, struct bt_notification_iterator *);

/**
 * Set a sink component's consumption callback.
 *
 * @param sink		Sink component instance
 * @param consume	Consumption callback
 * @returns		One of #bt_component_status values
 */
extern enum bt_component_status
bt_component_sink_set_consume_cb(struct bt_component *sink,
		bt_component_sink_consume_cb consume);

/**
 * Set a sink component's iterator addition callback.
 *
 * @param sink		Sink component instance
 * @param add_iterator	Iterator addition callback
 * @returns		One of #bt_component_status values
 */
extern enum bt_component_status
bt_component_sink_set_add_iterator_cb(struct bt_component *sink,
		bt_component_sink_add_iterator_cb add_iterator);

/* Defaults to 1. */
extern enum bt_component_status
bt_component_sink_set_minimum_input_count(struct bt_component *sink,
		unsigned int minimum);

/* Defaults to 1. */
extern enum bt_component_status
bt_component_sink_set_maximum_input_count(struct bt_component *sink,
		unsigned int maximum);

extern enum bt_component_status
bt_component_sink_get_input_count(struct bt_component *sink,
		unsigned int *count);

/* May return NULL after an interator has reached its end. */
extern enum bt_component_status
bt_component_sink_get_input_iterator(struct bt_component *sink,
		unsigned int input, struct bt_notification_iterator **iterator);

/** bt_component_filter */
/**
 * Iterator initialization function type.
 *
 * A notification iterator's private data, deinitialization, next, and get
 * callbacks must be set by this function.
 *
 * @param filter	Filter component instance
 * @param iterator	Notification iterator instance
 */
typedef enum bt_component_status (*bt_component_filter_init_iterator_cb)(
		struct bt_component *, struct bt_notification_iterator *);

/**
 * Iterator addition function type.
 *
 * A filter component may choose to refuse the addition of an iterator
 * by not returning BT_COMPONENT_STATUS_OK.
 *
 * @param filter	Filter component instance
 * @returns		One of #bt_component_status values
 */
typedef enum bt_component_status (*bt_component_filter_add_iterator_cb)(
		struct bt_component *, struct bt_notification_iterator *);

/**
 * Set a filter component's iterator initialization function.
 *
 * @param filter	Filter component instance
 * @param init_iterator	Notification iterator initialization callback
 */
extern enum bt_component_status
bt_component_filter_set_iterator_init_cb(struct bt_component *filter,
		bt_component_filter_init_iterator_cb init_iterator);

/**
 * Set a filter component's iterator addition callback.
 *
 * @param filter	Filter component instance
 * @param add_iterator	Iterator addition callback
 * @returns		One of #bt_component_status values
 */
extern enum bt_component_status
bt_component_filter_set_add_iterator_cb(struct bt_component *filter,
		bt_component_filter_add_iterator_cb add_iterator);

/* Defaults to 1. */
extern enum bt_component_status
bt_component_filter_set_minimum_input_count(struct bt_component *filter,
		unsigned int minimum);

/* Defaults to 1. */
extern enum bt_component_status
bt_component_filter_set_maximum_input_count(struct bt_component *filter,
		unsigned int maximum);

extern enum bt_component_status
bt_component_filter_get_input_count(struct bt_component *filter,
		unsigned int *count);

/* May return NULL after an interator has reached its end. */
extern enum bt_component_status
bt_component_filter_get_input_iterator(struct bt_component *filter,
		unsigned int input, struct bt_notification_iterator **iterator);

/**
 * Create an instance of a component from a component class.
 *
 * @param component_class	Component class of which to create an instance
 * @param name			Name of the new component instance, optional
 * @param params		A dictionary of component parameters
 * @returns			Returns a pointer to a new component instance
 */
extern struct bt_component *bt_component_create(
		struct bt_component_class *component_class, const char *name,
		struct bt_value *params);

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

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_COMPONENT_COMPONENT_H */
