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

#define BT_LOG_TAG "LIB/MSG-ITER"
#include "lib/logging.h"

#include "compat/compiler.h"
#include "compat/glib.h"
#include "lib/trace-ir/clock-class.h"
#include "lib/trace-ir/clock-snapshot.h"
#include <babeltrace2/trace-ir/field.h>
#include <babeltrace2/trace-ir/event.h>
#include "lib/trace-ir/event.h"
#include <babeltrace2/trace-ir/packet.h>
#include "lib/trace-ir/packet.h"
#include "lib/trace-ir/stream.h"
#include <babeltrace2/trace-ir/clock-class.h>
#include <babeltrace2/trace-ir/stream-class.h>
#include <babeltrace2/trace-ir/stream.h>
#include <babeltrace2/graph/connection.h>
#include <babeltrace2/graph/component.h>
#include <babeltrace2/graph/message.h>
#include <babeltrace2/graph/self-component.h>
#include <babeltrace2/graph/port.h>
#include <babeltrace2/graph/graph.h>
#include <babeltrace2/graph/message-iterator.h>
#include <babeltrace2/types.h>
#include "common/assert.h"
#include "lib/assert-pre.h"
#include "lib/assert-post.h"
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

#include "component-class.h"
#include "component.h"
#include "component-sink.h"
#include "component-source.h"
#include "connection.h"
#include "graph.h"
#include "message-iterator-class.h"
#include "message/discarded-items.h"
#include "message/event.h"
#include "message/iterator.h"
#include "message/message.h"
#include "message/message-iterator-inactivity.h"
#include "message/stream.h"
#include "message/packet.h"
#include "lib/func-status.h"

/*
 * TODO: Use graph's state (number of active iterators, etc.) and
 * possibly system specifications to make a better guess than this.
 */
#define MSG_BATCH_SIZE	15

#define BT_ASSERT_PRE_ITER_HAS_STATE_TO_SEEK(_iter)			\
	BT_ASSERT_PRE((_iter)->state == BT_MESSAGE_ITERATOR_STATE_ACTIVE || \
		(_iter)->state == BT_MESSAGE_ITERATOR_STATE_ENDED || \
		(_iter)->state == BT_MESSAGE_ITERATOR_STATE_LAST_SEEKING_RETURNED_AGAIN || \
		(_iter)->state == BT_MESSAGE_ITERATOR_STATE_LAST_SEEKING_RETURNED_ERROR, \
		"Message iterator is in the wrong state: %!+i", _iter)

static inline
void set_msg_iterator_state(struct bt_message_iterator *iterator,
		enum bt_message_iterator_state state)
{
	BT_ASSERT_DBG(iterator);
	BT_LIB_LOGD("Updating message iterator's state: new-state=%s",
		bt_message_iterator_state_string(state));
	iterator->state = state;
}

static
void bt_message_iterator_destroy(struct bt_object *obj)
{
	struct bt_message_iterator *iterator;

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
	BT_LIB_LOGI("Destroying self component input port message iterator object: "
		"%!+i", iterator);
	bt_message_iterator_try_finalize(iterator);

	if (iterator->connection) {
		/*
		 * Remove ourself from the originating connection so
		 * that it does not try to finalize a dangling pointer
		 * later.
		 */
		bt_connection_remove_iterator(iterator->connection, iterator);
		iterator->connection = NULL;
	}

	if (iterator->auto_seek.msgs) {
		while (!g_queue_is_empty(iterator->auto_seek.msgs)) {
			bt_object_put_ref_no_null_check(
				g_queue_pop_tail(iterator->auto_seek.msgs));
		}

		g_queue_free(iterator->auto_seek.msgs);
		iterator->auto_seek.msgs = NULL;
	}

	if (iterator->upstream_msg_iters) {
		/*
		 * At this point the message iterator is finalized, so
		 * it's detached from any upstream message iterator.
		 */
		BT_ASSERT(iterator->upstream_msg_iters->len == 0);
		g_ptr_array_free(iterator->upstream_msg_iters, TRUE);
		iterator->upstream_msg_iters = NULL;
	}

	if (iterator->msgs) {
		g_ptr_array_free(iterator->msgs, TRUE);
		iterator->msgs = NULL;
	}

	g_free(iterator);
}

BT_HIDDEN
void bt_message_iterator_try_finalize(
		struct bt_message_iterator *iterator)
{
	uint64_t i;
	bool call_user_finalize = true;

	BT_ASSERT(iterator);

	switch (iterator->state) {
	case BT_MESSAGE_ITERATOR_STATE_NON_INITIALIZED:
		/*
		 * If this function is called while the iterator is in the
		 * NON_INITIALIZED state, it means the user initialization
		 * method has either not been called, or has failed.  We
		 * therefore don't want to call the user finalization method.
		 * However, the initialization method might have created some
		 * upstream message iterators before failing, so we want to
		 * execute the rest of this function, which unlinks the related
		 * iterators.
		 */
		call_user_finalize = false;
		break;
	case BT_MESSAGE_ITERATOR_STATE_FINALIZED:
		/* Already finalized */
		BT_LIB_LOGD("Not finalizing message iterator: already finalized: "
			"%!+i", iterator);
		goto end;
	case BT_MESSAGE_ITERATOR_STATE_FINALIZING:
		/* Finalizing */
		BT_LIB_LOGF("Message iterator is already being finalized: "
			"%!+i", iterator);
		bt_common_abort();
	default:
		break;
	}

	BT_LIB_LOGD("Finalizing message iterator: %!+i", iterator);
	set_msg_iterator_state(iterator,
		BT_MESSAGE_ITERATOR_STATE_FINALIZING);
	BT_ASSERT(iterator->upstream_component);

	/* Call user-defined destroy method */
	if (call_user_finalize) {
		typedef void (*method_t)(void *);
		method_t method;
		struct bt_component_class *comp_class =
			iterator->upstream_component->class;
		struct bt_component_class_with_iterator_class *class_with_iter_class;

		BT_ASSERT(bt_component_class_has_message_iterator_class(comp_class));
		class_with_iter_class = container_of(comp_class,
			struct bt_component_class_with_iterator_class, parent);
		method = (method_t) class_with_iter_class->msg_iter_cls->methods.finalize;

		if (method) {
			const bt_error *saved_error;

			saved_error = bt_current_thread_take_error();

			BT_LIB_LOGD("Calling user's finalization method: %!+i",
				iterator);
			method(iterator);

			if (saved_error) {
				BT_CURRENT_THREAD_MOVE_ERROR_AND_RESET(saved_error);
			}
		}
	}

	/* Detach upstream message iterators */
	for (i = 0; i < iterator->upstream_msg_iters->len; i++) {
		struct bt_message_iterator *upstream_msg_iter =
			iterator->upstream_msg_iters->pdata[i];

		upstream_msg_iter->downstream_msg_iter = NULL;
	}

	g_ptr_array_set_size(iterator->upstream_msg_iters, 0);

	/* Detach downstream message iterator */
	if (iterator->downstream_msg_iter) {
		gboolean existed;

		BT_ASSERT(iterator->downstream_msg_iter->upstream_msg_iters);
		existed = g_ptr_array_remove_fast(
			iterator->downstream_msg_iter->upstream_msg_iters,
			iterator);
		BT_ASSERT(existed);
	}

	iterator->upstream_component = NULL;
	iterator->upstream_port = NULL;
	set_msg_iterator_state(iterator,
		BT_MESSAGE_ITERATOR_STATE_FINALIZED);
	BT_LIB_LOGD("Finalized message iterator: %!+i", iterator);

end:
	return;
}

BT_HIDDEN
void bt_message_iterator_set_connection(
		struct bt_message_iterator *iterator,
		struct bt_connection *connection)
{
	BT_ASSERT(iterator);
	iterator->connection = connection;
	BT_LIB_LOGI("Set message iterator's connection: "
		"%![iter-]+i, %![conn-]+x", iterator, connection);
}

static
enum bt_message_iterator_can_seek_beginning_status can_seek_ns_from_origin_true(
		struct bt_message_iterator *iterator,
		int64_t ns_from_origin, bt_bool *can_seek)
{
	*can_seek = BT_TRUE;

	return BT_FUNC_STATUS_OK;
}

static
enum bt_message_iterator_can_seek_beginning_status can_seek_beginning_true(
		struct bt_message_iterator *iterator,
		bt_bool *can_seek)
{
	*can_seek = BT_TRUE;

	return BT_FUNC_STATUS_OK;
}

static
int create_self_component_input_port_message_iterator(
		struct bt_self_message_iterator *self_downstream_msg_iter,
		struct bt_self_component_port_input *self_port,
		struct bt_message_iterator **message_iterator)
{
	bt_message_iterator_class_initialize_method init_method = NULL;
	struct bt_message_iterator *iterator =
		NULL;
	struct bt_message_iterator *downstream_msg_iter =
		(void *) self_downstream_msg_iter;
	struct bt_port *port = (void *) self_port;
	struct bt_port *upstream_port;
	struct bt_component *comp;
	struct bt_component *upstream_comp;
	struct bt_component_class *upstream_comp_cls;
	struct bt_component_class_with_iterator_class *upstream_comp_cls_with_iter_cls;
	int status;

	BT_ASSERT_PRE_NON_NULL(message_iterator, "Created message iterator");
	BT_ASSERT_PRE_NON_NULL(port, "Input port");
	comp = bt_port_borrow_component_inline(port);
	BT_ASSERT_PRE(bt_port_is_connected(port),
		"Input port is not connected: %![port-]+p", port);
	BT_ASSERT_PRE(comp, "Input port is not part of a component: %![port-]+p",
		port);
	BT_ASSERT(port->connection);
	upstream_port = port->connection->upstream_port;
	BT_ASSERT(upstream_port);
	upstream_comp = bt_port_borrow_component_inline(upstream_port);
	BT_ASSERT(upstream_comp);
	BT_ASSERT_PRE(
		bt_component_borrow_graph(upstream_comp)->config_state ==
			BT_GRAPH_CONFIGURATION_STATE_PARTIALLY_CONFIGURED ||
		bt_component_borrow_graph(upstream_comp)->config_state ==
			BT_GRAPH_CONFIGURATION_STATE_CONFIGURED,
		"Graph is not configured: %!+g",
		bt_component_borrow_graph(upstream_comp));
	upstream_comp_cls = upstream_comp->class;
	BT_ASSERT(upstream_comp->class->type ==
		BT_COMPONENT_CLASS_TYPE_SOURCE ||
		upstream_comp->class->type ==
		BT_COMPONENT_CLASS_TYPE_FILTER);
	BT_LIB_LOGI("Creating message iterator on self component input port: "
		"%![up-comp-]+c, %![up-port-]+p", upstream_comp, upstream_port);
	iterator = g_new0(
		struct bt_message_iterator, 1);
	if (!iterator) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one self component input port "
			"message iterator.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto error;
	}

	bt_object_init_shared(&iterator->base,
		bt_message_iterator_destroy);
	iterator->msgs = g_ptr_array_new();
	if (!iterator->msgs) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GPtrArray.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto error;
	}

	g_ptr_array_set_size(iterator->msgs, MSG_BATCH_SIZE);
	iterator->last_ns_from_origin = INT64_MIN;
	iterator->auto_seek.msgs = g_queue_new();
	if (!iterator->auto_seek.msgs) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GQueue.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto error;
	}

	iterator->upstream_msg_iters = g_ptr_array_new();
	if (!iterator->upstream_msg_iters) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GPtrArray.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto error;
	}

	iterator->upstream_component = upstream_comp;
	iterator->upstream_port = upstream_port;
	iterator->connection = iterator->upstream_port->connection;
	iterator->graph = bt_component_borrow_graph(upstream_comp);
	set_msg_iterator_state(iterator,
		BT_MESSAGE_ITERATOR_STATE_NON_INITIALIZED);

	/* Copy methods from the message iterator class to the message iterator. */
	BT_ASSERT(bt_component_class_has_message_iterator_class(upstream_comp_cls));
	upstream_comp_cls_with_iter_cls = container_of(upstream_comp_cls,
		struct bt_component_class_with_iterator_class, parent);

	iterator->methods.next =
		(bt_message_iterator_next_method)
			upstream_comp_cls_with_iter_cls->msg_iter_cls->methods.next;
	iterator->methods.seek_ns_from_origin =
		(bt_message_iterator_seek_ns_from_origin_method)
			upstream_comp_cls_with_iter_cls->msg_iter_cls->methods.seek_ns_from_origin;
	iterator->methods.seek_beginning =
		(bt_message_iterator_seek_beginning_method)
			upstream_comp_cls_with_iter_cls->msg_iter_cls->methods.seek_beginning;
	iterator->methods.can_seek_ns_from_origin =
		(bt_message_iterator_can_seek_ns_from_origin_method)
			upstream_comp_cls_with_iter_cls->msg_iter_cls->methods.can_seek_ns_from_origin;
	iterator->methods.can_seek_beginning =
		(bt_message_iterator_can_seek_beginning_method)
			upstream_comp_cls_with_iter_cls->msg_iter_cls->methods.can_seek_beginning;

	if (iterator->methods.seek_ns_from_origin &&
			!iterator->methods.can_seek_ns_from_origin) {
		iterator->methods.can_seek_ns_from_origin =
			(bt_message_iterator_can_seek_ns_from_origin_method)
				can_seek_ns_from_origin_true;
	}

	if (iterator->methods.seek_beginning &&
			!iterator->methods.can_seek_beginning) {
		iterator->methods.can_seek_beginning =
			(bt_message_iterator_can_seek_beginning_method)
				can_seek_beginning_true;
	}

	/* Call iterator's init method. */
	init_method = upstream_comp_cls_with_iter_cls->msg_iter_cls->methods.initialize;

	if (init_method) {
		enum bt_message_iterator_class_initialize_method_status iter_status;

		BT_LIB_LOGD("Calling user's initialization method: %!+i", iterator);
		iter_status = init_method(
			(struct bt_self_message_iterator *) iterator,
			&iterator->config,
			(struct bt_self_component_port_output *) upstream_port);
		BT_LOGD("User method returned: status=%s",
			bt_common_func_status_string(iter_status));
		BT_ASSERT_POST_NO_ERROR_IF_NO_ERROR_STATUS(iter_status);
		if (iter_status != BT_FUNC_STATUS_OK) {
			BT_LIB_LOGW_APPEND_CAUSE(
				"Component input port message iterator initialization method failed: "
				"%![iter-]+i, status=%s",
				iterator,
				bt_common_func_status_string(iter_status));
			status = iter_status;
			goto error;
		}

		iterator->config.frozen = true;
	}

	if (downstream_msg_iter) {
		/* Set this message iterator's downstream message iterator */
		iterator->downstream_msg_iter = downstream_msg_iter;

		/*
		 * Add this message iterator to the downstream message
		 * iterator's array of upstream message iterators.
		 */
		g_ptr_array_add(downstream_msg_iter->upstream_msg_iters,
			iterator);
	}

	set_msg_iterator_state(iterator,
		BT_MESSAGE_ITERATOR_STATE_ACTIVE);
	g_ptr_array_add(port->connection->iterators, iterator);
	BT_LIB_LOGI("Created message iterator on self component input port: "
		"%![up-port-]+p, %![up-comp-]+c, %![iter-]+i",
		upstream_port, upstream_comp, iterator);

	*message_iterator = iterator;
	status = BT_FUNC_STATUS_OK;
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(iterator);

end:
	return status;
}

bt_message_iterator_create_from_message_iterator_status
bt_message_iterator_create_from_message_iterator(
		struct bt_self_message_iterator *self_msg_iter,
		struct bt_self_component_port_input *input_port,
		struct bt_message_iterator **message_iterator)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(self_msg_iter, "Message iterator");
	return create_self_component_input_port_message_iterator(self_msg_iter,
		input_port, message_iterator);
}

bt_message_iterator_create_from_sink_component_status
bt_message_iterator_create_from_sink_component(
		struct bt_self_component_sink *self_comp,
		struct bt_self_component_port_input *input_port,
		struct bt_message_iterator **message_iterator)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(self_comp, "Sink component");
	return create_self_component_input_port_message_iterator(NULL,
		input_port, message_iterator);
}

void *bt_self_message_iterator_get_data(
		const struct bt_self_message_iterator *self_iterator)
{
	struct bt_message_iterator *iterator =
		(void *) self_iterator;

	BT_ASSERT_PRE_DEV_NON_NULL(iterator, "Message iterator");
	return iterator->user_data;
}

void bt_self_message_iterator_set_data(
		struct bt_self_message_iterator *self_iterator, void *data)
{
	struct bt_message_iterator *iterator =
		(void *) self_iterator;

	BT_ASSERT_PRE_DEV_NON_NULL(iterator, "Message iterator");
	iterator->user_data = data;
	BT_LIB_LOGD("Set message iterator's user data: "
		"%!+i, user-data-addr=%p", iterator, data);
}

void bt_self_message_iterator_configuration_set_can_seek_forward(
		bt_self_message_iterator_configuration *config,
		bt_bool can_seek_forward)
{
	BT_ASSERT_PRE_NON_NULL(config, "Message iterator configuration");
	BT_ASSERT_PRE_DEV_HOT(config, "Message iterator configuration", "");

	config->can_seek_forward = can_seek_forward;
}

/*
 * Validate that the default clock snapshot in `msg` doesn't make us go back in
 * time.
 */

BT_ASSERT_POST_DEV_FUNC
static
bool clock_snapshots_are_monotonic_one(
		struct bt_message_iterator *iterator,
		const bt_message *msg)
{
	const struct bt_clock_snapshot *clock_snapshot = NULL;
	bt_message_type message_type = bt_message_get_type(msg);
	int64_t ns_from_origin;
	enum bt_clock_snapshot_get_ns_from_origin_status clock_snapshot_status;

	/*
	 * The default is true: if we can't figure out the clock snapshot
	 * (or there is none), assume it is fine.
	 */
	bool result = true;

	switch (message_type) {
	case BT_MESSAGE_TYPE_EVENT:
	{
		struct bt_message_event *event_msg = (struct bt_message_event *) msg;
		clock_snapshot = event_msg->default_cs;
		break;
	}
	case BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY:
	{
		struct bt_message_message_iterator_inactivity *inactivity_msg =
			(struct bt_message_message_iterator_inactivity *) msg;
		clock_snapshot = inactivity_msg->cs;
		break;
	}
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
	case BT_MESSAGE_TYPE_PACKET_END:
	{
		struct bt_message_packet *packet_msg = (struct bt_message_packet *) msg;
		clock_snapshot = packet_msg->default_cs;
		break;
	}
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
	case BT_MESSAGE_TYPE_STREAM_END:
	{
		struct bt_message_stream *stream_msg = (struct bt_message_stream *) msg;
		if (stream_msg->default_cs_state != BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN) {
			goto end;
		}

		clock_snapshot = stream_msg->default_cs;
		break;
	}
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
	{
		struct bt_message_discarded_items *discarded_msg =
			(struct bt_message_discarded_items *) msg;

		clock_snapshot = discarded_msg->default_begin_cs;
		break;
	}
	}

	if (!clock_snapshot) {
		goto end;
	}

	clock_snapshot_status = bt_clock_snapshot_get_ns_from_origin(
		clock_snapshot, &ns_from_origin);
	if (clock_snapshot_status != BT_FUNC_STATUS_OK) {
		/*
		 * bt_clock_snapshot_get_ns_from_origin can return
		 * OVERFLOW_ERROR.  We don't really want to report an error to
		 * our caller, so just clear it.
		 */
		bt_current_thread_clear_error();
		goto end;
	}

	result = ns_from_origin >= iterator->last_ns_from_origin;
	iterator->last_ns_from_origin = ns_from_origin;
end:
	return result;
}

BT_ASSERT_POST_DEV_FUNC
static
bool clock_snapshots_are_monotonic(
		struct bt_message_iterator *iterator,
		bt_message_array_const msgs, uint64_t msg_count)
{
	uint64_t i;
	bool result;

	for (i = 0; i < msg_count; i++) {
		if (!clock_snapshots_are_monotonic_one(iterator, msgs[i])) {
			result = false;
			goto end;
		}
	}

	result = true;

end:
	return result;
}

/*
 * When a new stream begins, verify that the clock class tied to this
 * stream is compatible with what we've seen before.
 */

BT_ASSERT_POST_DEV_FUNC
static
bool clock_classes_are_compatible_one(struct bt_message_iterator *iterator,
		const struct bt_message *msg)
{
	enum bt_message_type message_type = bt_message_get_type(msg);
	bool result;

	if (message_type == BT_MESSAGE_TYPE_STREAM_BEGINNING) {
		const struct bt_message_stream *stream_msg = (struct bt_message_stream *) msg;
		const struct bt_clock_class *clock_class = stream_msg->stream->class->default_clock_class;
		bt_uuid clock_class_uuid = NULL;

		if (clock_class) {
			clock_class_uuid = bt_clock_class_get_uuid(clock_class);
		}

		switch (iterator->clock_expectation.type) {
		case CLOCK_EXPECTATION_UNSET:
			/*
			 * This is the first time we see a message with a clock
			 * snapshot: record the properties of that clock, against
			 * which we'll compare the clock properties of the following
			 * messages.
			 */

			if (!clock_class) {
				iterator->clock_expectation.type = CLOCK_EXPECTATION_NONE;
			} else if (bt_clock_class_origin_is_unix_epoch(clock_class)) {
				iterator->clock_expectation.type = CLOCK_EXPECTATION_ORIGIN_UNIX;
			} else if (clock_class_uuid) {
				iterator->clock_expectation.type = CLOCK_EXPECTATION_ORIGIN_OTHER_UUID;
				bt_uuid_copy(iterator->clock_expectation.uuid, clock_class_uuid);
			} else {
				iterator->clock_expectation.type = CLOCK_EXPECTATION_ORIGIN_OTHER_NO_UUID;
			}
			break;

		case CLOCK_EXPECTATION_NONE:
			if (clock_class) {
				BT_ASSERT_POST_DEV_MSG(
					"Expecting no clock class, got one: %![cc-]+K",
					clock_class);
				result = false;
				goto end;
			}

			break;

		case CLOCK_EXPECTATION_ORIGIN_UNIX:
			if (!clock_class) {
				BT_ASSERT_POST_DEV_MSG(
					"Expecting a clock class, got none.");
				result = false;
				goto end;
			}

			if (!bt_clock_class_origin_is_unix_epoch(clock_class)) {
				BT_ASSERT_POST_DEV_MSG(
					"Expecting a clock class with Unix epoch origin: %![cc-]+K",
					clock_class);
				result = false;
				goto end;
			}
			break;

		case CLOCK_EXPECTATION_ORIGIN_OTHER_UUID:
			if (!clock_class) {
				BT_ASSERT_POST_DEV_MSG(
					"Expecting a clock class, got none.");
				result = false;
				goto end;
			}

			if (bt_clock_class_origin_is_unix_epoch(clock_class)) {
				BT_ASSERT_POST_DEV_MSG(
					"Expecting a clock class without Unix epoch origin: %![cc-]+K",
					clock_class);
				result = false;
				goto end;
			}

			if (!clock_class_uuid) {
				BT_ASSERT_POST_DEV_MSG(
					"Expecting a clock class with UUID: %![cc-]+K",
					clock_class);
				result = false;
				goto end;
			}

			if (bt_uuid_compare(iterator->clock_expectation.uuid, clock_class_uuid)) {
				BT_ASSERT_POST_DEV_MSG(
					"Expecting a clock class with UUID, got one "
					"with a different UUID: %![cc-]+K, expected-uuid=%!u",
					clock_class, iterator->clock_expectation.uuid);
				result = false;
				goto end;
			}
			break;

		case CLOCK_EXPECTATION_ORIGIN_OTHER_NO_UUID:
			if (!clock_class) {
				BT_ASSERT_POST_DEV_MSG(
					"Expecting a clock class, got none.");
				result = false;
				goto end;
			}

			if (bt_clock_class_origin_is_unix_epoch(clock_class)) {
				BT_ASSERT_POST_DEV_MSG(
					"Expecting a clock class without Unix epoch origin: %![cc-]+K",
					clock_class);
				result = false;
				goto end;
			}

			if (clock_class_uuid) {
				BT_ASSERT_POST_DEV_MSG(
					"Expecting a clock class without UUID: %![cc-]+K",
					clock_class);
				result = false;
				goto end;
			}
			break;
		}
	}

	result = true;

end:
	return result;
}

BT_ASSERT_POST_DEV_FUNC
static
bool clock_classes_are_compatible(
		struct bt_message_iterator *iterator,
		bt_message_array_const msgs, uint64_t msg_count)
{
	uint64_t i;
	bool result;

	for (i = 0; i < msg_count; i++) {
		if (!clock_classes_are_compatible_one(iterator, msgs[i])) {
			result = false;
			goto end;
		}
	}

	result = true;

end:
	return result;
}

/*
 * Call the `next` method of the iterator.  Do some validation on the returned
 * messages.
 */

static
enum bt_message_iterator_class_next_method_status
call_iterator_next_method(
		struct bt_message_iterator *iterator,
		bt_message_array_const msgs, uint64_t capacity, uint64_t *user_count)
{
	enum bt_message_iterator_class_next_method_status status;

	BT_ASSERT_DBG(iterator->methods.next);
	BT_LOGD_STR("Calling user's \"next\" method.");
	status = iterator->methods.next(iterator, msgs, capacity, user_count);
	BT_LOGD("User method returned: status=%s, msg-count=%" PRIu64,
		bt_common_func_status_string(status), *user_count);

	if (status == BT_FUNC_STATUS_OK) {
		BT_ASSERT_POST_DEV(clock_classes_are_compatible(iterator, msgs, *user_count),
			"Clocks are not compatible");
		BT_ASSERT_POST_DEV(clock_snapshots_are_monotonic(iterator, msgs, *user_count),
			"Clock snapshots are not monotonic");
	}

	BT_ASSERT_POST_DEV_NO_ERROR_IF_NO_ERROR_STATUS(status);

	return status;
}

enum bt_message_iterator_next_status
bt_message_iterator_next(
		struct bt_message_iterator *iterator,
		bt_message_array_const *msgs, uint64_t *user_count)
{
	enum bt_message_iterator_next_status status = BT_FUNC_STATUS_OK;

	BT_ASSERT_PRE_DEV_NO_ERROR();
	BT_ASSERT_PRE_DEV_NON_NULL(iterator, "Message iterator");
	BT_ASSERT_PRE_DEV_NON_NULL(msgs, "Message array (output)");
	BT_ASSERT_PRE_DEV_NON_NULL(user_count, "Message count (output)");
	BT_ASSERT_PRE_DEV(iterator->state ==
		BT_MESSAGE_ITERATOR_STATE_ACTIVE,
		"Message iterator's \"next\" called, but "
		"message iterator is in the wrong state: %!+i", iterator);
	BT_ASSERT_DBG(iterator->upstream_component);
	BT_ASSERT_DBG(iterator->upstream_component->class);
	BT_ASSERT_PRE_DEV(
		bt_component_borrow_graph(iterator->upstream_component)->config_state !=
			BT_GRAPH_CONFIGURATION_STATE_CONFIGURING,
		"Graph is not configured: %!+g",
		bt_component_borrow_graph(iterator->upstream_component));
	BT_LIB_LOGD("Getting next self component input port "
		"message iterator's messages: %!+i, batch-size=%u",
		iterator, MSG_BATCH_SIZE);

	/*
	 * Call the user's "next" method to get the next messages
	 * and status.
	 */
	*user_count = 0;
	status = (int) call_iterator_next_method(iterator,
		(void *) iterator->msgs->pdata, MSG_BATCH_SIZE,
		user_count);
	BT_LOGD("User method returned: status=%s, msg-count=%" PRIu64,
		bt_common_func_status_string(status), *user_count);
	if (status < 0) {
		BT_LIB_LOGW_APPEND_CAUSE(
			"Component input port message iterator's \"next\" method failed: "
			"%![iter-]+i, status=%s",
			iterator, bt_common_func_status_string(status));
		goto end;
	}

	/*
	 * There is no way that this iterator could have been finalized
	 * during its "next" method, as the only way to do this is to
	 * put the last iterator's reference, and this can only be done
	 * by its downstream owner.
	 *
	 * For the same reason, there is no way that this iterator could
	 * have seeked (cannot seek a self message iterator).
	 */
	BT_ASSERT_DBG(iterator->state ==
		BT_MESSAGE_ITERATOR_STATE_ACTIVE);

	switch (status) {
	case BT_FUNC_STATUS_OK:
		BT_ASSERT_POST_DEV(*user_count <= MSG_BATCH_SIZE,
			"Invalid returned message count: greater than "
			"batch size: count=%" PRIu64 ", batch-size=%u",
			*user_count, MSG_BATCH_SIZE);
		*msgs = (void *) iterator->msgs->pdata;
		break;
	case BT_FUNC_STATUS_AGAIN:
		goto end;
	case BT_FUNC_STATUS_END:
		set_msg_iterator_state(iterator,
			BT_MESSAGE_ITERATOR_STATE_ENDED);
		goto end;
	default:
		/* Unknown non-error status */
		bt_common_abort();
	}

end:
	return status;
}

struct bt_component *
bt_message_iterator_borrow_component(
		struct bt_message_iterator *iterator)
{
	BT_ASSERT_PRE_DEV_NON_NULL(iterator, "Message iterator");
	return iterator->upstream_component;
}

struct bt_self_component *bt_self_message_iterator_borrow_component(
		struct bt_self_message_iterator *self_iterator)
{
	struct bt_message_iterator *iterator =
		(void *) self_iterator;

	BT_ASSERT_PRE_DEV_NON_NULL(iterator, "Message iterator");
	return (void *) iterator->upstream_component;
}

struct bt_self_component_port_output *bt_self_message_iterator_borrow_port(
		struct bt_self_message_iterator *self_iterator)
{
	struct bt_message_iterator *iterator =
		(void *) self_iterator;

	BT_ASSERT_PRE_DEV_NON_NULL(iterator, "Message iterator");
	return (void *) iterator->upstream_port;
}

enum bt_message_iterator_can_seek_ns_from_origin_status
bt_message_iterator_can_seek_ns_from_origin(
		struct bt_message_iterator *iterator,
		int64_t ns_from_origin, bt_bool *can_seek)
{
	enum bt_message_iterator_can_seek_ns_from_origin_status status;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	BT_ASSERT_PRE_NON_NULL(can_seek, "Result (output)");
	BT_ASSERT_PRE_ITER_HAS_STATE_TO_SEEK(iterator);
	BT_ASSERT_PRE(
		bt_component_borrow_graph(iterator->upstream_component)->config_state !=
			BT_GRAPH_CONFIGURATION_STATE_CONFIGURING,
		"Graph is not configured: %!+g",
		bt_component_borrow_graph(iterator->upstream_component));

	if (iterator->methods.can_seek_ns_from_origin) {
		/*
		 * Initialize to an invalid value, so we can post-assert that
		 * the method returned a valid value.
		 */
		*can_seek = -1;

		BT_LIB_LOGD("Calling user's \"can seek nanoseconds from origin\" method: %!+i",
			iterator);

		status = (int) iterator->methods.can_seek_ns_from_origin(iterator,
			ns_from_origin, can_seek);

		BT_ASSERT_POST_NO_ERROR_IF_NO_ERROR_STATUS(status);

		if (status != BT_FUNC_STATUS_OK) {
			BT_LIB_LOGW_APPEND_CAUSE(
				"Component input port message iterator's \"can seek nanoseconds from origin\" method failed: "
				"%![iter-]+i, status=%s",
				iterator, bt_common_func_status_string(status));
			goto end;
		}

		BT_ASSERT_POST(*can_seek == BT_TRUE || *can_seek == BT_FALSE,
			"Unexpected boolean value returned from user's \"can seek ns from origin\" method: val=%d, %![iter-]+i",
			*can_seek, iterator);

		BT_LIB_LOGD(
			"User's \"can seek nanoseconds from origin\" returned successfully: "
			"%![iter-]+i, can-seek=%d",
			iterator, *can_seek);

		if (*can_seek) {
			goto end;
		}
	}

	/*
	 * Automatic seeking fall back: if we can seek to the beginning and the
	 * iterator supports forward seeking then we can automatically seek to
	 * any timestamp.
	 */
	status = (int) bt_message_iterator_can_seek_beginning(
		iterator, can_seek);
	if (status != BT_FUNC_STATUS_OK) {
		goto end;
	}

	*can_seek = *can_seek && iterator->config.can_seek_forward;

end:
	return status;
}

enum bt_message_iterator_can_seek_beginning_status
bt_message_iterator_can_seek_beginning(
		struct bt_message_iterator *iterator,
		bt_bool *can_seek)
{
	enum bt_message_iterator_can_seek_beginning_status status;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	BT_ASSERT_PRE_NON_NULL(can_seek, "Result (output)");
	BT_ASSERT_PRE_ITER_HAS_STATE_TO_SEEK(iterator);
	BT_ASSERT_PRE(
		bt_component_borrow_graph(iterator->upstream_component)->config_state !=
			BT_GRAPH_CONFIGURATION_STATE_CONFIGURING,
		"Graph is not configured: %!+g",
		bt_component_borrow_graph(iterator->upstream_component));

	if (iterator->methods.can_seek_beginning) {
		/*
		 * Initialize to an invalid value, so we can post-assert that
		 * the method returned a valid value.
		 */
		*can_seek = -1;

		status = (int) iterator->methods.can_seek_beginning(iterator, can_seek);

		BT_ASSERT_POST(
			status != BT_FUNC_STATUS_OK ||
				*can_seek == BT_TRUE ||
				*can_seek == BT_FALSE,
			"Unexpected boolean value returned from user's \"can seek beginning\" method: val=%d, %![iter-]+i",
			*can_seek, iterator);
		BT_ASSERT_POST_NO_ERROR_IF_NO_ERROR_STATUS(status);
	} else {
		*can_seek = BT_FALSE;
		status = BT_FUNC_STATUS_OK;
	}

	return status;
}

static inline
void set_iterator_state_after_seeking(
		struct bt_message_iterator *iterator,
		int status)
{
	enum bt_message_iterator_state new_state = 0;

	/* Set iterator's state depending on seeking status */
	switch (status) {
	case BT_FUNC_STATUS_OK:
		new_state = BT_MESSAGE_ITERATOR_STATE_ACTIVE;
		break;
	case BT_FUNC_STATUS_AGAIN:
		new_state = BT_MESSAGE_ITERATOR_STATE_LAST_SEEKING_RETURNED_AGAIN;
		break;
	case BT_FUNC_STATUS_ERROR:
	case BT_FUNC_STATUS_MEMORY_ERROR:
		new_state = BT_MESSAGE_ITERATOR_STATE_LAST_SEEKING_RETURNED_ERROR;
		break;
	case BT_FUNC_STATUS_END:
		new_state = BT_MESSAGE_ITERATOR_STATE_ENDED;
		break;
	default:
		bt_common_abort();
	}

	set_msg_iterator_state(iterator, new_state);
}

static
void reset_iterator_expectations(
		struct bt_message_iterator *iterator)
{
	iterator->last_ns_from_origin = INT64_MIN;
	iterator->clock_expectation.type = CLOCK_EXPECTATION_UNSET;
}

static
bool message_iterator_can_seek_beginning(
		struct bt_message_iterator *iterator)
{
	enum bt_message_iterator_can_seek_beginning_status status;
	bt_bool can_seek;

	status = bt_message_iterator_can_seek_beginning(
		iterator, &can_seek);
	if (status != BT_FUNC_STATUS_OK) {
		can_seek = BT_FALSE;
	}

	return can_seek;
}

enum bt_message_iterator_seek_beginning_status
bt_message_iterator_seek_beginning(
		struct bt_message_iterator *iterator)
{
	int status;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	BT_ASSERT_PRE_ITER_HAS_STATE_TO_SEEK(iterator);
	BT_ASSERT_PRE(
		bt_component_borrow_graph(iterator->upstream_component)->config_state !=
			BT_GRAPH_CONFIGURATION_STATE_CONFIGURING,
		"Graph is not configured: %!+g",
		bt_component_borrow_graph(iterator->upstream_component));
	BT_ASSERT_PRE(message_iterator_can_seek_beginning(iterator),
		"Message iterator cannot seek beginning: %!+i", iterator);

	/*
	 * We are seeking, reset our expectations about how the following
	 * messages should look like.
	 */
	reset_iterator_expectations(iterator);

	BT_LIB_LOGD("Calling user's \"seek beginning\" method: %!+i", iterator);
	set_msg_iterator_state(iterator,
		BT_MESSAGE_ITERATOR_STATE_SEEKING);
	status = iterator->methods.seek_beginning(iterator);
	BT_LOGD("User method returned: status=%s",
		bt_common_func_status_string(status));
	BT_ASSERT_POST(status == BT_FUNC_STATUS_OK ||
		status == BT_FUNC_STATUS_ERROR ||
		status == BT_FUNC_STATUS_MEMORY_ERROR ||
		status == BT_FUNC_STATUS_AGAIN,
		"Unexpected status: %![iter-]+i, status=%s",
		iterator, bt_common_func_status_string(status));
	BT_ASSERT_POST_NO_ERROR_IF_NO_ERROR_STATUS(status);
	if (status < 0) {
		BT_LIB_LOGW_APPEND_CAUSE(
			"Component input port message iterator's \"seek beginning\" method failed: "
			"%![iter-]+i, status=%s",
			iterator, bt_common_func_status_string(status));
	}

	set_iterator_state_after_seeking(iterator, status);
	return status;
}

bt_bool
bt_message_iterator_can_seek_forward(
		bt_message_iterator *iterator)
{
	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");

	return iterator->config.can_seek_forward;
}

/*
 * Structure used to record the state of a given stream during the fast-forward
 * phase of an auto-seek.
 */
struct auto_seek_stream_state {
	/*
	 * Value representing which step of this timeline we are at.
	 *
	 *      time --->
	 *   [SB]  1  [PB]  2  [PE]  1  [SE]
	 *
	 * At each point in the timeline, the messages we need to replicate are:
	 *
	 *   1: Stream beginning
	 *   2: Stream beginning, packet beginning
	 *
	 * Before "Stream beginning" and after "Stream end", we don't need to
	 * replicate anything as the stream doesn't exist.
	 */
	enum {
		AUTO_SEEK_STREAM_STATE_STREAM_BEGAN,
		AUTO_SEEK_STREAM_STATE_PACKET_BEGAN,
	} state;

	/*
	 * If `state` is AUTO_SEEK_STREAM_STATE_PACKET_BEGAN, the packet we are
	 * in.  This is a weak reference, since the packet will always be
	 * alive by the time we use it.
	 */
	struct bt_packet *packet;

	/* Have we see a message with a clock snapshot yet? */
	bool seen_clock_snapshot;
};

static
struct auto_seek_stream_state *create_auto_seek_stream_state(void)
{
	return g_new0(struct auto_seek_stream_state, 1);
}

static
void destroy_auto_seek_stream_state(void *ptr)
{
	g_free(ptr);
}

static
GHashTable *create_auto_seek_stream_states(void)
{
	return g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL,
		destroy_auto_seek_stream_state);
}

static
void destroy_auto_seek_stream_states(GHashTable *stream_states)
{
	g_hash_table_destroy(stream_states);
}

/*
 * Handle one message while we are in the fast-forward phase of an auto-seek.
 *
 * Sets `*got_first` to true if the message's timestamp is greater or equal to
 * `ns_from_origin`.  In other words, if this is the first message after our
 * seek point.
 *
 * `stream_states` is an hash table of `bt_stream *` (weak reference) to
 * `struct auto_seek_stream_state` used to keep the state of each stream
 * during the fast-forward.
 */

static inline
int auto_seek_handle_message(
		struct bt_message_iterator *iterator,
		int64_t ns_from_origin, const struct bt_message *msg,
		bool *got_first, GHashTable *stream_states)
{
	int status = BT_FUNC_STATUS_OK;
	int64_t msg_ns_from_origin;
	const struct bt_clock_snapshot *clk_snapshot = NULL;
	int ret;

	BT_ASSERT_DBG(msg);
	BT_ASSERT_DBG(got_first);

	switch (msg->type) {
	case BT_MESSAGE_TYPE_EVENT:
	{
		const struct bt_message_event *event_msg =
			(const void *) msg;

		clk_snapshot = event_msg->default_cs;
		BT_ASSERT_POST_DEV(clk_snapshot,
			"Event message has no default clock snapshot: %!+n",
			event_msg);
		break;
	}
	case BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY:
	{
		const struct bt_message_message_iterator_inactivity *inactivity_msg =
			(const void *) msg;

		clk_snapshot = inactivity_msg->cs;
		BT_ASSERT_DBG(clk_snapshot);
		break;
	}
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
	case BT_MESSAGE_TYPE_PACKET_END:
	{
		const struct bt_message_packet *packet_msg =
			(const void *) msg;

		clk_snapshot = packet_msg->default_cs;
		BT_ASSERT_POST_DEV(clk_snapshot,
			"Packet message has no default clock snapshot: %!+n",
			packet_msg);
		break;
	}
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
	{
		struct bt_message_discarded_items *msg_disc_items =
			(void *) msg;

		BT_ASSERT_POST_DEV(msg_disc_items->default_begin_cs &&
			msg_disc_items->default_end_cs,
			"Discarded events/packets message has no default clock snapshots: %!+n",
			msg_disc_items);
		ret = bt_clock_snapshot_get_ns_from_origin(
			msg_disc_items->default_begin_cs,
			&msg_ns_from_origin);
		if (ret) {
			status = BT_FUNC_STATUS_ERROR;
			goto end;
		}

		if (msg_ns_from_origin >= ns_from_origin) {
			*got_first = true;
			goto push_msg;
		}

		ret = bt_clock_snapshot_get_ns_from_origin(
			msg_disc_items->default_end_cs,
			&msg_ns_from_origin);
		if (ret) {
			status = BT_FUNC_STATUS_ERROR;
			goto end;
		}

		if (msg_ns_from_origin >= ns_from_origin) {
			/*
			 * The discarded items message's beginning time
			 * is before the requested seeking time, but its
			 * end time is after. Modify the message so as
			 * to set its beginning time to the requested
			 * seeking time, and make its item count unknown
			 * as we don't know if items were really
			 * discarded within the new time range.
			 */
			uint64_t new_begin_raw_value = 0;

			ret = bt_clock_class_clock_value_from_ns_from_origin(
				msg_disc_items->default_end_cs->clock_class,
				ns_from_origin, &new_begin_raw_value);
			if (ret) {
				status = BT_FUNC_STATUS_ERROR;
				goto end;
			}

			bt_clock_snapshot_set_raw_value(
				msg_disc_items->default_begin_cs,
				new_begin_raw_value);
			msg_disc_items->count.base.avail =
				BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE;

			/*
			 * It is safe to push it because its beginning
			 * time is exactly the requested seeking time.
			 */
			goto push_msg;
		} else {
			goto skip_msg;
		}
	}
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
	case BT_MESSAGE_TYPE_STREAM_END:
	{
		struct bt_message_stream *stream_msg =
			(struct bt_message_stream *) msg;

		if (stream_msg->default_cs_state != BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN) {
			/* Ignore */
			goto skip_msg;
		}

		clk_snapshot = stream_msg->default_cs;
		break;
	}
	default:
		bt_common_abort();
	}

	BT_ASSERT_DBG(clk_snapshot);
	ret = bt_clock_snapshot_get_ns_from_origin(clk_snapshot,
		&msg_ns_from_origin);
	if (ret) {
		status = BT_FUNC_STATUS_ERROR;
		goto end;
	}

	if (msg_ns_from_origin >= ns_from_origin) {
		*got_first = true;
		goto push_msg;
	}

skip_msg:
	/* This message won't be sent downstream. */
	switch (msg->type) {
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
	{
		const struct bt_message_stream *stream_msg = (const void *) msg;
		struct auto_seek_stream_state *stream_state;

		/* Update stream's state: stream began. */
		stream_state = create_auto_seek_stream_state();
		if (!stream_state) {
			status = BT_FUNC_STATUS_MEMORY_ERROR;
			goto end;
		}

		stream_state->state = AUTO_SEEK_STREAM_STATE_STREAM_BEGAN;

		if (stream_msg->default_cs_state == BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN) {
			stream_state->seen_clock_snapshot = true;
		}

		BT_ASSERT_DBG(!bt_g_hash_table_contains(stream_states, stream_msg->stream));
		g_hash_table_insert(stream_states, stream_msg->stream, stream_state);
		break;
	}
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
	{
		const struct bt_message_packet *packet_msg =
			(const void *) msg;
		struct auto_seek_stream_state *stream_state;

		/* Update stream's state: packet began. */
		stream_state = g_hash_table_lookup(stream_states, packet_msg->packet->stream);
		BT_ASSERT_DBG(stream_state);
		BT_ASSERT_DBG(stream_state->state == AUTO_SEEK_STREAM_STATE_STREAM_BEGAN);
		stream_state->state = AUTO_SEEK_STREAM_STATE_PACKET_BEGAN;
		BT_ASSERT_DBG(!stream_state->packet);
		stream_state->packet = packet_msg->packet;

		if (packet_msg->packet->stream->class->packets_have_beginning_default_clock_snapshot) {
			stream_state->seen_clock_snapshot = true;
		}

		break;
	}
	case BT_MESSAGE_TYPE_EVENT:
	{
		const struct bt_message_event *event_msg = (const void *) msg;
		struct auto_seek_stream_state *stream_state;

		stream_state = g_hash_table_lookup(stream_states,
			event_msg->event->stream);
		BT_ASSERT_DBG(stream_state);

		// HELPME: are we sure that event messages have clock snapshots at this point?
		stream_state->seen_clock_snapshot = true;

		break;
	}
	case BT_MESSAGE_TYPE_PACKET_END:
	{
		const struct bt_message_packet *packet_msg =
			(const void *) msg;
		struct auto_seek_stream_state *stream_state;

		/* Update stream's state: packet ended. */
		stream_state = g_hash_table_lookup(stream_states, packet_msg->packet->stream);
		BT_ASSERT_DBG(stream_state);
		BT_ASSERT_DBG(stream_state->state == AUTO_SEEK_STREAM_STATE_PACKET_BEGAN);
		stream_state->state = AUTO_SEEK_STREAM_STATE_STREAM_BEGAN;
		BT_ASSERT_DBG(stream_state->packet);
		stream_state->packet = NULL;

		if (packet_msg->packet->stream->class->packets_have_end_default_clock_snapshot) {
			stream_state->seen_clock_snapshot = true;
		}

		break;
	}
	case BT_MESSAGE_TYPE_STREAM_END:
	{
		const struct bt_message_stream *stream_msg = (const void *) msg;
		struct auto_seek_stream_state *stream_state;

		stream_state = g_hash_table_lookup(stream_states, stream_msg->stream);
		BT_ASSERT_DBG(stream_state);
		BT_ASSERT_DBG(stream_state->state == AUTO_SEEK_STREAM_STATE_STREAM_BEGAN);
		BT_ASSERT_DBG(!stream_state->packet);

		/* Update stream's state: this stream doesn't exist anymore. */
		g_hash_table_remove(stream_states, stream_msg->stream);
		break;
	}
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
	{
		const struct bt_message_discarded_items *discarded_msg =
			(const void *) msg;
		struct auto_seek_stream_state *stream_state;

		stream_state = g_hash_table_lookup(stream_states, discarded_msg->stream);
		BT_ASSERT_DBG(stream_state);

		if ((msg->type == BT_MESSAGE_TYPE_DISCARDED_EVENTS && discarded_msg->stream->class->discarded_events_have_default_clock_snapshots) ||
			(msg->type == BT_MESSAGE_TYPE_DISCARDED_PACKETS && discarded_msg->stream->class->discarded_packets_have_default_clock_snapshots)) {
			stream_state->seen_clock_snapshot = true;
		}

		break;
	}
	default:
		break;
	}

	bt_object_put_ref_no_null_check(msg);
	msg = NULL;
	goto end;

push_msg:
	g_queue_push_tail(iterator->auto_seek.msgs, (void *) msg);
	msg = NULL;

end:
	BT_ASSERT_DBG(!msg || status != BT_FUNC_STATUS_OK);
	return status;
}

static
int find_message_ge_ns_from_origin(
		struct bt_message_iterator *iterator,
		int64_t ns_from_origin, GHashTable *stream_states)
{
	int status = BT_FUNC_STATUS_OK;
	enum bt_message_iterator_state init_state =
		iterator->state;
	const struct bt_message *messages[MSG_BATCH_SIZE];
	uint64_t user_count = 0;
	uint64_t i;
	bool got_first = false;

	BT_ASSERT_DBG(iterator);
	memset(&messages[0], 0, sizeof(messages[0]) * MSG_BATCH_SIZE);

	/*
	 * Make this iterator temporarily active (not seeking) to call
	 * the "next" method.
	 */
	set_msg_iterator_state(iterator,
		BT_MESSAGE_ITERATOR_STATE_ACTIVE);

	BT_ASSERT_DBG(iterator->methods.next);

	while (!got_first) {
		/*
		 * Call the user's "next" method to get the next
		 * messages and status.
		 */
		status = call_iterator_next_method(iterator,
			&messages[0], MSG_BATCH_SIZE, &user_count);
		BT_LOGD("User method returned: status=%s",
			bt_common_func_status_string(status));
		if (status < 0) {
			BT_LIB_LOGW_APPEND_CAUSE(
				"Component input port message iterator's \"next\" method failed: "
				"%![iter-]+i, status=%s",
				iterator, bt_common_func_status_string(status));
		}

		/*
		 * The user's "next" method must not do any action which
		 * would change the iterator's state.
		 */
		BT_ASSERT_DBG(iterator->state ==
			BT_MESSAGE_ITERATOR_STATE_ACTIVE);

		switch (status) {
		case BT_FUNC_STATUS_OK:
			BT_ASSERT_POST_DEV(user_count <= MSG_BATCH_SIZE,
				"Invalid returned message count: greater than "
				"batch size: count=%" PRIu64 ", batch-size=%u",
				user_count, MSG_BATCH_SIZE);
			break;
		case BT_FUNC_STATUS_AGAIN:
		case BT_FUNC_STATUS_ERROR:
		case BT_FUNC_STATUS_MEMORY_ERROR:
		case BT_FUNC_STATUS_END:
			goto end;
		default:
			bt_common_abort();
		}

		for (i = 0; i < user_count; i++) {
			if (got_first) {
				g_queue_push_tail(iterator->auto_seek.msgs,
					(void *) messages[i]);
				messages[i] = NULL;
				continue;
			}

			status = auto_seek_handle_message(iterator,
				ns_from_origin, messages[i], &got_first,
				stream_states);
			if (status == BT_FUNC_STATUS_OK) {
				/* Message was either pushed or moved */
				messages[i] = NULL;
			} else {
				goto end;
			}
		}
	}

end:
	for (i = 0; i < user_count; i++) {
		if (messages[i]) {
			bt_object_put_ref_no_null_check(messages[i]);
		}
	}

	set_msg_iterator_state(iterator, init_state);
	return status;
}

/*
 * This function is installed as the iterator's next callback after we have
 * auto-seeked (seeked to the beginning and fast-forwarded) to send the
 * messages saved in iterator->auto_seek.msgs.  Once this is done, the original
 * next callback is put back.
 */

static
enum bt_message_iterator_class_next_method_status post_auto_seek_next(
		struct bt_message_iterator *iterator,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	BT_ASSERT(!g_queue_is_empty(iterator->auto_seek.msgs));
	*count = 0;

	/*
	 * Move auto-seek messages to the output array (which is this
	 * iterator's base message array).
	 */
	while (capacity > 0 && !g_queue_is_empty(iterator->auto_seek.msgs)) {
		msgs[*count] = g_queue_pop_head(iterator->auto_seek.msgs);
		capacity--;
		(*count)++;
	}

	BT_ASSERT(*count > 0);

	if (g_queue_is_empty(iterator->auto_seek.msgs)) {
		/* No more auto-seek messages, restore user's next callback. */
		BT_ASSERT(iterator->auto_seek.original_next_callback);
		iterator->methods.next = iterator->auto_seek.original_next_callback;
		iterator->auto_seek.original_next_callback = NULL;
	}

	return BT_FUNC_STATUS_OK;
}

static inline
int clock_raw_value_from_ns_from_origin(const bt_clock_class *clock_class,
		int64_t ns_from_origin, uint64_t *raw_value)
{

	int64_t cc_offset_s = clock_class->offset_seconds;
	uint64_t cc_offset_cycles = clock_class->offset_cycles;
	uint64_t cc_freq = clock_class->frequency;

	return bt_common_clock_value_from_ns_from_origin(cc_offset_s,
		cc_offset_cycles, cc_freq, ns_from_origin, raw_value);
}

static
bool message_iterator_can_seek_ns_from_origin(
		struct bt_message_iterator *iterator,
		int64_t ns_from_origin)
{
	enum bt_message_iterator_can_seek_ns_from_origin_status status;
	bt_bool can_seek;

	status = bt_message_iterator_can_seek_ns_from_origin(
		iterator, ns_from_origin, &can_seek);
	if (status != BT_FUNC_STATUS_OK) {
		can_seek = BT_FALSE;
	}

	return can_seek;
}

enum bt_message_iterator_seek_ns_from_origin_status
bt_message_iterator_seek_ns_from_origin(
		struct bt_message_iterator *iterator,
		int64_t ns_from_origin)
{
	int status;
	GHashTable *stream_states = NULL;
	bt_bool can_seek_by_itself;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	BT_ASSERT_PRE_ITER_HAS_STATE_TO_SEEK(iterator);
	BT_ASSERT_PRE(
		bt_component_borrow_graph(iterator->upstream_component)->config_state !=
			BT_GRAPH_CONFIGURATION_STATE_CONFIGURING,
		"Graph is not configured: %!+g",
		bt_component_borrow_graph(iterator->upstream_component));
	/* The iterator must be able to seek ns from origin one way or another. */
	BT_ASSERT_PRE(
		message_iterator_can_seek_ns_from_origin(iterator, ns_from_origin),
		"Message iterator cannot seek nanoseconds from origin: %!+i, "
		"ns-from-origin=%" PRId64, iterator, ns_from_origin);
	set_msg_iterator_state(iterator,
		BT_MESSAGE_ITERATOR_STATE_SEEKING);

	/*
	 * We are seeking, reset our expectations about how the following
	 * messages should look like.
	 */
	reset_iterator_expectations(iterator);

	/* Check if the iterator can seek by itself.  If not we'll use autoseek. */
	if (iterator->methods.can_seek_ns_from_origin) {
		bt_message_iterator_class_can_seek_ns_from_origin_method_status
			can_seek_status;

		can_seek_status =
			iterator->methods.can_seek_ns_from_origin(
				iterator, ns_from_origin, &can_seek_by_itself);
		if (can_seek_status != BT_FUNC_STATUS_OK) {
			status = can_seek_status;
			goto end;
		}
	} else {
		can_seek_by_itself = false;
	}

	if (can_seek_by_itself) {
		/* The iterator knows how to seek to a particular time, let it handle this. */
		BT_ASSERT(iterator->methods.seek_ns_from_origin);
		BT_LIB_LOGD("Calling user's \"seek nanoseconds from origin\" method: "
			"%![iter-]+i, ns=%" PRId64, iterator, ns_from_origin);
		status = iterator->methods.seek_ns_from_origin(iterator,
			ns_from_origin);
		BT_LOGD("User method returned: status=%s",
			bt_common_func_status_string(status));
		BT_ASSERT_POST(status == BT_FUNC_STATUS_OK ||
			status == BT_FUNC_STATUS_ERROR ||
			status == BT_FUNC_STATUS_MEMORY_ERROR ||
			status == BT_FUNC_STATUS_AGAIN,
			"Unexpected status: %![iter-]+i, status=%s",
			iterator, bt_common_func_status_string(status));
		BT_ASSERT_POST_NO_ERROR_IF_NO_ERROR_STATUS(status);
		if (status < 0) {
			BT_LIB_LOGW_APPEND_CAUSE(
				"Component input port message iterator's \"seek nanoseconds from origin\" method failed: "
				"%![iter-]+i, status=%s",
				iterator, bt_common_func_status_string(status));
		}
	} else {
		/*
		 * The iterator doesn't know how to seek by itself to a
		 * particular time.  We will seek to the beginning and fast
		 * forward to the right place.
		 */
		enum bt_message_iterator_class_can_seek_beginning_method_status can_seek_status;
		bt_bool can_seek_beginning;

		can_seek_status = iterator->methods.can_seek_beginning(iterator,
			&can_seek_beginning);
		BT_ASSERT(can_seek_status == BT_FUNC_STATUS_OK);
		BT_ASSERT(can_seek_beginning);
		BT_ASSERT(iterator->methods.seek_beginning);
		BT_LIB_LOGD("Calling user's \"seek beginning\" method: %!+i",
			iterator);
		status = iterator->methods.seek_beginning(iterator);
		BT_LOGD("User method returned: status=%s",
			bt_common_func_status_string(status));
		BT_ASSERT_POST(status == BT_FUNC_STATUS_OK ||
			status == BT_FUNC_STATUS_ERROR ||
			status == BT_FUNC_STATUS_MEMORY_ERROR ||
			status == BT_FUNC_STATUS_AGAIN,
			"Unexpected status: %![iter-]+i, status=%s",
			iterator, bt_common_func_status_string(status));
		if (status < 0) {
			BT_LIB_LOGW_APPEND_CAUSE(
				"Component input port message iterator's \"seek beginning\" method failed: "
				"%![iter-]+i, status=%s",
				iterator, bt_common_func_status_string(status));
		}

		switch (status) {
		case BT_FUNC_STATUS_OK:
			break;
		case BT_FUNC_STATUS_ERROR:
		case BT_FUNC_STATUS_MEMORY_ERROR:
		case BT_FUNC_STATUS_AGAIN:
			goto end;
		default:
			bt_common_abort();
		}

		/*
		 * Find the first message which has a default clock
		 * snapshot greater than or equal to the requested
		 * seeking time, and move the received messages from
		 * this point in the batch to this iterator's auto-seek
		 * message queue.
		 */
		while (!g_queue_is_empty(iterator->auto_seek.msgs)) {
			bt_object_put_ref_no_null_check(
				g_queue_pop_tail(iterator->auto_seek.msgs));
		}

		stream_states = create_auto_seek_stream_states();
		if (!stream_states) {
			BT_LIB_LOGE_APPEND_CAUSE(
				"Failed to allocate one GHashTable.");
			status = BT_FUNC_STATUS_MEMORY_ERROR;
			goto end;
		}

		status = find_message_ge_ns_from_origin(iterator,
			ns_from_origin, stream_states);
		switch (status) {
		case BT_FUNC_STATUS_OK:
		case BT_FUNC_STATUS_END:
		{
			GHashTableIter iter;
			gpointer key, value;

			/*
			 * If some streams exist at the seek time, prepend the
			 * required messages to put those streams in the right
			 * state.
			 */
			g_hash_table_iter_init(&iter, stream_states);
			while (g_hash_table_iter_next (&iter, &key, &value)) {
				const bt_stream *stream = key;
				struct auto_seek_stream_state *stream_state =
					(struct auto_seek_stream_state *) value;
				bt_message *msg;
				const bt_clock_class *clock_class = bt_stream_class_borrow_default_clock_class_const(
					bt_stream_borrow_class_const(stream));
				/* Initialize to silence maybe-uninitialized warning. */
				uint64_t raw_value = 0;

				/*
				 * If we haven't seen a message with a clock snapshot, we don't know if our seek time is within
				 * the clock's range, so it wouldn't be safe to try to convert ns_from_origin to a clock value.
				 *
				 * Also, it would be a bit of a lie to generate a stream begin message with the seek time as its
				 * clock snapshot, because we don't really know if the stream existed at that time.  If we have
				 * seen a message with a clock snapshot in our seeking, then we are sure that the
				 * seek time is not below the clock range, and we know the stream was active at that
				 * time (and that we cut it short).
				 */
				if (stream_state->seen_clock_snapshot) {
					if (clock_raw_value_from_ns_from_origin(clock_class, ns_from_origin, &raw_value) != 0) {
						BT_LIB_LOGW("Could not convert nanoseconds from origin to clock value: ns-from-origin=%" PRId64 ", %![cc-]+K",
							ns_from_origin, clock_class);
						status = BT_FUNC_STATUS_ERROR;
						goto end;
					}
				}

				switch (stream_state->state) {
				case AUTO_SEEK_STREAM_STATE_PACKET_BEGAN:
					BT_ASSERT(stream_state->packet);
					BT_LIB_LOGD("Creating packet message: %![packet-]+a", stream_state->packet);

					if (stream->class->packets_have_beginning_default_clock_snapshot) {
						/*
						 * If we are in the PACKET_BEGAN state, it means we have seen a "packet beginning"
						 * message.  If "packet beginning" packets have clock snapshots, then we must have
						 * seen a clock snapshot.
						 */
						BT_ASSERT(stream_state->seen_clock_snapshot);

						msg = bt_message_packet_beginning_create_with_default_clock_snapshot(
							(bt_self_message_iterator *) iterator, stream_state->packet, raw_value);
					} else {
						msg = bt_message_packet_beginning_create((bt_self_message_iterator *) iterator,
								stream_state->packet);
					}

					if (!msg) {
						status = BT_FUNC_STATUS_MEMORY_ERROR;
						goto end;
					}

					g_queue_push_head(iterator->auto_seek.msgs, msg);
					msg = NULL;
					/* fall-thru */

				case AUTO_SEEK_STREAM_STATE_STREAM_BEGAN:
					msg = bt_message_stream_beginning_create(
						(bt_self_message_iterator *) iterator, stream);
					if (!msg) {
						status = BT_FUNC_STATUS_MEMORY_ERROR;
						goto end;
					}

					if (stream_state->seen_clock_snapshot) {
						bt_message_stream_beginning_set_default_clock_snapshot(msg, raw_value);
					}

					g_queue_push_head(iterator->auto_seek.msgs, msg);
					msg = NULL;
					break;
				}
			}

			/*
			 * If there are messages in the auto-seek
			 * message queue, replace the user's "next"
			 * method with a custom, temporary "next" method
			 * which returns them.
			 */
			if (!g_queue_is_empty(iterator->auto_seek.msgs)) {
				BT_ASSERT(!iterator->auto_seek.original_next_callback);
				iterator->auto_seek.original_next_callback = iterator->methods.next;

				iterator->methods.next =
					(bt_message_iterator_next_method)
						post_auto_seek_next;
			}

			/*
			 * `BT_FUNC_STATUS_END` becomes
			 * `BT_FUNC_STATUS_OK`: the next
			 * time this iterator's "next" method is called,
			 * it will return
			 * `BT_FUNC_STATUS_END`.
			 */
			status = BT_FUNC_STATUS_OK;
			break;
		}
		case BT_FUNC_STATUS_ERROR:
		case BT_FUNC_STATUS_MEMORY_ERROR:
		case BT_FUNC_STATUS_AGAIN:
			goto end;
		default:
			bt_common_abort();
		}
	}

	/*
	 * The following messages returned by the next method (including
	 * post_auto_seek_next) must be after (or at) `ns_from_origin`.
	 */
	iterator->last_ns_from_origin = ns_from_origin;

end:
	if (stream_states) {
		destroy_auto_seek_stream_states(stream_states);
		stream_states = NULL;
	}

	set_iterator_state_after_seeking(iterator, status);
	return status;
}

bt_bool bt_self_message_iterator_is_interrupted(
		const struct bt_self_message_iterator *self_msg_iter)
{
	const struct bt_message_iterator *iterator =
		(const void *) self_msg_iter;

	BT_ASSERT_PRE_NON_NULL(iterator, "Message iterator");
	return (bt_bool) bt_graph_is_interrupted(iterator->graph);
}

void bt_message_iterator_get_ref(
		const struct bt_message_iterator *iterator)
{
	bt_object_get_ref(iterator);
}

void bt_message_iterator_put_ref(
		const struct bt_message_iterator *iterator)
{
	bt_object_put_ref(iterator);
}
