#ifndef BABELTRACE_COMPONENT_FILTER_H
#define BABELTRACE_COMPONENT_FILTER_H

/*
 * BabelTrace - Filter Plug-in Interface
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <stdint.h>
#include <babeltrace/component/component.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_component;
struct bt_notification_iterator;

/**
 * Add a notification iterator to a filter component.
 *
 * @param component	Component instance
 * @param iterator	Notification iterator to add
 * @returns		One of #bt_component_status values
 */
extern
enum bt_component_status bt_component_filter_add_iterator(
		struct bt_component *component,
		struct bt_notification_iterator *iterator);

/**
 * Create an iterator on a component instance.
 *
 * @param component	Component instance
 * @returns		Notification iterator instance
 */
extern
struct bt_notification_iterator *bt_component_filter_create_iterator(
		struct bt_component *component);

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

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_COMPONENT_FILTER_H */
