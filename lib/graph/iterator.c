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
#include <babeltrace/graph/component-sink-internal.h>
#include <babeltrace/graph/message-const.h>
#include <babeltrace/graph/message-iterator-const.h>
#include <babeltrace/graph/message-iterator-internal.h>
#include <babeltrace/graph/self-component-port-input-message-iterator.h>
#include <babeltrace/graph/port-output-message-iterator.h>
#include <babeltrace/graph/message-internal.h>
#include <babeltrace/graph/message-event-const.h>
#include <babeltrace/graph/message-event-internal.h>
#include <babeltrace/graph/message-packet-beginning-const.h>
#include <babeltrace/graph/message-packet-end-const.h>
#include <babeltrace/graph/message-packet-internal.h>
#include <babeltrace/graph/message-stream-beginning-const.h>
#include <babeltrace/graph/message-stream-end-const.h>
#include <babeltrace/graph/message-stream-internal.h>
#include <babeltrace/graph/message-inactivity-internal.h>
#include <babeltrace/graph/message-discarded-items-internal.h>
#include <babeltrace/graph/message-stream-activity-internal.h>
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

#define BT_ASSERT_PRE_ITER_HAS_STATE_TO_SEEK(_iter)			\
	BT_ASSERT_PRE((_iter)->state == BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_ACTIVE || \
		(_iter)->state == BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_ENDED || \
		(_iter)->state == BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_LAST_SEEKING_RETURNED_AGAIN || \
		(_iter)->state == BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_LAST_SEEKING_RETURNED_ERROR, \
		"Message iterator is in the wrong state: %!+i", _iter)

static inline
void _set_self_comp_port_input_msg_iterator_state(
		struct bt_self_component_port_input_message_iterator *iterator,
		enum bt_self_component_port_input_message_iterator_state state)
{
	BT_ASSERT(iterator);
	BT_LIB_LOGD("Updating message iterator's state: new-state=%s",
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

	if (iterator->connection) {
		/*
		 * Remove ourself from the originating connection so
		 * that it does not try to finalize a dangling pointer
		 * later.
		 */
		bt_connection_remove_iterator(iterator->connection, iterator);
		iterator->connection = NULL;
	}

	if (iterator->auto_seek_msgs) {
		uint64_t i;

		/* Put any remaining message in the auto-seek array */
		for (i = 0; i < iterator->auto_seek_msgs->len; i++) {
			if (iterator->auto_seek_msgs->pdata[i]) {
				bt_object_put_no_null_check(
					iterator->auto_seek_msgs->pdata[i]);
			}
		}

		g_ptr_array_free(iterator->auto_seek_msgs, TRUE);
		iterator->auto_seek_msgs = NULL;
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
bt_bool can_seek_ns_from_origin_true(
		struct bt_self_component_port_input_message_iterator *iterator,
		int64_t ns_from_origin)
{
	return BT_TRUE;
}

static
bt_bool can_seek_beginning_true(
		struct bt_self_component_port_input_message_iterator *iterator)
{
	return BT_TRUE;
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

	iterator->auto_seek_msgs = g_ptr_array_new();
	if (!iterator->auto_seek_msgs) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
		ret = -1;
		goto end;
	}

	g_ptr_array_set_size(iterator->auto_seek_msgs, MSG_BATCH_SIZE);
	iterator->upstream_component = upstream_comp;
	iterator->upstream_port = upstream_port;
	iterator->connection = iterator->upstream_port->connection;
	iterator->graph = bt_component_borrow_graph(upstream_comp);
	set_self_comp_port_input_msg_iterator_state(iterator,
		BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_NON_INITIALIZED);

	switch (iterator->upstream_component->class->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		struct bt_component_class_source *src_comp_cls =
			(void *) iterator->upstream_component->class;

		iterator->methods.next =
			(bt_self_component_port_input_message_iterator_next_method)
				src_comp_cls->methods.msg_iter_next;
		iterator->methods.seek_ns_from_origin =
			(bt_self_component_port_input_message_iterator_seek_ns_from_origin_method)
				src_comp_cls->methods.msg_iter_seek_ns_from_origin;
		iterator->methods.seek_beginning =
			(bt_self_component_port_input_message_iterator_seek_beginning_method)
				src_comp_cls->methods.msg_iter_seek_beginning;
		iterator->methods.can_seek_ns_from_origin =
			(bt_self_component_port_input_message_iterator_can_seek_ns_from_origin_method)
				src_comp_cls->methods.msg_iter_can_seek_ns_from_origin;
		iterator->methods.can_seek_beginning =
			(bt_self_component_port_input_message_iterator_can_seek_beginning_method)
				src_comp_cls->methods.msg_iter_can_seek_beginning;
		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		struct bt_component_class_filter *flt_comp_cls =
			(void *) iterator->upstream_component->class;

		iterator->methods.next =
			(bt_self_component_port_input_message_iterator_next_method)
				flt_comp_cls->methods.msg_iter_next;
		iterator->methods.seek_ns_from_origin =
			(bt_self_component_port_input_message_iterator_seek_ns_from_origin_method)
				flt_comp_cls->methods.msg_iter_seek_ns_from_origin;
		iterator->methods.seek_beginning =
			(bt_self_component_port_input_message_iterator_seek_beginning_method)
				flt_comp_cls->methods.msg_iter_seek_beginning;
		iterator->methods.can_seek_ns_from_origin =
			(bt_self_component_port_input_message_iterator_can_seek_ns_from_origin_method)
				flt_comp_cls->methods.msg_iter_can_seek_ns_from_origin;
		iterator->methods.can_seek_beginning =
			(bt_self_component_port_input_message_iterator_can_seek_beginning_method)
				flt_comp_cls->methods.msg_iter_can_seek_beginning;
		break;
	}
	default:
		abort();
	}

	if (iterator->methods.seek_ns_from_origin &&
			!iterator->methods.can_seek_ns_from_origin) {
		iterator->methods.can_seek_ns_from_origin =
			(bt_self_component_port_input_message_iterator_can_seek_ns_from_origin_method)
				can_seek_ns_from_origin_true;
	}

	if (iterator->methods.seek_beginning &&
			!iterator->methods.can_seek_beginning) {
		iterator->methods.can_seek_beginning =
			(bt_self_component_port_input_message_iterator_seek_beginning_method)
				can_seek_beginning_true;
	}

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

enum bt_message_iterator_status
bt_self_component_port_input_message_iterator_next(
		struct bt_self_component_port_input_message_iterator *iterator,
		bt_message_array_const *msgs, uint64_t *user_count)
{
	int status = BT_MESSAGE_ITERATOR_STATUS_OK;

	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	BT_ASSERT_PRE_NON_NULL(msgs, "Message array (output)");
	BT_ASSERT_PRE_NON_NULL(user_count, "Message count (output)");
	BT_ASSERT_PRE(iterator->state ==
		BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_ACTIVE,
		"Message iterator's \"next\" called, but "
		"message iterator is in the wrong state: %!+i", iterator);
	BT_ASSERT(iterator->upstream_component);
	BT_ASSERT(iterator->upstream_component->class);
	BT_ASSERT_PRE(bt_component_borrow_graph(iterator->upstream_component)->is_configured,
		"Graph is not configured: %!+g",
		bt_component_borrow_graph(iterator->upstream_component));
	BT_LIB_LOGD("Getting next self component input port "
		"message iterator's messages: %!+i", iterator);

	/*
	 * Call the user's "next" method to get the next messages
	 * and status.
	 */
	BT_ASSERT(iterator->methods.next);
	BT_LOGD_STR("Calling user's \"next\" method.");
	status = iterator->methods.next(iterator,
		(void *) iterator->base.msgs->pdata, MSG_BATCH_SIZE,
		user_count);
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
	 *
	 * For the same reason, there is no way that this iterator could
	 * have seeked (cannot seek a self message iterator).
	 */
	BT_ASSERT(iterator->state ==
		BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_ACTIVE);
#endif

	switch (status) {
	case BT_MESSAGE_ITERATOR_STATUS_OK:
		BT_ASSERT_PRE(*user_count <= MSG_BATCH_SIZE,
			"Invalid returned message count: greater than "
			"batch size: count=%" PRIu64 ", batch-size=%u",
			*user_count, MSG_BATCH_SIZE);
		*msgs = (void *) iterator->base.msgs->pdata;
		break;
	case BT_MESSAGE_ITERATOR_STATUS_AGAIN:
		goto end;
	case BT_MESSAGE_ITERATOR_STATUS_END:
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

	/*
	 * As soon as the user calls this function, we mark the graph as
	 * being definitely configured.
	 */
	bt_graph_set_is_configured(iterator->graph, true);

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

struct bt_component *
bt_self_component_port_input_message_iterator_borrow_component(
		struct bt_self_component_port_input_message_iterator *iterator)
{
	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	return iterator->upstream_component;
}

const struct bt_component *
bt_self_component_port_input_message_iterator_borrow_component_const(
		const struct bt_self_component_port_input_message_iterator *iterator)
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
bt_port_output_message_iterator_create(struct bt_graph *graph,
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

bt_bool bt_self_component_port_input_message_iterator_can_seek_ns_from_origin(
		struct bt_self_component_port_input_message_iterator *iterator,
		int64_t ns_from_origin)
{
	bt_bool can = BT_FALSE;

	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	BT_ASSERT_PRE_ITER_HAS_STATE_TO_SEEK(iterator);
	BT_ASSERT_PRE(bt_component_borrow_graph(iterator->upstream_component)->is_configured,
		"Graph is not configured: %!+g",
		bt_component_borrow_graph(iterator->upstream_component));

	if (iterator->methods.can_seek_ns_from_origin) {
		can = iterator->methods.can_seek_ns_from_origin(iterator,
			ns_from_origin);
		goto end;
	}

	/*
	 * Automatic seeking fall back: if we can seek to the beginning,
	 * then we can automatically seek to any message.
	 */
	if (iterator->methods.can_seek_beginning) {
		can = iterator->methods.can_seek_beginning(iterator);
	}

end:
	return can;
}

bt_bool bt_self_component_port_input_message_iterator_can_seek_beginning(
		struct bt_self_component_port_input_message_iterator *iterator)
{
	bt_bool can = BT_FALSE;

	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	BT_ASSERT_PRE_ITER_HAS_STATE_TO_SEEK(iterator);
	BT_ASSERT_PRE(bt_component_borrow_graph(iterator->upstream_component)->is_configured,
		"Graph is not configured: %!+g",
		bt_component_borrow_graph(iterator->upstream_component));

	if (iterator->methods.can_seek_beginning) {
		can = iterator->methods.can_seek_beginning(iterator);
	}

	return can;
}

static inline
void _set_iterator_state_after_seeking(
		struct bt_self_component_port_input_message_iterator *iterator,
		enum bt_message_iterator_status status)
{
	enum bt_self_component_port_input_message_iterator_state new_state = 0;

	/* Set iterator's state depending on seeking status */
	switch (status) {
	case BT_MESSAGE_ITERATOR_STATUS_OK:
		new_state = BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_ACTIVE;
		break;
	case BT_MESSAGE_ITERATOR_STATUS_AGAIN:
		new_state = BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_LAST_SEEKING_RETURNED_AGAIN;
		break;
	case BT_MESSAGE_ITERATOR_STATUS_ERROR:
	case BT_MESSAGE_ITERATOR_STATUS_NOMEM:
		new_state = BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_LAST_SEEKING_RETURNED_ERROR;
		break;
	case BT_MESSAGE_ITERATOR_STATUS_END:
		new_state = BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_ENDED;
		break;
	default:
		abort();
	}

	set_self_comp_port_input_msg_iterator_state(iterator, new_state);
}

#ifdef BT_DEV_MODE
# define set_iterator_state_after_seeking	_set_iterator_state_after_seeking
#else
# define set_iterator_state_after_seeking(_iter, _status)
#endif

enum bt_message_iterator_status
bt_self_component_port_input_message_iterator_seek_beginning(
		struct bt_self_component_port_input_message_iterator *iterator)
{
	int status;

	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	BT_ASSERT_PRE_ITER_HAS_STATE_TO_SEEK(iterator);
	BT_ASSERT_PRE(bt_component_borrow_graph(iterator->upstream_component)->is_configured,
		"Graph is not configured: %!+g",
		bt_component_borrow_graph(iterator->upstream_component));
	BT_ASSERT_PRE(
		bt_self_component_port_input_message_iterator_can_seek_beginning(
			iterator),
		"Message iterator cannot seek beginning: %!+i", iterator);
	BT_LIB_LOGD("Calling user's \"seek beginning\" method: %!+i", iterator);
	set_self_comp_port_input_msg_iterator_state(iterator,
		BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_SEEKING);
	status = iterator->methods.seek_beginning(iterator);
	BT_LOGD("User method returned: status=%s",
		bt_message_iterator_status_string(status));
	BT_ASSERT_PRE(status == BT_MESSAGE_ITERATOR_STATUS_OK ||
		status == BT_MESSAGE_ITERATOR_STATUS_ERROR ||
		status == BT_MESSAGE_ITERATOR_STATUS_NOMEM ||
		status == BT_MESSAGE_ITERATOR_STATUS_AGAIN,
		"Unexpected status: %![iter-]+i, status=%s",
		iterator, bt_self_message_iterator_status_string(status));
	set_iterator_state_after_seeking(iterator, status);
	return status;
}

static inline
int get_message_ns_from_origin(const struct bt_message *msg,
		int64_t *ns_from_origin, bool *ignore)
{
	const struct bt_clock_snapshot *clk_snapshot = NULL;
	int ret = 0;

	switch (msg->type) {
	case BT_MESSAGE_TYPE_EVENT:
	{
		const struct bt_message_event *event_msg =
			(const void *) msg;

		clk_snapshot = event_msg->default_cs;
		BT_ASSERT_PRE(clk_snapshot,
			"Event has no default clock snapshot: %!+e",
			event_msg->event);
		break;
	}
	case BT_MESSAGE_TYPE_INACTIVITY:
	{
		const struct bt_message_inactivity *inactivity_msg =
			(const void *) msg;

		clk_snapshot = inactivity_msg->default_cs;
		BT_ASSERT(clk_snapshot);
		break;
	}
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
	case BT_MESSAGE_TYPE_PACKET_END:
		/* Ignore */
		goto end;
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
	{
		const struct bt_message_discarded_items *disc_items_msg =
			(const void *) msg;

		clk_snapshot = disc_items_msg->default_begin_cs;
		BT_ASSERT_PRE(clk_snapshot,
			"Discarded events/packets message has no default clock snapshot: %!+n",
			msg);
		break;
	}
	case BT_MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING:
	{
		const struct bt_message_stream_activity *stream_act_msg =
			(const void *) msg;

		switch (stream_act_msg->default_cs_state) {
		case BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_UNKNOWN:
		case BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_INFINITE:
			/*
			 * -inf is always less than any requested time,
			 * and we can't assume any specific time for an
			 * unknown clock snapshot, so skip this.
			 */
			goto set_ignore;
		case BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_KNOWN:
			clk_snapshot = stream_act_msg->default_cs;
			BT_ASSERT(clk_snapshot);
			break;
		default:
			abort();
		}

		break;
	}
	case BT_MESSAGE_TYPE_STREAM_ACTIVITY_END:
	{
		const struct bt_message_stream_activity *stream_act_msg =
			(const void *) msg;

		switch (stream_act_msg->default_cs_state) {
		case BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_UNKNOWN:
			/*
			 * We can't assume any specific time for an
			 * unknown clock snapshot, so skip this.
			 */
			goto set_ignore;
		case BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_INFINITE:
			/*
			 * +inf is always greater than any requested
			 * time.
			 */
			*ns_from_origin = INT64_MAX;
			goto end;
		case BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_KNOWN:
			clk_snapshot = stream_act_msg->default_cs;
			BT_ASSERT(clk_snapshot);
			break;
		default:
			abort();
		}

		break;
	}
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
	case BT_MESSAGE_TYPE_STREAM_END:
		/* Ignore */
		break;
	default:
		abort();
	}

set_ignore:
	if (!clk_snapshot) {
		*ignore = true;
		goto end;
	}

	ret = bt_clock_snapshot_get_ns_from_origin(clk_snapshot,
		ns_from_origin);

end:
	return ret;
}

static
enum bt_message_iterator_status find_message_ge_ns_from_origin(
		struct bt_self_component_port_input_message_iterator *iterator,
		int64_t ns_from_origin)
{
	int status;
	enum bt_self_component_port_input_message_iterator_state init_state =
		iterator->state;
	const struct bt_message *messages[MSG_BATCH_SIZE];
	uint64_t user_count = 0;
	uint64_t i;

	BT_ASSERT(iterator);
	memset(&messages[0], 0, sizeof(messages[0]) * MSG_BATCH_SIZE);

	/*
	 * Make this iterator temporarily active (not seeking) to call
	 * the "next" method.
	 */
	set_self_comp_port_input_msg_iterator_state(iterator,
		BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_ACTIVE);

	BT_ASSERT(iterator->methods.next);

	while (true) {
		/*
		 * Call the user's "next" method to get the next
		 * messages and status.
		 */
		BT_LOGD_STR("Calling user's \"next\" method.");
		status = iterator->methods.next(iterator,
			&messages[0], MSG_BATCH_SIZE, &user_count);
		BT_LOGD("User method returned: status=%s",
			bt_message_iterator_status_string(status));

#ifdef BT_DEV_MODE
		/*
		 * The user's "next" method must not do any action which
		 * would change the iterator's state.
		 */
		BT_ASSERT(iterator->state ==
			BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_ACTIVE);
#endif

		switch (status) {
		case BT_MESSAGE_ITERATOR_STATUS_OK:
			BT_ASSERT_PRE(user_count <= MSG_BATCH_SIZE,
				"Invalid returned message count: greater than "
				"batch size: count=%" PRIu64 ", batch-size=%u",
				user_count, MSG_BATCH_SIZE);
			break;
		case BT_MESSAGE_ITERATOR_STATUS_AGAIN:
		case BT_MESSAGE_ITERATOR_STATUS_ERROR:
		case BT_MESSAGE_ITERATOR_STATUS_NOMEM:
		case BT_MESSAGE_ITERATOR_STATUS_END:
			goto end;
		default:
			abort();
		}

		/*
		 * Find first message which has a default clock snapshot
		 * that is greater than or equal to the requested value.
		 *
		 * For event and inactivity messages, compare with the
		 * default clock snapshot.
		 *
		 * For packet beginning messages, compare with the
		 * default beginning clock snapshot, if any.
		 *
		 * For packet end messages, compare with the default end
		 * clock snapshot, if any.
		 *
		 * For stream beginning, stream end, ignore.
		 */
		for (i = 0; i < user_count; i++) {
			const struct bt_message *msg = messages[i];
			int64_t msg_ns_from_origin;
			bool ignore = false;
			int ret;

			BT_ASSERT(msg);
			ret = get_message_ns_from_origin(msg, &msg_ns_from_origin,
				&ignore);
			if (ret) {
				status = BT_MESSAGE_ITERATOR_STATUS_ERROR;
				goto end;
			}

			if (ignore) {
				/* Skip message without a clock snapshot */
				continue;
			}

			if (msg_ns_from_origin >= ns_from_origin) {
				/*
				 * We found it: move this message and
				 * the following ones to the iterator's
				 * auto-seek message array.
				 */
				uint64_t j;

				for (j = i; j < user_count; j++) {
					iterator->auto_seek_msgs->pdata[j - i] =
						(void *) messages[j];
					messages[j] = NULL;
				}

				iterator->auto_seek_msg_count = user_count - i;
				goto end;
			}

			bt_object_put_no_null_check(msg);
			messages[i] = NULL;
		}
	}

end:
	for (i = 0; i < user_count; i++) {
		if (messages[i]) {
			bt_object_put_no_null_check(messages[i]);
		}
	}

	set_self_comp_port_input_msg_iterator_state(iterator, init_state);
	return status;
}

static
enum bt_self_message_iterator_status post_auto_seek_next(
		struct bt_self_component_port_input_message_iterator *iterator,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	BT_ASSERT(iterator->auto_seek_msg_count <= capacity);
	BT_ASSERT(iterator->auto_seek_msg_count > 0);

	/*
	 * Move auto-seek messages to the output array (which is this
	 * iterator's base message array.
	 */
	memcpy(&msgs[0], &iterator->auto_seek_msgs->pdata[0],
		sizeof(msgs[0]) * iterator->auto_seek_msg_count);
	memset(&iterator->auto_seek_msgs->pdata[0], 0,
		sizeof(iterator->auto_seek_msgs->pdata[0]) *
		iterator->auto_seek_msg_count);
	*count = iterator->auto_seek_msg_count;

	/* Restore real user's "next" method */
	switch (iterator->upstream_component->class->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		struct bt_component_class_source *src_comp_cls =
			(void *) iterator->upstream_component->class;

		iterator->methods.next =
			(bt_self_component_port_input_message_iterator_next_method)
				src_comp_cls->methods.msg_iter_next;
		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		struct bt_component_class_filter *flt_comp_cls =
			(void *) iterator->upstream_component->class;

		iterator->methods.next =
			(bt_self_component_port_input_message_iterator_next_method)
				flt_comp_cls->methods.msg_iter_next;
		break;
	}
	default:
		abort();
	}

	return BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
}

enum bt_message_iterator_status
bt_self_component_port_input_message_iterator_seek_ns_from_origin(
		struct bt_self_component_port_input_message_iterator *iterator,
		int64_t ns_from_origin)
{
	int status;

	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	BT_ASSERT_PRE_ITER_HAS_STATE_TO_SEEK(iterator);
	BT_ASSERT_PRE(bt_component_borrow_graph(iterator->upstream_component)->is_configured,
		"Graph is not configured: %!+g",
		bt_component_borrow_graph(iterator->upstream_component));
	BT_ASSERT_PRE(
		bt_self_component_port_input_message_iterator_can_seek_ns_from_origin(
			iterator, ns_from_origin),
		"Message iterator cannot seek nanoseconds from origin: %!+i, "
		"ns-from-origin=%" PRId64, iterator, ns_from_origin);
	set_self_comp_port_input_msg_iterator_state(iterator,
		BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_STATE_SEEKING);

	if (iterator->methods.seek_ns_from_origin) {
		BT_LIB_LOGD("Calling user's \"seek nanoseconds from origin\" method: "
			"%![iter-]+i, ns=%" PRId64, iterator, ns_from_origin);
		status = iterator->methods.seek_ns_from_origin(iterator,
			ns_from_origin);
		BT_LOGD("User method returned: status=%s",
			bt_message_iterator_status_string(status));
		BT_ASSERT_PRE(status == BT_MESSAGE_ITERATOR_STATUS_OK ||
			status == BT_MESSAGE_ITERATOR_STATUS_ERROR ||
			status == BT_MESSAGE_ITERATOR_STATUS_NOMEM ||
			status == BT_MESSAGE_ITERATOR_STATUS_AGAIN,
			"Unexpected status: %![iter-]+i, status=%s",
			iterator,
			bt_self_message_iterator_status_string(status));
	} else {
		/* Start automatic seeking: seek beginning first */
		BT_ASSERT(iterator->methods.can_seek_beginning(iterator));
		BT_ASSERT(iterator->methods.seek_beginning);
		BT_LIB_LOGD("Calling user's \"seek beginning\" method: %!+i",
			iterator);
		status = iterator->methods.seek_beginning(iterator);
		BT_LOGD("User method returned: status=%s",
			bt_message_iterator_status_string(status));
		BT_ASSERT_PRE(status == BT_MESSAGE_ITERATOR_STATUS_OK ||
			status == BT_MESSAGE_ITERATOR_STATUS_ERROR ||
			status == BT_MESSAGE_ITERATOR_STATUS_NOMEM ||
			status == BT_MESSAGE_ITERATOR_STATUS_AGAIN,
			"Unexpected status: %![iter-]+i, status=%s",
			iterator,
			bt_self_message_iterator_status_string(status));
		switch (status) {
		case BT_MESSAGE_ITERATOR_STATUS_OK:
			break;
		case BT_MESSAGE_ITERATOR_STATUS_ERROR:
		case BT_MESSAGE_ITERATOR_STATUS_NOMEM:
		case BT_MESSAGE_ITERATOR_STATUS_AGAIN:
			goto end;
		default:
			abort();
		}

		/*
		 * Find the first message which has a default clock
		 * snapshot greater than or equal to the requested
		 * nanoseconds from origin, and move the received
		 * messages from this point in the batch to this
		 * iterator's auto-seek message array.
		 */
		status = find_message_ge_ns_from_origin(iterator,
			ns_from_origin);
		switch (status) {
		case BT_MESSAGE_ITERATOR_STATUS_OK:
			/*
			 * Replace the user's "next" method with a
			 * custom, temporary "next" method which returns
			 * the messages in the iterator's message array.
			 */
			iterator->methods.next =
				(bt_self_component_port_input_message_iterator_next_method)
					post_auto_seek_next;
			break;
		case BT_MESSAGE_ITERATOR_STATUS_ERROR:
		case BT_MESSAGE_ITERATOR_STATUS_NOMEM:
		case BT_MESSAGE_ITERATOR_STATUS_AGAIN:
			goto end;
		case BT_MESSAGE_ITERATOR_STATUS_END:
			/*
			 * The iterator reached the end: just return
			 * `BT_MESSAGE_ITERATOR_STATUS_OK` here, as if
			 * the seeking operation occured: the next
			 * "next" method will return
			 * `BT_MESSAGE_ITERATOR_STATUS_END` itself.
			 */
			break;
		default:
			abort();
		}
	}

end:
	set_iterator_state_after_seeking(iterator, status);

	if (status == BT_MESSAGE_ITERATOR_STATUS_END) {
		status = BT_MESSAGE_ITERATOR_STATUS_OK;
	}

	return status;
}

static inline
bt_self_component_port_input_message_iterator *
borrow_output_port_message_iterator_upstream_iterator(
		struct bt_port_output_message_iterator *iterator)
{
	struct bt_component_class_sink_colander_priv_data *colander_data;

	BT_ASSERT(iterator);
	colander_data = (void *) iterator->colander->parent.user_data;
	BT_ASSERT(colander_data);
	BT_ASSERT(colander_data->msg_iter);
	return colander_data->msg_iter;
}

bt_bool bt_port_output_message_iterator_can_seek_ns_from_origin(
		struct bt_port_output_message_iterator *iterator,
		int64_t ns_from_origin)
{
	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	return bt_self_component_port_input_message_iterator_can_seek_ns_from_origin(
		borrow_output_port_message_iterator_upstream_iterator(
			iterator), ns_from_origin);
}

bt_bool bt_port_output_message_iterator_can_seek_beginning(
		struct bt_port_output_message_iterator *iterator)
{
	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	return bt_self_component_port_input_message_iterator_can_seek_beginning(
		borrow_output_port_message_iterator_upstream_iterator(
			iterator));
}

enum bt_message_iterator_status bt_port_output_message_iterator_seek_ns_from_origin(
		struct bt_port_output_message_iterator *iterator,
		int64_t ns_from_origin)
{
	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	return bt_self_component_port_input_message_iterator_seek_ns_from_origin(
		borrow_output_port_message_iterator_upstream_iterator(iterator),
		ns_from_origin);
}

enum bt_message_iterator_status bt_port_output_message_iterator_seek_beginning(
		struct bt_port_output_message_iterator *iterator)
{
	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	return bt_self_component_port_input_message_iterator_seek_beginning(
		borrow_output_port_message_iterator_upstream_iterator(
			iterator));
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
