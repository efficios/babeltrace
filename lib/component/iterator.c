/*
 * iterator.c
 *
 * Babeltrace Notification Iterator
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

#include <babeltrace/compiler.h>
#include <babeltrace/ref.h>
#include <babeltrace/component/component.h>
#include <babeltrace/component/component-source-internal.h>
#include <babeltrace/component/component-class-internal.h>
#include <babeltrace/component/notification/iterator.h>
#include <babeltrace/component/notification/iterator-internal.h>

static
void bt_notification_iterator_destroy(struct bt_object *obj)
{
	struct bt_notification_iterator *iterator;
	struct bt_component_class *comp_class;

	assert(obj);
	iterator = container_of(obj, struct bt_notification_iterator,
			base);
	assert(iterator->component);
	comp_class = iterator->component->class;

	/* Call user-defined destroy method */
	switch (comp_class->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		struct bt_component_class_source *source_class;

		source_class = container_of(comp_class, struct bt_component_class_source, parent);

		if (source_class->methods.iterator.destroy) {
			source_class->methods.iterator.destroy(iterator);
		}
		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		struct bt_component_class_filter *filter_class;

		filter_class = container_of(comp_class, struct bt_component_class_filter, parent);

		if (filter_class->methods.iterator.destroy) {
			filter_class->methods.iterator.destroy(iterator);
		}
		break;
	}
	default:
		/* Unreachable */
		assert(0);
	}

	BT_PUT(iterator->component);
	g_free(iterator);
}

BT_HIDDEN
struct bt_notification_iterator *bt_notification_iterator_create(
		struct bt_component *component)
{
	enum bt_component_class_type type;
	struct bt_notification_iterator *iterator = NULL;

	if (!component) {
		goto end;
	}

	type = bt_component_get_class_type(component);
	switch (type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	case BT_COMPONENT_CLASS_TYPE_FILTER:
		break;
	default:
		goto end;
	}

	iterator = g_new0(struct bt_notification_iterator, 1);
	if (!iterator) {
		goto end;
	}

	iterator->component = bt_get(component);
	bt_object_init(iterator, bt_notification_iterator_destroy);
end:
	return iterator;
}

BT_HIDDEN
enum bt_notification_iterator_status bt_notification_iterator_validate(
		struct bt_notification_iterator *iterator)
{
	enum bt_notification_iterator_status ret =
			BT_NOTIFICATION_ITERATOR_STATUS_OK;

	if (!iterator) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_INVAL;
		goto end;
	}
end:
	return ret;
}

void *bt_notification_iterator_get_private_data(
		struct bt_notification_iterator *iterator)
{
	return iterator ? iterator->user_data : NULL;
}

enum bt_notification_iterator_status
bt_notification_iterator_set_private_data(
		struct bt_notification_iterator *iterator, void *data)
{
	enum bt_notification_iterator_status ret =
			BT_NOTIFICATION_ITERATOR_STATUS_OK;

	if (!iterator) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_INVAL;
		goto end;
	}

	iterator->user_data = data;
end:
	return ret;
}

struct bt_notification *bt_notification_iterator_get_notification(
		struct bt_notification_iterator *iterator)
{
	bt_component_class_notification_iterator_get_method get_method = NULL;

	assert(iterator);
	assert(iterator->component);
	assert(iterator->component->class);

	switch (iterator->component->class->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		struct bt_component_class_source *source_class =
			container_of(iterator->component->class,
				struct bt_component_class_source, parent);

		assert(source_class->methods.iterator.get);
		get_method = source_class->methods.iterator.get;
		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		struct bt_component_class_filter *filter_class =
			container_of(iterator->component->class,
				struct bt_component_class_filter, parent);

		assert(filter_class->methods.iterator.get);
		get_method = filter_class->methods.iterator.get;
		break;
	}
	default:
		assert(false);
		break;
	}

	assert(get_method);
	return get_method(iterator);
}

enum bt_notification_iterator_status
bt_notification_iterator_next(struct bt_notification_iterator *iterator)
{
	bt_component_class_notification_iterator_next_method next_method = NULL;

	assert(iterator);
	assert(iterator->component);
	assert(iterator->component->class);

	switch (iterator->component->class->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		struct bt_component_class_source *source_class =
			container_of(iterator->component->class,
				struct bt_component_class_source, parent);

		assert(source_class->methods.iterator.next);
		next_method = source_class->methods.iterator.next;
		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		struct bt_component_class_filter *filter_class =
			container_of(iterator->component->class,
				struct bt_component_class_filter, parent);

		assert(filter_class->methods.iterator.next);
		next_method = filter_class->methods.iterator.next;
		break;
	}
	default:
		assert(false);
		break;
	}

	assert(next_method);
	return next_method(iterator);
}

struct bt_component *bt_notification_iterator_get_component(
		struct bt_notification_iterator *iterator)
{
	return bt_get(iterator->component);
}

enum bt_notification_iterator_status bt_notification_iterator_seek_time(
		struct bt_notification_iterator *iterator,
		enum bt_notification_iterator_seek_origin seek_origin,
		int64_t time)
{
	enum bt_notification_iterator_status ret =
			BT_NOTIFICATION_ITERATOR_STATUS_UNSUPPORTED;
	return ret;
}
