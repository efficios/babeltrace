#ifndef CTF_NOTIF_ITER_H
#define CTF_NOTIF_ITER_H

/*
 * Babeltrace - CTF notification iterator
 *                  ¯¯¯¯¯        ¯¯¯¯
 * Copyright (c) 2015-2016 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015-2016 Philippe Proulx <pproulx@efficios.com>
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
#include <stdio.h>
#include <stddef.h>
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/ctf-ir/event.h>
#include <babeltrace/graph/clock-class-priority-map.h>
#include <babeltrace/babeltrace-internal.h>

/**
 * @file ctf-notif-iter.h
 *
 * CTF notification iterator
 *     ¯¯¯¯¯        ¯¯¯¯
 * This is a common internal API used by CTF source plugins. It allows
 * one to get notifications from a user-provided medium.
 */

/**
 * Medium operations status codes.
 */
enum bt_ctf_notif_iter_medium_status {
	/**
	 * End of file.
	 *
	 * The medium function called by the notification iterator
	 * function reached the end of the file.
	 */
	BT_CTF_NOTIF_ITER_MEDIUM_STATUS_EOF =	-4,

	/**
	 * There is no data available right now, try again later.
	 */
	BT_CTF_NOTIF_ITER_MEDIUM_STATUS_AGAIN =	-3,

	/** Invalid argument. */
	BT_CTF_NOTIF_ITER_MEDIUM_STATUS_INVAL =	-2,

	/** General error. */
	BT_CTF_NOTIF_ITER_MEDIUM_STATUS_ERROR =	-1,

	/** Everything okay. */
	BT_CTF_NOTIF_ITER_MEDIUM_STATUS_OK =		0,
};

/**
 * CTF notification iterator API status code.
 */
enum bt_ctf_notif_iter_status {
	/**
	 * End of file.
	 *
	 * The medium function called by the notification iterator
	 * function reached the end of the file.
	 */
	BT_CTF_NOTIF_ITER_STATUS_EOF =	-4,

	/**
	 * There is no data available right now, try again later.
	 *
	 * Some condition resulted in the
	 * bt_ctf_notif_iter_medium_ops::request_bytes() user function not
	 * having access to any data now. You should retry calling the
	 * last called notification iterator function once the situation
	 * is resolved.
	 */
	BT_CTF_NOTIF_ITER_STATUS_AGAIN =	-3,

	/** Invalid argument. */
	BT_CTF_NOTIF_ITER_STATUS_INVAL =	-2,

	/** General error. */
	BT_CTF_NOTIF_ITER_STATUS_ERROR =	-1,

	/** Everything okay. */
	BT_CTF_NOTIF_ITER_STATUS_OK =	0,
};

/**
 * Medium operations.
 *
 * Those user functions are called by the notification iterator
 * functions to request medium actions.
 */
struct bt_ctf_notif_iter_medium_ops {
	/**
	 * Returns the next byte buffer to be used by the binary file
	 * reader to deserialize binary data.
	 *
	 * This function \em must be defined.
	 *
	 * The purpose of this function is to return a buffer of bytes
	 * to the notification iterator, of a maximum of \p request_sz
	 * bytes. If this function cannot return a buffer of at least
	 * \p request_sz bytes, it may return a smaller buffer. In
	 * either cases, \p buffer_sz must be set to the returned buffer
	 * size (in bytes).
	 *
	 * The returned buffer's ownership remains the medium, in that
	 * it won't be freed by the notification iterator functions. The
	 * returned buffer won't be modified by the notification
	 * iterator functions either.
	 *
	 * When this function is called for the first time for a given
	 * file, the offset within the file is considered to be 0. The
	 * next times this function is called, the returned buffer's
	 * byte offset within the complete file must be the previous
	 * offset plus the last returned value of \p buffer_sz by this
	 * medium.
	 *
	 * This function must return one of the following statuses:
	 *
	 *   - <b>#BT_CTF_NOTIF_ITER_MEDIUM_STATUS_OK</b>: Everything
	 *     is okay, i.e. \p buffer_sz is set to a positive value
	 *     reflecting the number of available bytes in the buffer
	 *     starting at the address written in \p buffer_addr.
	 *   - <b>#BT_CTF_NOTIF_ITER_MEDIUM_STATUS_AGAIN</b>: No data is
	 *     available right now. In this case, the notification
	 *     iterator function called by the user returns
	 *     #BT_CTF_NOTIF_ITER_STATUS_AGAIN, and it is the user's
	 *     responsibility to make sure enough data becomes available
	 *     before calling the \em same notification iterator
	 *     function again to continue the decoding process.
	 *   - <b>#BT_CTF_NOTIF_ITER_MEDIUM_STATUS_EOF</b>: The end of
	 *     the file was reached, and no more data will ever be
	 *     available for this file. In this case, the notification
	 *     iterator function called by the user returns
	 *     #BT_CTF_NOTIF_ITER_STATUS_EOF. This must \em not be
	 *     returned when returning at least one byte of data to the
	 *     caller, i.e. this must be returned when there's
	 *     absolutely nothing left; should the request size be
	 *     larger than what's left in the file, this function must
	 *     return what's left, setting \p buffer_sz to the number of
	 *     remaining bytes, and return
	 *     #BT_CTF_NOTIF_ITER_MEDIUM_STATUS_EOF on the \em following
	 *     call.
	 *   - <b>#BT_CTF_NOTIF_ITER_MEDIUM_STATUS_ERROR</b>: A fatal
	 *     error occured during this operation. In this case, the
	 *     notification iterator function called by the user returns
	 *     #BT_CTF_NOTIF_ITER_STATUS_ERROR.
	 *
	 * If #BT_CTF_NOTIF_ITER_MEDIUM_STATUS_OK is not returned, the
	 * values of \p buffer_sz and \p buffer_addr are \em ignored by
	 * the caller.
	 *
	 * @param request_sz	Requested buffer size (bytes)
	 * @param buffer_addr	Returned buffer address
	 * @param buffer_sz	Returned buffer's size (bytes)
	 * @param data		User data
	 * @returns		Status code (see description above)
	 */
	enum bt_ctf_notif_iter_medium_status (* request_bytes)(
			size_t request_sz, uint8_t **buffer_addr,
			size_t *buffer_sz, void *data);

	/**
	 * Returns a stream instance (weak reference) for the given
	 * stream class.
	 *
	 * This is called after a packet header is read, and the
	 * corresponding stream class is found by the notification
	 * iterator.
	 *
	 * @param stream_class	Stream class associated to the stream
	 * @param data		User data
	 * @returns		Stream instance (weak reference) or
	 *			\c NULL on error
	 */
	struct bt_ctf_stream * (* get_stream)(
			struct bt_ctf_stream_class *stream_class, void *data);
};

/** CTF notification iterator. */
struct bt_ctf_notif_iter;

// TODO: Replace by the real thing
enum bt_ctf_notif_iter_notif_type {
	BT_CTF_NOTIF_ITER_NOTIF_NEW_PACKET,
	BT_CTF_NOTIF_ITER_NOTIF_END_OF_PACKET,
	BT_CTF_NOTIF_ITER_NOTIF_EVENT,
};

struct bt_ctf_notif_iter_notif {
	enum bt_ctf_notif_iter_notif_type type;
};

struct bt_ctf_notif_iter_notif_new_packet {
	struct bt_ctf_notif_iter_notif base;
	struct bt_ctf_packet *packet;
};

struct bt_ctf_notif_iter_notif_end_of_packet {
	struct bt_ctf_notif_iter_notif base;
	struct bt_ctf_packet *packet;
};

struct bt_ctf_notif_iter_notif_event {
	struct bt_ctf_notif_iter_notif base;
	struct bt_ctf_event *event;
};

/**
 * Creates a CTF notification iterator.
 *
 * Upon successful completion, the reference count of \p trace is
 * incremented.
 *
 * @param trace			Trace to read
 * @param cc_prio_map		Clock class priority map to use when
 *				creating the event notifications
 * @param max_request_sz	Maximum buffer size, in bytes, to
 *				request to
 *				bt_ctf_notif_iter_medium_ops::request_bytes()
 * 				at a time
 * @param medops		Medium operations
 * @param medops_data		User data (passed to medium operations)
 * @param err_stream		Error stream (can be \c NULL to disable)
 * @returns			New CTF notification iterator on
 *				success, or \c NULL on error
 */
BT_HIDDEN
struct bt_ctf_notif_iter *bt_ctf_notif_iter_create(struct bt_ctf_trace *trace,
	struct bt_clock_class_priority_map *cc_prio_map,
	size_t max_request_sz, struct bt_ctf_notif_iter_medium_ops medops,
	void *medops_data, FILE *err_stream);

/**
 * Destroys a CTF notification iterator, freeing all internal resources.
 *
 * The registered trace's reference count is decremented.
 *
 * @param notif_iter		CTF notification iterator
 */
BT_HIDDEN
void bt_ctf_notif_iter_destroy(struct bt_ctf_notif_iter *notif_iter);

/**
 * Returns the next notification from a CTF notification iterator.
 *
 * Upon successful completion, #BT_CTF_NOTIF_ITER_STATUS_OK is
 * returned, and the next notification is written to \p notif.
 * In this case, the caller is responsible for calling
 * bt_notification_put() on the returned notification.
 *
 * If this function returns #BT_CTF_NOTIF_ITER_STATUS_AGAIN, the caller
 * should make sure that data becomes available to its medium, and
 * call this function again, until another status is returned.
 *
 * @param notif_iter		CTF notification iterator
 * @param notification		Returned notification if the function's
 *				return value is #BT_CTF_NOTIF_ITER_STATUS_OK
 * @returns			One of #bt_ctf_notif_iter_status values
 */
BT_HIDDEN
enum bt_ctf_notif_iter_status bt_ctf_notif_iter_get_next_notification(
		struct bt_ctf_notif_iter *notit,
		struct bt_notification **notification);

#endif /* CTF_NOTIF_ITER_H */
