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

#include <babeltrace/compiler-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ctf-ir/packet-internal.h>
#include <babeltrace/ctf-ir/stream-internal.h>
#include <babeltrace/graph/connection-internal.h>
#include <babeltrace/graph/component.h>
#include <babeltrace/graph/component-source-internal.h>
#include <babeltrace/graph/component-class-internal.h>
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
#include <babeltrace/graph/port.h>
#include <babeltrace/types.h>
#include <stdint.h>

struct stream_state {
	struct bt_ctf_stream *stream; /* owned by this */
	struct bt_ctf_packet *cur_packet; /* owned by this */
	bt_bool is_ended;
};

enum action_type {
	ACTION_TYPE_PUSH_NOTIF,
	ACTION_TYPE_MAP_PORT_TO_COMP_IN_STREAM,
	ACTION_TYPE_ADD_STREAM_STATE,
	ACTION_TYPE_SET_STREAM_STATE_IS_ENDED,
	ACTION_TYPE_SET_STREAM_STATE_CUR_PACKET,
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
			struct bt_ctf_stream *stream; /* owned by this */
			struct bt_component *component; /* owned by this */
			struct bt_port *port; /* owned by this */
		} map_port_to_comp_in_stream;

		/* ACTION_TYPE_ADD_STREAM_STATE */
		struct {
			struct bt_ctf_stream *stream; /* owned by this */
			struct stream_state *stream_state; /* owned by this */
		} add_stream_state;

		/* ACTION_TYPE_SET_STREAM_STATE_IS_ENDED */
		struct {
			struct stream_state *stream_state; /* weak */
		} set_stream_state_is_ended;

		/* ACTION_TYPE_SET_STREAM_STATE_CUR_PACKET */
		struct {
			struct stream_state *stream_state; /* weak */
			struct bt_ctf_packet *packet; /* owned by this */
		} set_stream_state_cur_packet;
	} payload;
};

static
void stream_destroy_listener(struct bt_ctf_stream *stream, void *data)
{
	struct bt_notification_iterator *iterator = data;

	/* Remove associated stream state */
	g_hash_table_remove(iterator->stream_states, stream);
}

static
void destroy_stream_state(struct stream_state *stream_state)
{
	if (!stream_state) {
		return;
	}

	bt_put(stream_state->cur_packet);
	bt_put(stream_state->stream);
	g_free(stream_state);
}

static
void destroy_action(struct action *action)
{
	assert(action);

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
	default:
		assert(BT_FALSE);
	}
}

static
void add_action(struct bt_notification_iterator *iterator,
		struct action *action)
{
	g_array_append_val(iterator->actions, *action);
}

static
void clear_actions(struct bt_notification_iterator *iterator)
{
	size_t i;

	for (i = 0; i < iterator->actions->len; i++) {
		struct action *action = &g_array_index(iterator->actions,
			struct action, i);

		destroy_action(action);
	}

	g_array_set_size(iterator->actions, 0);
}

static
void apply_actions(struct bt_notification_iterator *iterator)
{
	size_t i;

	for (i = 0; i < iterator->actions->len; i++) {
		struct action *action = &g_array_index(iterator->actions,
			struct action, i);

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
			bt_ctf_stream_map_component_to_port(
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
			bt_ctf_stream_add_destroy_listener(
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
		default:
			assert(BT_FALSE);
		}
	}

	clear_actions(iterator);
}

static
struct stream_state *create_stream_state(struct bt_ctf_stream *stream)
{
	struct stream_state *stream_state = g_new0(struct stream_state, 1);

	if (!stream_state) {
		goto end;
	}

	/*
	 * We keep a reference to the stream until we know it's ended
	 * because we need to be able to create an automatic "stream
	 * end" notification when the user's "next" method returns
	 * BT_NOTIFICATION_ITERATOR_STATUS_END.
	 *
	 * We put this reference when the stream is marked as ended.
	 */
	stream_state->stream = bt_get(stream);

end:
	return stream_state;
}

static
void bt_notification_iterator_destroy(struct bt_object *obj)
{
	struct bt_notification_iterator *iterator;

	assert(obj);

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
	iterator = container_of(obj, struct bt_notification_iterator, base);
	bt_notification_iterator_finalize(iterator);

	if (iterator->queue) {
		struct bt_notification *notif;

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
			assert(stream_gptr);
			bt_ctf_stream_remove_destroy_listener(
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

	bt_put(iterator->current_notification);
	g_free(iterator);
}

BT_HIDDEN
void bt_notification_iterator_finalize(
		struct bt_notification_iterator *iterator)
{
	struct bt_component_class *comp_class = NULL;
	bt_component_class_notification_iterator_finalize_method
		finalize_method = NULL;

	assert(iterator);

	switch (iterator->state) {
	case BT_NOTIFICATION_ITERATOR_STATE_FINALIZED:
	case BT_NOTIFICATION_ITERATOR_STATE_FINALIZED_AND_ENDED:
		/* Already finalized */
		return;
	default:
		break;
	}

	if (iterator->state == BT_NOTIFICATION_ITERATOR_STATE_ENDED) {
		iterator->state = BT_NOTIFICATION_ITERATOR_STATE_FINALIZED_AND_ENDED;
	} else {
		iterator->state = BT_NOTIFICATION_ITERATOR_STATE_FINALIZED;
	}

	assert(iterator->upstream_component);
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
		assert(0);
	}

	if (finalize_method) {
		finalize_method(
			bt_private_notification_iterator_from_notification_iterator(iterator));
	}

	iterator->upstream_component = NULL;
	iterator->upstream_port = NULL;
}

BT_HIDDEN
void bt_notification_iterator_set_connection(
		struct bt_notification_iterator *iterator,
		struct bt_connection *connection)
{
	assert(iterator);
	iterator->connection = connection;
}

static
int create_subscription_mask_from_notification_types(
		struct bt_notification_iterator *iterator,
		const enum bt_notification_type *notif_types)
{
	const enum bt_notification_type *notif_type;
	int ret = 0;

	assert(notif_types);
	iterator->subscription_mask = 0;

	for (notif_type = notif_types;
			*notif_type != BT_NOTIFICATION_TYPE_SENTINEL;
			notif_type++) {
		switch (*notif_type) {
		case BT_NOTIFICATION_TYPE_ALL:
			iterator->subscription_mask |=
				BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_EVENT |
				BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_INACTIVITY |
				BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_STREAM_BEGIN |
				BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_STREAM_END |
				BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_PACKET_BEGIN |
				BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_PACKET_END;
			break;
		case BT_NOTIFICATION_TYPE_EVENT:
			iterator->subscription_mask |= BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_EVENT;
			break;
		case BT_NOTIFICATION_TYPE_INACTIVITY:
			iterator->subscription_mask |= BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_INACTIVITY;
			break;
		case BT_NOTIFICATION_TYPE_STREAM_BEGIN:
			iterator->subscription_mask |= BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_STREAM_BEGIN;
			break;
		case BT_NOTIFICATION_TYPE_STREAM_END:
			iterator->subscription_mask |= BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_STREAM_END;
			break;
		case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
			iterator->subscription_mask |= BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_PACKET_BEGIN;
			break;
		case BT_NOTIFICATION_TYPE_PACKET_END:
			iterator->subscription_mask |= BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_PACKET_END;
			break;
		default:
			ret = -1;
			goto end;
		}
	}

end:
	return ret;
}

BT_HIDDEN
struct bt_notification_iterator *bt_notification_iterator_create(
		struct bt_component *upstream_comp,
		struct bt_port *upstream_port,
		const enum bt_notification_type *notification_types,
		struct bt_connection *connection)
{
	enum bt_component_class_type type;
	struct bt_notification_iterator *iterator = NULL;

	assert(upstream_comp);
	assert(upstream_port);
	assert(notification_types);
	assert(bt_port_is_connected(upstream_port));

	type = bt_component_get_class_type(upstream_comp);
	switch (type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	case BT_COMPONENT_CLASS_TYPE_FILTER:
		break;
	default:
		goto error;
	}

	iterator = g_new0(struct bt_notification_iterator, 1);
	if (!iterator) {
		goto error;
	}

	bt_object_init(iterator, bt_notification_iterator_destroy);

	if (create_subscription_mask_from_notification_types(iterator,
			notification_types)) {
		goto error;
	}

	iterator->stream_states = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, (GDestroyNotify) destroy_stream_state);
	if (!iterator->stream_states) {
		goto error;
	}

	iterator->queue = g_queue_new();
	if (!iterator->queue) {
		goto error;
	}

	iterator->actions = g_array_new(FALSE, FALSE, sizeof(struct action));
	if (!iterator->actions) {
		goto error;
	}

	iterator->upstream_component = upstream_comp;
	iterator->upstream_port = upstream_port;
	iterator->connection = connection;
	iterator->state = BT_NOTIFICATION_ITERATOR_STATE_ACTIVE;
	goto end;

error:
	BT_PUT(iterator);

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
		ret = BT_NOTIFICATION_ITERATOR_STATUS_INVALID;
		goto end;
	}
end:
	return ret;
}

void *bt_private_notification_iterator_get_user_data(
		struct bt_private_notification_iterator *private_iterator)
{
	struct bt_notification_iterator *iterator =
		bt_notification_iterator_from_private(private_iterator);

	return iterator ? iterator->user_data : NULL;
}

enum bt_notification_iterator_status
bt_private_notification_iterator_set_user_data(
		struct bt_private_notification_iterator *private_iterator,
		void *data)
{
	enum bt_notification_iterator_status ret =
			BT_NOTIFICATION_ITERATOR_STATUS_OK;
	struct bt_notification_iterator *iterator =
		bt_notification_iterator_from_private(private_iterator);

	if (!iterator) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_INVALID;
		goto end;
	}

	iterator->user_data = data;
end:
	return ret;
}

struct bt_notification *bt_notification_iterator_get_notification(
		struct bt_notification_iterator *iterator)
{
	struct bt_notification *notification = NULL;

	if (!iterator) {
		goto end;
	}

	notification = bt_get(iterator->current_notification);

end:
	return notification;
}

static
enum bt_notification_iterator_notif_type
bt_notification_iterator_notif_type_from_notif_type(
		enum bt_notification_type notif_type)
{
	enum bt_notification_iterator_notif_type iter_notif_type;

	switch (notif_type) {
	case BT_NOTIFICATION_TYPE_EVENT:
		iter_notif_type = BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_EVENT;
		break;
	case BT_NOTIFICATION_TYPE_INACTIVITY:
		iter_notif_type = BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_INACTIVITY;
		break;
	case BT_NOTIFICATION_TYPE_STREAM_BEGIN:
		iter_notif_type = BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_STREAM_BEGIN;
		break;
	case BT_NOTIFICATION_TYPE_STREAM_END:
		iter_notif_type = BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_STREAM_END;
		break;
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
		iter_notif_type = BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_PACKET_BEGIN;
		break;
	case BT_NOTIFICATION_TYPE_PACKET_END:
		iter_notif_type = BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_PACKET_END;
		break;
	default:
		assert(BT_FALSE);
	}

	return iter_notif_type;
}

static
bt_bool validate_notification(struct bt_notification_iterator *iterator,
		struct bt_notification *notif,
		struct bt_ctf_stream *notif_stream,
		struct bt_ctf_packet *notif_packet)
{
	bt_bool is_valid = BT_TRUE;
	struct stream_state *stream_state;
	struct bt_port *stream_comp_cur_port;

	assert(notif_stream);
	stream_comp_cur_port =
		bt_ctf_stream_port_for_component(notif_stream,
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
			is_valid = BT_FALSE;
			goto end;
		}

	}

	stream_state = g_hash_table_lookup(iterator->stream_states,
		notif_stream);
	if (stream_state) {
		if (stream_state->is_ended) {
			/*
			 * There's a new notification which has a
			 * reference to a stream which, from this
			 * iterator's point of view, is ended ("end of
			 * stream" notification was returned). This is
			 * bad: the API guarantees that it can never
			 * happen.
			 */
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
			is_valid = BT_FALSE;
			goto end;
		case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
			if (notif_packet == stream_state->cur_packet) {
				/* Duplicate "packet begin" notification */
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
bt_bool is_subscribed_to_notification_type(struct bt_notification_iterator *iterator,
		enum bt_notification_type notif_type)
{
	uint32_t iter_notif_type =
		(uint32_t) bt_notification_iterator_notif_type_from_notif_type(
			notif_type);

	return (iter_notif_type & iterator->subscription_mask) ? BT_TRUE : BT_FALSE;
}

static
void add_action_push_notif(struct bt_notification_iterator *iterator,
		struct bt_notification *notif)
{
	struct action action = {
		.type = ACTION_TYPE_PUSH_NOTIF,
	};

	assert(notif);

	if (!is_subscribed_to_notification_type(iterator, notif->type)) {
		return;
	}

	action.payload.push_notif.notif = bt_get(notif);
	add_action(iterator, &action);
}

static
int add_action_push_notif_stream_begin(
		struct bt_notification_iterator *iterator,
		struct bt_ctf_stream *stream)
{
	int ret = 0;
	struct bt_notification *stream_begin_notif = NULL;

	if (!is_subscribed_to_notification_type(iterator,
			BT_NOTIFICATION_TYPE_STREAM_BEGIN)) {
		goto end;
	}

	assert(stream);
	stream_begin_notif = bt_notification_stream_begin_create(stream);
	if (!stream_begin_notif) {
		goto error;
	}

	add_action_push_notif(iterator, stream_begin_notif);
	goto end;

error:
	ret = -1;

end:
	bt_put(stream_begin_notif);
	return ret;
}

static
int add_action_push_notif_stream_end(
		struct bt_notification_iterator *iterator,
		struct bt_ctf_stream *stream)
{
	int ret = 0;
	struct bt_notification *stream_end_notif = NULL;

	if (!is_subscribed_to_notification_type(iterator,
			BT_NOTIFICATION_TYPE_STREAM_END)) {
		goto end;
	}

	assert(stream);
	stream_end_notif = bt_notification_stream_end_create(stream);
	if (!stream_end_notif) {
		goto error;
	}

	add_action_push_notif(iterator, stream_end_notif);
	goto end;

error:
	ret = -1;

end:
	bt_put(stream_end_notif);
	return ret;
}

static
int add_action_push_notif_packet_begin(
		struct bt_notification_iterator *iterator,
		struct bt_ctf_packet *packet)
{
	int ret = 0;
	struct bt_notification *packet_begin_notif = NULL;

	if (!is_subscribed_to_notification_type(iterator,
			BT_NOTIFICATION_TYPE_PACKET_BEGIN)) {
		goto end;
	}

	assert(packet);
	packet_begin_notif = bt_notification_packet_begin_create(packet);
	if (!packet_begin_notif) {
		goto error;
	}

	add_action_push_notif(iterator, packet_begin_notif);
	goto end;

error:
	ret = -1;

end:
	bt_put(packet_begin_notif);
	return ret;
}

static
int add_action_push_notif_packet_end(
		struct bt_notification_iterator *iterator,
		struct bt_ctf_packet *packet)
{
	int ret = 0;
	struct bt_notification *packet_end_notif = NULL;

	if (!is_subscribed_to_notification_type(iterator,
			BT_NOTIFICATION_TYPE_PACKET_END)) {
		goto end;
	}

	assert(packet);
	packet_end_notif = bt_notification_packet_end_create(packet);
	if (!packet_end_notif) {
		goto error;
	}

	add_action_push_notif(iterator, packet_end_notif);
	goto end;

error:
	ret = -1;

end:
	bt_put(packet_end_notif);
	return ret;
}

static
void add_action_set_stream_state_is_ended(
		struct bt_notification_iterator *iterator,
		struct stream_state *stream_state)
{
	struct action action = {
		.type = ACTION_TYPE_SET_STREAM_STATE_IS_ENDED,
		.payload.set_stream_state_is_ended = {
			.stream_state = stream_state,
		},
	};

	assert(stream_state);
	add_action(iterator, &action);
}

static
void add_action_set_stream_state_cur_packet(
		struct bt_notification_iterator *iterator,
		struct stream_state *stream_state,
		struct bt_ctf_packet *packet)
{
	struct action action = {
		.type = ACTION_TYPE_SET_STREAM_STATE_CUR_PACKET,
		.payload.set_stream_state_cur_packet = {
			.stream_state = stream_state,
			.packet = bt_get(packet),
		},
	};

	assert(stream_state);
	add_action(iterator, &action);
}

static
int ensure_stream_state_exists(struct bt_notification_iterator *iterator,
		struct bt_notification *stream_begin_notif,
		struct bt_ctf_stream *notif_stream,
		struct stream_state **stream_state)
{
	int ret = 0;

	if (!notif_stream) {
		/*
		 * The notification does not reference any stream: no
		 * need to get or create a stream state.
		 */
		goto end;
	}

	*stream_state = g_hash_table_lookup(iterator->stream_states,
		notif_stream);
	if (!*stream_state) {
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

		*stream_state = create_stream_state(notif_stream);
		if (!stream_state) {
			goto error;
		}

		action.payload.add_stream_state.stream_state =
			*stream_state;
		add_action(iterator, &action);

		if (stream_begin_notif) {
			add_action_push_notif(iterator, stream_begin_notif);
		} else {
			ret = add_action_push_notif_stream_begin(iterator,
				notif_stream);
			if (ret) {
				goto error;
			}
		}
	}

	goto end;

error:
	destroy_stream_state(*stream_state);
	ret = -1;

end:
	return ret;
}

static
int handle_packet_switch(struct bt_notification_iterator *iterator,
		struct bt_notification *packet_begin_notif,
		struct bt_ctf_packet *new_packet,
		struct stream_state *stream_state)
{
	int ret = 0;

	if (stream_state->cur_packet == new_packet) {
		goto end;
	}

	if (stream_state->cur_packet) {
		/* End of the current packet */
		ret = add_action_push_notif_packet_end(iterator,
			stream_state->cur_packet);
		if (ret) {
			goto error;
		}
	}

	/* Beginning of the new packet */
	if (packet_begin_notif) {
		add_action_push_notif(iterator, packet_begin_notif);
	} else if (new_packet) {
		ret = add_action_push_notif_packet_begin(iterator,
			new_packet);
		if (ret) {
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
		struct bt_notification_iterator *iterator,
		struct bt_notification *notif,
		struct bt_ctf_stream *notif_stream)
{
	int ret = 0;
	struct stream_state *stream_state;

	assert(notif->type == BT_NOTIFICATION_TYPE_STREAM_BEGIN);
	assert(notif_stream);
	ret = ensure_stream_state_exists(iterator, notif, notif_stream,
		&stream_state);
	if (ret) {
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
		struct bt_notification_iterator *iterator,
		struct bt_notification *notif,
		struct bt_ctf_stream *notif_stream)
{
	int ret = 0;
	struct stream_state *stream_state;

	assert(notif->type == BT_NOTIFICATION_TYPE_STREAM_END);
	assert(notif_stream);
	ret = ensure_stream_state_exists(iterator, NULL, notif_stream,
		&stream_state);
	if (ret) {
		goto error;
	}

	ret = handle_packet_switch(iterator, NULL, NULL, stream_state);
	if (ret) {
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
int handle_notif_packet_begin(
		struct bt_notification_iterator *iterator,
		struct bt_notification *notif,
		struct bt_ctf_stream *notif_stream,
		struct bt_ctf_packet *notif_packet)
{
	int ret = 0;
	struct stream_state *stream_state;

	assert(notif->type == BT_NOTIFICATION_TYPE_PACKET_BEGIN);
	assert(notif_packet);
	ret = ensure_stream_state_exists(iterator, NULL, notif_stream,
		&stream_state);
	if (ret) {
		goto error;
	}

	ret = handle_packet_switch(iterator, notif, notif_packet, stream_state);
	if (ret) {
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
		struct bt_notification_iterator *iterator,
		struct bt_notification *notif,
		struct bt_ctf_stream *notif_stream,
		struct bt_ctf_packet *notif_packet)
{
	int ret = 0;
	struct stream_state *stream_state;

	assert(notif->type == BT_NOTIFICATION_TYPE_PACKET_END);
	assert(notif_packet);
	ret = ensure_stream_state_exists(iterator, NULL, notif_stream,
		&stream_state);
	if (ret) {
		goto error;
	}

	ret = handle_packet_switch(iterator, NULL, notif_packet, stream_state);
	if (ret) {
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
		struct bt_notification_iterator *iterator,
		struct bt_notification *notif,
		struct bt_ctf_stream *notif_stream,
		struct bt_ctf_packet *notif_packet)
{
	int ret = 0;
	struct stream_state *stream_state;

	assert(notif->type == BT_NOTIFICATION_TYPE_EVENT);
	assert(notif_packet);
	ret = ensure_stream_state_exists(iterator, NULL, notif_stream,
		&stream_state);
	if (ret) {
		goto error;
	}

	ret = handle_packet_switch(iterator, NULL, notif_packet, stream_state);
	if (ret) {
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
		struct bt_notification_iterator *iterator,
		struct bt_notification *notif)
{
	int ret = 0;
	struct bt_ctf_event *notif_event = NULL;
	struct bt_ctf_stream *notif_stream = NULL;
	struct bt_ctf_packet *notif_packet = NULL;

	assert(notif);

	// TODO: Skip most of this if the iterator is only subscribed
	//       to event/inactivity notifications.

	/* Get the stream and packet referred by the notification */
	switch (notif->type) {
	case BT_NOTIFICATION_TYPE_EVENT:
		notif_event = bt_notification_event_borrow_event(notif);
		assert(notif_event);
		notif_packet = bt_ctf_event_borrow_packet(notif_event);
		assert(notif_packet);
		break;
	case BT_NOTIFICATION_TYPE_STREAM_BEGIN:
		notif_stream =
			bt_notification_stream_begin_borrow_stream(notif);
		assert(notif_stream);
		break;
	case BT_NOTIFICATION_TYPE_STREAM_END:
		notif_stream = bt_notification_stream_end_borrow_stream(notif);
		assert(notif_stream);
		break;
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
		notif_packet =
			bt_notification_packet_begin_borrow_packet(notif);
		assert(notif_packet);
		break;
	case BT_NOTIFICATION_TYPE_PACKET_END:
		notif_packet = bt_notification_packet_end_borrow_packet(notif);
		assert(notif_packet);
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
		goto error;
	}

	if (notif_packet) {
		notif_stream = bt_ctf_packet_borrow_stream(notif_packet);
		assert(notif_stream);
	}

	if (!notif_stream) {
		/*
		 * The notification has no reference to a stream: it
		 * cannot cause the creation of automatic notifications.
		 */
		goto end;
	}

	if (!validate_notification(iterator, notif, notif_stream,
			notif_packet)) {
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
	case BT_NOTIFICATION_TYPE_INACTIVITY:
		add_action_push_notif(iterator, notif);
		break;
	default:
		break;
	}

	if (ret) {
		goto error;
	}

	apply_actions(iterator);
	goto end;

error:
	ret = -1;

end:
	return ret;
}

static
int handle_end(struct bt_notification_iterator *iterator)
{
	GHashTableIter stream_state_iter;
	gpointer stream_gptr, stream_state_gptr;
	int ret = 0;

	/*
	 * Emit a "stream end" notification for each non-ended stream
	 * known by this iterator and mark them as ended.
	 */
	g_hash_table_iter_init(&stream_state_iter, iterator->stream_states);

	while (g_hash_table_iter_next(&stream_state_iter, &stream_gptr,
			&stream_state_gptr)) {
		struct stream_state *stream_state = stream_state_gptr;

		assert(stream_state_gptr);

		if (stream_state->is_ended) {
			continue;
		}

		ret = handle_packet_switch(iterator, NULL, NULL, stream_state);
		if (ret) {
			goto error;
		}

		ret = add_action_push_notif_stream_end(iterator, stream_gptr);
		if (ret) {
			goto error;
		}

		add_action_set_stream_state_is_ended(iterator, stream_state);
	}

	apply_actions(iterator);
	goto end;

error:
	ret = -1;

end:
	return ret;
}

static
enum bt_notification_iterator_status ensure_queue_has_notifications(
		struct bt_notification_iterator *iterator)
{
	struct bt_private_notification_iterator *priv_iterator =
		bt_private_notification_iterator_from_notification_iterator(iterator);
	bt_component_class_notification_iterator_next_method next_method = NULL;
	struct bt_notification_iterator_next_return next_return = {
		.status = BT_NOTIFICATION_ITERATOR_STATUS_OK,
		.notification = NULL,
	};
	enum bt_notification_iterator_status status =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;
	int ret;

	assert(iterator);

	if (iterator->queue->length > 0) {
		/* We already have enough */
		goto end;
	}

	switch (iterator->state) {
	case BT_NOTIFICATION_ITERATOR_STATE_FINALIZED_AND_ENDED:
		status = BT_NOTIFICATION_ITERATOR_STATUS_CANCELED;
		goto end;
	case BT_NOTIFICATION_ITERATOR_STATE_ENDED:
		status = BT_NOTIFICATION_ITERATOR_STATUS_END;
		goto end;
	default:
		break;
	}

	assert(iterator->upstream_component);
	assert(iterator->upstream_component->class);

	/* Pick the appropriate "next" method */
	switch (iterator->upstream_component->class->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		struct bt_component_class_source *source_class =
			container_of(iterator->upstream_component->class,
				struct bt_component_class_source, parent);

		assert(source_class->methods.iterator.next);
		next_method = source_class->methods.iterator.next;
		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		struct bt_component_class_filter *filter_class =
			container_of(iterator->upstream_component->class,
				struct bt_component_class_filter, parent);

		assert(filter_class->methods.iterator.next);
		next_method = filter_class->methods.iterator.next;
		break;
	}
	default:
		assert(BT_FALSE);
		break;
	}

	/*
	 * Call the user's "next" method to get the next notification
	 * and status.
	 */
	assert(next_method);

	while (iterator->queue->length == 0) {
		next_return = next_method(priv_iterator);
		if (next_return.status < 0) {
			status = next_return.status;
			goto end;
		}

		switch (next_return.status) {
		case BT_NOTIFICATION_ITERATOR_STATUS_END:
			ret = handle_end(iterator);
			if (ret) {
				status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
				goto end;
			}

			if (iterator->state == BT_NOTIFICATION_ITERATOR_STATE_FINALIZED) {
				iterator->state =
					BT_NOTIFICATION_ITERATOR_STATE_FINALIZED_AND_ENDED;

				if (iterator->queue->length == 0) {
					status = BT_NOTIFICATION_ITERATOR_STATUS_CANCELED;
				}
			} else {
				iterator->state =
					BT_NOTIFICATION_ITERATOR_STATE_ENDED;

				if (iterator->queue->length == 0) {
					status = BT_NOTIFICATION_ITERATOR_STATUS_END;
				}
			}
			goto end;
		case BT_NOTIFICATION_ITERATOR_STATUS_AGAIN:
			status = BT_NOTIFICATION_ITERATOR_STATUS_AGAIN;
			goto end;
		case BT_NOTIFICATION_ITERATOR_STATUS_OK:
			if (!next_return.notification) {
				status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
				goto end;
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
				status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
				goto end;
			}
			break;
		default:
			/* Unknown non-error status */
			assert(BT_FALSE);
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
		status = BT_NOTIFICATION_ITERATOR_STATUS_INVALID;
		goto end;
	}

	/*
	 * Make sure that the iterator's queue contains at least one
	 * notification.
	 */
	status = ensure_queue_has_notifications(iterator);
	if (status != BT_NOTIFICATION_ITERATOR_STATUS_OK) {
		goto end;
	}

	/*
	 * Move the notification at the tail of the queue to the
	 * iterator's current notification.
	 */
	assert(iterator->queue->length > 0);
	bt_put(iterator->current_notification);
	iterator->current_notification = g_queue_pop_tail(iterator->queue);
	assert(iterator->current_notification);

end:
	return status;
}

struct bt_component *bt_notification_iterator_get_component(
		struct bt_notification_iterator *iterator)
{
	return bt_get(iterator->upstream_component);
}

struct bt_private_component *
bt_private_notification_iterator_get_private_component(
		struct bt_private_notification_iterator *private_iterator)
{
	return bt_private_component_from_component(
		bt_notification_iterator_get_component(
			bt_notification_iterator_from_private(private_iterator)));
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
