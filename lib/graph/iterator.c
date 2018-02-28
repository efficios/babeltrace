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
	bt_bool is_ended;
};

enum action_type {
	ACTION_TYPE_PUSH_NOTIF,
	ACTION_TYPE_MAP_PORT_TO_COMP_IN_STREAM,
	ACTION_TYPE_ADD_STREAM_STATE,
	ACTION_TYPE_SET_STREAM_STATE_IS_ENDED,
	ACTION_TYPE_SET_STREAM_STATE_CUR_PACKET,
	ACTION_TYPE_UPDATE_STREAM_STATE_DISCARDED_PACKETS,
	ACTION_TYPE_UPDATE_STREAM_STATE_DISCARDED_EVENTS,
};

struct action {
	enum action_type type;
	union {
		/* ACTION_TYPE_PUSH_NOTIF */
		struct {
			struct bt_notification *notif; /* owned by this */
		} push_notif;

		/* ACTION_TYPE_MAP_PORT_TO_COMP_IN_STREAM */
		struct {
			struct bt_stream *stream; /* owned by this */
			struct bt_component *component; /* owned by this */
			struct bt_port *port; /* owned by this */
		} map_port_to_comp_in_stream;

		/* ACTION_TYPE_ADD_STREAM_STATE */
		struct {
			struct bt_stream *stream; /* owned by this */
			struct stream_state *stream_state; /* owned by this */
		} add_stream_state;

		/* ACTION_TYPE_SET_STREAM_STATE_IS_ENDED */
		struct {
			struct stream_state *stream_state; /* weak */
		} set_stream_state_is_ended;

		/* ACTION_TYPE_SET_STREAM_STATE_CUR_PACKET */
		struct {
			struct stream_state *stream_state; /* weak */
			struct bt_packet *packet; /* owned by this */
		} set_stream_state_cur_packet;

		/* ACTION_TYPE_UPDATE_STREAM_STATE_DISCARDED_PACKETS */
		/* ACTION_TYPE_UPDATE_STREAM_STATE_DISCARDED_EVENTS */
		struct {
			struct stream_state *stream_state; /* weak */
			struct bt_clock_value *cur_begin; /* owned by this */
			uint64_t cur_count;
		} update_stream_state_discarded_elements;
	} payload;
};

static
void stream_destroy_listener(struct bt_stream *stream, void *data)
{
	struct bt_notification_iterator_private_connection *iterator = data;

	/* Remove associated stream state */
	g_hash_table_remove(iterator->stream_states, stream);
}

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

static
void destroy_action(struct action *action)
{
	BT_ASSERT(action);

	switch (action->type) {
	case ACTION_TYPE_PUSH_NOTIF:
		BT_PUT(action->payload.push_notif.notif);
		break;
	case ACTION_TYPE_MAP_PORT_TO_COMP_IN_STREAM:
		BT_PUT(action->payload.map_port_to_comp_in_stream.stream);
		BT_PUT(action->payload.map_port_to_comp_in_stream.component);
		BT_PUT(action->payload.map_port_to_comp_in_stream.port);
		break;
	case ACTION_TYPE_ADD_STREAM_STATE:
		BT_PUT(action->payload.add_stream_state.stream);
		destroy_stream_state(
			action->payload.add_stream_state.stream_state);
		action->payload.add_stream_state.stream_state = NULL;
		break;
	case ACTION_TYPE_SET_STREAM_STATE_CUR_PACKET:
		BT_PUT(action->payload.set_stream_state_cur_packet.packet);
		break;
	case ACTION_TYPE_SET_STREAM_STATE_IS_ENDED:
		break;
	case ACTION_TYPE_UPDATE_STREAM_STATE_DISCARDED_PACKETS:
	case ACTION_TYPE_UPDATE_STREAM_STATE_DISCARDED_EVENTS:
		BT_PUT(action->payload.update_stream_state_discarded_elements.cur_begin);
		break;
	default:
		BT_LOGF("Unexpected action's type: type=%d", action->type);
		abort();
	}
}

static
void add_action(struct bt_notification_iterator_private_connection *iterator,
		struct action *action)
{
	g_array_append_val(iterator->actions, *action);
}

static
void clear_actions(struct bt_notification_iterator_private_connection *iterator)
{
	size_t i;

	for (i = 0; i < iterator->actions->len; i++) {
		struct action *action = &g_array_index(iterator->actions,
			struct action, i);

		destroy_action(action);
	}

	g_array_set_size(iterator->actions, 0);
}

static inline
const char *action_type_string(enum action_type type)
{
	switch (type) {
	case ACTION_TYPE_PUSH_NOTIF:
		return "ACTION_TYPE_PUSH_NOTIF";
	case ACTION_TYPE_MAP_PORT_TO_COMP_IN_STREAM:
		return "ACTION_TYPE_MAP_PORT_TO_COMP_IN_STREAM";
	case ACTION_TYPE_ADD_STREAM_STATE:
		return "ACTION_TYPE_ADD_STREAM_STATE";
	case ACTION_TYPE_SET_STREAM_STATE_IS_ENDED:
		return "ACTION_TYPE_SET_STREAM_STATE_IS_ENDED";
	case ACTION_TYPE_SET_STREAM_STATE_CUR_PACKET:
		return "ACTION_TYPE_SET_STREAM_STATE_CUR_PACKET";
	case ACTION_TYPE_UPDATE_STREAM_STATE_DISCARDED_PACKETS:
		return "ACTION_TYPE_UPDATE_STREAM_STATE_DISCARDED_PACKETS";
	case ACTION_TYPE_UPDATE_STREAM_STATE_DISCARDED_EVENTS:
		return "ACTION_TYPE_UPDATE_STREAM_STATE_DISCARDED_EVENTS";
	default:
		return "(unknown)";
	}
}

static
void apply_actions(struct bt_notification_iterator_private_connection *iterator)
{
	size_t i;

	BT_LOGV("Applying notification's iterator current actions: "
		"count=%u", iterator->actions->len);

	for (i = 0; i < iterator->actions->len; i++) {
		struct action *action = &g_array_index(iterator->actions,
			struct action, i);

		BT_LOGV("Applying action: index=%zu, type=%s",
			i, action_type_string(action->type));

		switch (action->type) {
		case ACTION_TYPE_PUSH_NOTIF:
			/* Move notification to queue */
			g_queue_push_head(iterator->queue,
				action->payload.push_notif.notif);
			bt_notification_freeze(
				action->payload.push_notif.notif);
			action->payload.push_notif.notif = NULL;
			break;
		case ACTION_TYPE_MAP_PORT_TO_COMP_IN_STREAM:
			bt_stream_map_component_to_port(
				action->payload.map_port_to_comp_in_stream.stream,
				action->payload.map_port_to_comp_in_stream.component,
				action->payload.map_port_to_comp_in_stream.port);
			break;
		case ACTION_TYPE_ADD_STREAM_STATE:
			/* Move stream state to hash table */
			g_hash_table_insert(iterator->stream_states,
				action->payload.add_stream_state.stream,
				action->payload.add_stream_state.stream_state);

			action->payload.add_stream_state.stream_state = NULL;
			break;
		case ACTION_TYPE_SET_STREAM_STATE_IS_ENDED:
			/*
			 * We know that this stream is ended. We need to
			 * remember this as long as the stream exists to
			 * enforce that the same stream does not end
			 * twice.
			 *
			 * Here we add a destroy listener to the stream
			 * which we put after (becomes weak as the hash
			 * table key). If we were the last object to own
			 * this stream, the destroy listener is called
			 * when we call bt_put() which removes this
			 * stream state completely. This is important
			 * because the memory used by this stream object
			 * could be reused for another stream, and they
			 * must have different states.
			 */
			bt_stream_add_destroy_listener(
				action->payload.set_stream_state_is_ended.stream_state->stream,
				stream_destroy_listener, iterator);
			action->payload.set_stream_state_is_ended.stream_state->is_ended = BT_TRUE;
			BT_PUT(action->payload.set_stream_state_is_ended.stream_state->stream);
			break;
		case ACTION_TYPE_SET_STREAM_STATE_CUR_PACKET:
			/* Move packet to stream state's current packet */
			BT_MOVE(action->payload.set_stream_state_cur_packet.stream_state->cur_packet,
				action->payload.set_stream_state_cur_packet.packet);
			break;
		case ACTION_TYPE_UPDATE_STREAM_STATE_DISCARDED_PACKETS:
		case ACTION_TYPE_UPDATE_STREAM_STATE_DISCARDED_EVENTS:
		{
			struct discarded_elements_state *state;

			if (action->type == ACTION_TYPE_UPDATE_STREAM_STATE_DISCARDED_PACKETS) {
				state = &action->payload.update_stream_state_discarded_elements.stream_state->discarded_packets_state;
			} else {
				state = &action->payload.update_stream_state_discarded_elements.stream_state->discarded_events_state;
			}

			BT_MOVE(state->cur_begin,
				action->payload.update_stream_state_discarded_elements.cur_begin);
			state->cur_count = action->payload.update_stream_state_discarded_elements.cur_count;
			break;
		}
		default:
			BT_LOGF("Unexpected action's type: type=%d",
				action->type);
			abort();
		}
	}

	clear_actions(iterator);
}

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
	 * seqnum = 0.
	 */
	stream_state->discarded_packets_state.cur_count = -1ULL;

	/*
	 * We keep a reference to the stream until we know it's ended
	 * because we need to be able to create an automatic "stream
	 * end" notification when the user's "next" method returns
	 * BT_NOTIFICATION_ITERATOR_STATUS_END.
	 *
	 * We put this reference when the stream is marked as ended.
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
	struct bt_notification_iterator *iterator =
		container_of(obj, struct bt_notification_iterator, base);

	BT_LOGD_STR("Putting current notification.");
	bt_put(iterator->current_notification);
	g_free(iterator);
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
	obj->ref_count.count++;
	iterator = (void *) container_of(obj, struct bt_notification_iterator, base);
	BT_LOGD("Destroying private connection notification iterator object: addr=%p",
		iterator);
	bt_private_connection_notification_iterator_finalize(iterator);

	if (iterator->queue) {
		struct bt_notification *notif;

		BT_LOGD("Putting notifications in queue.");

		while ((notif = g_queue_pop_tail(iterator->queue))) {
			bt_put(notif);
		}

		g_queue_free(iterator->queue);
	}

	if (iterator->stream_states) {
		/*
		 * Remove our destroy listener from each stream which
		 * has a state in this iterator. Otherwise the destroy
		 * listener would be called with an invalid/other
		 * notification iterator object.
		 */
		GHashTableIter ht_iter;
		gpointer stream_gptr, stream_state_gptr;

		g_hash_table_iter_init(&ht_iter, iterator->stream_states);

		while (g_hash_table_iter_next(&ht_iter, &stream_gptr, &stream_state_gptr)) {
			BT_ASSERT(stream_gptr);

			BT_LOGD_STR("Removing stream's destroy listener for notification iterator.");
			bt_stream_remove_destroy_listener(
				(void *) stream_gptr, stream_destroy_listener,
				iterator);
		}

		g_hash_table_destroy(iterator->stream_states);
	}

	if (iterator->actions) {
		g_array_free(iterator->actions, TRUE);
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
int create_subscription_mask_from_notification_types(
		struct bt_notification_iterator_private_connection *iterator,
		const enum bt_notification_type *notif_types)
{
	const enum bt_notification_type *notif_type;
	int ret = 0;

	BT_ASSERT(notif_types);
	iterator->subscription_mask = 0;

	for (notif_type = notif_types;
			*notif_type != BT_NOTIFICATION_TYPE_SENTINEL;
			notif_type++) {
		switch (*notif_type) {
		case BT_NOTIFICATION_TYPE_ALL:
			iterator->subscription_mask |=
				BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_EVENT |
				BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_INACTIVITY |
				BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_STREAM_BEGIN |
				BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_STREAM_END |
				BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_PACKET_BEGIN |
				BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_PACKET_END |
				BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_DISCARDED_EVENTS |
				BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_DISCARDED_PACKETS;
			break;
		case BT_NOTIFICATION_TYPE_EVENT:
			iterator->subscription_mask |= BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_EVENT;
			break;
		case BT_NOTIFICATION_TYPE_INACTIVITY:
			iterator->subscription_mask |= BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_INACTIVITY;
			break;
		case BT_NOTIFICATION_TYPE_STREAM_BEGIN:
			iterator->subscription_mask |= BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_STREAM_BEGIN;
			break;
		case BT_NOTIFICATION_TYPE_STREAM_END:
			iterator->subscription_mask |= BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_STREAM_END;
			break;
		case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
			iterator->subscription_mask |= BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_PACKET_BEGIN;
			break;
		case BT_NOTIFICATION_TYPE_PACKET_END:
			iterator->subscription_mask |= BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_PACKET_END;
			break;
		case BT_NOTIFICATION_TYPE_DISCARDED_EVENTS:
			iterator->subscription_mask |= BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_DISCARDED_EVENTS;
			break;
		case BT_NOTIFICATION_TYPE_DISCARDED_PACKETS:
			iterator->subscription_mask |= BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_DISCARDED_PACKETS;
			break;
		default:
			ret = -1;
			goto end;
		}

		BT_LOGV("Added notification type to subscription mask: "
			"type=%s, mask=%x",
			bt_notification_type_string(*notif_type),
			iterator->subscription_mask);
	}

end:
	return ret;
}

static
void init_notification_iterator(struct bt_notification_iterator *iterator,
		enum bt_notification_iterator_type type,
		bt_object_release_func destroy)
{
	bt_object_init(iterator, destroy);
	iterator->type = type;
}

BT_HIDDEN
enum bt_connection_status bt_private_connection_notification_iterator_create(
		struct bt_component *upstream_comp,
		struct bt_port *upstream_port,
		const enum bt_notification_type *notification_types,
		struct bt_connection *connection,
		struct bt_notification_iterator_private_connection **user_iterator)
{
	enum bt_connection_status status = BT_CONNECTION_STATUS_OK;
	enum bt_component_class_type type;
	struct bt_notification_iterator_private_connection *iterator = NULL;

	BT_ASSERT(upstream_comp);
	BT_ASSERT(upstream_port);
	BT_ASSERT(notification_types);
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

	if (create_subscription_mask_from_notification_types(iterator,
			notification_types)) {
		BT_LOGW_STR("Cannot create subscription mask from notification types.");
		status = BT_CONNECTION_STATUS_INVALID;
		goto end;
	}

	iterator->stream_states = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, (GDestroyNotify) destroy_stream_state);
	if (!iterator->stream_states) {
		BT_LOGE_STR("Failed to allocate a GHashTable.");
		status = BT_CONNECTION_STATUS_NOMEM;
		goto end;
	}

	iterator->queue = g_queue_new();
	if (!iterator->queue) {
		BT_LOGE_STR("Failed to allocate a GQueue.");
		status = BT_CONNECTION_STATUS_NOMEM;
		goto end;
	}

	iterator->actions = g_array_new(FALSE, FALSE, sizeof(struct action));
	if (!iterator->actions) {
		BT_LOGE_STR("Failed to allocate a GArray.");
		status = BT_CONNECTION_STATUS_NOMEM;
		goto end;
	}

	iterator->upstream_component = upstream_comp;
	iterator->upstream_port = upstream_port;
	iterator->connection = connection;
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
	struct bt_notification_iterator_private_connection *iterator =
		bt_private_connection_notification_iterator_borrow_from_private(private_iterator);

	return iterator ? iterator->user_data : NULL;
}

enum bt_notification_iterator_status
bt_private_connection_private_notification_iterator_set_user_data(
		struct bt_private_connection_private_notification_iterator *private_iterator,
		void *data)
{
	enum bt_notification_iterator_status ret =
			BT_NOTIFICATION_ITERATOR_STATUS_OK;
	struct bt_notification_iterator_private_connection *iterator =
		bt_private_connection_notification_iterator_borrow_from_private(private_iterator);

	if (!iterator) {
		BT_LOGW_STR("Invalid parameter: notification iterator is NULL.");
		ret = BT_NOTIFICATION_ITERATOR_STATUS_INVALID;
		goto end;
	}

	iterator->user_data = data;
	BT_LOGV("Set notification iterator's user data: "
		"iter-addr=%p, user-data-addr=%p", iterator, data);

end:
	return ret;
}

struct bt_notification *bt_notification_iterator_get_notification(
		struct bt_notification_iterator *iterator)
{
	struct bt_notification *notification = NULL;

	if (!iterator) {
		BT_LOGW_STR("Invalid parameter: notification iterator is NULL.");
		goto end;
	}

	notification = bt_get(
		bt_notification_iterator_borrow_current_notification(iterator));

end:
	return notification;
}

static
enum bt_private_connection_notification_iterator_notif_type
bt_notification_iterator_notif_type_from_notif_type(
		enum bt_notification_type notif_type)
{
	enum bt_private_connection_notification_iterator_notif_type iter_notif_type;

	switch (notif_type) {
	case BT_NOTIFICATION_TYPE_EVENT:
		iter_notif_type = BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_EVENT;
		break;
	case BT_NOTIFICATION_TYPE_INACTIVITY:
		iter_notif_type = BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_INACTIVITY;
		break;
	case BT_NOTIFICATION_TYPE_STREAM_BEGIN:
		iter_notif_type = BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_STREAM_BEGIN;
		break;
	case BT_NOTIFICATION_TYPE_STREAM_END:
		iter_notif_type = BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_STREAM_END;
		break;
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
		iter_notif_type = BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_PACKET_BEGIN;
		break;
	case BT_NOTIFICATION_TYPE_PACKET_END:
		iter_notif_type = BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_PACKET_END;
		break;
	case BT_NOTIFICATION_TYPE_DISCARDED_EVENTS:
		iter_notif_type = BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_DISCARDED_EVENTS;
		break;
	case BT_NOTIFICATION_TYPE_DISCARDED_PACKETS:
		iter_notif_type = BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_DISCARDED_PACKETS;
		break;
	default:
		abort();
	}

	return iter_notif_type;
}

static
bt_bool validate_notification(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_notification *notif,
		struct bt_stream *notif_stream,
		struct bt_packet *notif_packet)
{
	bt_bool is_valid = BT_TRUE;
	struct stream_state *stream_state;
	struct bt_port *stream_comp_cur_port;

	BT_ASSERT(notif_stream);
	stream_comp_cur_port =
		bt_stream_port_for_component(notif_stream,
			iterator->upstream_component);
	if (!stream_comp_cur_port) {
		/*
		 * This is the first time this notification iterator
		 * bumps into this stream. Add an action to map the
		 * iterator's upstream component to the iterator's
		 * upstream port in this stream.
		 */
		struct action action = {
			.type = ACTION_TYPE_MAP_PORT_TO_COMP_IN_STREAM,
			.payload.map_port_to_comp_in_stream = {
				.stream = bt_get(notif_stream),
				.component = bt_get(iterator->upstream_component),
				.port = bt_get(iterator->upstream_port),
			},
		};

		add_action(iterator, &action);
	} else {
		if (stream_comp_cur_port != iterator->upstream_port) {
			/*
			 * It looks like two different ports of the same
			 * component are emitting notifications which
			 * have references to the same stream. This is
			 * bad: the API guarantees that it can never
			 * happen.
			 */
			BT_LOGW("Two different ports of the same component are emitting notifications which refer to the same stream: "
				"stream-addr=%p, stream-name=\"%s\", "
				"stream-comp-cur-port-addr=%p, "
				"stream-comp-cur-port-name=%p, "
				"iter-upstream-port-addr=%p, "
				"iter-upstream-port-name=%s",
				notif_stream,
				bt_stream_get_name(notif_stream),
				stream_comp_cur_port,
				bt_port_get_name(stream_comp_cur_port),
				iterator->upstream_port,
				bt_port_get_name(iterator->upstream_port));
			is_valid = BT_FALSE;
			goto end;
		}

	}

	stream_state = g_hash_table_lookup(iterator->stream_states,
		notif_stream);
	if (stream_state) {
		BT_LOGV("Stream state already exists: "
			"stream-addr=%p, stream-name=\"%s\", "
			"stream-state-addr=%p",
			notif_stream,
			bt_stream_get_name(notif_stream), stream_state);

		if (stream_state->is_ended) {
			/*
			 * There's a new notification which has a
			 * reference to a stream which, from this
			 * iterator's point of view, is ended ("end of
			 * stream" notification was returned). This is
			 * bad: the API guarantees that it can never
			 * happen.
			 */
			BT_LOGW("Stream is already ended: "
				"stream-addr=%p, stream-name=\"%s\"",
				notif_stream,
				bt_stream_get_name(notif_stream));
			is_valid = BT_FALSE;
			goto end;
		}

		switch (notif->type) {
		case BT_NOTIFICATION_TYPE_STREAM_BEGIN:
			/*
			 * We already have a stream state, which means
			 * we already returned a "stream begin"
			 * notification: this is an invalid duplicate.
			 */
			BT_LOGW("Duplicate stream beginning notification: "
				"stream-addr=%p, stream-name=\"%s\"",
				notif_stream,
				bt_stream_get_name(notif_stream));
			is_valid = BT_FALSE;
			goto end;
		case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
			if (notif_packet == stream_state->cur_packet) {
				/* Duplicate "packet begin" notification */
				BT_LOGW("Duplicate stream beginning notification: "
					"stream-addr=%p, stream-name=\"%s\", "
					"packet-addr=%p",
					notif_stream,
					bt_stream_get_name(notif_stream),
					notif_packet);
				is_valid = BT_FALSE;
				goto end;
			}
			break;
		default:
			break;
		}
	}

end:
	return is_valid;
}

static
bt_bool is_subscribed_to_notification_type(
		struct bt_notification_iterator_private_connection *iterator,
		enum bt_notification_type notif_type)
{
	uint32_t iter_notif_type =
		(uint32_t) bt_notification_iterator_notif_type_from_notif_type(
			notif_type);

	return (iter_notif_type & iterator->subscription_mask) ? BT_TRUE : BT_FALSE;
}

static
void add_action_push_notif(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_notification *notif)
{
	struct action action = {
		.type = ACTION_TYPE_PUSH_NOTIF,
	};

	BT_ASSERT(notif);

	if (!is_subscribed_to_notification_type(iterator, notif->type)) {
		return;
	}

	action.payload.push_notif.notif = bt_get(notif);
	add_action(iterator, &action);
	BT_LOGV("Added \"push notification\" action: notif-addr=%p", notif);
}

static
int add_action_push_notif_stream_begin(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_stream *stream)
{
	int ret = 0;
	struct bt_notification *stream_begin_notif = NULL;

	if (!is_subscribed_to_notification_type(iterator,
			BT_NOTIFICATION_TYPE_STREAM_BEGIN)) {
		BT_LOGV("Not adding \"push stream beginning notification\" action: "
			"notification iterator is not subscribed: addr=%p",
			iterator);
		goto end;
	}

	BT_ASSERT(stream);
	stream_begin_notif = bt_notification_stream_begin_create(stream);
	if (!stream_begin_notif) {
		BT_LOGE_STR("Cannot create stream beginning notification.");
		goto error;
	}

	add_action_push_notif(iterator, stream_begin_notif);
	BT_LOGV("Added \"push stream beginning notification\" action: "
		"stream-addr=%p, stream-name=\"%s\"",
		stream, bt_stream_get_name(stream));
	goto end;

error:
	ret = -1;

end:
	bt_put(stream_begin_notif);
	return ret;
}

static
int add_action_push_notif_stream_end(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_stream *stream)
{
	int ret = 0;
	struct bt_notification *stream_end_notif = NULL;

	if (!is_subscribed_to_notification_type(iterator,
			BT_NOTIFICATION_TYPE_STREAM_END)) {
		BT_LOGV("Not adding \"push stream end notification\" action: "
			"notification iterator is not subscribed: addr=%p",
			iterator);
		goto end;
	}

	BT_ASSERT(stream);
	stream_end_notif = bt_notification_stream_end_create(stream);
	if (!stream_end_notif) {
		BT_LOGE_STR("Cannot create stream end notification.");
		goto error;
	}

	add_action_push_notif(iterator, stream_end_notif);
	BT_LOGV("Added \"push stream end notification\" action: "
		"stream-addr=%p, stream-name=\"%s\"",
		stream, bt_stream_get_name(stream));
	goto end;

error:
	ret = -1;

end:
	bt_put(stream_end_notif);
	return ret;
}

static
int add_action_push_notif_packet_begin(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_packet *packet)
{
	int ret = 0;
	struct bt_notification *packet_begin_notif = NULL;

	if (!is_subscribed_to_notification_type(iterator,
			BT_NOTIFICATION_TYPE_PACKET_BEGIN)) {
		BT_LOGV("Not adding \"push packet beginning notification\" action: "
			"notification iterator is not subscribed: addr=%p",
			iterator);
		goto end;
	}

	BT_ASSERT(packet);
	packet_begin_notif = bt_notification_packet_begin_create(packet);
	if (!packet_begin_notif) {
		BT_LOGE_STR("Cannot create packet beginning notification.");
		goto error;
	}

	add_action_push_notif(iterator, packet_begin_notif);
	BT_LOGV("Added \"push packet beginning notification\" action: "
		"packet-addr=%p", packet);
	goto end;

error:
	ret = -1;

end:
	bt_put(packet_begin_notif);
	return ret;
}

static
int add_action_push_notif_packet_end(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_packet *packet)
{
	int ret = 0;
	struct bt_notification *packet_end_notif = NULL;

	if (!is_subscribed_to_notification_type(iterator,
			BT_NOTIFICATION_TYPE_PACKET_END)) {
		BT_LOGV("Not adding \"push packet end notification\" action: "
			"notification iterator is not subscribed: addr=%p",
			iterator);
		goto end;
	}

	BT_ASSERT(packet);
	packet_end_notif = bt_notification_packet_end_create(packet);
	if (!packet_end_notif) {
		BT_LOGE_STR("Cannot create packet end notification.");
		goto error;
	}

	add_action_push_notif(iterator, packet_end_notif);
	BT_LOGV("Added \"push packet end notification\" action: "
		"packet-addr=%p", packet);
	goto end;

error:
	ret = -1;

end:
	bt_put(packet_end_notif);
	return ret;
}

static
void add_action_set_stream_state_is_ended(
		struct bt_notification_iterator_private_connection *iterator,
		struct stream_state *stream_state)
{
	struct action action = {
		.type = ACTION_TYPE_SET_STREAM_STATE_IS_ENDED,
		.payload.set_stream_state_is_ended = {
			.stream_state = stream_state,
		},
	};

	BT_ASSERT(stream_state);
	add_action(iterator, &action);
	BT_LOGV("Added \"set stream state's ended\" action: "
		"stream-state-addr=%p", stream_state);
}

static
void add_action_set_stream_state_cur_packet(
		struct bt_notification_iterator_private_connection *iterator,
		struct stream_state *stream_state,
		struct bt_packet *packet)
{
	struct action action = {
		.type = ACTION_TYPE_SET_STREAM_STATE_CUR_PACKET,
		.payload.set_stream_state_cur_packet = {
			.stream_state = stream_state,
			.packet = bt_get(packet),
		},
	};

	BT_ASSERT(stream_state);
	add_action(iterator, &action);
	BT_LOGV("Added \"set stream state's current packet\" action: "
		"stream-state-addr=%p, packet-addr=%p",
		stream_state, packet);
}

static
void add_action_update_stream_state_discarded_elements(
		struct bt_notification_iterator_private_connection *iterator,
		enum action_type type,
		struct stream_state *stream_state,
		struct bt_clock_value *cur_begin,
		uint64_t cur_count)
{
	struct action action = {
		.type = type,
		.payload.update_stream_state_discarded_elements = {
			.stream_state = stream_state,
			.cur_begin = bt_get(cur_begin),
			.cur_count = cur_count,
		},
	};

	BT_ASSERT(stream_state);
	BT_ASSERT(type == ACTION_TYPE_UPDATE_STREAM_STATE_DISCARDED_PACKETS ||
			type == ACTION_TYPE_UPDATE_STREAM_STATE_DISCARDED_EVENTS);
	add_action(iterator, &action);
	if (type == ACTION_TYPE_UPDATE_STREAM_STATE_DISCARDED_PACKETS) {
		BT_LOGV("Added \"update stream state's discarded packets\" action: "
			"stream-state-addr=%p, cur-begin-addr=%p, cur-count=%" PRIu64,
			stream_state, cur_begin, cur_count);
	} else if (type == ACTION_TYPE_UPDATE_STREAM_STATE_DISCARDED_EVENTS) {
		BT_LOGV("Added \"update stream state's discarded events\" action: "
			"stream-state-addr=%p, cur-begin-addr=%p, cur-count=%" PRIu64,
			stream_state, cur_begin, cur_count);
	}
}

static
int ensure_stream_state_exists(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_notification *stream_begin_notif,
		struct bt_stream *notif_stream,
		struct stream_state **_stream_state)
{
	int ret = 0;
	struct stream_state *stream_state = NULL;

	if (!notif_stream) {
		/*
		 * The notification does not reference any stream: no
		 * need to get or create a stream state.
		 */
		goto end;
	}

	stream_state = g_hash_table_lookup(iterator->stream_states,
		notif_stream);
	if (!stream_state) {
		/*
		 * This iterator did not bump into this stream yet:
		 * create a stream state and a "stream begin"
		 * notification.
		 */
		struct action action = {
			.type = ACTION_TYPE_ADD_STREAM_STATE,
			.payload.add_stream_state = {
				.stream = bt_get(notif_stream),
				.stream_state = NULL,
			},
		};

		stream_state = create_stream_state(notif_stream);
		if (!stream_state) {
			BT_LOGE_STR("Cannot create stream state.");
			goto error;
		}

		action.payload.add_stream_state.stream_state = stream_state;
		add_action(iterator, &action);

		if (stream_begin_notif) {
			add_action_push_notif(iterator, stream_begin_notif);
		} else {
			ret = add_action_push_notif_stream_begin(iterator,
				notif_stream);
			if (ret) {
				BT_LOGE_STR("Cannot add \"push stream beginning notification\" action.");
				goto error;
			}
		}
	}
	goto end;

error:
	destroy_stream_state(stream_state);
	stream_state = NULL;
	ret = -1;

end:
	*_stream_state = stream_state;
	return ret;
}

static
struct bt_field *get_struct_field_uint(struct bt_field *struct_field,
		const char *field_name)
{
	struct bt_field *field = NULL;
	struct bt_field_type *ft = NULL;

	field = bt_field_structure_get_field_by_name(struct_field,
		field_name);
	if (!field) {
		BT_LOGV_STR("`%s` field does not exist.");
		goto end;
	}

	if (!bt_field_is_integer(field)) {
		BT_LOGV("Skipping `%s` field because its type is not an integer field type: "
			"field-addr=%p, ft-addr=%p, ft-id=%s", field_name,
			field, ft, bt_field_type_id_string(
				bt_field_type_get_type_id(ft)));
		BT_PUT(field);
		goto end;
	}

	ft = bt_field_get_type(field);
	BT_ASSERT(ft);

	if (bt_field_type_integer_is_signed(ft)) {
		BT_LOGV("Skipping `%s` integer field because its type is signed: "
			"field-addr=%p, ft-addr=%p", field_name, field, ft);
		BT_PUT(field);
		goto end;
	}

end:
	bt_put(ft);
	return field;
}

static
uint64_t get_packet_context_events_discarded(struct bt_packet *packet)
{
	struct bt_field *packet_context = NULL;
	struct bt_field *field = NULL;
	uint64_t retval = -1ULL;
	int ret;

	packet_context = bt_packet_get_context(packet);
	if (!packet_context) {
		goto end;
	}

	field = get_struct_field_uint(packet_context, "events_discarded");
	if (!field) {
		BT_LOGV("`events_discarded` field does not exist in packet's context field: "
			"packet-addr=%p, packet-context-field-addr=%p",
			packet, packet_context);
		goto end;
	}

	BT_ASSERT(bt_field_is_integer(field));
	ret = bt_field_unsigned_integer_get_value(field, &retval);
	if (ret) {
		BT_LOGV("Cannot get raw value of packet's context field's `events_discarded` integer field: "
			"packet-addr=%p, field-addr=%p",
			packet, field);
		retval = -1ULL;
		goto end;
	}

end:
	bt_put(packet_context);
	bt_put(field);
	return retval;
}

static
uint64_t get_packet_context_packet_seq_num(struct bt_packet *packet)
{
	struct bt_field *packet_context = NULL;
	struct bt_field *field = NULL;
	uint64_t retval = -1ULL;
	int ret;

	packet_context = bt_packet_get_context(packet);
	if (!packet_context) {
		goto end;
	}

	field = get_struct_field_uint(packet_context, "packet_seq_num");
	if (!field) {
		BT_LOGV("`packet_seq_num` field does not exist in packet's context field: "
			"packet-addr=%p, packet-context-field-addr=%p",
			packet, packet_context);
		goto end;
	}

	BT_ASSERT(bt_field_is_integer(field));
	ret = bt_field_unsigned_integer_get_value(field, &retval);
	if (ret) {
		BT_LOGV("Cannot get raw value of packet's context field's `packet_seq_num` integer field: "
			"packet-addr=%p, field-addr=%p",
			packet, field);
		retval = -1ULL;
		goto end;
	}

end:
	bt_put(packet_context);
	bt_put(field);
	return retval;
}

static
int handle_discarded_packets(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_packet *packet,
		struct bt_clock_value *ts_begin,
		struct bt_clock_value *ts_end,
		struct stream_state *stream_state)
{
	struct bt_notification *notif = NULL;
	uint64_t diff;
	uint64_t prev_count, next_count;
	int ret = 0;

	next_count = get_packet_context_packet_seq_num(packet);
	if (next_count == -1ULL) {
		/*
		 * Stream does not have seqnum field, skip discarded
		 * packets feature.
		 */
		goto end;
	}
	prev_count = stream_state->discarded_packets_state.cur_count;

	if (prev_count != -1ULL) {
		if (next_count < prev_count) {
			BT_LOGW("Current value of packet's context field's `packet_seq_num` field is lesser than the previous value for the same stream: "
				"not updating the stream state's current value: "
				"packet-addr=%p, prev-count=%" PRIu64 ", "
				"cur-count=%" PRIu64,
				packet, prev_count, next_count);
			goto end;
		}
		if (next_count == prev_count) {
			BT_LOGW("Current value of packet's context field's `packet_seq_num` field is equal to the previous value for the same stream: "
				"not updating the stream state's current value: "
				"packet-addr=%p, prev-count=%" PRIu64 ", "
				"cur-count=%" PRIu64,
				packet, prev_count, next_count);
			goto end;
		}

		diff = next_count - prev_count;
		if (diff > 1) {
			/*
			 * Add a discarded packets notification. The packets
			 * are considered to be lost between the state's last time
			 * and the current packet's beginning time.
			 * The counter is expected to monotonically increase of
			 * 1 for each packet. Therefore, the number of missing
			 * packets is 'diff - 1'.
			 */
			notif = bt_notification_discarded_elements_create(
				BT_NOTIFICATION_TYPE_DISCARDED_PACKETS,
				stream_state->stream,
				stream_state->discarded_packets_state.cur_begin,
				ts_begin, diff - 1);
			if (!notif) {
				BT_LOGE_STR("Cannot create discarded packets notification.");
				ret = -1;
				goto end;
			}

			add_action_push_notif(iterator, notif);
		}
	}

	add_action_update_stream_state_discarded_elements(iterator,
		ACTION_TYPE_UPDATE_STREAM_STATE_DISCARDED_PACKETS,
		stream_state, ts_end, next_count);

end:
	bt_put(notif);
	return ret;
}

static
int handle_discarded_events(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_packet *packet,
		struct bt_clock_value *ts_begin,
		struct bt_clock_value *ts_end,
		struct stream_state *stream_state)
{
	struct bt_notification *notif = NULL;
	uint64_t diff;
	uint64_t next_count;
	int ret = 0;

	next_count = get_packet_context_events_discarded(packet);
	if (next_count == -1ULL) {
		next_count = stream_state->discarded_events_state.cur_count;
		goto update_state;
	}

	if (next_count < stream_state->discarded_events_state.cur_count) {
		BT_LOGW("Current value of packet's context field's `events_discarded` field is lesser than the previous value for the same stream: "
			"not updating the stream state's current value: "
			"packet-addr=%p, prev-count=%" PRIu64 ", "
			"cur-count=%" PRIu64,
			packet, stream_state->discarded_events_state.cur_count,
			next_count);
		goto end;
	}

	diff = next_count - stream_state->discarded_events_state.cur_count;
	if (diff > 0) {
		/*
		 * Add a discarded events notification. The events are
		 * considered to be lost betweem the state's last time
		 * and the current packet's end time.
		 */
		notif = bt_notification_discarded_elements_create(
			BT_NOTIFICATION_TYPE_DISCARDED_EVENTS,
			stream_state->stream,
			stream_state->discarded_events_state.cur_begin,
			ts_end, diff);
		if (!notif) {
			BT_LOGE_STR("Cannot create discarded events notification.");
			ret = -1;
			goto end;
		}

		add_action_push_notif(iterator, notif);
	}

update_state:
	add_action_update_stream_state_discarded_elements(iterator,
		ACTION_TYPE_UPDATE_STREAM_STATE_DISCARDED_EVENTS,
		stream_state, ts_end, next_count);

end:
	bt_put(notif);
	return ret;
}

static
int get_field_clock_value(struct bt_field *root_field,
		const char *field_name,
		struct bt_clock_value **user_clock_val)
{
	struct bt_field *field;
	struct bt_field_type *ft = NULL;
	struct bt_clock_class *clock_class = NULL;
	struct bt_clock_value *clock_value = NULL;
	uint64_t val;
	int ret = 0;

	field = get_struct_field_uint(root_field, field_name);
	if (!field) {
		/* Not an error: skip this */
		goto end;
	}

	ft = bt_field_get_type(field);
	BT_ASSERT(ft);
	clock_class = bt_field_type_integer_get_mapped_clock_class(ft);
	if (!clock_class) {
		BT_LOGW("Integer field type has no mapped clock class but it's expected to have one: "
			"ft-addr=%p", ft);
		ret = -1;
		goto end;
	}

	ret = bt_field_unsigned_integer_get_value(field, &val);
	if (ret) {
		BT_LOGW("Cannot get integer field's raw value: "
			"field-addr=%p", field);
		ret = -1;
		goto end;
	}

	clock_value = bt_clock_value_create(clock_class, val);
	if (!clock_value) {
		BT_LOGE_STR("Cannot create clock value from clock class.");
		ret = -1;
		goto end;
	}

	/* Move clock value to user */
	*user_clock_val = clock_value;
	clock_value = NULL;

end:
	bt_put(field);
	bt_put(ft);
	bt_put(clock_class);
	bt_put(clock_value);
	return ret;
}

static
int get_ts_begin_ts_end_from_packet(struct bt_packet *packet,
		struct bt_clock_value **user_ts_begin,
		struct bt_clock_value **user_ts_end)
{
	struct bt_field *packet_context = NULL;
	struct bt_clock_value *ts_begin = NULL;
	struct bt_clock_value *ts_end = NULL;
	int ret = 0;

	packet_context = bt_packet_get_context(packet);
	if (!packet_context) {
		goto end;
	}

	ret = get_field_clock_value(packet_context, "timestamp_begin",
		&ts_begin);
	if (ret) {
		BT_LOGW("Cannot create clock value for packet context's `timestamp_begin` field: "
			"packet-addr=%p, packet-context-field-addr=%p",
			packet, packet_context);
		goto end;
	}

	ret = get_field_clock_value(packet_context, "timestamp_end",
		&ts_end);
	if (ret) {
		BT_LOGW("Cannot create clock value for packet context's `timestamp_begin` field: "
			"packet-addr=%p, packet-context-field-addr=%p",
			packet, packet_context);
		goto end;
	}

	/* Move clock values to user */
	*user_ts_begin = ts_begin;
	ts_begin = NULL;
	*user_ts_end = ts_end;
	ts_end = NULL;

end:
	bt_put(packet_context);
	bt_put(ts_begin);
	bt_put(ts_end);
	return ret;
}

static
int handle_discarded_elements(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_packet *packet, struct stream_state *stream_state)
{
	struct bt_clock_value *ts_begin = NULL;
	struct bt_clock_value *ts_end = NULL;
	int ret;

	ret = get_ts_begin_ts_end_from_packet(packet, &ts_begin, &ts_end);
	if (ret) {
		BT_LOGW("Cannot get packet's beginning or end clock values: "
			"packet-addr=%p, ret=%d", packet, ret);
		ret = -1;
		goto end;
	}

	ret = handle_discarded_packets(iterator, packet, ts_begin, ts_end,
		stream_state);
	if (ret) {
		BT_LOGW("Cannot handle discarded packets for packet: "
			"packet-addr=%p, ret=%d", packet, ret);
		ret = -1;
		goto end;
	}

	ret = handle_discarded_events(iterator, packet, ts_begin, ts_end,
		stream_state);
	if (ret) {
		BT_LOGW("Cannot handle discarded events for packet: "
			"packet-addr=%p, ret=%d", packet, ret);
		ret = -1;
		goto end;
	}

end:
	bt_put(ts_begin);
	bt_put(ts_end);
	return ret;
}

static
int handle_packet_switch(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_notification *packet_begin_notif,
		struct bt_packet *new_packet,
		struct stream_state *stream_state)
{
	int ret = 0;

	if (stream_state->cur_packet == new_packet) {
		goto end;
	}

	BT_LOGV("Handling packet switch: "
		"cur-packet-addr=%p, new-packet-addr=%p",
		stream_state->cur_packet, new_packet);

	if (stream_state->cur_packet) {
		/* End of the current packet */
		ret = add_action_push_notif_packet_end(iterator,
			stream_state->cur_packet);
		if (ret) {
			BT_LOGE_STR("Cannot add \"push packet end notification\" action.");
			goto error;
		}
	}

	/*
	 * Check the new packet's context fields for discarded packets
	 * and events to emit those automatic notifications.
	 */
	ret = handle_discarded_elements(iterator, new_packet, stream_state);
	if (ret) {
		BT_LOGE_STR("Cannot handle discarded elements for new packet.");
		goto error;
	}

	/* Beginning of the new packet */
	if (packet_begin_notif) {
		add_action_push_notif(iterator, packet_begin_notif);
	} else if (new_packet) {
		ret = add_action_push_notif_packet_begin(iterator,
			new_packet);
		if (ret) {
			BT_LOGE_STR("Cannot add \"push packet beginning notification\" action.");
			goto error;
		}
	}

	add_action_set_stream_state_cur_packet(iterator, stream_state,
		new_packet);
	goto end;

error:
	ret = -1;

end:
	return ret;
}

static
int handle_notif_stream_begin(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_notification *notif,
		struct bt_stream *notif_stream)
{
	int ret = 0;
	struct stream_state *stream_state;

	BT_ASSERT(notif->type == BT_NOTIFICATION_TYPE_STREAM_BEGIN);
	BT_ASSERT(notif_stream);
	ret = ensure_stream_state_exists(iterator, notif, notif_stream,
		&stream_state);
	if (ret) {
		BT_LOGE_STR("Cannot ensure that stream state exists.");
		goto error;
	}

	goto end;

error:
	ret = -1;

end:
	return ret;
}

static
int handle_notif_stream_end(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_notification *notif,
		struct bt_stream *notif_stream)
{
	int ret = 0;
	struct stream_state *stream_state;

	BT_ASSERT(notif->type == BT_NOTIFICATION_TYPE_STREAM_END);
	BT_ASSERT(notif_stream);
	ret = ensure_stream_state_exists(iterator, NULL, notif_stream,
		&stream_state);
	if (ret) {
		BT_LOGE_STR("Cannot ensure that stream state exists.");
		goto error;
	}

	ret = handle_packet_switch(iterator, NULL, NULL, stream_state);
	if (ret) {
		BT_LOGE_STR("Cannot handle packet switch.");
		goto error;
	}

	add_action_push_notif(iterator, notif);
	add_action_set_stream_state_is_ended(iterator, stream_state);
	goto end;

error:
	ret = -1;

end:
	return ret;
}

static
int handle_notif_discarded_elements(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_notification *notif,
		struct bt_stream *notif_stream)
{
	int ret = 0;
	struct stream_state *stream_state;

	BT_ASSERT(notif->type == BT_NOTIFICATION_TYPE_DISCARDED_EVENTS ||
			notif->type == BT_NOTIFICATION_TYPE_DISCARDED_PACKETS);
	BT_ASSERT(notif_stream);
	ret = ensure_stream_state_exists(iterator, NULL, notif_stream,
		&stream_state);
	if (ret) {
		BT_LOGE_STR("Cannot ensure that stream state exists.");
		goto error;
	}

	add_action_push_notif(iterator, notif);
	goto end;

error:
	ret = -1;

end:
	return ret;
}

static
int handle_notif_packet_begin(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_notification *notif,
		struct bt_stream *notif_stream,
		struct bt_packet *notif_packet)
{
	int ret = 0;
	struct stream_state *stream_state;

	BT_ASSERT(notif->type == BT_NOTIFICATION_TYPE_PACKET_BEGIN);
	BT_ASSERT(notif_packet);
	ret = ensure_stream_state_exists(iterator, NULL, notif_stream,
		&stream_state);
	if (ret) {
		BT_LOGE_STR("Cannot ensure that stream state exists.");
		goto error;
	}

	ret = handle_packet_switch(iterator, notif, notif_packet, stream_state);
	if (ret) {
		BT_LOGE_STR("Cannot handle packet switch.");
		goto error;
	}

	goto end;

error:
	ret = -1;

end:
	return ret;
}

static
int handle_notif_packet_end(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_notification *notif,
		struct bt_stream *notif_stream,
		struct bt_packet *notif_packet)
{
	int ret = 0;
	struct stream_state *stream_state;

	BT_ASSERT(notif->type == BT_NOTIFICATION_TYPE_PACKET_END);
	BT_ASSERT(notif_packet);
	ret = ensure_stream_state_exists(iterator, NULL, notif_stream,
		&stream_state);
	if (ret) {
		BT_LOGE_STR("Cannot ensure that stream state exists.");
		goto error;
	}

	ret = handle_packet_switch(iterator, NULL, notif_packet, stream_state);
	if (ret) {
		BT_LOGE_STR("Cannot handle packet switch.");
		goto error;
	}

	/* End of the current packet */
	add_action_push_notif(iterator, notif);
	add_action_set_stream_state_cur_packet(iterator, stream_state, NULL);
	goto end;

error:
	ret = -1;

end:
	return ret;
}

static
int handle_notif_event(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_notification *notif,
		struct bt_stream *notif_stream,
		struct bt_packet *notif_packet)
{
	int ret = 0;
	struct stream_state *stream_state;

	BT_ASSERT(notif->type == BT_NOTIFICATION_TYPE_EVENT);
	BT_ASSERT(notif_packet);
	ret = ensure_stream_state_exists(iterator, NULL, notif_stream,
		&stream_state);
	if (ret) {
		BT_LOGE_STR("Cannot ensure that stream state exists.");
		goto error;
	}

	ret = handle_packet_switch(iterator, NULL, notif_packet, stream_state);
	if (ret) {
		BT_LOGE_STR("Cannot handle packet switch.");
		goto error;
	}

	add_action_push_notif(iterator, notif);
	goto end;

error:
	ret = -1;

end:
	return ret;
}

static
int enqueue_notification_and_automatic(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_notification *notif)
{
	int ret = 0;
	struct bt_event *notif_event = NULL;
	struct bt_stream *notif_stream = NULL;
	struct bt_packet *notif_packet = NULL;

	BT_ASSERT(notif);

	BT_LOGV("Enqueuing user notification and automatic notifications: "
		"iter-addr=%p, notif-addr=%p", iterator, notif);

	// TODO: Skip most of this if the iterator is only subscribed
	//       to event/inactivity notifications.

	/* Get the stream and packet referred by the notification */
	switch (notif->type) {
	case BT_NOTIFICATION_TYPE_EVENT:
		notif_event = bt_notification_event_borrow_event(notif);
		BT_ASSERT(notif_event);
		notif_packet = bt_event_borrow_packet(notif_event);
		BT_ASSERT(notif_packet);
		break;
	case BT_NOTIFICATION_TYPE_STREAM_BEGIN:
		notif_stream =
			bt_notification_stream_begin_borrow_stream(notif);
		BT_ASSERT(notif_stream);
		break;
	case BT_NOTIFICATION_TYPE_STREAM_END:
		notif_stream = bt_notification_stream_end_borrow_stream(notif);
		BT_ASSERT(notif_stream);
		break;
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
		notif_packet =
			bt_notification_packet_begin_borrow_packet(notif);
		BT_ASSERT(notif_packet);
		break;
	case BT_NOTIFICATION_TYPE_PACKET_END:
		notif_packet = bt_notification_packet_end_borrow_packet(notif);
		BT_ASSERT(notif_packet);
		break;
	case BT_NOTIFICATION_TYPE_INACTIVITY:
		/* Always valid */
		goto handle_notif;
	default:
		/*
		 * Invalid type of notification. Only the notification
		 * types above are allowed to be returned by a user
		 * component.
		 */
		BT_LOGF("Unexpected notification type at this point: "
			"notif-addr=%p, notif-type=%s", notif,
			bt_notification_type_string(notif->type));
		abort();
	}

	if (notif_packet) {
		notif_stream = bt_packet_borrow_stream(notif_packet);
		BT_ASSERT(notif_stream);
	}

	if (!notif_stream) {
		/*
		 * The notification has no reference to a stream: it
		 * cannot cause the creation of automatic notifications.
		 */
		BT_LOGV_STR("Notification has no reference to any stream: skipping automatic notification generation.");
		goto end;
	}

	if (!validate_notification(iterator, notif, notif_stream,
			notif_packet)) {
		BT_LOGW_STR("Invalid notification.");
		goto error;
	}

handle_notif:
	switch (notif->type) {
	case BT_NOTIFICATION_TYPE_EVENT:
		ret = handle_notif_event(iterator, notif, notif_stream,
			notif_packet);
		break;
	case BT_NOTIFICATION_TYPE_STREAM_BEGIN:
		ret = handle_notif_stream_begin(iterator, notif, notif_stream);
		break;
	case BT_NOTIFICATION_TYPE_STREAM_END:
		ret = handle_notif_stream_end(iterator, notif, notif_stream);
		break;
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
		ret = handle_notif_packet_begin(iterator, notif, notif_stream,
			notif_packet);
		break;
	case BT_NOTIFICATION_TYPE_PACKET_END:
		ret = handle_notif_packet_end(iterator, notif, notif_stream,
			notif_packet);
		break;
	case BT_NOTIFICATION_TYPE_DISCARDED_EVENTS:
	case BT_NOTIFICATION_TYPE_DISCARDED_PACKETS:
		ret = handle_notif_discarded_elements(iterator, notif,
			notif_stream);
		break;
	case BT_NOTIFICATION_TYPE_INACTIVITY:
		add_action_push_notif(iterator, notif);
		break;
	default:
		break;
	}

	if (ret) {
		BT_LOGW_STR("Failed to handle notification for automatic notification generation.");
		goto error;
	}

	apply_actions(iterator);
	BT_LOGV("Enqueued user notification and automatic notifications: "
		"iter-addr=%p, notif-addr=%p", iterator, notif);
	goto end;

error:
	ret = -1;

end:
	return ret;
}

static
int handle_end(struct bt_notification_iterator_private_connection *iterator)
{
	GHashTableIter stream_state_iter;
	gpointer stream_gptr, stream_state_gptr;
	int ret = 0;

	BT_LOGV("Handling end of iteration: addr=%p", iterator);

	/*
	 * Emit a "stream end" notification for each non-ended stream
	 * known by this iterator and mark them as ended.
	 */
	g_hash_table_iter_init(&stream_state_iter, iterator->stream_states);

	while (g_hash_table_iter_next(&stream_state_iter, &stream_gptr,
			&stream_state_gptr)) {
		struct stream_state *stream_state = stream_state_gptr;

		BT_ASSERT(stream_state_gptr);

		if (stream_state->is_ended) {
			continue;
		}

		ret = handle_packet_switch(iterator, NULL, NULL, stream_state);
		if (ret) {
			BT_LOGE_STR("Cannot handle packet switch.");
			goto error;
		}

		ret = add_action_push_notif_stream_end(iterator, stream_gptr);
		if (ret) {
			BT_LOGE_STR("Cannot add \"push stream end notification\" action.");
			goto error;
		}

		add_action_set_stream_state_is_ended(iterator, stream_state);
	}

	apply_actions(iterator);
	BT_LOGV("Handled end of iteration: addr=%p", iterator);
	goto end;

error:
	ret = -1;

end:
	return ret;
}

static
enum bt_notification_iterator_status ensure_queue_has_notifications(
		struct bt_notification_iterator_private_connection *iterator)
{
	struct bt_private_connection_private_notification_iterator *priv_iterator =
		bt_private_connection_private_notification_iterator_from_notification_iterator(iterator);
	bt_component_class_notification_iterator_next_method next_method = NULL;
	struct bt_notification_iterator_next_method_return next_return = {
		.status = BT_NOTIFICATION_ITERATOR_STATUS_OK,
		.notification = NULL,
	};
	enum bt_notification_iterator_status status =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;
	int ret;

	BT_ASSERT(iterator);
	BT_LOGD("Ensuring that notification iterator's queue has at least one notification: "
		"iter-addr=%p, queue-size=%u, iter-state=%s",
		iterator, iterator->queue->length,
		bt_private_connection_notification_iterator_state_string(iterator->state));

	if (iterator->queue->length > 0) {
		/*
		 * We already have enough. Even if this notification
		 * iterator is finalized, its user can still flush its
		 * current queue's content by calling its "next" method
		 * since this content is local and has no impact on what
		 * used to be the iterator's upstream component.
		 */
		BT_LOGD_STR("Queue already has at least one notification.");
		goto end;
	}

	switch (iterator->state) {
	case BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_FINALIZED_AND_ENDED:
	case BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_FINALIZED:
		BT_LOGD_STR("Notification iterator's \"next\" called, but it is finalized.");
		status = BT_NOTIFICATION_ITERATOR_STATUS_CANCELED;
		goto end;
	case BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_ENDED:
		BT_LOGD_STR("Notification iterator is ended.");
		status = BT_NOTIFICATION_ITERATOR_STATUS_END;
		goto end;
	default:
		break;
	}

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

	while (iterator->queue->length == 0) {
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
			 * The user's "next" method, somehow, cancelled
			 * its own notification iterator. This can
			 * happen, for example, when the user's method
			 * removes the port on which there's the
			 * connection from which the iterator was
			 * created. In this case, said connection is
			 * ended, and all its notification iterators are
			 * finalized.
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
			ret = handle_end(iterator);
			if (ret) {
				BT_LOGW_STR("Cannot handle end of iteration.");
				status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
				goto end;
			}

			BT_ASSERT(iterator->state ==
				BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_ACTIVE);
			iterator->state = BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_ENDED;

			if (iterator->queue->length == 0) {
				status = BT_NOTIFICATION_ITERATOR_STATUS_END;
			}

			BT_LOGD("Set new status: status=%s",
				bt_notification_iterator_status_string(status));
			goto end;
		case BT_NOTIFICATION_ITERATOR_STATUS_AGAIN:
			status = BT_NOTIFICATION_ITERATOR_STATUS_AGAIN;
			goto end;
		case BT_NOTIFICATION_ITERATOR_STATUS_OK:
			if (!next_return.notification) {
				BT_LOGW_STR("User method returned BT_NOTIFICATION_ITERATOR_STATUS_OK, but notification is NULL.");
				status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
				goto end;
			}

			/*
			 * Ignore some notifications which are always
			 * automatically generated by the notification
			 * iterator to make sure they have valid values.
			 */
			switch (next_return.notification->type) {
			case BT_NOTIFICATION_TYPE_DISCARDED_PACKETS:
			case BT_NOTIFICATION_TYPE_DISCARDED_EVENTS:
				BT_LOGV("Ignoring discarded elements notification returned by notification iterator's \"next\" method: "
					"notif-type=%s",
					bt_notification_type_string(next_return.notification->type));
				BT_PUT(next_return.notification);
				continue;
			default:
				break;
			}

			/*
			 * We know the notification is valid. Before we
			 * push it to the head of the queue, push the
			 * appropriate automatic notifications if any.
			 */
			ret = enqueue_notification_and_automatic(iterator,
				next_return.notification);
			BT_PUT(next_return.notification);
			if (ret) {
				BT_LOGW("Cannot enqueue notification and automatic notifications.");
				status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
				goto end;
			}
			break;
		default:
			/* Unknown non-error status */
			abort();
		}
	}

end:
	return status;
}

enum bt_notification_iterator_status
bt_notification_iterator_next(struct bt_notification_iterator *iterator)
{
	enum bt_notification_iterator_status status;

	if (!iterator) {
		BT_LOGW_STR("Invalid parameter: notification iterator is NULL.");
		status = BT_NOTIFICATION_ITERATOR_STATUS_INVALID;
		goto end;
	}

	BT_LOGD("Notification iterator's \"next\": iter-addr=%p", iterator);

	switch (iterator->type) {
	case BT_NOTIFICATION_ITERATOR_TYPE_PRIVATE_CONNECTION:
	{
		struct bt_notification_iterator_private_connection *priv_conn_iter =
			(void *) iterator;
		struct bt_notification *notif;

		/*
		 * Make sure that the iterator's queue contains at least
		 * one notification.
		 */
		status = ensure_queue_has_notifications(priv_conn_iter);
		if (status != BT_NOTIFICATION_ITERATOR_STATUS_OK) {
			goto end;
		}

		/*
		 * Move the notification at the tail of the queue to the
		 * iterator's current notification.
		 */
		BT_ASSERT(priv_conn_iter->queue->length > 0);
		notif = g_queue_pop_tail(priv_conn_iter->queue);
		bt_notification_iterator_replace_current_notification(
			iterator, notif);
		bt_put(notif);
		break;
	}
	case BT_NOTIFICATION_ITERATOR_TYPE_OUTPUT_PORT:
	{
		struct bt_notification_iterator_output_port *out_port_iter =
			(void *) iterator;

		/*
		 * Keep current notification in case there's an error:
		 * restore this notification so that the current
		 * notification is not changed from the user's point of
		 * view.
		 */
		struct bt_notification *old_notif =
			bt_get(bt_notification_iterator_borrow_current_notification(iterator));
		enum bt_graph_status graph_status;

		/*
		 * Put current notification since it's possibly
		 * about to be replaced by a new one by the
		 * colander sink.
		 */
		bt_notification_iterator_replace_current_notification(
			iterator, NULL);
		graph_status = bt_graph_consume_sink_no_check(
			out_port_iter->graph,
			out_port_iter->colander);
		switch (graph_status) {
		case BT_GRAPH_STATUS_CANCELED:
			status = BT_NOTIFICATION_ITERATOR_STATUS_CANCELED;
			break;
		case BT_GRAPH_STATUS_AGAIN:
			status = BT_NOTIFICATION_ITERATOR_STATUS_AGAIN;
			break;
		case BT_GRAPH_STATUS_END:
			status = BT_NOTIFICATION_ITERATOR_STATUS_END;
			break;
		case BT_GRAPH_STATUS_NOMEM:
			status = BT_NOTIFICATION_ITERATOR_STATUS_NOMEM;
			break;
		case BT_GRAPH_STATUS_OK:
			status = BT_NOTIFICATION_ITERATOR_STATUS_OK;
			BT_ASSERT(bt_notification_iterator_borrow_current_notification(iterator));
			break;
		default:
			/* Other errors */
			status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
		}

		if (status != BT_NOTIFICATION_ITERATOR_STATUS_OK) {
			/* Error/exception: restore old notification */
			bt_notification_iterator_replace_current_notification(
				iterator, old_notif);
		}

		bt_put(old_notif);
		break;
	}
	default:
		BT_LOGF("Unknown notification iterator type: addr=%p, type=%d",
			iterator, iterator->type);
		abort();
	}

end:
	return status;
}

struct bt_component *bt_private_connection_notification_iterator_get_component(
		struct bt_notification_iterator *iterator)
{
	struct bt_component *comp = NULL;
	struct bt_notification_iterator_private_connection *iter_priv_conn;

	if (!iterator) {
		BT_LOGW_STR("Invalid parameter: notification iterator is NULL.");
		goto end;
	}

	if (iterator->type != BT_NOTIFICATION_ITERATOR_TYPE_PRIVATE_CONNECTION) {
		BT_LOGW_STR("Invalid parameter: notification iterator was not created from a private connection.");
		goto end;
	}

	iter_priv_conn = (void *) iterator;
	comp = bt_get(iter_priv_conn->upstream_component);

end:
	return comp;
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
	BT_LOGD_STR("Putting graph.");
	bt_put(iterator->graph);
	BT_LOGD_STR("Putting output port.");
	bt_put(iterator->output_port);
	BT_LOGD_STR("Putting colander sink component.");
	bt_put(iterator->colander);
	destroy_base_notification_iterator(obj);
}

struct bt_notification_iterator *bt_output_port_notification_iterator_create(
		struct bt_port *output_port,
		const char *colander_component_name,
		const enum bt_notification_type *notification_types)
{
	struct bt_notification_iterator *iterator_base = NULL;
	struct bt_notification_iterator_output_port *iterator = NULL;
	struct bt_component_class *colander_comp_cls = NULL;
	struct bt_component *output_port_comp = NULL;
	struct bt_component *colander_comp;
	struct bt_graph *graph = NULL;
	enum bt_graph_status graph_status;
	const char *colander_comp_name;
	struct bt_port *colander_in_port = NULL;
	struct bt_component_class_sink_colander_data colander_data;

	if (!output_port) {
		BT_LOGW_STR("Invalid parameter: port is NULL.");
		goto error;
	}

	if (bt_port_get_type(output_port) != BT_PORT_TYPE_OUTPUT) {
		BT_LOGW_STR("Invalid parameter: port is not an output port.");
		goto error;
	}

	output_port_comp = bt_port_get_component(output_port);
	if (!output_port_comp) {
		BT_LOGW("Cannot get output port's component: "
			"port-addr=%p, port-name=\"%s\"",
			output_port, bt_port_get_name(output_port));
		goto error;
	}

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
	iterator_base = (void *) iterator;
	colander_comp_name =
		colander_component_name ? colander_component_name : "colander";
	colander_data.notification = &iterator_base->current_notification;
	colander_data.notification_types = notification_types;
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
	 * sink and moved to the base notification iterator's current
	 * notification member.
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
bt_private_connection_notification_iterator_from_private(
		struct bt_private_connection_private_notification_iterator *private_notification_iterator)
{
	return bt_get(
		bt_private_connection_notification_iterator_borrow_from_private(
			private_notification_iterator));
}
