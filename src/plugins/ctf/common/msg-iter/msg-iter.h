#ifndef CTF_MSG_ITER_H
#define CTF_MSG_ITER_H

/*
 * Babeltrace - CTF message iterator
 *
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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <babeltrace2/babeltrace.h>
#include "common/macros.h"

#include "../metadata/ctf-meta.h"

/**
 * @file ctf-msg-iter.h
 *
 * CTF message iterator
 *
 * This is a common internal API used by CTF source plugins. It allows
 * one to get messages from a user-provided medium.
 */

/**
 * Medium operations status codes.  These use the same values as
 * libbabeltrace2.
 */
enum ctf_msg_iter_medium_status {
	/**
	 * End of file.
	 *
	 * The medium function called by the message iterator
	 * function reached the end of the file.
	 */
	CTF_MSG_ITER_MEDIUM_STATUS_EOF = 1,

	/**
	 * There is no data available right now, try again later.
	 */
	CTF_MSG_ITER_MEDIUM_STATUS_AGAIN = 11,

	/** General error. */
	CTF_MSG_ITER_MEDIUM_STATUS_ERROR = -1,

	/** Memory error. */
	CTF_MSG_ITER_MEDIUM_STATUS_MEMORY_ERROR = -12,

	/** Everything okay. */
	CTF_MSG_ITER_MEDIUM_STATUS_OK = 0,
};

/**
 * CTF message iterator API status code.
 */
enum ctf_msg_iter_status {
	/**
	 * End of file.
	 *
	 * The medium function called by the message iterator
	 * function reached the end of the file.
	 */
	CTF_MSG_ITER_STATUS_EOF = CTF_MSG_ITER_MEDIUM_STATUS_EOF,

	/**
	 * There is no data available right now, try again later.
	 *
	 * Some condition resulted in the
	 * ctf_msg_iter_medium_ops::request_bytes() user function not
	 * having access to any data now. You should retry calling the
	 * last called message iterator function once the situation
	 * is resolved.
	 */
	CTF_MSG_ITER_STATUS_AGAIN = CTF_MSG_ITER_MEDIUM_STATUS_AGAIN,

	/** General error. */
	CTF_MSG_ITER_STATUS_ERROR = CTF_MSG_ITER_MEDIUM_STATUS_ERROR,

	/** Memory error. */
	CTF_MSG_ITER_STATUS_MEMORY_ERROR  = CTF_MSG_ITER_MEDIUM_STATUS_MEMORY_ERROR,

	/** Everything okay. */
	CTF_MSG_ITER_STATUS_OK = CTF_MSG_ITER_MEDIUM_STATUS_OK,
};

/**
 * Medium operations.
 *
 * Those user functions are called by the message iterator
 * functions to request medium actions.
 */
struct ctf_msg_iter_medium_ops {
	/**
	 * Returns the next byte buffer to be used by the binary file
	 * reader to deserialize binary data.
	 *
	 * This function \em must be defined.
	 *
	 * The purpose of this function is to return a buffer of bytes
	 * to the message iterator, of a maximum of \p request_sz
	 * bytes. If this function cannot return a buffer of at least
	 * \p request_sz bytes, it may return a smaller buffer. In
	 * either cases, \p buffer_sz must be set to the returned buffer
	 * size (in bytes).
	 *
	 * The returned buffer's ownership remains the medium, in that
	 * it won't be freed by the message iterator functions. The
	 * returned buffer won't be modified by the message
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
	 *   - <b>#CTF_MSG_ITER_MEDIUM_STATUS_OK</b>: Everything
	 *     is okay, i.e. \p buffer_sz is set to a positive value
	 *     reflecting the number of available bytes in the buffer
	 *     starting at the address written in \p buffer_addr.
	 *   - <b>#CTF_MSG_ITER_MEDIUM_STATUS_AGAIN</b>: No data is
	 *     available right now. In this case, the message
	 *     iterator function called by the user returns
	 *     #CTF_MSG_ITER_STATUS_AGAIN, and it is the user's
	 *     responsibility to make sure enough data becomes available
	 *     before calling the \em same message iterator
	 *     function again to continue the decoding process.
	 *   - <b>#CTF_MSG_ITER_MEDIUM_STATUS_EOF</b>: The end of
	 *     the file was reached, and no more data will ever be
	 *     available for this file. In this case, the message
	 *     iterator function called by the user returns
	 *     #CTF_MSG_ITER_STATUS_EOF. This must \em not be
	 *     returned when returning at least one byte of data to the
	 *     caller, i.e. this must be returned when there's
	 *     absolutely nothing left; should the request size be
	 *     larger than what's left in the file, this function must
	 *     return what's left, setting \p buffer_sz to the number of
	 *     remaining bytes, and return
	 *     #CTF_MSG_ITER_MEDIUM_STATUS_EOF on the \em following
	 *     call.
	 *   - <b>#CTF_MSG_ITER_MEDIUM_STATUS_ERROR</b>: A fatal
	 *     error occurred during this operation. In this case, the
	 *     message iterator function called by the user returns
	 *     #CTF_MSG_ITER_STATUS_ERROR.
	 *
	 * If #CTF_MSG_ITER_MEDIUM_STATUS_OK is not returned, the
	 * values of \p buffer_sz and \p buffer_addr are \em ignored by
	 * the caller.
	 *
	 * @param request_sz	Requested buffer size (bytes)
	 * @param buffer_addr	Returned buffer address
	 * @param buffer_sz	Returned buffer's size (bytes)
	 * @param data		User data
	 * @returns		Status code (see description above)
	 */
	enum ctf_msg_iter_medium_status (* request_bytes)(size_t request_sz,
			uint8_t **buffer_addr, size_t *buffer_sz, void *data);

	/**
	 * Repositions the underlying stream's position.
	 *
	 * This *optional* method repositions the underlying stream
	 * to a given absolute position in the medium.
	 *
	 * @param offset	Offset to use for the given directive
	 * @param data		User data
	 * @returns		One of #ctf_msg_iter_medium_status values
	 */
	enum ctf_msg_iter_medium_status (* seek)(off_t offset, void *data);

	/**
	 * Called when the message iterator wishes to inform the medium that it
	 * is about to start a new packet.
	 *
	 * After the iterator has called switch_packet, the following call to
	 * request_bytes must return the content at the start of the next
	 * packet.	 */
	enum ctf_msg_iter_medium_status (* switch_packet)(void *data);

	/**
	 * Returns a stream instance (weak reference) for the given
	 * stream class.
	 *
	 * This is called after a packet header is read, and the
	 * corresponding stream class is found by the message
	 * iterator.
	 *
	 * @param stream_class	Stream class of the stream to get
	 * @param stream_id	Stream (instance) ID of the stream
	 *			to get (-1ULL if not available)
	 * @param data		User data
	 * @returns		Stream instance (weak reference) or
	 *			\c NULL on error
	 */
	bt_stream * (* borrow_stream)(bt_stream_class *stream_class,
			int64_t stream_id, void *data);
};

/** CTF message iterator. */
struct ctf_msg_iter;

/**
 * Creates a CTF message iterator.
 *
 * Upon successful completion, the reference count of \p trace is
 * incremented.
 *
 * @param trace			Trace to read
 * @param max_request_sz	Maximum buffer size, in bytes, to
 *				request to
 *				ctf_msg_iter_medium_ops::request_bytes()
 * 				at a time
 * @param medops		Medium operations
 * @param medops_data		User data (passed to medium operations)
 * @returns			New CTF message iterator on
 *				success, or \c NULL on error
 */
BT_HIDDEN
struct ctf_msg_iter *ctf_msg_iter_create(
		struct ctf_trace_class *tc,
		size_t max_request_sz,
		struct ctf_msg_iter_medium_ops medops,
		void *medops_data,
		bt_logging_level log_level,
		bt_self_component *self_comp,
		bt_self_message_iterator *self_msg_iter);

/**
 * Destroys a CTF message iterator, freeing all internal resources.
 *
 * The registered trace's reference count is decremented.
 *
 * @param msg_iter		CTF message iterator
 */
BT_HIDDEN
void ctf_msg_iter_destroy(struct ctf_msg_iter *msg_iter);

/**
 * Returns the next message from a CTF message iterator.
 *
 * Upon successful completion, #CTF_MSG_ITER_STATUS_OK is
 * returned, and the next message is written to \p msg.
 * In this case, the caller is responsible for calling
 * bt_message_put() on the returned message.
 *
 * If this function returns #CTF_MSG_ITER_STATUS_AGAIN, the caller
 * should make sure that data becomes available to its medium, and
 * call this function again, until another status is returned.
 *
 * @param msg_iter		CTF message iterator
 * @param message		Returned message if the function's
 *				return value is #CTF_MSG_ITER_STATUS_OK
 * @returns			One of #ctf_msg_iter_status values
 */
BT_HIDDEN
enum ctf_msg_iter_status ctf_msg_iter_get_next_message(
		struct ctf_msg_iter *msg_it,
		const bt_message **message);

struct ctf_msg_iter_packet_properties {
	int64_t exp_packet_total_size;
	int64_t exp_packet_content_size;
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
enum ctf_msg_iter_status ctf_msg_iter_get_packet_properties(
		struct ctf_msg_iter *msg_it,
		struct ctf_msg_iter_packet_properties *props);

BT_HIDDEN
enum ctf_msg_iter_status ctf_msg_iter_curr_packet_first_event_clock_snapshot(
		struct ctf_msg_iter *msg_it, uint64_t *first_event_cs);

BT_HIDDEN
enum ctf_msg_iter_status ctf_msg_iter_curr_packet_last_event_clock_snapshot(
		struct ctf_msg_iter *msg_it, uint64_t *last_event_cs);

BT_HIDDEN
enum ctf_msg_iter_status ctf_msg_iter_seek(
		struct ctf_msg_iter *msg_it, off_t offset);

/*
 * Resets the iterator so that the next requested medium bytes are
 * assumed to be the first bytes of a new stream. Depending on
 * ctf_msg_iter_set_emit_stream_beginning_message(), the first message
 * which this iterator emits after calling ctf_msg_iter_reset() is of
 * type `CTF_MESSAGE_TYPE_STREAM_BEGINNING`.
 */
BT_HIDDEN
void ctf_msg_iter_reset(struct ctf_msg_iter *msg_it);

/*
 * Like ctf_msg_iter_reset(), but preserves stream-dependent state.
 */
BT_HIDDEN
void ctf_msg_iter_reset_for_next_stream_file(struct ctf_msg_iter *msg_it);

BT_HIDDEN
void ctf_msg_iter_set_dry_run(struct ctf_msg_iter *msg_it,
		bool val);

static inline
const char *ctf_msg_iter_medium_status_string(
		enum ctf_msg_iter_medium_status status)
{
	switch (status) {
	case CTF_MSG_ITER_MEDIUM_STATUS_EOF:
		return "EOF";
	case CTF_MSG_ITER_MEDIUM_STATUS_AGAIN:
		return "AGAIN";
	case CTF_MSG_ITER_MEDIUM_STATUS_ERROR:
		return "ERROR";
	case CTF_MSG_ITER_MEDIUM_STATUS_OK:
		return "OK";
	default:
		return "(unknown)";
	}
}

static inline
const char *ctf_msg_iter_status_string(
		enum ctf_msg_iter_status status)
{
	switch (status) {
	case CTF_MSG_ITER_STATUS_EOF:
		return "EOF";
	case CTF_MSG_ITER_STATUS_AGAIN:
		return "AGAIN";
	case CTF_MSG_ITER_STATUS_ERROR:
		return "ERROR";
	case CTF_MSG_ITER_STATUS_OK:
		return "OK";
	default:
		return "(unknown)";
	}
}

#endif /* CTF_MSG_ITER_H */
