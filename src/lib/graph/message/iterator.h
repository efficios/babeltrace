#ifndef BABELTRACE_GRAPH_MESSAGE_ITERATOR_INTERNAL_H
#define BABELTRACE_GRAPH_MESSAGE_ITERATOR_INTERNAL_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include "common/macros.h"
#include "lib/object.h"
#include <babeltrace2/graph/connection.h>
#include <babeltrace2/graph/message.h>
#include <babeltrace2/types.h>
#include "common/assert.h"
#include <stdbool.h>
#include "common/uuid.h"

struct bt_port;
struct bt_graph;

enum bt_message_iterator_state {
	/* Iterator is not initialized */
	BT_MESSAGE_ITERATOR_STATE_NON_INITIALIZED,

	/* Iterator is active, not at the end yet, and not finalized */
	BT_MESSAGE_ITERATOR_STATE_ACTIVE,

	/*
	 * Iterator is ended, not finalized yet: the "next" method
	 * returns BT_MESSAGE_ITERATOR_STATUS_END.
	 */
	BT_MESSAGE_ITERATOR_STATE_ENDED,

	/* Iterator is currently being finalized */
	BT_MESSAGE_ITERATOR_STATE_FINALIZING,

	/* Iterator is finalized */
	BT_MESSAGE_ITERATOR_STATE_FINALIZED,

	/* Iterator is seeking */
	BT_MESSAGE_ITERATOR_STATE_SEEKING,

	/* Iterator did seek, but returned `BT_MESSAGE_ITERATOR_STATUS_AGAIN` */
	BT_MESSAGE_ITERATOR_STATE_LAST_SEEKING_RETURNED_AGAIN,

	/* Iterator did seek, but returned error status */
	BT_MESSAGE_ITERATOR_STATE_LAST_SEEKING_RETURNED_ERROR,
};

typedef enum bt_message_iterator_class_next_method_status
(*bt_message_iterator_next_method)(
		void *, bt_message_array_const, uint64_t, uint64_t *);

typedef enum bt_message_iterator_class_seek_ns_from_origin_method_status
(*bt_message_iterator_seek_ns_from_origin_method)(
		void *, int64_t);

typedef enum bt_message_iterator_class_seek_beginning_method_status
(*bt_message_iterator_seek_beginning_method)(
		void *);

typedef enum bt_message_iterator_class_can_seek_ns_from_origin_method_status
(*bt_message_iterator_can_seek_ns_from_origin_method)(
		void *, int64_t, bt_bool *);

typedef enum bt_message_iterator_class_can_seek_beginning_method_status
(*bt_message_iterator_can_seek_beginning_method)(
		void *, bt_bool *);

struct bt_self_message_iterator_configuration {
	bool frozen;
	bool can_seek_forward;
};

struct bt_message_iterator {
	struct bt_object base;
	GPtrArray *msgs;
	struct bt_component *upstream_component; /* Weak */
	struct bt_port *upstream_port; /* Weak */
	struct bt_connection *connection; /* Weak */
	struct bt_graph *graph; /* Weak */
	struct bt_self_message_iterator_configuration config;

	/*
	 * Array of
	 * `struct bt_message_iterator *`
	 * (weak).
	 *
	 * This is an array of upstream message iterators on which this
	 * iterator depends. The references are weak: an upstream
	 * message iterator is responsible for removing its entry within
	 * this array on finalization/destruction.
	 */
	GPtrArray *upstream_msg_iters;

	/*
	 * Downstream message iterator which depends on this message
	 * iterator (weak).
	 *
	 * This can be `NULL` if this message iterator's owner is a sink
	 * component.
	 */
	struct bt_message_iterator *downstream_msg_iter;

	struct {
		bt_message_iterator_next_method next;

		/* These two are always both set or both unset. */
		bt_message_iterator_seek_ns_from_origin_method seek_ns_from_origin;
		bt_message_iterator_can_seek_ns_from_origin_method can_seek_ns_from_origin;

		/* These two are always both set or both unset. */
		bt_message_iterator_seek_beginning_method seek_beginning;
		bt_message_iterator_can_seek_beginning_method can_seek_beginning;
	} methods;

	enum bt_message_iterator_state state;

	/*
	 * Timestamp of the last received message (or INT64_MIN in the
	 * beginning, or after a seek to beginning).
	 */
	int64_t last_ns_from_origin;

	struct {
		enum {
			/* We haven't recorded clock properties yet. */
			CLOCK_EXPECTATION_UNSET,

			/* Expect to have no clock. */
			CLOCK_EXPECTATION_NONE,

			/* Clock with origin_is_unix_epoch true.*/
			CLOCK_EXPECTATION_ORIGIN_UNIX,

			/* Clock with origin_is_unix_epoch false, with a UUID.*/
			CLOCK_EXPECTATION_ORIGIN_OTHER_UUID,

			/* Clock with origin_is_unix_epoch false, without a UUID.*/
			CLOCK_EXPECTATION_ORIGIN_OTHER_NO_UUID,
		} type;

		/*
		 * Expected UUID of the clock, if `type`is CLOCK_EXPECTATION_ORIGIN_OTHER_UUID.
		 *
		 * If the clock's origin is the unix epoch, the UUID is
		 * irrelevant (as the clock will be correlatable with other
		 * clocks having the same origin).
		 */
		bt_uuid_t uuid;
	} clock_expectation;

	/*
	 * Data necessary for auto seek (the seek-to-beginning then fast-forward
	 * seek strategy).
	 */
	struct {
		/*
		 * Queue of `const bt_message *` (owned by this queue).
		 *
		 * When fast-forwarding, we get the messages from upstream in
		 * batches. Once we have found the first message with timestamp
		 * greater or equal to the seek time, we put it and all of the
		 * following message of the batch in this queue.  They will be
		 * sent on the next "next" call on this iterator.
		 *
		 * The messages are in chronological order (i.e. the first to
		 * send is the first of the queue).
		 */
		GQueue *msgs;

		/*
		 * After auto-seeking, we replace the iterator's `next` callback
		 * with our own, which returns the contents of the `msgs` queue.
		 * This field is where we save the original callback, so we can
		 * restore it.
		 */
		void *original_next_callback;
	} auto_seek;

	void *user_data;
};

BT_HIDDEN
void bt_message_iterator_try_finalize(
		struct bt_message_iterator *iterator);

BT_HIDDEN
void bt_message_iterator_set_connection(
		struct bt_message_iterator *iterator,
		struct bt_connection *connection);

static inline
const char *bt_message_iterator_state_string(
		enum bt_message_iterator_state state)
{
	switch (state) {
	case BT_MESSAGE_ITERATOR_STATE_ACTIVE:
		return "ACTIVE";
	case BT_MESSAGE_ITERATOR_STATE_ENDED:
		return "ENDED";
	case BT_MESSAGE_ITERATOR_STATE_FINALIZING:
		return "FINALIZING";
	case BT_MESSAGE_ITERATOR_STATE_FINALIZED:
		return "FINALIZED";
	case BT_MESSAGE_ITERATOR_STATE_SEEKING:
		return "SEEKING";
	case BT_MESSAGE_ITERATOR_STATE_LAST_SEEKING_RETURNED_AGAIN:
		return "LAST_SEEKING_RETURNED_AGAIN";
	case BT_MESSAGE_ITERATOR_STATE_LAST_SEEKING_RETURNED_ERROR:
		return "LAST_SEEKING_RETURNED_ERROR";
	default:
		return "(unknown)";
	}
};

#endif /* BABELTRACE_GRAPH_MESSAGE_ITERATOR_INTERNAL_H */
