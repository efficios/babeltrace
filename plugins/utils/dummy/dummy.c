/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/plugin/plugin-dev.h>
#include <babeltrace/component/component.h>
#include <babeltrace/component/component-sink.h>
#include <babeltrace/component/notification/iterator.h>
#include <babeltrace/component/notification/notification.h>
#include <assert.h>

enum bt_component_status dummy_consume(struct bt_component *component)
{
	enum bt_component_status ret;
	struct bt_notification *notif = NULL;
	struct bt_notification_iterator *it = NULL;
	unsigned int it_count;
	size_t i;
	bool got_one = false;

	ret = bt_component_sink_get_input_count(component, &it_count);
	assert(ret == 0);

	for (i = 0; i < it_count; i++) {
		enum bt_notification_iterator_status it_ret;

		ret = bt_component_sink_get_input_iterator(component, i, &it);
		assert(ret == 0);
		it_ret = bt_notification_iterator_next(it);
		switch (it_ret) {
		case BT_NOTIFICATION_ITERATOR_STATUS_ERROR:
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		case BT_NOTIFICATION_ITERATOR_STATUS_END:
			ret = BT_COMPONENT_STATUS_END;
			BT_PUT(it);
			continue;
		default:
			break;
		}

		notif = bt_notification_iterator_get_notification(it);
		if (!notif) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}

		/*
		 * Dummy! I'm doing nothing with this notification,
		 * NOTHING.
		 */
		got_one = true;
		BT_PUT(it);
		BT_PUT(notif);
	}

	if (!got_one) {
		ret = BT_COMPONENT_STATUS_END;
	}

end:
	bt_put(it);
	bt_put(notif);
	return ret;
}
