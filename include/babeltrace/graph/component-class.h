#ifndef BABELTRACE_GRAPH_COMPONENT_CLASS_H
#define BABELTRACE_GRAPH_COMPONENT_CLASS_H

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

#include <stdint.h>

/* For enum bt_component_status */
#include <babeltrace/graph/component-status.h>

/* For enum bt_notification_iterator_status */
#include <babeltrace/graph/notification-iterator.h>

/* For enum bt_query_status */
#include <babeltrace/graph/query-executor.h>

/* For bt_bool */
#include <babeltrace/types.h>

/* For bt_notification_array */
#include <babeltrace/graph/notification.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_component_class;
struct bt_component;
struct bt_private_component;
struct bt_private_port;
struct bt_port;
struct bt_value;
struct bt_private_connection_private_notification_iterator;
struct bt_query_executor;

/**
 * Component class type.
 */
enum bt_component_class_type {
	BT_COMPONENT_CLASS_TYPE_UNKNOWN =	-1,

	/** A source component is a notification generator. */
	BT_COMPONENT_CLASS_TYPE_SOURCE =	0,

	/** A sink component handles incoming notifications. */
	BT_COMPONENT_CLASS_TYPE_SINK =		1,

	/** A filter component implements both Source and Sink interfaces. */
	BT_COMPONENT_CLASS_TYPE_FILTER =	2,
};

struct bt_component_class_query_method_return {
	struct bt_value *result;
	enum bt_query_status status;
};

typedef enum bt_component_status (*bt_component_class_init_method)(
		struct bt_private_component *private_component,
		struct bt_value *params, void *init_method_data);

typedef void (*bt_component_class_finalize_method)(
		struct bt_private_component *private_component);

typedef enum bt_notification_iterator_status
		(*bt_component_class_notification_iterator_init_method)(
		struct bt_private_connection_private_notification_iterator *notification_iterator,
		struct bt_private_port *private_port);

typedef void (*bt_component_class_notification_iterator_finalize_method)(
		struct bt_private_connection_private_notification_iterator *notification_iterator);

typedef enum bt_notification_iterator_status
(*bt_component_class_notification_iterator_next_method)(
		struct bt_private_connection_private_notification_iterator *notification_iterator,
		bt_notification_array notifs, uint64_t capacity,
		uint64_t *count);

typedef struct bt_component_class_query_method_return (*bt_component_class_query_method)(
		struct bt_component_class *component_class,
		struct bt_query_executor *query_executor,
		const char *object, struct bt_value *params);

typedef enum bt_component_status (*bt_component_class_accept_port_connection_method)(
		struct bt_private_component *private_component,
		struct bt_private_port *self_private_port,
		struct bt_port *other_port);

typedef void (*bt_component_class_port_connected_method)(
		struct bt_private_component *private_component,
		struct bt_private_port *self_private_port,
		struct bt_port *other_port);

typedef void (*bt_component_class_port_disconnected_method)(
		struct bt_private_component *private_component,
		struct bt_private_port *private_port);

extern int bt_component_class_set_init_method(
		struct bt_component_class *component_class,
		bt_component_class_init_method method);

extern int bt_component_class_set_finalize_method(
		struct bt_component_class *component_class,
		bt_component_class_finalize_method method);

extern int bt_component_class_set_accept_port_connection_method(
		struct bt_component_class *component_class,
		bt_component_class_accept_port_connection_method method);

extern int bt_component_class_set_port_connected_method(
		struct bt_component_class *component_class,
		bt_component_class_port_connected_method method);

extern int bt_component_class_set_port_disconnected_method(
		struct bt_component_class *component_class,
		bt_component_class_port_disconnected_method method);

extern int bt_component_class_set_query_method(
		struct bt_component_class *component_class,
		bt_component_class_query_method method);

extern int bt_component_class_set_description(
		struct bt_component_class *component_class,
		const char *description);

extern int bt_component_class_set_help(
		struct bt_component_class *component_class,
		const char *help);

extern int bt_component_class_freeze(
		struct bt_component_class *component_class);

/**
 * Get a component class' name.
 *
 * @param component_class	Component class of which to get the name
 * @returns			Name of the component class
 */
extern const char *bt_component_class_get_name(
		struct bt_component_class *component_class);

/**
 * Get a component class' description.
 *
 * Component classes may provide an optional description. It may, however,
 * opt not to.
 *
 * @param component_class	Component class of which to get the description
 * @returns			Description of the component class, or NULL.
 */
extern const char *bt_component_class_get_description(
		struct bt_component_class *component_class);

extern const char *bt_component_class_get_help(
		struct bt_component_class *component_class);

/**
 * Get a component class' type.
 *
 * @param component_class	Component class of which to get the type
 * @returns			One of #bt_component_type
 */
extern enum bt_component_class_type bt_component_class_get_type(
		struct bt_component_class *component_class);

static inline
bt_bool bt_component_class_is_source(struct bt_component_class *component_class)
{
	return bt_component_class_get_type(component_class) ==
		BT_COMPONENT_CLASS_TYPE_SOURCE;
}

static inline
bt_bool bt_component_class_is_filter(struct bt_component_class *component_class)
{
	return bt_component_class_get_type(component_class) ==
		BT_COMPONENT_CLASS_TYPE_FILTER;
}

static inline
bt_bool bt_component_class_is_sink(struct bt_component_class *component_class)
{
	return bt_component_class_get_type(component_class) ==
		BT_COMPONENT_CLASS_TYPE_SINK;
}

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_COMPONENT_CLASS_H */
