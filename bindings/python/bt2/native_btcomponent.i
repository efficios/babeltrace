/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

%{
#include <babeltrace/component/component.h>
#include <babeltrace/component/component-source.h>
#include <babeltrace/component/component-sink.h>
#include <babeltrace/component/component-filter.h>
%}

/* Types */
struct bt_component;

/* Status */
enum bt_component_status {
	BT_COMPONENT_STATUS_OK =		0,
	BT_COMPONENT_STATUS_END =		1,
	BT_COMPONENT_STATUS_AGAIN =		2,
	BT_COMPONENT_STATUS_ERROR =		-1,
	BT_COMPONENT_STATUS_UNSUPPORTED =	-2,
	BT_COMPONENT_STATUS_INVALID =		-3,
	BT_COMPONENT_STATUS_NOMEM =		-4,
};

/* General functions */
struct bt_component *bt_component_create_with_init_method_data(
		struct bt_component_class *component_class, const char *name,
		struct bt_value *params, void *init_method_data);
const char *bt_component_get_name(struct bt_component *component);
struct bt_component_class *bt_component_get_class(
		struct bt_component *component);
enum bt_component_class_type bt_component_get_class_type(
		struct bt_component *component);

/* Source component functions */
struct bt_notification_iterator *bt_component_source_create_notification_iterator_with_init_method_data(
        struct bt_component *component, void *init_method_data);

/* Filter component functions */
%{
static struct bt_notification_iterator *bt_component_filter_get_input_iterator_private(
		struct bt_component *filter, unsigned int index)
{
	struct bt_notification_iterator *iterator = NULL;

	if (bt_component_filter_get_input_iterator(filter, index, &iterator)) {
		iterator = NULL;
		goto end;
	}

end:
	return iterator;
}
%}

struct bt_notification_iterator *bt_component_filter_create_notification_iterator_with_init_method_data(
		struct bt_component *component, void *init_method_data);
enum bt_component_status bt_component_filter_add_iterator(
		struct bt_component *component,
		struct bt_notification_iterator *iterator);
enum bt_component_status bt_component_filter_set_minimum_input_count(
		struct bt_component *filter, unsigned int minimum);
enum bt_component_status bt_component_filter_set_maximum_input_count(
		struct bt_component *filter, unsigned int maximum);
enum bt_component_status bt_component_filter_get_input_count(
		struct bt_component *filter, unsigned int *OUTPUT);
struct bt_notification_iterator *bt_component_filter_get_input_iterator_private(
		struct bt_component *filter, unsigned int index);

/* Sink component functions */
%{
static struct bt_notification_iterator *bt_component_sink_get_input_iterator_private(
		struct bt_component *sink, unsigned int index)
{
	struct bt_notification_iterator *iterator = NULL;

	if (bt_component_sink_get_input_iterator(sink, index, &iterator)) {
		iterator = NULL;
		goto end;
	}

end:
	return iterator;
}
%}

enum bt_component_status bt_component_sink_add_iterator(
		struct bt_component *component,
		struct bt_notification_iterator *iterator);
enum bt_component_status bt_component_sink_consume(
		struct bt_component *component);
enum bt_component_status bt_component_sink_set_minimum_input_count(
		struct bt_component *sink, unsigned int minimum);
enum bt_component_status bt_component_sink_set_maximum_input_count(
		struct bt_component *sink, unsigned int maximum);
enum bt_component_status bt_component_sink_get_input_count(
		struct bt_component *sink, unsigned int *OUTPUT);
struct bt_notification_iterator *bt_component_sink_get_input_iterator_private(
		struct bt_component *sink, unsigned int index);
