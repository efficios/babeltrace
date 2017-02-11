#ifndef BABELTRACE_COMPONENT_COMPONENT_CLASS_INTERNAL_H
#define BABELTRACE_COMPONENT_COMPONENT_CLASS_INTERNAL_H

/*
 * BabelTrace - Component Class Internal
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

#include <babeltrace/component/component.h>
#include <babeltrace/component/component-class.h>
#include <babeltrace/component/component-class-source.h>
#include <babeltrace/component/component-class-filter.h>
#include <babeltrace/component/component-class-sink.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/object-internal.h>
#include <stdbool.h>
#include <glib.h>

struct bt_component_class;

typedef void (*bt_component_class_destroy_listener_func)(
		struct bt_component_class *class, void *data);

struct bt_component_class_destroy_listener {
	bt_component_class_destroy_listener_func func;
	void *data;
};

struct bt_component_class {
	struct bt_object base;
	enum bt_component_class_type type;
	GString *name;
	GString *description;
	GString *help;
	struct {
		bt_component_class_init_method init;
		bt_component_class_destroy_method destroy;
		bt_component_class_query_info_method query_info;
	} methods;
	/* Array of struct bt_component_class_destroy_listener */
	GArray *destroy_listeners;
	bool frozen;
};

struct bt_component_class_iterator_methods {
	bt_component_class_notification_iterator_init_method init;
	bt_component_class_notification_iterator_destroy_method destroy;
	bt_component_class_notification_iterator_get_method get;
	bt_component_class_notification_iterator_next_method next;
	bt_component_class_notification_iterator_seek_time_method seek_time;
};

struct bt_component_class_source {
	struct bt_component_class parent;
	struct {
		struct bt_component_class_iterator_methods iterator;
	} methods;
};

struct bt_component_class_sink {
	struct bt_component_class parent;
	struct {
		bt_component_class_sink_consume_method consume;
		bt_component_class_sink_add_iterator_method add_iterator;
	} methods;
};

struct bt_component_class_filter {
	struct bt_component_class parent;
	struct {
		struct bt_component_class_iterator_methods iterator;
		bt_component_class_filter_add_iterator_method add_iterator;
	} methods;
};

BT_HIDDEN
int bt_component_class_add_destroy_listener(struct bt_component_class *class,
		bt_component_class_destroy_listener_func func, void *data);

#endif /* BABELTRACE_COMPONENT_COMPONENT_CLASS_INTERNAL_H */
