#ifndef BABELTRACE_PLUGIN_NOTIFICATION_ITERATOR_H
#define BABELTRACE_PLUGIN_NOTIFICATION_ITERATOR_H

/*
 * BabelTrace - Plug-in Notification Iterator
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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_notification;
struct bt_notification_iterator;

/**
 * Status code. Errors are always negative.
 */
enum bt_notification_iterator_status {
	/** Invalid arguments. */
	/* -22 for compatibility with -EINVAL */
	BT_NOTIFICATION_ITERATOR_STATUS_INVAL = -22,

	/** End of trace. */
	BT_NOTIFICATION_ITERATOR_STATUS_EOT = -3,

	/** General error. */
	BT_NOTIFICATION_ITERATOR_STATUS_ERROR = -2,

	/** Unsupported iterator feature. */
	BT_NOTIFICATION_ITERATOR_STATUS_UNSUPPORTED = -1,

	/** No error, okay. */
	BT_NOTIFICATION_ITERATOR_STATUS_OK = 0,
};

/**
 * Notification iterator seek reference.
 */
enum bt_notification_iterator_seek_type {
	/** Seek at a time relative to the beginning of the trace. */
	BT_NOTIFICATION_ITERATOR_SEEK_TYPE_BEGIN = 0,

	/** Seek at a time relative to the current position. */
	BT_NOTIFICATION_ITERATOR_SEEK_TYPE_CURRENT = 1,

	/** Seek at a time relative to the end of the trace. */
	BT_NOTIFICATION_ITERATOR_SEEK_TYPE_END = 1,
};

/**
 * Get current notification at iterator's position.
 *
 * This functions will <b>not</b> advance the cursor's position.
 * The returned notification's reference count is already incremented.
 *
 * @param iterator	Iterator instance
 * @returns		Returns a bt_notification instance
 *
 * @see bt_notification_put()
 */
extern struct bt_notification *bt_notification_iterator_get_notification(
		struct bt_notification_iterator *iterator);

/**
 * Advance the iterator's position forward.
 *
 * This function can be called repeatedly to iterate through the iterator's
 * associated trace.
 *
 * @param iterator	Iterator instance
 * @returns		Returns a bt_notification instance
 *
 * @see bt_notification_iterator_get_notification()
 */
extern enum bt_notification_iterator_status
bt_notification_iterator_next(struct bt_notification_iterator *iterator);

/**
 * Seek iterator to position.
 *
 * Sets the iterator's position for the trace associated with the iterator.
 * The new position is computed by adding \p time to the position specified
 * by \p whence.
 *
 * @param iterator	Iterator instance
 * @param whence	One of #bt_notification_iterator_seek_type values.
 * @returns		One of #bt_notification_iterator_status values;
 *			if \iterator does not support seeking,
 *			#BT_NOTIFICATION_ITERATOR_STATUS_UNSUPPORTED is
 *			returned.
 *
 * @see bt_notification_iterator_get_notification()
 */
extern enum bt_notification_iterator_status *bt_notification_iterator_seek(
		struct bt_notification_iterator *iterator, int whence,
		int64_t time);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PLUGIN_NOTIFICATION_ITERATOR_H */
