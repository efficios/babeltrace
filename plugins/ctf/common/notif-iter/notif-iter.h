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
#include <babeltrace/babeltrace.h>
#include <babeltrace/babeltrace-internal.h>

#include "../metadata/ctf-meta.h"

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
enum bt_notif_iter_medium_status {
	/**
	 * End of file.
	 *
	 * The medium function called by the notification iterator
	 * function reached the end of the file.
	 */
	BT_NOTIF_ITER_MEDIUM_STATUS_EOF = 1,

	/**
	 * There is no data available right now, try again later.
	 */
	BT_NOTIF_ITER_MEDIUM_STATUS_AGAIN = 11,

	/** Unsupported operation. */
	BT_NOTIF_ITER_MEDIUM_STATUS_UNSUPPORTED = -3,

	/** Invalid argument. */
	BT_NOTIF_ITER_MEDIUM_STATUS_INVAL = -2,

	/** General error. */
	BT_NOTIF_ITER_MEDIUM_STATUS_ERROR = -1,

	/** Everything okay. */
	BT_NOTIF_ITER_MEDIUM_STATUS_OK = 0,
};

/**
 * CTF notification iterator API status code.
 */
enum bt_notif_iter_status {
	/**
	 * End of file.
	 *
	 * The medium function called by the notification iterator
	 * function reached the end of the file.
	 */
	BT_NOTIF_ITER_STATUS_EOF = BT_NOTIF_ITER_MEDIUM_STATUS_EOF,

	/**
	 * There is no data available right now, try again later.
	 *
	 * Some condition resulted in the
	 * bt_notif_iter_medium_ops::request_bytes() user function not
	 * having access to any data now. You should retry calling the
	 * last called notification iterator function once the situation
	 * is resolved.
	 */
	BT_NOTIF_ITER_STATUS_AGAIN = BT_NOTIF_ITER_MEDIUM_STATUS_AGAIN,

	/** Invalid argument. */
	BT_NOTIF_ITER_STATUS_INVAL = BT_NOTIF_ITER_MEDIUM_STATUS_INVAL,

	/** Unsupported operation. */
	BT_NOTIF_ITER_STATUS_UNSUPPORTED = BT_NOTIF_ITER_MEDIUM_STATUS_UNSUPPORTED,

	/** General error. */
	BT_NOTIF_ITER_STATUS_ERROR = BT_NOTIF_ITER_MEDIUM_STATUS_ERROR,

	/** Everything okay. */
	BT_NOTIF_ITER_STATUS_OK =	0,
};

/**
 * CTF notification iterator seek operation directives.
 */
enum bt_notif_iter_seek_whence {
	/**
	 * Set the iterator's position to an absolute offset in the underlying
	 * medium.
	 */
	BT_NOTIF_ITER_SEEK_WHENCE_SET,
};

/**
 * Medium operations.
 *
 * Those user functions are called by the notification iterator
 * functions to request medium actions.
 */
struct bt_notif_iter_medium_ops {
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
	 *   - <b>#BT_NOTIF_ITER_MEDIUM_STATUS_OK</b>: Everything
	 *     is okay, i.e. \p buffer_sz is set to a positive value
	 *     reflecting the number of available bytes in the buffer
	 *     starting at the address written in \p buffer_addr.
	 *   - <b>#BT_NOTIF_ITER_MEDIUM_STATUS_AGAIN</b>: No data is
	 *     available right now. In this case, the notification
	 *     iterator function called by the user returns
	 *     #BT_NOTIF_ITER_STATUS_AGAIN, and it is the user's
	 *     responsibility to make sure enough data becomes available
	 *     before calling the \em same notification iterator
	 *     function again to continue the decoding process.
	 *   - <b>#BT_NOTIF_ITER_MEDIUM_STATUS_EOF</b>: The end of
	 *     the file was reached, and no more data will ever be
	 *     available for this file. In this case, the notification
	 *     iterator function called by the user returns
	 *     #BT_NOTIF_ITER_STATUS_EOF. This must \em not be
	 *     returned when returning at least one byte of data to the
	 *     caller, i.e. this must be returned when there's
	 *     absolutely nothing left; should the request size be
	 *     larger than what's left in the file, this function must
	 *     return what's left, setting \p buffer_sz to the number of
	 *     remaining bytes, and return
	 *     #BT_NOTIF_ITER_MEDIUM_STATUS_EOF on the \em following
	 *     call.
	 *   - <b>#BT_NOTIF_ITER_MEDIUM_STATUS_ERROR</b>: A fatal
	 *     error occured during this operation. In this case, the
	 *     notification iterator function called by the user returns
	 *     #BT_NOTIF_ITER_STATUS_ERROR.
	 *
	 * If #BT_NOTIF_ITER_MEDIUM_STATUS_OK is not returned, the
	 * values of \p buffer_sz and \p buffer_addr are \em ignored by
	 * the caller.
	 *
	 * @param request_sz	Requested buffer size (bytes)
	 * @param buffer_addr	Returned buffer address
	 * @param buffer_sz	Returned buffer's size (bytes)
	 * @param data		User data
	 * @returns		Status code (see description above)
	 */
	enum bt_notif_iter_medium_status (* request_bytes)(
			size_t request_sz, uint8_t **buffer_addr,
			size_t *buffer_sz, void *data);

	/**
	 * Repositions the underlying stream's position.
	 *
	 * This *optional* method repositions the underlying stream
	 * to a given absolute or relative position, as indicated by
	 * the whence directive.
	 *
	 * @param whence	One of #bt_notif_iter_seek_whence values
	 * @param offset	Offset to use for the given directive
	 * @param data		User data
	 * @returns		One of #bt_notif_iter_medium_status values
	 */
	enum bt_notif_iter_medium_status (* seek)(
			enum bt_notif_iter_seek_whence whence,
			off_t offset, void *data);

	/**
	 * Returns a stream instance (weak reference) for the given
	 * stream class.
	 *
	 * This is called after a packet header is read, and the
	 * corresponding stream class is found by the notification
	 * iterator.
	 *
	 * @param stream_class	Stream class of the stream to get
	 * @param stream_id	Stream (instance) ID of the stream
	 *			to get (-1ULL if not available)
	 * @param data		User data
	 * @returns		Stream instance (weak reference) or
	 *			\c NULL on error
	 */
	struct bt_stream * (* borrow_stream)(
			struct bt_stream_class *stream_class,
			int64_t stream_id, void *data);
};

/** CTF notification iterator. */
struct bt_notif_iter;

/**
 * Creates a CTF notification iterator.
 *
 * Upon successful completion, the reference count of \p trace is
 * incremented.
 *
 * @param trace			Trace to read
 * @param max_request_sz	Maximum buffer size, in bytes, to
 *				request to
 *				bt_notif_iter_medium_ops::request_bytes()
 * 				at a time
 * @param medops		Medium operations
 * @param medops_data		User data (passed to medium operations)
 * @returns			New CTF notification iterator on
 *				success, or \c NULL on error
 */
BT_HIDDEN
struct bt_notif_iter *bt_notif_iter_create(struct ctf_trace_class *tc,
	size_t max_request_sz, struct bt_notif_iter_medium_ops medops,
	void *medops_data);

/**
 * Destroys a CTF notification iterator, freeing all internal resources.
 *
 * The registered trace's reference count is decremented.
 *
 * @param notif_iter		CTF notification iterator
 */
BT_HIDDEN
void bt_notif_iter_destroy(struct bt_notif_iter *notif_iter);

/**
 * Returns the next notification from a CTF notification iterator.
 *
 * Upon successful completion, #BT_NOTIF_ITER_STATUS_OK is
 * returned, and the next notification is written to \p notif.
 * In this case, the caller is responsible for calling
 * bt_notification_put() on the returned notification.
 *
 * If this function returns #BT_NOTIF_ITER_STATUS_AGAIN, the caller
 * should make sure that data becomes available to its medium, and
 * call this function again, until another status is returned.
 *
 * @param notif_iter		CTF notification iterator
 * @param notification		Returned notification if the function's
 *				return value is #BT_NOTIF_ITER_STATUS_OK
 * @returns			One of #bt_notif_iter_status values
 */
BT_HIDDEN
enum bt_notif_iter_status bt_notif_iter_get_next_notification(
		struct bt_notif_iter *notit,
		struct bt_private_connection_private_notification_iterator *notif_iter,
		struct bt_notification **notification);

/**
 * Returns the first packet header and context fields. This function
 * never needs to call the `borrow_stream()` medium operation because
 * it does not create packet or event objects.
 *
 * @param notif_iter		CTF notification iterator
 * @param packet_header_field	Packet header field (\c NULL if there's
 *				no packet header field)
 * @param packet_context_field	Packet context field (\c NULL if there's
 *				no packet context field)
 * @returns			One of #bt_notif_iter_status values
 */
BT_HIDDEN
enum bt_notif_iter_status bt_notif_iter_borrow_packet_header_context_fields(
		struct bt_notif_iter *notit,
		struct bt_field **packet_header_field,
		struct bt_field **packet_context_field);

struct bt_notif_iter_packet_properties {
	uint64_t exp_packet_total_size;
	uint64_t exp_packet_content_size;
	uint64_t stream_class_id;
	int64_t data_stream_id;

	struct {
		uint64_t discarded_events;
		uint64_t packets;
		uint64_t beginning_clock;
		uint64_t end_clock;
	} snapshots;
};

BT_HIDDEN
enum bt_notif_iter_status bt_notif_iter_get_packet_properties(
		struct bt_notif_iter *notit,
		struct bt_notif_iter_packet_properties *props);

BT_HIDDEN
void bt_notif_iter_set_medops_data(struct bt_notif_iter *notit,
		void *medops_data);

BT_HIDDEN
enum bt_notif_iter_status bt_notif_iter_seek(
		struct bt_notif_iter *notit, off_t offset);

/*
 * Get the current packet's offset in bytes relative to the media's initial
 * position.
 */
BT_HIDDEN
off_t bt_notif_iter_get_current_packet_offset(
		struct bt_notif_iter *notit);

/* Get the current packet's size (in bits). */
BT_HIDDEN
off_t bt_notif_iter_get_current_packet_size(
		struct bt_notif_iter *notit);

/*
 * Resets the iterator so that the next requested medium bytes are
 * assumed to be the first bytes of a new stream. The first notification
 * which this iterator emits after calling bt_notif_iter_reset() is a
 * BT_NOTIFICATION_TYPE_STREAM_BEGIN one.
 */
BT_HIDDEN
void bt_notif_iter_reset(struct bt_notif_iter *notit);

/*
 * Notify the iterator that the trace class changed somehow (new
 * stream/event classes).
 */
BT_HIDDEN
void bt_notif_trace_class_changed(struct bt_notif_iter *notit);

static inline
const char *bt_notif_iter_medium_status_string(
		enum bt_notif_iter_medium_status status)
{
	switch (status) {
	case BT_NOTIF_ITER_MEDIUM_STATUS_EOF:
		return "BT_NOTIF_ITER_MEDIUM_STATUS_EOF";
	case BT_NOTIF_ITER_MEDIUM_STATUS_AGAIN:
		return "BT_NOTIF_ITER_MEDIUM_STATUS_AGAIN";
	case BT_NOTIF_ITER_MEDIUM_STATUS_INVAL:
		return "BT_NOTIF_ITER_MEDIUM_STATUS_INVAL";
	case BT_NOTIF_ITER_MEDIUM_STATUS_ERROR:
		return "BT_NOTIF_ITER_MEDIUM_STATUS_ERROR";
	case BT_NOTIF_ITER_MEDIUM_STATUS_OK:
		return "BT_NOTIF_ITER_MEDIUM_STATUS_OK";
	default:
		return "(unknown)";
	}
}

static inline
const char *bt_notif_iter_status_string(
		enum bt_notif_iter_status status)
{
	switch (status) {
	case BT_NOTIF_ITER_STATUS_EOF:
		return "BT_NOTIF_ITER_STATUS_EOF";
	case BT_NOTIF_ITER_STATUS_AGAIN:
		return "BT_NOTIF_ITER_STATUS_AGAIN";
	case BT_NOTIF_ITER_STATUS_INVAL:
		return "BT_NOTIF_ITER_STATUS_INVAL";
	case BT_NOTIF_ITER_STATUS_ERROR:
		return "BT_NOTIF_ITER_STATUS_ERROR";
	case BT_NOTIF_ITER_STATUS_OK:
		return "BT_NOTIF_ITER_STATUS_OK";
	default:
		return "(unknown)";
	}
}

#endif /* CTF_NOTIF_ITER_H */
