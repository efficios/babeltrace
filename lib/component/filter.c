/*
 * filter.c
 *
 * Babeltrace Filter Component
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

#include <babeltrace/compiler.h>
#include <babeltrace/values.h>
#include <babeltrace/component/input.h>
#include <babeltrace/component/filter-internal.h>
#include <babeltrace/component/component-internal.h>
#include <babeltrace/component/component-class-internal.h>
#include <babeltrace/component/notification/notification.h>
#include <babeltrace/component/notification/iterator-internal.h>

enum bt_component_status bt_component_filter_set_minimum_input_count(
		struct bt_component *component,
		unsigned int minimum)
{
	struct bt_component_filter *filter;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!component) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (bt_component_get_class_type(component) != BT_COMPONENT_CLASS_TYPE_FILTER) {
		ret = BT_COMPONENT_STATUS_UNSUPPORTED;
		goto end;
	}

	if (!component->initializing) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	filter = container_of(component, struct bt_component_filter, parent);
	filter->input.min_count = minimum;
end:
	return ret;
}

enum bt_component_status bt_component_filter_set_maximum_input_count(
		struct bt_component *component,
		unsigned int maximum)
{
	struct bt_component_filter *filter;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!component) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (bt_component_get_class_type(component) != BT_COMPONENT_CLASS_TYPE_FILTER) {
		ret = BT_COMPONENT_STATUS_UNSUPPORTED;
		goto end;
	}

	if (!component->initializing) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	filter = container_of(component, struct bt_component_filter, parent);
	filter->input.max_count = maximum;
end:
	return ret;
}

enum bt_component_status
bt_component_filter_get_input_count(struct bt_component *component,
		unsigned int *count)
{
	struct bt_component_filter *filter;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!component || !count) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (bt_component_get_class_type(component) != BT_COMPONENT_CLASS_TYPE_FILTER) {
		ret = BT_COMPONENT_STATUS_UNSUPPORTED;
		goto end;
	}

	filter = container_of(component, struct bt_component_filter, parent);
	*count = (unsigned int) filter->input.iterators->len;
end:
	return ret;
}

enum bt_component_status
bt_component_filter_get_input_iterator(struct bt_component *component,
		unsigned int input, struct bt_notification_iterator **iterator)
{
	struct bt_component_filter *filter;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!component || !iterator) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (bt_component_get_class_type(component) != BT_COMPONENT_CLASS_TYPE_FILTER) {
		ret = BT_COMPONENT_STATUS_UNSUPPORTED;
		goto end;
	}

	filter = container_of(component, struct bt_component_filter, parent);
	if (input >= (unsigned int) filter->input.iterators->len) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	*iterator = bt_get(g_ptr_array_index(filter->input.iterators, input));
end:
	return ret;
}

enum bt_component_status bt_component_filter_add_iterator(
		struct bt_component *component,
		struct bt_notification_iterator *iterator)
{
	struct bt_component_filter *filter;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_component_class_filter *filter_class;

	if (!component || !iterator) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (bt_component_get_class_type(component) != BT_COMPONENT_CLASS_TYPE_FILTER) {
		ret = BT_COMPONENT_STATUS_UNSUPPORTED;
		goto end;
	}

	filter = container_of(component, struct bt_component_filter, parent);
	if (filter->input.iterators->len == filter->input.max_count) {
		ret = BT_COMPONENT_STATUS_UNSUPPORTED;
		goto end;
	}

	filter_class = container_of(component->class, struct bt_component_class_filter, parent);

	if (filter_class->methods.add_iterator) {
		ret = filter_class->methods.add_iterator(component, iterator);
		if (ret != BT_COMPONENT_STATUS_OK) {
			goto end;
		}
	}

	g_ptr_array_add(filter->input.iterators, bt_get(iterator));
end:
	return ret;
}

struct bt_notification_iterator *bt_component_filter_create_iterator(
		struct bt_component *component)
{
	return bt_component_create_iterator(component);
}

static
void bt_component_filter_destroy(struct bt_component *component)
{
	struct bt_component_filter *filter = container_of(component,
			struct bt_component_filter, parent);

	component_input_fini(&filter->input);
}

BT_HIDDEN
struct bt_component *bt_component_filter_create(
		struct bt_component_class *class, struct bt_value *params)
{
	struct bt_component_filter *filter = NULL;
	enum bt_component_status ret;

	filter = g_new0(struct bt_component_filter, 1);
	if (!filter) {
		goto end;
	}

	filter->parent.class = bt_get(class);
	ret = bt_component_init(&filter->parent, bt_component_filter_destroy);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	if (component_input_init(&filter->input)) {
		goto error;
	}
end:
	return filter ? &filter->parent : NULL;
error:
	BT_PUT(filter);
	goto end;
}

BT_HIDDEN
enum bt_component_status bt_component_filter_validate(
		struct bt_component *component)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_component_filter *filter;

	if (!component) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (!component->class) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (component->class->type != BT_COMPONENT_CLASS_TYPE_FILTER) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	filter = container_of(component, struct bt_component_filter, parent);
	ret = component_input_validate(&filter->input);
	if (ret) {
		goto end;
	}
end:
	return ret;
}
