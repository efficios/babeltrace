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
#include <babeltrace/plugin/component.h>
#include <babeltrace/plugin/source-internal.h>
#include <babeltrace/plugin/notification/iterator.h>
#include <babeltrace/plugin/notification/iterator-internal.h>

static
void bt_notification_iterator_destroy(struct bt_object *obj)
{
	struct bt_notification_iterator *iterator;

	assert(obj);
	iterator = container_of(obj, struct bt_notification_iterator,
			base);
	assert(iterator->user_destroy || !iterator->user_data);
	iterator->user_destroy(iterator);
	g_free(iterator);
}

BT_HIDDEN
struct bt_notification_iterator *bt_notification_iterator_create(
		struct bt_component *component)
{
	struct bt_notification_iterator *iterator = NULL;

	if (!component ||
		bt_component_get_type(component) != BT_COMPONENT_TYPE_SOURCE) {
		goto end;
	}

	iterator = g_new0(struct bt_notification_iterator, 1);
	if (!iterator) {
		goto end;
	}

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

	if (!iterator || !iterator->get || !iterator->next) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_INVAL;
		goto end;
	}
end:
	return ret;
}

enum bt_notification_iterator_status bt_notification_iterator_set_get_cb(
		struct bt_notification_iterator *iterator,
		bt_notification_iterator_get_cb get)
{
	enum bt_notification_iterator_status ret =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;

	if (!iterator || !get) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_INVAL;
		goto end;
	}

	
end:
	return ret;
}
