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

#define BT_LOG_TAG "MSG-ITER"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/compiler-internal.h>
#include <babeltrace/trace-ir/field.h>
#include <babeltrace/trace-ir/event-const.h>
#include <babeltrace/trace-ir/event-internal.h>
#include <babeltrace/trace-ir/packet-const.h>
#include <babeltrace/trace-ir/packet-internal.h>
#include <babeltrace/trace-ir/stream-internal.h>
#include <babeltrace/graph/connection-const.h>
#include <babeltrace/graph/connection-internal.h>
#include <babeltrace/graph/component-const.h>
#include <babeltrace/graph/component-internal.h>
#include <babeltrace/graph/component-source-internal.h>
#include <babeltrace/graph/component-class-internal.h>
#include <babeltrace/graph/component-class-sink-colander-internal.h>
#include <babeltrace/graph/component-sink-const.h>
#include <babeltrace/graph/message-const.h>
#include <babeltrace/graph/message-iterator.h>
#include <babeltrace/graph/message-iterator-internal.h>
#include <babeltrace/graph/self-component-port-input-message-iterator.h>
#include <babeltrace/graph/port-output-message-iterator.h>
#include <babeltrace/graph/message-internal.h>
#include <babeltrace/graph/message-event-const.h>
#include <babeltrace/graph/message-event-internal.h>
#include <babeltrace/graph/message-packet-const.h>
#include <babeltrace/graph/message-packet-internal.h>
#include <babeltrace/graph/message-stream-const.h>
#include <babeltrace/graph/message-stream-internal.h>
#include <babeltrace/graph/port-const.h>
#include <babeltrace/graph/graph.h>
#include <babeltrace/graph/graph-const.h>
#include <babeltrace/graph/graph-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>

/*
 * TODO: Use graph's state (number of active iterators, etc.) and
 * possibly system specifications to make a better guess than this.
 */
#define MSG_BATCH_SIZE	15

struct stream_state {
	const struct bt_stream *stream; /* owned by this */
	const struct bt_packet *cur_packet; /* owned by this */
	uint64_t expected_msg_seq_num;
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
	BT_OBJECT_PUT_REF_AND_RESET(stream_state->cur_packet);
	BT_LOGV_STR("Putting stream state's stream.");
	BT_OBJECT_PUT_REF_AND_RESET(stream_state->stream);
	g_free(stream_state);
}

BT_ASSERT_PRE_FUNC
static
struct stream_state *create_stream_state(const struct bt_stream *stream)
{
	struct stream_state *stream_state = g_new0(struct stream_state, 1);

	if (!stream_state) {
		BT_LOGE_STR("Failed to allocate one stream state.");
		goto end;
	}

	/*
	 * We keep a reference to the stream until we know it's ended.
	 */
	stream_state->stream = stream;
	bt_object_get_no_null_check(stream_state->stream);
	BT_LIB_LOGV("Created stream state: %![stream-]+s, "
		"stream-state-addr=%p",
		stream, stream_state);

end:
	return stream_state;
}

static inline
void _set_self_comp_port_input_msg_iterator_state(
		struct bt_self_component_port_input_message_iterator *iterator,
		enum bt_self_component_port_input_message_iterator_state state)
{
	BT_ASSERT(iterator);
	BT_LIB_LOGD("Updating message iterator's state: "
		"new-state=%s",
		bt_self_component_port_input_message_iterator_state_string(state));
	iterator->state = state;
}

#ifdef BT_DEV_MODE
# define set_self_comp_port_input_msg_iterator_state _set_self_comp_port_input_msg_iterator_state
#else
# define set_self_comp_port_input_msg_iterator_state(_a, _b)
#endif

static
void destroy_base_message_iterator(struct bt_object *obj)
{
	struct bt_message_iterator *iterator = (void *) obj;

	BT_ASSERT(iterator);

	if (iterator->msgs) {
		g_ptr_array_free(iterator->msgs, TRUE);
		iterator->msgs = NULL;
	}

	g_free(iterator);
}

static
void bt_self_component_port_input_message_iterator_destroy(struct bt_object *obj)
{
	struct bt_self_component_port_input_message_iterator *iterator;

	BT_ASSERT(obj);

	/*
	 * The message iterator's reference count is 0 if we're
	 * here. Increment it to avoid a double-destroy (possibly
	 * infinitely recursive). This could happen for example if the
	 * message iterator's finalization function does
	 * bt_object_get_ref() (or anything that causes
	 * bt_object_get_ref() to be called) on itself (ref. count goes
	 * from 0 to 1), and then bt_object_put_ref(): the reference
	 * count would go from 1 to 0 again and this function would be
	 * called again.
	 */
	obj->ref_count++;
	iterator = (void *) obj;
	BT_LIB_LOGD("Destroying self component input port message iterator object: "
		"%!+i", iterator);
	bt_self_component_port_input_message_iterator_try_finalize(iterator);

	if (iterator->stream_states) {
		/*
		 * Remove our destroy listener from each stream which
		 * has a state in this iterator. Otherwise the destroy
		 * listener would be called with an invalid/other
		 * message iterator object.
		 */
		g_hash_table_destroy(iterator->stream_states);
		iterator->stream_states = NULL;
	}

	if (iterator->connection) {
		/*
		 * Remove ourself from the originating connection so
		 * that it does not try to finalize a dangling pointer
		 * later.
		 */
		bt_connection_remove_iterator(iterator->connection, iterator);
		iterator->connection = NULL;
	}

	destroy_base_message_iterator(obj);
}

BT_HIDDEN
void bt_self_component_port_input_message_iterator_try_finalize(
		struct bt_self_component_port_input_message_iterator *iterator)
{
	typedef void (*method_t)(void *);

	struct bt_component_class *comp_class = NULL;
	method_t method = NULL;

	BT_ASSERT(iterator);

	switch (iterator->state) {
	case BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_NON_INITIALIZED:
		/* Skip user finalization if user initialization failed */
		BT_LIB_LOGD("Not finalizing non-initialized message iterator: "
			"%!+i", iterator);
		goto end;
	case BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_FINALIZED:
		/* Already finalized */
		BT_LIB_LOGD("Not finalizing message iterator: already finalized: "
			"%!+i", iterator);
		goto end;
	case BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_FINALIZING:
		/* Already finalized */
		BT_LIB_LOGF("Message iterator is already being finalized: "
			"%!+i", iterator);
		abort();
	default:
		break;
	}

	BT_LIB_LOGD("Finalizing message iterator: %!+i", iterator);
	set_self_comp_port_input_msg_iterator_state(iterator,
		BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_FINALIZING);
	BT_ASSERT(iterator->upstream_component);
	comp_class = iterator->upstream_component->class;

	/* Call user-defined destroy method */
	switch (comp_class->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		struct bt_component_class_source *src_comp_cls =
			(void *) comp_class;

		method = (method_t) src_comp_cls->methods.msg_iter_finalize;
		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		struct bt_component_class_filter *flt_comp_cls =
			(void *) comp_class;

		method = (method_t) flt_comp_cls->methods.msg_iter_finalize;
		break;
	}
	default:
		/* Unreachable */
		abort();
	}

	if (method) {
		BT_LIB_LOGD("Calling user's finalization method: %!+i",
			iterator);
		method(iterator);
	}

	iterator->upstream_component = NULL;
	iterator->upstream_port = NULL;
	set_self_comp_port_input_msg_iterator_state(iterator,
		BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_FINALIZED);
	BT_LIB_LOGD("Finalized message iterator: %!+i", iterator);

end:
	return;
}

BT_HIDDEN
void bt_self_component_port_input_message_iterator_set_connection(
		struct bt_self_component_port_input_message_iterator *iterator,
		struct bt_connection *connection)
{
	BT_ASSERT(iterator);
	iterator->connection = connection;
	BT_LIB_LOGV("Set message iterator's connection: "
		"%![iter-]+i, %![conn-]+x", iterator, connection);
}

static
int init_message_iterator(struct bt_message_iterator *iterator,
		enum bt_message_iterator_type type,
		bt_object_release_func destroy)
{
	int ret = 0;

	bt_object_init_shared(&iterator->base, destroy);
	iterator->type = type;
	iterator->msgs = g_ptr_array_new();
	if (!iterator->msgs) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
		ret = -1;
		goto end;
	}

	g_ptr_array_set_size(iterator->msgs, MSG_BATCH_SIZE);

end:
	return ret;
}

static
struct bt_self_component_port_input_message_iterator *
bt_self_component_port_input_message_iterator_create_initial(
		struct bt_component *upstream_comp,
		struct bt_port *upstream_port)
{
	int ret;
	struct bt_self_component_port_input_message_iterator *iterator = NULL;

	BT_ASSERT(upstream_comp);
	BT_ASSERT(upstream_port);
	BT_ASSERT(bt_port_is_connected(upstream_port));
	BT_LIB_LOGD("Creating initial message iterator on self component input port: "
		"%![up-comp-]+c, %![up-port-]+p", upstream_comp, upstream_port);
	BT_ASSERT(bt_component_get_class_type(upstream_comp) ==
		BT_COMPONENT_CLASS_TYPE_SOURCE ||
		bt_component_get_class_type(upstream_comp) ==
		BT_COMPONENT_CLASS_TYPE_FILTER);
	iterator = g_new0(
		struct bt_self_component_port_input_message_iterator, 1);
	if (!iterator) {
		BT_LOGE_STR("Failed to allocate one self component input port "
			"message iterator.");
		goto end;
	}

	ret = init_message_iterator((void *) iterator,
		BT_MESSAGE_ITERATOR_TYPE_SELF_COMPONENT_PORT_INPUT,
		bt_self_component_port_input_message_iterator_destroy);
	if (ret) {
		/* init_message_iterator() logs errors */
		BT_OBJECT_PUT_REF_AND_RESET(iterator);
		goto end;
	}

	iterator->stream_states = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, (GDestroyNotify) destroy_stream_state);
	if (!iterator->stream_states) {
		BT_LOGE_STR("Failed to allocate a GHashTable.");
		BT_OBJECT_PUT_REF_AND_RESET(iterator);
		goto end;
	}

	iterator->upstream_component = upstream_comp;
	iterator->upstream_port = upstream_port;
	iterator->connection = iterator->upstream_port->connection;
	iterator->graph = bt_component_borrow_graph(upstream_comp);
	set_self_comp_port_input_msg_iterator_state(iterator,
		BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_NON_INITIALIZED);
	BT_LIB_LOGD("Created initial message iterator on self component input port: "
		"%![up-port-]+p, %![up-comp-]+c, %![iter-]+i",
		upstream_port, upstream_comp, iterator);

end:
	return iterator;
}

struct bt_self_component_port_input_message_iterator *
bt_self_component_port_input_message_iterator_create(
		struct bt_self_component_port_input *self_port)
{
	typedef enum bt_self_message_iterator_status (*init_method_t)(
			void *, void *, void *);

	init_method_t init_method = NULL;
	struct bt_self_component_port_input_message_iterator *iterator =
		NULL;
	struct bt_port *port = (void *) self_port;
	struct bt_port *upstream_port;
	struct bt_component *comp;
	struct bt_component *upstream_comp;
	struct bt_component_class *upstream_comp_cls;

	BT_ASSERT_PRE_NON_NULL(port, "Port");
	comp = bt_port_borrow_component_inline(port);
	BT_ASSERT_PRE(bt_port_is_connected(port),
		"Port is not connected: %![port-]+p", port);
	BT_ASSERT_PRE(comp, "Port is not part of a component: %![port-]+p",
		port);
	BT_ASSERT_PRE(!bt_component_graph_is_canceled(comp),
		"Port's component's graph is canceled: "
		"%![port-]+p, %![comp-]+c", port, comp);
	BT_ASSERT(port->connection);
	upstream_port = port->connection->upstream_port;
	BT_ASSERT(upstream_port);
	upstream_comp = bt_port_borrow_component_inline(upstream_port);
	BT_ASSERT(upstream_comp);
	upstream_comp_cls = upstream_comp->class;
	BT_ASSERT(upstream_comp->class->type ==
		BT_COMPONENT_CLASS_TYPE_SOURCE ||
		upstream_comp->class->type ==
		BT_COMPONENT_CLASS_TYPE_FILTER);
	iterator = bt_self_component_port_input_message_iterator_create_initial(
		upstream_comp, upstream_port);
	if (!iterator) {
		BT_LOGW_STR("Cannot create self component input port "
			"message iterator.");
		goto end;
	}

	switch (upstream_comp_cls->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		struct bt_component_class_source *src_comp_cls =
			(void *) upstream_comp_cls;

		init_method =
			(init_method_t) src_comp_cls->methods.msg_iter_init;
		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		struct bt_component_class_filter *flt_comp_cls =
			(void *) upstream_comp_cls;

		init_method =
			(init_method_t) flt_comp_cls->methods.msg_iter_init;
		break;
	}
	default:
		/* Unreachable */
		abort();
	}

	if (init_method) {
		int iter_status;

		BT_LIB_LOGD("Calling user's initialization method: %!+i", iterator);
		iter_status = init_method(iterator, upstream_comp,
			upstream_port);
		BT_LOGD("User method returned: status=%s",
			bt_message_iterator_status_string(iter_status));
		if (iter_status != BT_MESSAGE_ITERATOR_STATUS_OK) {
			BT_LOGW_STR("Initialization method failed.");
			BT_OBJECT_PUT_REF_AND_RESET(iterator);
			goto end;
		}
	}

	set_self_comp_port_input_msg_iterator_state(iterator,
		BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_ACTIVE);
	g_ptr_array_add(port->connection->iterators, iterator);
	BT_LIB_LOGD("Created message iterator on self component input port: "
		"%![up-port-]+p, %![up-comp-]+c, %![iter-]+i",
		upstream_port, upstream_comp, iterator);

end:
	return iterator;
}

void *bt_self_message_iterator_get_data(
		const struct bt_self_message_iterator *self_iterator)
{
	struct bt_self_component_port_input_message_iterator *iterator =
		(void *) self_iterator;

	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	return iterator->user_data;
}

void bt_self_message_iterator_set_data(
		struct bt_self_message_iterator *self_iterator, void *data)
{
	struct bt_self_component_port_input_message_iterator *iterator =
		(void *) self_iterator;

	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	iterator->user_data = data;
	BT_LIB_LOGV("Set message iterator's user data: "
		"%!+i, user-data-addr=%p", iterator, data);
}

BT_ASSERT_PRE_FUNC
static inline
void bt_message_borrow_packet_stream(const struct bt_message *msg,
		const struct bt_stream **stream,
		const struct bt_packet **packet)
{
	BT_ASSERT(msg);

	switch (msg->type) {
	case BT_MESSAGE_TYPE_EVENT:
		*packet = bt_event_borrow_packet_const(
			bt_message_event_borrow_event_const(msg));
		*stream = bt_packet_borrow_stream_const(*packet);
		break;
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
		*stream = bt_message_stream_beginning_borrow_stream_const(msg);
		break;
	case BT_MESSAGE_TYPE_STREAM_END:
		*stream = bt_message_stream_end_borrow_stream_const(msg);
		break;
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		*packet = bt_message_packet_beginning_borrow_packet_const(msg);
		*stream = bt_packet_borrow_stream_const(*packet);
		break;
	case BT_MESSAGE_TYPE_PACKET_END:
		*packet = bt_message_packet_end_borrow_packet_const(msg);
		*stream = bt_packet_borrow_stream_const(*packet);
		break;
	default:
		break;
	}
}

BT_ASSERT_PRE_FUNC
static inline
bool validate_message(
		struct bt_self_component_port_input_message_iterator *iterator,
		const struct bt_message *c_msg)
{
	bool is_valid = true;
	struct stream_state *stream_state;
	const struct bt_stream *stream = NULL;
	const struct bt_packet *packet = NULL;
	struct bt_message *msg = (void *) c_msg;

	BT_ASSERT(msg);
	bt_message_borrow_packet_stream(c_msg, &stream, &packet);

	if (!stream) {
		/* we don't care about messages not attached to streams */
		goto end;
	}

	stream_state = g_hash_table_lookup(iterator->stream_states, stream);
	if (!stream_state) {
		/*
		 * No stream state for this stream: this message
		 * MUST be a BT_MESSAGE_TYPE_STREAM_BEGINNING message
		 * and its sequence number must be 0.
		 */
		if (c_msg->type != BT_MESSAGE_TYPE_STREAM_BEGINNING) {
			BT_ASSERT_PRE_MSG("Unexpected message: missing a "
				"BT_MESSAGE_TYPE_STREAM_BEGINNING "
				"message prior to this message: "
				"%![stream-]+s", stream);
			is_valid = false;
			goto end;
		}

		if (c_msg->seq_num == -1ULL) {
			msg->seq_num = 0;
		}

		if (c_msg->seq_num != 0) {
			BT_ASSERT_PRE_MSG("Unexpected message sequence "
				"number for this message iterator: "
				"this is the first message for this "
				"stream, expecting sequence number 0: "
				"seq-num=%" PRIu64 ", %![stream-]+s",
				c_msg->seq_num, stream);
			is_valid = false;
			goto end;
		}

		stream_state = create_stream_state(stream);
		if (!stream_state) {
			abort();
		}

		g_hash_table_insert(iterator->stream_states,
			(void *) stream, stream_state);
		stream_state->expected_msg_seq_num++;
		goto end;
	}

	if (stream_state->is_ended) {
		/*
		 * There's a new message which has a reference to a
		 * stream which, from this iterator's point of view, is
		 * ended ("end of stream" message was returned).
		 * This is bad: the API guarantees that it can never
		 * happen.
		 */
		BT_ASSERT_PRE_MSG("Stream is already ended: %![stream-]+s",
			stream);
		is_valid = false;
		goto end;
	}

	if (c_msg->seq_num == -1ULL) {
		msg->seq_num = stream_state->expected_msg_seq_num;
	}

	if (c_msg->seq_num != -1ULL &&
			c_msg->seq_num != stream_state->expected_msg_seq_num) {
		BT_ASSERT_PRE_MSG("Unexpected message sequence number: "
			"seq-num=%" PRIu64 ", "
			"expected-seq-num=%" PRIu64 ", %![stream-]+s",
			c_msg->seq_num, stream_state->expected_msg_seq_num,
			stream);
		is_valid = false;
		goto end;
	}

	switch (c_msg->type) {
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
		BT_ASSERT_PRE_MSG("Unexpected BT_MESSAGE_TYPE_STREAM_BEGINNING "
			"message at this point: msg-seq-num=%" PRIu64 ", "
			"%![stream-]+s", c_msg->seq_num, stream);
		is_valid = false;
		goto end;
	case BT_MESSAGE_TYPE_STREAM_END:
		if (stream_state->cur_packet) {
			BT_ASSERT_PRE_MSG("Unexpected BT_MESSAGE_TYPE_STREAM_END "
				"message: missing a "
				"BT_MESSAGE_TYPE_PACKET_END message "
				"prior to this message: "
				"msg-seq-num=%" PRIu64 ", "
				"%![stream-]+s", c_msg->seq_num, stream);
			is_valid = false;
			goto end;
		}
		stream_state->expected_msg_seq_num++;
		stream_state->is_ended = true;
		goto end;
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		if (stream_state->cur_packet) {
			BT_ASSERT_PRE_MSG("Unexpected BT_MESSAGE_TYPE_PACKET_BEGINNING "
				"message at this point: missing a "
				"BT_MESSAGE_TYPE_PACKET_END message "
				"prior to this message: "
				"msg-seq-num=%" PRIu64 ", %![stream-]+s, "
				"%![packet-]+a", c_msg->seq_num, stream,
				packet);
			is_valid = false;
			goto end;
		}
		stream_state->expected_msg_seq_num++;
		stream_state->cur_packet = packet;
		bt_object_get_no_null_check(stream_state->cur_packet);
		goto end;
	case BT_MESSAGE_TYPE_PACKET_END:
		if (!stream_state->cur_packet) {
			BT_ASSERT_PRE_MSG("Unexpected BT_MESSAGE_TYPE_PACKET_END "
				"message at this point: missing a "
				"BT_MESSAGE_TYPE_PACKET_BEGINNING message "
				"prior to this message: "
				"msg-seq-num=%" PRIu64 ", %![stream-]+s, "
				"%![packet-]+a", c_msg->seq_num, stream,
				packet);
			is_valid = false;
			goto end;
		}
		stream_state->expected_msg_seq_num++;
		BT_OBJECT_PUT_REF_AND_RESET(stream_state->cur_packet);
		goto end;
	case BT_MESSAGE_TYPE_EVENT:
		if (packet != stream_state->cur_packet) {
			BT_ASSERT_PRE_MSG("Unexpected packet for "
				"BT_MESSAGE_TYPE_EVENT message: "
				"msg-seq-num=%" PRIu64 ", %![stream-]+s, "
				"%![msg-packet-]+a, %![expected-packet-]+a",
				c_msg->seq_num, stream,
				stream_state->cur_packet, packet);
			is_valid = false;
			goto end;
		}
		stream_state->expected_msg_seq_num++;
		goto end;
	default:
		break;
	}

end:
	return is_valid;
}

BT_ASSERT_PRE_FUNC
static inline
bool validate_messages(
		struct bt_self_component_port_input_message_iterator *iterator,
		uint64_t count)
{
	bool ret = true;
	bt_message_array_const msgs =
		(void *) iterator->base.msgs->pdata;
	uint64_t i;

	for (i = 0; i < count; i++) {
		ret = validate_message(iterator, msgs[i]);
		if (!ret) {
			break;
		}
	}

	return ret;
}

BT_ASSERT_PRE_FUNC
static inline bool self_comp_port_input_msg_iter_can_end(
		struct bt_self_component_port_input_message_iterator *iterator)
{
	GHashTableIter iter;
	gpointer stream_key, state_value;
	bool ret = true;

	/*
	 * Verify that this iterator received a
	 * BT_MESSAGE_TYPE_STREAM_END message for each stream
	 * which has a state.
	 */

	g_hash_table_iter_init(&iter, iterator->stream_states);

	while (g_hash_table_iter_next(&iter, &stream_key, &state_value)) {
		struct stream_state *stream_state = (void *) state_value;

		BT_ASSERT(stream_state);
		BT_ASSERT(stream_key);

		if (!stream_state->is_ended) {
			BT_ASSERT_PRE_MSG("Ending message iterator, "
				"but stream is not ended: "
				"%![stream-]s", stream_key);
			ret = false;
			goto end;
		}
	}

end:
	return ret;
}

enum bt_message_iterator_status
bt_self_component_port_input_message_iterator_next(
		struct bt_self_component_port_input_message_iterator *iterator,
		bt_message_array_const *msgs, uint64_t *user_count)
{
	typedef enum bt_self_message_iterator_status (*method_t)(
			void *, bt_message_array_const, uint64_t, uint64_t *);

	method_t method = NULL;
	struct bt_component_class *comp_cls;
	int status = BT_MESSAGE_ITERATOR_STATUS_OK;

	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	BT_ASSERT_PRE_NON_NULL(msgs, "Message array (output)");
	BT_ASSERT_PRE_NON_NULL(user_count, "Message count (output)");
	BT_ASSERT_PRE(iterator->state ==
		BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_ACTIVE,
		"Message iterator's \"next\" called, but "
		"iterator is in the wrong state: %!+i", iterator);
	BT_ASSERT(iterator->upstream_component);
	BT_ASSERT(iterator->upstream_component->class);
	BT_LIB_LOGD("Getting next self component input port "
		"message iterator's messages: %!+i", iterator);
	comp_cls = iterator->upstream_component->class;

	/* Pick the appropriate "next" method */
	switch (comp_cls->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		struct bt_component_class_source *src_comp_cls =
			(void *) comp_cls;

		method = (method_t) src_comp_cls->methods.msg_iter_next;
		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		struct bt_component_class_filter *flt_comp_cls =
			(void *) comp_cls;

		method = (method_t) flt_comp_cls->methods.msg_iter_next;
		break;
	}
	default:
		abort();
	}

	/*
	 * Call the user's "next" method to get the next messages
	 * and status.
	 */
	BT_ASSERT(method);
	BT_LOGD_STR("Calling user's \"next\" method.");
	status = method(iterator, (void *) iterator->base.msgs->pdata,
		MSG_BATCH_SIZE, user_count);
	BT_LOGD("User method returned: status=%s",
		bt_message_iterator_status_string(status));
	if (status < 0) {
		BT_LOGW_STR("User method failed.");
		goto end;
	}

#ifdef BT_DEV_MODE
	/*
	 * There is no way that this iterator could have been finalized
	 * during its "next" method, as the only way to do this is to
	 * put the last iterator's reference, and this can only be done
	 * by its downstream owner.
	 */
	BT_ASSERT(iterator->state ==
		BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_ACTIVE);
#endif

	switch (status) {
	case BT_MESSAGE_ITERATOR_STATUS_OK:
		BT_ASSERT_PRE(validate_messages(iterator, *user_count),
			"Messages are invalid at this point: "
			"%![msg-iter-]+i, count=%" PRIu64,
			iterator, *user_count);
		*msgs = (void *) iterator->base.msgs->pdata;
		break;
	case BT_MESSAGE_ITERATOR_STATUS_AGAIN:
		goto end;
	case BT_MESSAGE_ITERATOR_STATUS_END:
		BT_ASSERT_PRE(self_comp_port_input_msg_iter_can_end(iterator),
			"Message iterator cannot end at this point: "
			"%!+i", iterator);
		set_self_comp_port_input_msg_iterator_state(iterator,
			BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_ENDED);
		goto end;
	default:
		/* Unknown non-error status */
		abort();
	}

end:
	return status;
}

enum bt_message_iterator_status bt_port_output_message_iterator_next(
		struct bt_port_output_message_iterator *iterator,
		bt_message_array_const *msgs_to_user,
		uint64_t *count_to_user)
{
	enum bt_message_iterator_status status;
	enum bt_graph_status graph_status;

	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	BT_ASSERT_PRE_NON_NULL(msgs_to_user, "Message array (output)");
	BT_ASSERT_PRE_NON_NULL(count_to_user, "Message count (output)");
	BT_LIB_LOGD("Getting next output port message iterator's messages: "
		"%!+i", iterator);

	graph_status = bt_graph_consume_sink_no_check(iterator->graph,
		iterator->colander);
	switch (graph_status) {
	case BT_GRAPH_STATUS_CANCELED:
	case BT_GRAPH_STATUS_AGAIN:
	case BT_GRAPH_STATUS_END:
	case BT_GRAPH_STATUS_NOMEM:
		status = (int) graph_status;
		break;
	case BT_GRAPH_STATUS_OK:
		status = BT_MESSAGE_ITERATOR_STATUS_OK;

		/*
		 * On success, the colander sink moves the messages
		 * to this iterator's array and sets this iterator's
		 * message count: move them to the user.
		 */
		*msgs_to_user = (void *) iterator->base.msgs->pdata;
		*count_to_user = iterator->count;
		break;
	default:
		/* Other errors */
		status = BT_MESSAGE_ITERATOR_STATUS_ERROR;
	}

	return status;
}

struct bt_component *bt_self_component_port_input_message_iterator_borrow_component(
		struct bt_self_component_port_input_message_iterator *iterator)
{
	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	return iterator->upstream_component;
}

struct bt_self_component *bt_self_message_iterator_borrow_component(
		struct bt_self_message_iterator *self_iterator)
{
	struct bt_self_component_port_input_message_iterator *iterator =
		(void *) self_iterator;

	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	return (void *) iterator->upstream_component;
}

struct bt_self_port_output *bt_self_message_iterator_borrow_port(
		struct bt_self_message_iterator *self_iterator)
{
	struct bt_self_component_port_input_message_iterator *iterator =
		(void *) self_iterator;

	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	return (void *) iterator->upstream_port;
}

static
void bt_port_output_message_iterator_destroy(struct bt_object *obj)
{
	struct bt_port_output_message_iterator *iterator = (void *) obj;

	BT_LIB_LOGD("Destroying output port message iterator object: %!+i",
		iterator);
	BT_LOGD_STR("Putting graph.");
	BT_OBJECT_PUT_REF_AND_RESET(iterator->graph);
	BT_LOGD_STR("Putting colander sink component.");
	BT_OBJECT_PUT_REF_AND_RESET(iterator->colander);
	destroy_base_message_iterator(obj);
}

struct bt_port_output_message_iterator *
bt_port_output_message_iterator_create(
		struct bt_graph *graph,
		const struct bt_port_output *output_port)
{
	struct bt_port_output_message_iterator *iterator = NULL;
	struct bt_component_class_sink *colander_comp_cls = NULL;
	struct bt_component *output_port_comp = NULL;
	struct bt_component_sink *colander_comp;
	enum bt_graph_status graph_status;
	struct bt_port_input *colander_in_port = NULL;
	struct bt_component_class_sink_colander_data colander_data;
	int ret;

	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(output_port, "Output port");
	output_port_comp = bt_port_borrow_component_inline(
		(const void *) output_port);
	BT_ASSERT_PRE(output_port_comp,
		"Output port has no component: %!+p", output_port);
	BT_ASSERT_PRE(bt_component_borrow_graph(output_port_comp) ==
		(void *) graph,
		"Output port is not part of graph: %![graph-]+g, %![port-]+p",
		graph, output_port);

	/* Create message iterator */
	BT_LIB_LOGD("Creating message iterator on output port: "
		"%![port-]+p, %![comp-]+c", output_port, output_port_comp);
	iterator = g_new0(struct bt_port_output_message_iterator, 1);
	if (!iterator) {
		BT_LOGE_STR("Failed to allocate one output port message iterator.");
		goto error;
	}

	ret = init_message_iterator((void *) iterator,
		BT_MESSAGE_ITERATOR_TYPE_PORT_OUTPUT,
		bt_port_output_message_iterator_destroy);
	if (ret) {
		/* init_message_iterator() logs errors */
		BT_OBJECT_PUT_REF_AND_RESET(iterator);
		goto end;
	}

	/* Create colander component */
	colander_comp_cls = bt_component_class_sink_colander_get();
	if (!colander_comp_cls) {
		BT_LOGW("Cannot get colander sink component class.");
		goto error;
	}

	iterator->graph = graph;
	bt_object_get_no_null_check(iterator->graph);
	colander_data.msgs = (void *) iterator->base.msgs->pdata;
	colander_data.count_addr = &iterator->count;

	/* Hope that nobody uses this very unique name */
	graph_status =
		bt_graph_add_sink_component_with_init_method_data(
			(void *) graph, colander_comp_cls,
			"colander-36ac3409-b1a8-4d60-ab1f-4fdf341a8fb1",
			NULL, &colander_data, (void *) &iterator->colander);
	if (graph_status != BT_GRAPH_STATUS_OK) {
		BT_LIB_LOGW("Cannot add colander sink component to graph: "
			"%1[graph-]+g, status=%s", graph,
			bt_graph_status_string(graph_status));
		goto error;
	}

	/*
	 * Connect provided output port to the colander component's
	 * input port.
	 */
	colander_in_port =
		(void *) bt_component_sink_borrow_input_port_by_index_const(
			(void *) iterator->colander, 0);
	BT_ASSERT(colander_in_port);
	graph_status = bt_graph_connect_ports(graph,
		output_port, colander_in_port, NULL);
	if (graph_status != BT_GRAPH_STATUS_OK) {
		BT_LIB_LOGW("Cannot add colander sink component to graph: "
			"%![graph-]+g, %![comp-]+c, status=%s", graph,
			iterator->colander,
			bt_graph_status_string(graph_status));
		goto error;
	}

	/*
	 * At this point everything went fine. Make the graph
	 * nonconsumable forever so that only this message iterator
	 * can consume (thanks to bt_graph_consume_sink_no_check()).
	 * This avoids leaking the message created by the colander
	 * sink and moved to the message iterator's message
	 * member.
	 */
	bt_graph_set_can_consume(iterator->graph, false);
	goto end;

error:
	if (iterator && iterator->graph && iterator->colander) {
		int ret;

		/* Remove created colander component from graph if any */
		colander_comp = iterator->colander;
		BT_OBJECT_PUT_REF_AND_RESET(iterator->colander);

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
			(void *) colander_comp);
		BT_ASSERT(ret == 0);
	}

	BT_OBJECT_PUT_REF_AND_RESET(iterator);

end:
	bt_object_put_ref(colander_comp_cls);
	return (void *) iterator;
}

void bt_port_output_message_iterator_get_ref(
		const struct bt_port_output_message_iterator *iterator)
{
	bt_object_get_ref(iterator);
}

void bt_port_output_message_iterator_put_ref(
		const struct bt_port_output_message_iterator *iterator)
{
	bt_object_put_ref(iterator);
}

void bt_self_component_port_input_message_iterator_get_ref(
		const struct bt_self_component_port_input_message_iterator *iterator)
{
	bt_object_get_ref(iterator);
}

void bt_self_component_port_input_message_iterator_put_ref(
		const struct bt_self_component_port_input_message_iterator *iterator)
{
	bt_object_put_ref(iterator);
}
