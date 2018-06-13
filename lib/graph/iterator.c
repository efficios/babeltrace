/*
 * iterator.c
 *
 * Babeltrace Notification Iterator
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#define BT_LOG_TAG "NOTIF-ITER"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/compiler-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ctf-ir/packet-internal.h>
#include <babeltrace/ctf-ir/stream-internal.h>
#include <babeltrace/graph/connection.h>
#include <babeltrace/graph/connection-internal.h>
#include <babeltrace/graph/component.h>
#include <babeltrace/graph/component-source-internal.h>
#include <babeltrace/graph/component-class-internal.h>
#include <babeltrace/graph/component-class-sink-colander-internal.h>
#include <babeltrace/graph/component-sink.h>
#include <babeltrace/graph/notification.h>
#include <babeltrace/graph/notification-iterator.h>
#include <babeltrace/graph/notification-iterator-internal.h>
#include <babeltrace/graph/notification-internal.h>
#include <babeltrace/graph/notification-event.h>
#include <babeltrace/graph/notification-event-internal.h>
#include <babeltrace/graph/notification-packet.h>
#include <babeltrace/graph/notification-packet-internal.h>
#include <babeltrace/graph/notification-stream.h>
#include <babeltrace/graph/notification-stream-internal.h>
#include <babeltrace/graph/notification-discarded-elements-internal.h>
#include <babeltrace/graph/port.h>
#include <babeltrace/graph/graph-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>

struct discarded_elements_state {
	struct bt_clock_value *cur_begin;
	uint64_t cur_count;
};

struct stream_state {
	struct bt_stream *stream; /* owned by this */
	struct bt_packet *cur_packet; /* owned by this */
	struct discarded_elements_state discarded_packets_state;
	struct discarded_elements_state discarded_events_state;
	uint64_t expected_notif_seq_num;
	bt_bool is_ended;
};

BT_ASSERT_PRE_FUNC
static
void destroy_stream_state(struct stream_state *stream_state)
{
	if (!stream_state) {
		return;
	}

	BT_LOGV("Destroying stream state: stream-state-addr=%p", stream_state);
	BT_LOGV_STR("Putting stream state's current packet.");
	bt_put(stream_state->cur_packet);
	BT_LOGV_STR("Putting stream state's stream.");
	bt_put(stream_state->stream);
	bt_put(stream_state->discarded_packets_state.cur_begin);
	bt_put(stream_state->discarded_events_state.cur_begin);
	g_free(stream_state);
}

BT_ASSERT_PRE_FUNC
static
struct stream_state *create_stream_state(struct bt_stream *stream)
{
	struct stream_state *stream_state = g_new0(struct stream_state, 1);

	if (!stream_state) {
		BT_LOGE_STR("Failed to allocate one stream state.");
		goto end;
	}

	/*
	 * The packet index is a monotonic counter which may not start
	 * at 0 at the beginning of the stream. We therefore need to
	 * have an internal object initial state of -1ULL to distinguish
	 * between initial state and having seen a packet with
	 * the sequence number 0.
	 */
	stream_state->discarded_packets_state.cur_count = -1ULL;

	/*
	 * We keep a reference to the stream until we know it's ended.
	 */
	stream_state->stream = bt_get(stream);
	BT_LOGV("Created stream state: stream-addr=%p, stream-name=\"%s\", "
		"stream-state-addr=%p",
		stream, bt_stream_get_name(stream), stream_state);

end:
	return stream_state;
}

static
void destroy_base_notification_iterator(struct bt_object *obj)
{
	BT_ASSERT(obj);
	g_free(obj);
}

static
void bt_private_connection_notification_iterator_destroy(struct bt_object *obj)
{
	struct bt_notification_iterator_private_connection *iterator;

	BT_ASSERT(obj);

	/*
	 * The notification iterator's reference count is 0 if we're
	 * here. Increment it to avoid a double-destroy (possibly
	 * infinitely recursive). This could happen for example if the
	 * notification iterator's finalization function does bt_get()
	 * (or anything that causes bt_get() to be called) on itself
	 * (ref. count goes from 0 to 1), and then bt_put(): the
	 * reference count would go from 1 to 0 again and this function
	 * would be called again.
	 */
	obj->ref_count++;
	iterator = (void *) obj;
	BT_LOGD("Destroying private connection notification iterator object: addr=%p",
		iterator);
	bt_private_connection_notification_iterator_finalize(iterator);

	if (iterator->stream_states) {
		/*
		 * Remove our destroy listener from each stream which
		 * has a state in this iterator. Otherwise the destroy
		 * listener would be called with an invalid/other
		 * notification iterator object.
		 */
		g_hash_table_destroy(iterator->stream_states);
	}

	if (iterator->connection) {
		/*
		 * Remove ourself from the originating connection so
		 * that it does not try to finalize a dangling pointer
		 * later.
		 */
		bt_connection_remove_iterator(iterator->connection, iterator);
	}

	destroy_base_notification_iterator(obj);
}

BT_HIDDEN
void bt_private_connection_notification_iterator_finalize(
		struct bt_notification_iterator_private_connection *iterator)
{
	struct bt_component_class *comp_class = NULL;
	bt_component_class_notification_iterator_finalize_method
		finalize_method = NULL;

	BT_ASSERT(iterator);

	switch (iterator->state) {
	case BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_NON_INITIALIZED:
		/* Skip user finalization if user initialization failed */
		BT_LOGD("Not finalizing non-initialized notification iterator: "
			"addr=%p", iterator);
		return;
	case BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_FINALIZED:
	case BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_FINALIZED_AND_ENDED:
		/* Already finalized */
		BT_LOGD("Not finalizing notification iterator: already finalized: "
			"addr=%p", iterator);
		return;
	default:
		break;
	}

	BT_LOGD("Finalizing notification iterator: addr=%p", iterator);

	if (iterator->state == BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_ENDED) {
		BT_LOGD("Updating notification iterator's state: "
			"new-state=BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_FINALIZED_AND_ENDED");
		iterator->state = BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_FINALIZED_AND_ENDED;
	} else {
		BT_LOGD("Updating notification iterator's state: "
			"new-state=BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_FINALIZED");
		iterator->state = BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_FINALIZED;
	}

	BT_ASSERT(iterator->upstream_component);
	comp_class = iterator->upstream_component->class;

	/* Call user-defined destroy method */
	switch (comp_class->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		struct bt_component_class_source *source_class;

		source_class = container_of(comp_class, struct bt_component_class_source, parent);
		finalize_method = source_class->methods.iterator.finalize;
		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		struct bt_component_class_filter *filter_class;

		filter_class = container_of(comp_class, struct bt_component_class_filter, parent);
		finalize_method = filter_class->methods.iterator.finalize;
		break;
	}
	default:
		/* Unreachable */
		abort();
	}

	if (finalize_method) {
		BT_LOGD("Calling user's finalization method: addr=%p",
			iterator);
		finalize_method(
			bt_private_connection_private_notification_iterator_from_notification_iterator(iterator));
	}

	iterator->upstream_component = NULL;
	iterator->upstream_port = NULL;
	BT_LOGD("Finalized notification iterator: addr=%p", iterator);
}

BT_HIDDEN
void bt_private_connection_notification_iterator_set_connection(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_connection *connection)
{
	BT_ASSERT(iterator);
	iterator->connection = connection;
	BT_LOGV("Set notification iterator's connection: "
		"iter-addr=%p, conn-addr=%p", iterator, connection);
}

static
void init_notification_iterator(struct bt_notification_iterator *iterator,
		enum bt_notification_iterator_type type,
		bt_object_release_func destroy)
{
	bt_object_init_shared(&iterator->base, destroy);
	iterator->type = type;
}

BT_HIDDEN
enum bt_connection_status bt_private_connection_notification_iterator_create(
		struct bt_component *upstream_comp,
		struct bt_port *upstream_port,
		struct bt_connection *connection,
		struct bt_notification_iterator_private_connection **user_iterator)
{
	enum bt_connection_status status = BT_CONNECTION_STATUS_OK;
	enum bt_component_class_type type;
	struct bt_notification_iterator_private_connection *iterator = NULL;

	BT_ASSERT(upstream_comp);
	BT_ASSERT(upstream_port);
	BT_ASSERT(bt_port_is_connected(upstream_port));
	BT_ASSERT(user_iterator);
	BT_LOGD("Creating notification iterator on private connection: "
		"upstream-comp-addr=%p, upstream-comp-name=\"%s\", "
		"upstream-port-addr=%p, upstream-port-name=\"%s\", "
		"conn-addr=%p",
		upstream_comp, bt_component_get_name(upstream_comp),
		upstream_port, bt_port_get_name(upstream_port),
		connection);
	type = bt_component_get_class_type(upstream_comp);
	BT_ASSERT(type == BT_COMPONENT_CLASS_TYPE_SOURCE ||
		type == BT_COMPONENT_CLASS_TYPE_FILTER);
	iterator = g_new0(struct bt_notification_iterator_private_connection, 1);
	if (!iterator) {
		BT_LOGE_STR("Failed to allocate one private connection notification iterator.");
		status = BT_CONNECTION_STATUS_NOMEM;
		goto end;
	}

	init_notification_iterator((void *) iterator,
		BT_NOTIFICATION_ITERATOR_TYPE_PRIVATE_CONNECTION,
		bt_private_connection_notification_iterator_destroy);

	iterator->stream_states = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, (GDestroyNotify) destroy_stream_state);
	if (!iterator->stream_states) {
		BT_LOGE_STR("Failed to allocate a GHashTable.");
		status = BT_CONNECTION_STATUS_NOMEM;
		goto end;
	}

	iterator->upstream_component = upstream_comp;
	iterator->upstream_port = upstream_port;
	iterator->connection = connection;
	iterator->graph = bt_component_borrow_graph(upstream_comp);
	iterator->state = BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_NON_INITIALIZED;
	BT_LOGD("Created notification iterator: "
		"upstream-comp-addr=%p, upstream-comp-name=\"%s\", "
		"upstream-port-addr=%p, upstream-port-name=\"%s\", "
		"conn-addr=%p, iter-addr=%p",
		upstream_comp, bt_component_get_name(upstream_comp),
		upstream_port, bt_port_get_name(upstream_port),
		connection, iterator);

	/* Move reference to user */
	*user_iterator = iterator;
	iterator = NULL;

end:
	bt_put(iterator);
	return status;
}

void *bt_private_connection_private_notification_iterator_get_user_data(
		struct bt_private_connection_private_notification_iterator *private_iterator)
{
	struct bt_notification_iterator_private_connection *iterator = (void *)
		bt_private_connection_notification_iterator_borrow_from_private(private_iterator);

	BT_ASSERT_PRE_NON_NULL(private_iterator, "Notification iterator");
	return iterator->user_data;
}

enum bt_notification_iterator_status
bt_private_connection_private_notification_iterator_set_user_data(
		struct bt_private_connection_private_notification_iterator *private_iterator,
		void *data)
{
	struct bt_notification_iterator_private_connection *iterator = (void *)
		bt_private_connection_notification_iterator_borrow_from_private(private_iterator);

	BT_ASSERT_PRE_NON_NULL(iterator, "Notification iterator");
	iterator->user_data = data;
	BT_LOGV("Set notification iterator's user data: "
		"iter-addr=%p, user-data-addr=%p", iterator, data);
	return BT_NOTIFICATION_ITERATOR_STATUS_OK;
}

struct bt_graph *bt_private_connection_private_notification_iterator_borrow_graph(
		struct bt_private_connection_private_notification_iterator *private_iterator)
{
	struct bt_notification_iterator_private_connection *iterator = (void *)
		bt_private_connection_notification_iterator_borrow_from_private(
			private_iterator);

	BT_ASSERT_PRE_NON_NULL(iterator, "Notification iterator");
	return iterator->graph;
}

BT_ASSERT_PRE_FUNC
static inline
void bt_notification_borrow_packet_stream(struct bt_notification *notif,
		struct bt_stream **stream, struct bt_packet **packet)
{
	BT_ASSERT(notif);

	switch (notif->type) {
	case BT_NOTIFICATION_TYPE_EVENT:
		*packet = bt_event_borrow_packet(
			bt_notification_event_borrow_event(notif));
		*stream = bt_packet_borrow_stream(*packet);
		break;
	case BT_NOTIFICATION_TYPE_STREAM_BEGIN:
		*stream = bt_notification_stream_begin_borrow_stream(notif);
		break;
	case BT_NOTIFICATION_TYPE_STREAM_END:
		*stream = bt_notification_stream_end_borrow_stream(notif);
		break;
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
		*packet = bt_notification_packet_begin_borrow_packet(notif);
		*stream = bt_packet_borrow_stream(*packet);
		break;
	case BT_NOTIFICATION_TYPE_PACKET_END:
		*packet = bt_notification_packet_end_borrow_packet(notif);
		*stream = bt_packet_borrow_stream(*packet);
		break;
	default:
		break;
	}
}

BT_ASSERT_PRE_FUNC
static inline
bool validate_notification(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_notification *notif)
{
	bool is_valid = true;
	struct stream_state *stream_state;
	struct bt_stream *stream = NULL;
	struct bt_packet *packet = NULL;

	BT_ASSERT(notif);
	bt_notification_borrow_packet_stream(notif, &stream, &packet);

	if (!stream) {
		/* we don't care about notifications not attached to streams */
		goto end;
	}

	stream_state = g_hash_table_lookup(iterator->stream_states, stream);
	if (!stream_state) {
		/*
		 * No stream state for this stream: this notification
		 * MUST be a BT_NOTIFICATION_TYPE_STREAM_BEGIN notification
		 * and its sequence number must be 0.
		 */
		if (notif->type != BT_NOTIFICATION_TYPE_STREAM_BEGIN) {
			BT_ASSERT_PRE_MSG("Unexpected notification: missing a "
				"BT_NOTIFICATION_TYPE_STREAM_BEGIN "
				"notification prior to this notification: "
				"%![stream-]+s", stream);
			is_valid = false;
			goto end;
		}

		if (notif->seq_num == -1ULL) {
			notif->seq_num = 0;
		}

		if (notif->seq_num != 0) {
			BT_ASSERT_PRE_MSG("Unexpected notification sequence "
				"number for this notification iterator: "
				"this is the first notification for this "
				"stream, expecting sequence number 0: "
				"seq-num=%" PRIu64 ", %![stream-]+s",
				notif->seq_num, stream);
			is_valid = false;
			goto end;
		}

		stream_state = create_stream_state(stream);
		if (!stream_state) {
			abort();
		}

		g_hash_table_insert(iterator->stream_states, stream,
			stream_state);
		stream_state->expected_notif_seq_num++;
		goto end;
	}

	if (stream_state->is_ended) {
		/*
		 * There's a new notification which has a reference to a
		 * stream which, from this iterator's point of view, is
		 * ended ("end of stream" notification was returned).
		 * This is bad: the API guarantees that it can never
		 * happen.
		 */
		BT_ASSERT_PRE_MSG("Stream is already ended: %![stream-]+s",
			stream);
		is_valid = false;
		goto end;
	}

	if (notif->seq_num == -1ULL) {
		notif->seq_num = stream_state->expected_notif_seq_num;
	}

	if (notif->seq_num != -1ULL &&
			notif->seq_num != stream_state->expected_notif_seq_num) {
		BT_ASSERT_PRE_MSG("Unexpected notification sequence number: "
			"seq-num=%" PRIu64 ", "
			"expected-seq-num=%" PRIu64 ", %![stream-]+s",
			notif->seq_num, stream_state->expected_notif_seq_num,
			stream);
		is_valid = false;
		goto end;
	}

	switch (notif->type) {
	case BT_NOTIFICATION_TYPE_STREAM_BEGIN:
		BT_ASSERT_PRE_MSG("Unexpected BT_NOTIFICATION_TYPE_STREAM_BEGIN "
			"notification at this point: notif-seq-num=%" PRIu64 ", "
			"%![stream-]+s", notif->seq_num, stream);
		is_valid = false;
		goto end;
	case BT_NOTIFICATION_TYPE_STREAM_END:
		if (stream_state->cur_packet) {
			BT_ASSERT_PRE_MSG("Unexpected BT_NOTIFICATION_TYPE_STREAM_END "
				"notification: missing a "
				"BT_NOTIFICATION_TYPE_PACKET_END notification "
				"prior to this notification: "
				"notif-seq-num=%" PRIu64 ", "
				"%![stream-]+s", notif->seq_num, stream);
			is_valid = false;
			goto end;
		}
		stream_state->expected_notif_seq_num++;
		stream_state->is_ended = true;
		goto end;
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
		if (stream_state->cur_packet) {
			BT_ASSERT_PRE_MSG("Unexpected BT_NOTIFICATION_TYPE_PACKET_BEGIN "
				"notification at this point: missing a "
				"BT_NOTIFICATION_TYPE_PACKET_END notification "
				"prior to this notification: "
				"notif-seq-num=%" PRIu64 ", %![stream-]+s, "
				"%![packet-]+a", notif->seq_num, stream,
				packet);
			is_valid = false;
			goto end;
		}
		stream_state->expected_notif_seq_num++;
		stream_state->cur_packet = bt_get(packet);
		goto end;
	case BT_NOTIFICATION_TYPE_PACKET_END:
		if (!stream_state->cur_packet) {
			BT_ASSERT_PRE_MSG("Unexpected BT_NOTIFICATION_TYPE_PACKET_END "
				"notification at this point: missing a "
				"BT_NOTIFICATION_TYPE_PACKET_BEGIN notification "
				"prior to this notification: "
				"notif-seq-num=%" PRIu64 ", %![stream-]+s, "
				"%![packet-]+a", notif->seq_num, stream,
				packet);
			is_valid = false;
			goto end;
		}
		stream_state->expected_notif_seq_num++;
		BT_PUT(stream_state->cur_packet);
		goto end;
	case BT_NOTIFICATION_TYPE_EVENT:
		if (packet != stream_state->cur_packet) {
			BT_ASSERT_PRE_MSG("Unexpected packet for "
				"BT_NOTIFICATION_TYPE_EVENT notification: "
				"notif-seq-num=%" PRIu64 ", %![stream-]+s, "
				"%![notif-packet-]+a, %![expected-packet-]+a",
				notif->seq_num, stream,
				stream_state->cur_packet, packet);
			is_valid = false;
			goto end;
		}
		stream_state->expected_notif_seq_num++;
		goto end;
	default:
		break;
	}

end:
	return is_valid;
}

BT_ASSERT_PRE_FUNC
static inline bool priv_conn_notif_iter_can_end(
		struct bt_notification_iterator_private_connection *iterator)
{
	GHashTableIter iter;
	gpointer stream_key, state_value;
	bool ret = true;

	/*
	 * Verify that this iterator received a
	 * BT_NOTIFICATION_TYPE_STREAM_END notification for each stream
	 * which has a state.
	 */

	g_hash_table_iter_init(&iter, iterator->stream_states);

	while (g_hash_table_iter_next(&iter, &stream_key, &state_value)) {
		struct stream_state *stream_state = (void *) state_value;

		BT_ASSERT(stream_state);
		BT_ASSERT(stream_key);

		if (!stream_state->is_ended) {
			BT_ASSERT_PRE_MSG("Ending notification iterator, "
				"but stream is not ended: "
				"%![stream-]s", stream_key);
			ret = false;
			goto end;
		}
	}

end:
	return ret;
}

enum bt_notification_iterator_status
bt_private_connection_notification_iterator_next(
		struct bt_notification_iterator *user_iterator,
		struct bt_notification **user_notif)
{
	struct bt_notification_iterator_private_connection *iterator =
		(void *) user_iterator;
	struct bt_private_connection_private_notification_iterator *priv_iterator =
		bt_private_connection_private_notification_iterator_from_notification_iterator(iterator);
	bt_component_class_notification_iterator_next_method next_method = NULL;
	struct bt_notification_iterator_next_method_return next_return = {
		.status = BT_NOTIFICATION_ITERATOR_STATUS_OK,
		.notification = NULL,
	};
	enum bt_notification_iterator_status status =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;

	BT_ASSERT_PRE_NON_NULL(user_iterator, "Notification iterator");
	BT_ASSERT_PRE_NON_NULL(user_notif, "Notification");
	BT_ASSERT_PRE(user_iterator->type ==
		BT_NOTIFICATION_ITERATOR_TYPE_PRIVATE_CONNECTION,
		"Notification iterator was not created from a private connection: "
		"%!+i", iterator);
	BT_LIB_LOGD("Getting next private connection notification iterator's notification: %!+i",
		iterator);
	BT_ASSERT_PRE(iterator->state ==
		BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_ACTIVE,
		"Notification iterator's \"next\" called, but "
		"iterator is in the wrong state: %!+i", iterator);
	BT_ASSERT(iterator->upstream_component);
	BT_ASSERT(iterator->upstream_component->class);

	/* Pick the appropriate "next" method */
	switch (iterator->upstream_component->class->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		struct bt_component_class_source *source_class =
			container_of(iterator->upstream_component->class,
				struct bt_component_class_source, parent);

		BT_ASSERT(source_class->methods.iterator.next);
		next_method = source_class->methods.iterator.next;
		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		struct bt_component_class_filter *filter_class =
			container_of(iterator->upstream_component->class,
				struct bt_component_class_filter, parent);

		BT_ASSERT(filter_class->methods.iterator.next);
		next_method = filter_class->methods.iterator.next;
		break;
	}
	default:
		abort();
	}

	/*
	 * Call the user's "next" method to get the next notification
	 * and status.
	 */
	BT_ASSERT(next_method);
	BT_LOGD_STR("Calling user's \"next\" method.");
	next_return = next_method(priv_iterator);
	BT_LOGD("User method returned: status=%s",
		bt_notification_iterator_status_string(next_return.status));
	if (next_return.status < 0) {
		BT_LOGW_STR("User method failed.");
		status = next_return.status;
		goto end;
	}

	if (iterator->state == BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_FINALIZED ||
			iterator->state == BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_FINALIZED_AND_ENDED) {
		/*
		 * The user's "next" method, somehow, cancelled its own
		 * notification iterator. This can happen, for example,
		 * when the user's method removes the port on which
		 * there's the connection from which the iterator was
		 * created. In this case, said connection is ended, and
		 * all its notification iterators are finalized.
		 *
		 * Only bt_put() the returned notification if
		 * the status is
		 * BT_NOTIFICATION_ITERATOR_STATUS_OK because
		 * otherwise this field could be garbage.
		 */
		if (next_return.status ==
				BT_NOTIFICATION_ITERATOR_STATUS_OK) {
			bt_put(next_return.notification);
		}

		status = BT_NOTIFICATION_ITERATOR_STATUS_CANCELED;
		goto end;
	}

	switch (next_return.status) {
	case BT_NOTIFICATION_ITERATOR_STATUS_END:
		BT_ASSERT_PRE(priv_conn_notif_iter_can_end(iterator),
			"Notification iterator cannot end at this point: "
			"%!+i", iterator);
		BT_ASSERT(iterator->state ==
			BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_ACTIVE);
		iterator->state = BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_ENDED;
		status = BT_NOTIFICATION_ITERATOR_STATUS_END;
		BT_LOGD("Set new status: status=%s",
			bt_notification_iterator_status_string(status));
		goto end;
	case BT_NOTIFICATION_ITERATOR_STATUS_AGAIN:
		status = BT_NOTIFICATION_ITERATOR_STATUS_AGAIN;
		goto end;
	case BT_NOTIFICATION_ITERATOR_STATUS_OK:
		BT_ASSERT_PRE(next_return.notification,
			"User method returned BT_NOTIFICATION_ITERATOR_STATUS_OK, but notification is NULL: "
			"%!+i", iterator);
		BT_ASSERT_PRE(validate_notification(iterator,
			next_return.notification),
			"Notification is invalid at this point: "
			"%![notif-iter-]+i, %![notif-]+n",
			iterator, next_return.notification);
		*user_notif = next_return.notification;
		break;
	default:
		/* Unknown non-error status */
		abort();
	}

end:
	return status;
}

enum bt_notification_iterator_status
bt_output_port_notification_iterator_next(
		struct bt_notification_iterator *iterator,
		struct bt_notification **user_notif)
{
	enum bt_notification_iterator_status status;
	struct bt_notification_iterator_output_port *out_port_iter =
		(void *) iterator;
	enum bt_graph_status graph_status;

	BT_ASSERT_PRE_NON_NULL(iterator, "Notification iterator");
	BT_ASSERT_PRE_NON_NULL(user_notif, "Notification");
	BT_ASSERT_PRE(iterator->type ==
		BT_NOTIFICATION_ITERATOR_TYPE_OUTPUT_PORT,
		"Notification iterator was not created from an output port: "
		"%!+i", iterator);
	BT_LIB_LOGD("Getting next output port notification iterator's notification: %!+i",
		iterator);
	graph_status = bt_graph_consume_sink_no_check(
		out_port_iter->graph, out_port_iter->colander);
	switch (graph_status) {
	case BT_GRAPH_STATUS_CANCELED:
		BT_ASSERT(!out_port_iter->notif);
		status = BT_NOTIFICATION_ITERATOR_STATUS_CANCELED;
		break;
	case BT_GRAPH_STATUS_AGAIN:
		BT_ASSERT(!out_port_iter->notif);
		status = BT_NOTIFICATION_ITERATOR_STATUS_AGAIN;
		break;
	case BT_GRAPH_STATUS_END:
		BT_ASSERT(!out_port_iter->notif);
		status = BT_NOTIFICATION_ITERATOR_STATUS_END;
		break;
	case BT_GRAPH_STATUS_NOMEM:
		BT_ASSERT(!out_port_iter->notif);
		status = BT_NOTIFICATION_ITERATOR_STATUS_NOMEM;
		break;
	case BT_GRAPH_STATUS_OK:
		BT_ASSERT(out_port_iter->notif);
		status = BT_NOTIFICATION_ITERATOR_STATUS_OK;
		*user_notif = out_port_iter->notif;
		out_port_iter->notif = NULL;
		break;
	default:
		/* Other errors */
		status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
	}

	return status;
}

struct bt_component *bt_private_connection_notification_iterator_get_component(
		struct bt_notification_iterator *iterator)
{
	struct bt_notification_iterator_private_connection *iter_priv_conn;

	BT_ASSERT_PRE_NON_NULL(iterator, "Notification iterator");
	BT_ASSERT_PRE(iterator->type ==
		BT_NOTIFICATION_ITERATOR_TYPE_PRIVATE_CONNECTION,
		"Notification iterator was not created from a private connection: "
		"%!+i", iterator);
	iter_priv_conn = (void *) iterator;
	return bt_get(iter_priv_conn->upstream_component);
}

struct bt_private_component *
bt_private_connection_private_notification_iterator_get_private_component(
		struct bt_private_connection_private_notification_iterator *private_iterator)
{
	return bt_private_component_from_component(
		bt_private_connection_notification_iterator_get_component(
			(void *) bt_private_connection_notification_iterator_borrow_from_private(private_iterator)));
}

static
void bt_output_port_notification_iterator_destroy(struct bt_object *obj)
{
	struct bt_notification_iterator_output_port *iterator =
		(void *) container_of(obj, struct bt_notification_iterator, base);

	BT_LOGD("Destroying output port notification iterator object: addr=%p",
		iterator);
	BT_ASSERT(!iterator->notif);
	BT_LOGD_STR("Putting graph.");
	bt_put(iterator->graph);
	BT_LOGD_STR("Putting colander sink component.");
	bt_put(iterator->colander);
	destroy_base_notification_iterator(obj);
}

struct bt_notification_iterator *bt_output_port_notification_iterator_create(
		struct bt_port *output_port,
		const char *colander_component_name)
{
	struct bt_notification_iterator_output_port *iterator = NULL;
	struct bt_component_class *colander_comp_cls = NULL;
	struct bt_component *output_port_comp = NULL;
	struct bt_component *colander_comp;
	struct bt_graph *graph = NULL;
	enum bt_graph_status graph_status;
	const char *colander_comp_name;
	struct bt_port *colander_in_port = NULL;
	struct bt_component_class_sink_colander_data colander_data;

	BT_ASSERT_PRE_NON_NULL(output_port, "Output port");
	BT_ASSERT_PRE(bt_port_get_type(output_port) == BT_PORT_TYPE_OUTPUT,
		"Port is not an output port: %!+p", output_port);
	output_port_comp = bt_port_get_component(output_port);
	BT_ASSERT_PRE(output_port_comp,
		"Output port has no component: %!+p", output_port);
	graph = bt_component_get_graph(output_port_comp);
	BT_ASSERT(graph);

	/* Create notification iterator */
	BT_LOGD("Creating notification iterator on output port: "
		"comp-addr=%p, comp-name\"%s\", port-addr=%p, port-name=\"%s\"",
		output_port_comp, bt_component_get_name(output_port_comp),
		output_port, bt_port_get_name(output_port));
	iterator = g_new0(struct bt_notification_iterator_output_port, 1);
	if (!iterator) {
		BT_LOGE_STR("Failed to allocate one output port notification iterator.");
		goto error;
	}

	init_notification_iterator((void *) iterator,
		BT_NOTIFICATION_ITERATOR_TYPE_OUTPUT_PORT,
		bt_output_port_notification_iterator_destroy);

	/* Create colander component */
	colander_comp_cls = bt_component_class_sink_colander_get();
	if (!colander_comp_cls) {
		BT_LOGW("Cannot get colander sink component class.");
		goto error;
	}

	BT_MOVE(iterator->graph, graph);
	colander_comp_name =
		colander_component_name ? colander_component_name : "colander";
	colander_data.notification = &iterator->notif;
	graph_status = bt_graph_add_component_with_init_method_data(
		iterator->graph, colander_comp_cls, colander_comp_name,
		NULL, &colander_data, &iterator->colander);
	if (graph_status != BT_GRAPH_STATUS_OK) {
		BT_LOGW("Cannot add colander sink component to graph: "
			"graph-addr=%p, name=\"%s\", graph-status=%s",
			iterator->graph, colander_comp_name,
			bt_graph_status_string(graph_status));
		goto error;
	}

	/*
	 * Connect provided output port to the colander component's
	 * input port.
	 */
	colander_in_port = bt_component_sink_get_input_port_by_index(
		iterator->colander, 0);
	BT_ASSERT(colander_in_port);
	graph_status = bt_graph_connect_ports(iterator->graph,
		output_port, colander_in_port, NULL);
	if (graph_status != BT_GRAPH_STATUS_OK) {
		BT_LOGW("Cannot add colander sink component to graph: "
			"graph-addr=%p, name=\"%s\", graph-status=%s",
			iterator->graph, colander_comp_name,
			bt_graph_status_string(graph_status));
		goto error;
	}

	/*
	 * At this point everything went fine. Make the graph
	 * nonconsumable forever so that only this notification iterator
	 * can consume (thanks to bt_graph_consume_sink_no_check()).
	 * This avoids leaking the notification created by the colander
	 * sink and moved to the notification iterator's notification
	 * member.
	 */
	bt_graph_set_can_consume(iterator->graph, BT_FALSE);
	goto end;

error:
	if (iterator && iterator->graph && iterator->colander) {
		int ret;

		/* Remove created colander component from graph if any */
		colander_comp = iterator->colander;
		BT_PUT(iterator->colander);

		/*
		 * At this point the colander component's reference
		 * count is 0 because iterator->colander was the only
		 * owner. We also know that it is not connected because
		 * this is the last operation before this function
		 * succeeds.
		 *
		 * Since we honor the preconditions here,
		 * bt_graph_remove_unconnected_component() always
		 * succeeds.
		 */
		ret = bt_graph_remove_unconnected_component(iterator->graph,
			colander_comp);
		BT_ASSERT(ret == 0);
	}

	BT_PUT(iterator);

end:
	bt_put(colander_in_port);
	bt_put(colander_comp_cls);
	bt_put(output_port_comp);
	bt_put(graph);
	return (void *) iterator;
}

struct bt_notification_iterator *
bt_private_connection_notification_iterator_borrow_from_private(
		struct bt_private_connection_private_notification_iterator *private_notification_iterator)
{
	return (void *) private_notification_iterator;
}
