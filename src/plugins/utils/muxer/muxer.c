/*
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

#define BT_COMP_LOG_SELF_COMP (muxer_comp->self_comp)
#define BT_LOG_OUTPUT_LEVEL (muxer_comp->log_level)
#define BT_LOG_TAG "PLUGIN/FLT.UTILS.MUXER"
#include "logging/comp-logging.h"

#include "common/macros.h"
#include "common/uuid.h"
#include <babeltrace2/babeltrace.h>
#include <glib.h>
#include <stdbool.h>
#include <inttypes.h>
#include "common/assert.h"
#include "common/common.h"
#include <stdlib.h>
#include <string.h>

#include "plugins/common/muxing/muxing.h"
#include "plugins/common/param-validation/param-validation.h"

#include "muxer.h"

struct muxer_comp {
	/* Weak refs */
	bt_self_component_filter *self_comp_flt;
	bt_self_component *self_comp;

	unsigned int next_port_num;
	size_t available_input_ports;
	bool initializing_muxer_msg_iter;
	bt_logging_level log_level;
};

struct muxer_upstream_msg_iter {
	struct muxer_comp *muxer_comp;

	/* Owned by this, NULL if ended */
	bt_message_iterator *msg_iter;

	/* Contains `const bt_message *`, owned by this */
	GQueue *msgs;
};

enum muxer_msg_iter_clock_class_expectation {
	MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_ANY = 0,
	MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_NONE,
	MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_ABSOLUTE,
	MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_NOT_ABS_SPEC_UUID,
	MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_NOT_ABS_NO_UUID,
};

struct muxer_msg_iter {
	struct muxer_comp *muxer_comp;

	/* Weak */
	bt_self_message_iterator *self_msg_iter;

	/*
	 * Array of struct muxer_upstream_msg_iter * (owned by this).
	 *
	 * NOTE: This array is searched in linearly to find the youngest
	 * current message. Keep this until benchmarks confirm that
	 * another data structure is faster than this for our typical
	 * use cases.
	 */
	GPtrArray *active_muxer_upstream_msg_iters;

	/*
	 * Array of struct muxer_upstream_msg_iter * (owned by this).
	 *
	 * We move ended message iterators from
	 * `active_muxer_upstream_msg_iters` to this array so as to be
	 * able to restore them when seeking.
	 */
	GPtrArray *ended_muxer_upstream_msg_iters;

	/* Last time returned in a message */
	int64_t last_returned_ts_ns;

	/* Clock class expectation state */
	enum muxer_msg_iter_clock_class_expectation clock_class_expectation;

	/*
	 * Expected clock class UUID, only valid when
	 * clock_class_expectation is
	 * MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_NOT_ABS_SPEC_UUID.
	 */
	bt_uuid_t expected_clock_class_uuid;

	/*
	 * Saved error.  If we hit an error in the _next method, but have some
	 * messages ready to return, we save the error here and return it on
	 * the next _next call.
	 */
	bt_message_iterator_class_next_method_status next_saved_status;
	const struct bt_error *next_saved_error;
};

static
void empty_message_queue(struct muxer_upstream_msg_iter *upstream_msg_iter)
{
	const bt_message *msg;

	while ((msg = g_queue_pop_head(upstream_msg_iter->msgs))) {
		bt_message_put_ref(msg);
	}
}

static
void destroy_muxer_upstream_msg_iter(
		struct muxer_upstream_msg_iter *muxer_upstream_msg_iter)
{
	struct muxer_comp *muxer_comp;

	if (!muxer_upstream_msg_iter) {
		return;
	}

	muxer_comp = muxer_upstream_msg_iter->muxer_comp;
	BT_COMP_LOGD("Destroying muxer's upstream message iterator wrapper: "
		"addr=%p, msg-iter-addr=%p, queue-len=%u",
		muxer_upstream_msg_iter,
		muxer_upstream_msg_iter->msg_iter,
		muxer_upstream_msg_iter->msgs->length);
	bt_message_iterator_put_ref(
		muxer_upstream_msg_iter->msg_iter);

	if (muxer_upstream_msg_iter->msgs) {
		empty_message_queue(muxer_upstream_msg_iter);
		g_queue_free(muxer_upstream_msg_iter->msgs);
	}

	g_free(muxer_upstream_msg_iter);
}

static
int muxer_msg_iter_add_upstream_msg_iter(struct muxer_msg_iter *muxer_msg_iter,
		bt_message_iterator *self_msg_iter)
{
	int ret = 0;
	struct muxer_upstream_msg_iter *muxer_upstream_msg_iter =
		g_new0(struct muxer_upstream_msg_iter, 1);
	struct muxer_comp *muxer_comp = muxer_msg_iter->muxer_comp;

	if (!muxer_upstream_msg_iter) {
		BT_COMP_LOGE_STR("Failed to allocate one muxer's upstream message iterator wrapper.");
		goto error;
	}

	muxer_upstream_msg_iter->muxer_comp = muxer_comp;
	muxer_upstream_msg_iter->msg_iter = self_msg_iter;
	bt_message_iterator_get_ref(muxer_upstream_msg_iter->msg_iter);
	muxer_upstream_msg_iter->msgs = g_queue_new();
	if (!muxer_upstream_msg_iter->msgs) {
		BT_COMP_LOGE_STR("Failed to allocate a GQueue.");
		goto error;
	}

	g_ptr_array_add(muxer_msg_iter->active_muxer_upstream_msg_iters,
		muxer_upstream_msg_iter);
	BT_COMP_LOGD("Added muxer's upstream message iterator wrapper: "
		"addr=%p, muxer-msg-iter-addr=%p, msg-iter-addr=%p",
		muxer_upstream_msg_iter, muxer_msg_iter,
		self_msg_iter);

	goto end;

error:
	destroy_muxer_upstream_msg_iter(muxer_upstream_msg_iter);
	ret = -1;

end:
	return ret;
}

static
bt_self_component_add_port_status add_available_input_port(
		bt_self_component_filter *self_comp)
{
	struct muxer_comp *muxer_comp = bt_self_component_get_data(
		bt_self_component_filter_as_self_component(self_comp));
	bt_self_component_add_port_status status =
		BT_SELF_COMPONENT_ADD_PORT_STATUS_OK;
	GString *port_name = NULL;

	BT_ASSERT(muxer_comp);
	port_name = g_string_new("in");
	if (!port_name) {
		BT_COMP_LOGE_STR("Failed to allocate a GString.");
		status = BT_SELF_COMPONENT_ADD_PORT_STATUS_MEMORY_ERROR;
		goto end;
	}

	g_string_append_printf(port_name, "%u", muxer_comp->next_port_num);
	status = bt_self_component_filter_add_input_port(
		self_comp, port_name->str, NULL, NULL);
	if (status != BT_SELF_COMPONENT_ADD_PORT_STATUS_OK) {
		BT_COMP_LOGE("Cannot add input port to muxer component: "
			"port-name=\"%s\", comp-addr=%p, status=%s",
			port_name->str, self_comp,
			bt_common_func_status_string(status));
		goto end;
	}

	muxer_comp->available_input_ports++;
	muxer_comp->next_port_num++;
	BT_COMP_LOGI("Added one input port to muxer component: "
		"port-name=\"%s\", comp-addr=%p",
		port_name->str, self_comp);

end:
	if (port_name) {
		g_string_free(port_name, TRUE);
	}

	return status;
}

static
bt_self_component_add_port_status create_output_port(
		bt_self_component_filter *self_comp)
{
	return bt_self_component_filter_add_output_port(
		self_comp, "out", NULL, NULL);
}

static
void destroy_muxer_comp(struct muxer_comp *muxer_comp)
{
	if (!muxer_comp) {
		return;
	}

	g_free(muxer_comp);
}

static
struct bt_param_validation_map_value_entry_descr muxer_params[] = {
	BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
};

BT_HIDDEN
bt_component_class_initialize_method_status muxer_init(
		bt_self_component_filter *self_comp_flt,
		bt_self_component_filter_configuration *config,
		const bt_value *params, void *init_data)
{
	bt_component_class_initialize_method_status status;
	bt_self_component_add_port_status add_port_status;
	bt_self_component *self_comp =
		bt_self_component_filter_as_self_component(self_comp_flt);
	struct muxer_comp *muxer_comp = g_new0(struct muxer_comp, 1);
	bt_logging_level log_level = bt_component_get_logging_level(
		bt_self_component_as_component(self_comp));
	enum bt_param_validation_status validation_status;
	gchar *validate_error = NULL;

	BT_COMP_LOG_CUR_LVL(BT_LOG_INFO, log_level, self_comp,
		"Initializing muxer component: "
		"comp-addr=%p, params-addr=%p", self_comp, params);

	if (!muxer_comp) {
		BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, log_level, self_comp,
			"Failed to allocate one muxer component.");
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		goto error;
	}

	muxer_comp->log_level = log_level;
	muxer_comp->self_comp = self_comp;
	muxer_comp->self_comp_flt = self_comp_flt;

	validation_status = bt_param_validation_validate(params,
		muxer_params, &validate_error);
	if (validation_status == BT_PARAM_VALIDATION_STATUS_MEMORY_ERROR) {
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		goto error;
	} else if (validation_status == BT_PARAM_VALIDATION_STATUS_VALIDATION_ERROR) {
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
		BT_COMP_LOGE_APPEND_CAUSE(self_comp, "%s", validate_error);
		goto error;
	}

	bt_self_component_set_data(self_comp, muxer_comp);
	add_port_status = add_available_input_port(self_comp_flt);
	if (add_port_status != BT_SELF_COMPONENT_ADD_PORT_STATUS_OK) {
		BT_COMP_LOGE("Cannot ensure that at least one muxer component's input port is available: "
			"muxer-comp-addr=%p, status=%s",
			muxer_comp,
			bt_common_func_status_string(add_port_status));
		status = (int) add_port_status;
		goto error;
	}

	add_port_status = create_output_port(self_comp_flt);
	if (add_port_status != BT_SELF_COMPONENT_ADD_PORT_STATUS_OK) {
		BT_COMP_LOGE("Cannot create muxer component's output port: "
			"muxer-comp-addr=%p, status=%s",
			muxer_comp,
			bt_common_func_status_string(add_port_status));
		status = (int) add_port_status;
		goto error;
	}

	BT_COMP_LOGI("Initialized muxer component: "
		"comp-addr=%p, params-addr=%p, muxer-comp-addr=%p",
		self_comp, params, muxer_comp);

	status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;
	goto end;

error:
	destroy_muxer_comp(muxer_comp);
	bt_self_component_set_data(self_comp, NULL);

end:
	g_free(validate_error);
	return status;
}

BT_HIDDEN
void muxer_finalize(bt_self_component_filter *self_comp)
{
	struct muxer_comp *muxer_comp = bt_self_component_get_data(
		bt_self_component_filter_as_self_component(self_comp));

	BT_COMP_LOGI("Finalizing muxer component: comp-addr=%p",
		self_comp);
	destroy_muxer_comp(muxer_comp);
}

static
bt_message_iterator_create_from_message_iterator_status
create_msg_iter_on_input_port(struct muxer_comp *muxer_comp,
		struct muxer_msg_iter *muxer_msg_iter,
		bt_self_component_port_input *self_port,
		bt_message_iterator **msg_iter)
{
	const bt_port *port = bt_self_component_port_as_port(
		bt_self_component_port_input_as_self_component_port(
			self_port));
	bt_message_iterator_create_from_message_iterator_status
		status;

	BT_ASSERT(port);
	BT_ASSERT(bt_port_is_connected(port));

	// TODO: Advance the iterator to >= the time of the latest
	//       returned message by the muxer message
	//       iterator which creates it.
	status = bt_message_iterator_create_from_message_iterator(
		muxer_msg_iter->self_msg_iter, self_port, msg_iter);
	if (status != BT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_OK) {
		BT_COMP_LOGE("Cannot create upstream message iterator on input port: "
			"port-addr=%p, port-name=\"%s\"",
			port, bt_port_get_name(port));
		goto end;
	}

	BT_COMP_LOGI("Created upstream message iterator on input port: "
		"port-addr=%p, port-name=\"%s\", msg-iter-addr=%p",
		port, bt_port_get_name(port), msg_iter);

end:
	return status;
}

static
bt_message_iterator_class_next_method_status muxer_upstream_msg_iter_next(
		struct muxer_upstream_msg_iter *muxer_upstream_msg_iter,
		bool *is_ended)
{
	struct muxer_comp *muxer_comp = muxer_upstream_msg_iter->muxer_comp;
	bt_message_iterator_class_next_method_status status;
	bt_message_iterator_next_status input_port_iter_status;
	bt_message_array_const msgs;
	uint64_t i;
	uint64_t count;

	BT_COMP_LOGD("Calling upstream message iterator's \"next\" method: "
		"muxer-upstream-msg-iter-wrap-addr=%p, msg-iter-addr=%p",
		muxer_upstream_msg_iter,
		muxer_upstream_msg_iter->msg_iter);
	input_port_iter_status = bt_message_iterator_next(
		muxer_upstream_msg_iter->msg_iter, &msgs, &count);
	BT_COMP_LOGD("Upstream message iterator's \"next\" method returned: "
		"status=%s",
		bt_common_func_status_string(input_port_iter_status));

	switch (input_port_iter_status) {
	case BT_MESSAGE_ITERATOR_NEXT_STATUS_OK:
		/*
		 * Message iterator's current message is
		 * valid: it must be considered for muxing operations.
		 */
		BT_COMP_LOGD_STR("Validated upstream message iterator wrapper.");
		BT_ASSERT_DBG(count > 0);

		/* Move messages to our queue */
		for (i = 0; i < count; i++) {
			/*
			 * Push to tail in order; other side
			 * (muxer_msg_iter_do_next_one()) consumes
			 * from the head first.
			 */
			g_queue_push_tail(muxer_upstream_msg_iter->msgs,
				(void *) msgs[i]);
		}
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
		break;
	case BT_MESSAGE_ITERATOR_NEXT_STATUS_AGAIN:
		/*
		 * Message iterator's current message is not
		 * valid anymore. Return
		 * BT_MESSAGE_ITERATOR_NEXT_STATUS_AGAIN immediately.
		 */
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_AGAIN;
		break;
	case BT_MESSAGE_ITERATOR_NEXT_STATUS_END:	/* Fall-through. */
		/*
		 * Message iterator reached the end: release it. It
		 * won't be considered again to find the youngest
		 * message.
		 */
		*is_ended = true;
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
		break;
	case BT_MESSAGE_ITERATOR_NEXT_STATUS_ERROR:
	case BT_MESSAGE_ITERATOR_NEXT_STATUS_MEMORY_ERROR:
		/* Error status code */
		BT_COMP_LOGE_APPEND_CAUSE(muxer_comp->self_comp,
			"Upstream iterator's next method returned an error: status=%s",
			bt_common_func_status_string(input_port_iter_status));
		status = (int) input_port_iter_status;
		break;
	default:
		/* Unsupported status code */
		BT_COMP_LOGE_APPEND_CAUSE(muxer_comp->self_comp,
			"Unsupported status code: status=%s",
			bt_common_func_status_string(input_port_iter_status));
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
		break;
	}

	return status;
}

static
int get_msg_ts_ns(struct muxer_comp *muxer_comp,
		struct muxer_msg_iter *muxer_msg_iter,
		const bt_message *msg, int64_t last_returned_ts_ns,
		int64_t *ts_ns)
{
	const bt_clock_snapshot *clock_snapshot = NULL;
	int ret = 0;
	const bt_stream_class *stream_class = NULL;
	bt_message_type msg_type;

	BT_ASSERT_DBG(msg);
	BT_ASSERT_DBG(ts_ns);
	BT_COMP_LOGD("Getting message's timestamp: "
		"muxer-msg-iter-addr=%p, msg-addr=%p, "
		"last-returned-ts=%" PRId64,
		muxer_msg_iter, msg, last_returned_ts_ns);

	if (G_UNLIKELY(muxer_msg_iter->clock_class_expectation ==
			MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_NONE)) {
		*ts_ns = last_returned_ts_ns;
		goto end;
	}

	msg_type = bt_message_get_type(msg);

	if (G_UNLIKELY(msg_type == BT_MESSAGE_TYPE_PACKET_BEGINNING)) {
		stream_class = bt_stream_borrow_class_const(
			bt_packet_borrow_stream_const(
				bt_message_packet_beginning_borrow_packet_const(
					msg)));
	} else if (G_UNLIKELY(msg_type == BT_MESSAGE_TYPE_PACKET_END)) {
		stream_class = bt_stream_borrow_class_const(
			bt_packet_borrow_stream_const(
				bt_message_packet_end_borrow_packet_const(
					msg)));
	} else if (G_UNLIKELY(msg_type == BT_MESSAGE_TYPE_DISCARDED_EVENTS)) {
		stream_class = bt_stream_borrow_class_const(
			bt_message_discarded_events_borrow_stream_const(msg));
	} else if (G_UNLIKELY(msg_type == BT_MESSAGE_TYPE_DISCARDED_PACKETS)) {
		stream_class = bt_stream_borrow_class_const(
			bt_message_discarded_packets_borrow_stream_const(msg));
	}

	switch (msg_type) {
	case BT_MESSAGE_TYPE_EVENT:
		BT_ASSERT_DBG(bt_message_event_borrow_stream_class_default_clock_class_const(
				msg));
		clock_snapshot = bt_message_event_borrow_default_clock_snapshot_const(
			msg);
		break;
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		if (bt_stream_class_packets_have_beginning_default_clock_snapshot(
				stream_class)) {
			clock_snapshot = bt_message_packet_beginning_borrow_default_clock_snapshot_const(
				msg);
		} else {
			goto no_clock_snapshot;
		}

		break;
	case BT_MESSAGE_TYPE_PACKET_END:
		if (bt_stream_class_packets_have_end_default_clock_snapshot(
				stream_class)) {
			clock_snapshot = bt_message_packet_end_borrow_default_clock_snapshot_const(
				msg);
		} else {
			goto no_clock_snapshot;
		}

		break;
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
	{
		enum bt_message_stream_clock_snapshot_state snapshot_state =
			bt_message_stream_beginning_borrow_default_clock_snapshot_const(
				msg, &clock_snapshot);
		if (snapshot_state == BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_UNKNOWN) {
			goto no_clock_snapshot;
		}

		break;
	}
	case BT_MESSAGE_TYPE_STREAM_END:
	{
		enum bt_message_stream_clock_snapshot_state snapshot_state =
			bt_message_stream_end_borrow_default_clock_snapshot_const(
				msg, &clock_snapshot);
		if (snapshot_state == BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_UNKNOWN) {
			goto no_clock_snapshot;
		}

		break;
	}
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
		if (bt_stream_class_discarded_events_have_default_clock_snapshots(
				stream_class)) {
			clock_snapshot = bt_message_discarded_events_borrow_beginning_default_clock_snapshot_const(
				msg);
		} else {
			goto no_clock_snapshot;
		}

		break;
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
		if (bt_stream_class_discarded_packets_have_default_clock_snapshots(
				stream_class)) {
			clock_snapshot = bt_message_discarded_packets_borrow_beginning_default_clock_snapshot_const(
				msg);
		} else {
			goto no_clock_snapshot;
		}

		break;
	case BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY:
		clock_snapshot = bt_message_message_iterator_inactivity_borrow_clock_snapshot_const(
			msg);
		break;
	default:
		/* All the other messages have a higher priority */
		BT_COMP_LOGD_STR("Message has no timestamp: using the last returned timestamp.");
		*ts_ns = last_returned_ts_ns;
		goto end;
	}

	ret = bt_clock_snapshot_get_ns_from_origin(clock_snapshot, ts_ns);
	if (ret) {
		BT_COMP_LOGE("Cannot get nanoseconds from Epoch of clock snapshot: "
			"clock-snapshot-addr=%p", clock_snapshot);
		goto error;
	}

	goto end;

no_clock_snapshot:
	BT_COMP_LOGD_STR("Message's default clock snapshot is missing: "
		"using the last returned timestamp.");
	*ts_ns = last_returned_ts_ns;
	goto end;

error:
	ret = -1;

end:
	if (ret == 0) {
		BT_COMP_LOGD("Found message's timestamp: "
			"muxer-msg-iter-addr=%p, msg-addr=%p, "
			"last-returned-ts=%" PRId64 ", ts=%" PRId64,
			muxer_msg_iter, msg, last_returned_ts_ns,
			*ts_ns);
	}

	return ret;
}

static inline
int validate_clock_class(struct muxer_msg_iter *muxer_msg_iter,
		struct muxer_comp *muxer_comp,
		const bt_clock_class *clock_class)
{
	int ret = 0;
	const uint8_t *cc_uuid;
	const char *cc_name;

	BT_ASSERT_DBG(clock_class);
	cc_uuid = bt_clock_class_get_uuid(clock_class);
	cc_name = bt_clock_class_get_name(clock_class);

	if (muxer_msg_iter->clock_class_expectation ==
			MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_ANY) {
		/*
		 * This is the first clock class that this muxer message
		 * iterator encounters. Its properties determine what to expect
		 * for the whole lifetime of the iterator.
		 */
		if (bt_clock_class_origin_is_unix_epoch(clock_class)) {
			/* Expect absolute clock classes */
			muxer_msg_iter->clock_class_expectation =
				MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_ABSOLUTE;
		} else {
			if (cc_uuid) {
				/*
				 * Expect non-absolute clock classes
				 * with a specific UUID.
				 */
				muxer_msg_iter->clock_class_expectation =
					MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_NOT_ABS_SPEC_UUID;
				bt_uuid_copy(muxer_msg_iter->expected_clock_class_uuid,	cc_uuid);
			} else {
				/*
				 * Expect non-absolute clock classes
				 * with no UUID.
				 */
				muxer_msg_iter->clock_class_expectation =
					MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_NOT_ABS_NO_UUID;
			}
		}
	}

	switch (muxer_msg_iter->clock_class_expectation) {
	case MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_ABSOLUTE:
		if (!bt_clock_class_origin_is_unix_epoch(clock_class)) {
			BT_COMP_LOGE("Expecting an absolute clock class, "
				"but got a non-absolute one: "
				"clock-class-addr=%p, clock-class-name=\"%s\"",
				clock_class, cc_name);
			goto error;
		}
		break;
	case MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_NOT_ABS_NO_UUID:
		if (bt_clock_class_origin_is_unix_epoch(clock_class)) {
			BT_COMP_LOGE("Expecting a non-absolute clock class with no UUID, "
				"but got an absolute one: "
				"clock-class-addr=%p, clock-class-name=\"%s\"",
				clock_class, cc_name);
			goto error;
		}

		if (cc_uuid) {
			BT_COMP_LOGE("Expecting a non-absolute clock class with no UUID, "
				"but got one with a UUID: "
				"clock-class-addr=%p, clock-class-name=\"%s\", "
				"uuid=\"" BT_UUID_FMT "\"",
				clock_class, cc_name, BT_UUID_FMT_VALUES(cc_uuid));
			goto error;
		}
		break;
	case MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_NOT_ABS_SPEC_UUID:
		if (bt_clock_class_origin_is_unix_epoch(clock_class)) {
			BT_COMP_LOGE("Expecting a non-absolute clock class with a specific UUID, "
				"but got an absolute one: "
				"clock-class-addr=%p, clock-class-name=\"%s\"",
				clock_class, cc_name);
			goto error;
		}

		if (!cc_uuid) {
			BT_COMP_LOGE("Expecting a non-absolute clock class with a specific UUID, "
				"but got one with no UUID: "
				"clock-class-addr=%p, clock-class-name=\"%s\"",
				clock_class, cc_name);
			goto error;
		}

		if (bt_uuid_compare(muxer_msg_iter->expected_clock_class_uuid, cc_uuid) != 0) {
			BT_COMP_LOGE("Expecting a non-absolute clock class with a specific UUID, "
				"but got one with different UUID: "
				"clock-class-addr=%p, clock-class-name=\"%s\", "
				"expected-uuid=\"" BT_UUID_FMT "\", "
				"uuid=\"" BT_UUID_FMT "\"",
				clock_class, cc_name,
				BT_UUID_FMT_VALUES(muxer_msg_iter->expected_clock_class_uuid),
				BT_UUID_FMT_VALUES(cc_uuid));
			goto error;
		}
		break;
	case MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_NONE:
		BT_COMP_LOGE("Expecting no clock class, but got one: "
			"clock-class-addr=%p, clock-class-name=\"%s\"",
			clock_class, cc_name);
		goto error;
	default:
		/* Unexpected */
		BT_COMP_LOGF("Unexpected clock class expectation: "
			"expectation-code=%d",
			muxer_msg_iter->clock_class_expectation);
		bt_common_abort();
	}

	goto end;

error:
	ret = -1;

end:
	return ret;
}

static inline
int validate_new_stream_clock_class(struct muxer_msg_iter *muxer_msg_iter,
		struct muxer_comp *muxer_comp, const bt_stream *stream)
{
	int ret = 0;
	const bt_stream_class *stream_class =
		bt_stream_borrow_class_const(stream);
	const bt_clock_class *clock_class =
		bt_stream_class_borrow_default_clock_class_const(stream_class);

	if (!clock_class) {
		if (muxer_msg_iter->clock_class_expectation ==
			MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_ANY) {
			/* Expect no clock class */
			muxer_msg_iter->clock_class_expectation =
				MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_NONE;
		} else if (muxer_msg_iter->clock_class_expectation !=
				MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_NONE) {
			BT_COMP_LOGE("Expecting stream class without a default clock class: "
				"stream-class-addr=%p, stream-class-name=\"%s\", "
				"stream-class-id=%" PRIu64,
				stream_class, bt_stream_class_get_name(stream_class),
				bt_stream_class_get_id(stream_class));
			ret = -1;
		}

		goto end;
	}

	ret = validate_clock_class(muxer_msg_iter, muxer_comp, clock_class);

end:
	return ret;
}

/*
 * This function finds the youngest available message amongst the
 * non-ended upstream message iterators and returns the upstream
 * message iterator which has it, or
 * BT_MESSAGE_ITERATOR_STATUS_END if there's no available
 * message.
 *
 * This function does NOT:
 *
 * * Update any upstream message iterator.
 * * Check the upstream message iterators to retry.
 *
 * On sucess, this function sets *muxer_upstream_msg_iter to the
 * upstream message iterator of which the current message is
 * the youngest, and sets *ts_ns to its time.
 */
static
bt_message_iterator_class_next_method_status
muxer_msg_iter_youngest_upstream_msg_iter(
		struct muxer_comp *muxer_comp,
		struct muxer_msg_iter *muxer_msg_iter,
		struct muxer_upstream_msg_iter **muxer_upstream_msg_iter,
		int64_t *ts_ns)
{
	size_t i;
	int ret;
	int64_t youngest_ts_ns = INT64_MAX;
	bt_message_iterator_class_next_method_status status =
		BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;

	BT_ASSERT_DBG(muxer_comp);
	BT_ASSERT_DBG(muxer_msg_iter);
	BT_ASSERT_DBG(muxer_upstream_msg_iter);
	*muxer_upstream_msg_iter = NULL;

	for (i = 0; i < muxer_msg_iter->active_muxer_upstream_msg_iters->len;
			i++) {
		const bt_message *msg;
		struct muxer_upstream_msg_iter *cur_muxer_upstream_msg_iter =
			g_ptr_array_index(
				muxer_msg_iter->active_muxer_upstream_msg_iters,
				i);
		int64_t msg_ts_ns;

		if (!cur_muxer_upstream_msg_iter->msg_iter) {
			/* This upstream message iterator is ended */
			BT_COMP_LOGT("Skipping ended upstream message iterator: "
				"muxer-upstream-msg-iter-wrap-addr=%p",
				cur_muxer_upstream_msg_iter);
			continue;
		}

		BT_ASSERT_DBG(cur_muxer_upstream_msg_iter->msgs->length > 0);
		msg = g_queue_peek_head(cur_muxer_upstream_msg_iter->msgs);
		BT_ASSERT_DBG(msg);

		if (G_UNLIKELY(bt_message_get_type(msg) ==
				BT_MESSAGE_TYPE_STREAM_BEGINNING)) {
			ret = validate_new_stream_clock_class(
				muxer_msg_iter, muxer_comp,
				bt_message_stream_beginning_borrow_stream_const(
					msg));
			if (ret) {
				/*
				 * validate_new_stream_clock_class() logs
				 * errors.
				 */
				status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
				goto end;
			}
		} else if (G_UNLIKELY(bt_message_get_type(msg) ==
				BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY)) {
			const bt_clock_snapshot *cs;

			cs = bt_message_message_iterator_inactivity_borrow_clock_snapshot_const(
				msg);
			ret = validate_clock_class(muxer_msg_iter, muxer_comp,
				bt_clock_snapshot_borrow_clock_class_const(cs));
			if (ret) {
				/* validate_clock_class() logs errors */
				status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
				goto end;
			}
		}

		ret = get_msg_ts_ns(muxer_comp, muxer_msg_iter, msg,
			muxer_msg_iter->last_returned_ts_ns, &msg_ts_ns);
		if (ret) {
			/* get_msg_ts_ns() logs errors */
			*muxer_upstream_msg_iter = NULL;
			status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
			goto end;
		}

		/*
		 * Update the current message iterator if it has not been set
		 * yet, or if its current message has a timestamp smaller than
		 * the previously selected youngest message.
		 */
		if (G_UNLIKELY(*muxer_upstream_msg_iter == NULL) ||
				msg_ts_ns < youngest_ts_ns) {
			*muxer_upstream_msg_iter =
				cur_muxer_upstream_msg_iter;
			youngest_ts_ns = msg_ts_ns;
			*ts_ns = youngest_ts_ns;
		} else if (msg_ts_ns == youngest_ts_ns) {
			/*
			 * The currently selected message to be sent downstream
			 * next has the exact same timestamp that of the
			 * current candidate message. We must break the tie
			 * in a predictable manner.
			 */
			const bt_message *selected_msg = g_queue_peek_head(
				(*muxer_upstream_msg_iter)->msgs);
			BT_COMP_LOGD_STR("Two of the next message candidates have the same timestamps, pick one deterministically.");

			/*
			 * Order the messages in an arbitrary but determinitic
			 * way.
			 */
			ret = common_muxing_compare_messages(msg, selected_msg);
			if (ret < 0) {
				/*
				 * The `msg` should go first. Update the next
				 * iterator and the current timestamp.
				 */
				*muxer_upstream_msg_iter =
					cur_muxer_upstream_msg_iter;
				youngest_ts_ns = msg_ts_ns;
				*ts_ns = youngest_ts_ns;
			} else if (ret == 0) {
				/* Unable to pick which one should go first. */
				BT_COMP_LOGW("Cannot deterministically pick next upstream message iterator because they have identical next messages: "
					"muxer-upstream-msg-iter-wrap-addr=%p"
					"cur-muxer-upstream-msg-iter-wrap-addr=%p",
					*muxer_upstream_msg_iter,
					cur_muxer_upstream_msg_iter);
			}
		}
	}

	if (!*muxer_upstream_msg_iter) {
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END;
		*ts_ns = INT64_MIN;
	}

end:
	return status;
}

static
bt_message_iterator_class_next_method_status
validate_muxer_upstream_msg_iter(
	struct muxer_upstream_msg_iter *muxer_upstream_msg_iter,
	bool *is_ended)
{
	struct muxer_comp *muxer_comp = muxer_upstream_msg_iter->muxer_comp;
	bt_message_iterator_class_next_method_status status;

	BT_COMP_LOGD("Validating muxer's upstream message iterator wrapper: "
		"muxer-upstream-msg-iter-wrap-addr=%p",
		muxer_upstream_msg_iter);

	if (muxer_upstream_msg_iter->msgs->length > 0 ||
			!muxer_upstream_msg_iter->msg_iter) {
		BT_COMP_LOGD("Already valid or not considered: "
			"queue-len=%u, upstream-msg-iter-addr=%p",
			muxer_upstream_msg_iter->msgs->length,
			muxer_upstream_msg_iter->msg_iter);
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
		goto end;
	}

	/* muxer_upstream_msg_iter_next() logs details/errors */
	status = muxer_upstream_msg_iter_next(muxer_upstream_msg_iter,
		is_ended);

end:
	return status;
}

static
bt_message_iterator_class_next_method_status
validate_muxer_upstream_msg_iters(
		struct muxer_msg_iter *muxer_msg_iter)
{
	struct muxer_comp *muxer_comp = muxer_msg_iter->muxer_comp;
	bt_message_iterator_class_next_method_status status;
	size_t i;

	BT_COMP_LOGD("Validating muxer's upstream message iterator wrappers: "
		"muxer-msg-iter-addr=%p", muxer_msg_iter);

	for (i = 0; i < muxer_msg_iter->active_muxer_upstream_msg_iters->len;
			i++) {
		bool is_ended = false;
		struct muxer_upstream_msg_iter *muxer_upstream_msg_iter =
			g_ptr_array_index(
				muxer_msg_iter->active_muxer_upstream_msg_iters,
				i);

		status = validate_muxer_upstream_msg_iter(
			muxer_upstream_msg_iter, &is_ended);
		if (status != BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK) {
			if (status < 0) {
				BT_COMP_LOGE_APPEND_CAUSE(muxer_comp->self_comp,
					"Cannot validate muxer's upstream message iterator wrapper: "
					"muxer-msg-iter-addr=%p, "
					"muxer-upstream-msg-iter-wrap-addr=%p",
					muxer_msg_iter,
					muxer_upstream_msg_iter);
			} else {
				BT_COMP_LOGD("Cannot validate muxer's upstream message iterator wrapper: "
					"muxer-msg-iter-addr=%p, "
					"muxer-upstream-msg-iter-wrap-addr=%p",
					muxer_msg_iter,
					muxer_upstream_msg_iter);
			}

			goto end;
		}

		/*
		 * Move this muxer upstream message iterator to the
		 * array of ended iterators if it's ended.
		 */
		if (G_UNLIKELY(is_ended)) {
			BT_COMP_LOGD("Muxer's upstream message iterator wrapper: ended or canceled: "
				"muxer-msg-iter-addr=%p, "
				"muxer-upstream-msg-iter-wrap-addr=%p",
				muxer_msg_iter, muxer_upstream_msg_iter);
			g_ptr_array_add(
				muxer_msg_iter->ended_muxer_upstream_msg_iters,
				muxer_upstream_msg_iter);
			muxer_msg_iter->active_muxer_upstream_msg_iters->pdata[i] = NULL;

			/*
			 * Use g_ptr_array_remove_fast() because the
			 * order of those elements is not important.
			 */
			g_ptr_array_remove_index_fast(
				muxer_msg_iter->active_muxer_upstream_msg_iters,
				i);
			i--;
		}
	}

	status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;

end:
	return status;
}

static inline
bt_message_iterator_class_next_method_status muxer_msg_iter_do_next_one(
		struct muxer_comp *muxer_comp,
		struct muxer_msg_iter *muxer_msg_iter,
		const bt_message **msg)
{
	bt_message_iterator_class_next_method_status status;
	struct muxer_upstream_msg_iter *muxer_upstream_msg_iter = NULL;
	/* Initialize to avoid -Wmaybe-uninitialized warning with gcc 4.8. */
	int64_t next_return_ts = 0;

	status = validate_muxer_upstream_msg_iters(muxer_msg_iter);
	if (status != BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK) {
		/* validate_muxer_upstream_msg_iters() logs details */
		goto end;
	}

	/*
	 * At this point we know that all the existing upstream
	 * message iterators are valid. We can find the one,
	 * amongst those, of which the current message is the
	 * youngest.
	 */
	status = muxer_msg_iter_youngest_upstream_msg_iter(muxer_comp,
			muxer_msg_iter, &muxer_upstream_msg_iter,
			&next_return_ts);
	if (status < 0 || status == BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END) {
		if (status < 0) {
			BT_COMP_LOGE_APPEND_CAUSE(muxer_comp->self_comp,
				"Cannot find the youngest upstream message iterator wrapper: "
				"status=%s",
				bt_common_func_status_string(status));
		} else {
			BT_COMP_LOGD("Cannot find the youngest upstream message iterator wrapper: "
				"status=%s",
				bt_common_func_status_string(status));
		}

		goto end;
	}

	if (next_return_ts < muxer_msg_iter->last_returned_ts_ns) {
		BT_COMP_LOGE_APPEND_CAUSE(muxer_comp->self_comp,
			"Youngest upstream message iterator wrapper's timestamp is less than muxer's message iterator's last returned timestamp: "
			"muxer-msg-iter-addr=%p, ts=%" PRId64 ", "
			"last-returned-ts=%" PRId64,
			muxer_msg_iter, next_return_ts,
			muxer_msg_iter->last_returned_ts_ns);
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
		goto end;
	}

	BT_COMP_LOGD("Found youngest upstream message iterator wrapper: "
		"muxer-msg-iter-addr=%p, "
		"muxer-upstream-msg-iter-wrap-addr=%p, "
		"ts=%" PRId64,
		muxer_msg_iter, muxer_upstream_msg_iter, next_return_ts);
	BT_ASSERT_DBG(status ==
		BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK);
	BT_ASSERT_DBG(muxer_upstream_msg_iter);

	/*
	 * Consume from the queue's head: other side
	 * (muxer_upstream_msg_iter_next()) writes to the tail.
	 */
	*msg = g_queue_pop_head(muxer_upstream_msg_iter->msgs);
	BT_ASSERT_DBG(*msg);
	muxer_msg_iter->last_returned_ts_ns = next_return_ts;

end:
	return status;
}

static
bt_message_iterator_class_next_method_status muxer_msg_iter_do_next(
		struct muxer_comp *muxer_comp,
		struct muxer_msg_iter *muxer_msg_iter,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	bt_message_iterator_class_next_method_status status;
	uint64_t i = 0;

	if (G_UNLIKELY(muxer_msg_iter->next_saved_error)) {
		/*
		 * Last time we were called, we hit an error but had some
		 * messages to deliver, so we stashed the error here.  Return
		 * it now.
		 */
		BT_CURRENT_THREAD_MOVE_ERROR_AND_RESET(muxer_msg_iter->next_saved_error);
		status = muxer_msg_iter->next_saved_status;
		goto end;
	}

	do {
		status = muxer_msg_iter_do_next_one(muxer_comp,
			muxer_msg_iter, &msgs[i]);
		if (status == BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK) {
			i++;
		}
	} while (i < capacity && status == BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK);

	if (i > 0) {
		/*
		 * Even if muxer_msg_iter_do_next_one() returned
		 * something else than
		 * BT_MESSAGE_ITERATOR_STATUS_OK, we accumulated
		 * message objects in the output message
		 * array, so we need to return
		 * BT_MESSAGE_ITERATOR_STATUS_OK so that they are
		 * transfered to downstream. This other status occurs
		 * again the next time muxer_msg_iter_do_next() is
		 * called, possibly without any accumulated
		 * message, in which case we'll return it.
		 */
		if (status < 0) {
			/*
			 * Save this error for the next _next call.  Assume that
			 * this component always appends error causes when
			 * returning an error status code, which will cause the
			 * current thread error to be non-NULL.
			 */
			muxer_msg_iter->next_saved_error = bt_current_thread_take_error();
			BT_ASSERT(muxer_msg_iter->next_saved_error);
			muxer_msg_iter->next_saved_status = status;
		}

		*count = i;
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
	}

end:
	return status;
}

static
void destroy_muxer_msg_iter(struct muxer_msg_iter *muxer_msg_iter)
{
	struct muxer_comp *muxer_comp;

	if (!muxer_msg_iter) {
		return;
	}

	muxer_comp = muxer_msg_iter->muxer_comp;
	BT_COMP_LOGD("Destroying muxer component's message iterator: "
		"muxer-msg-iter-addr=%p", muxer_msg_iter);

	if (muxer_msg_iter->active_muxer_upstream_msg_iters) {
		BT_COMP_LOGD_STR("Destroying muxer's active upstream message iterator wrappers.");
		g_ptr_array_free(
			muxer_msg_iter->active_muxer_upstream_msg_iters, TRUE);
	}

	if (muxer_msg_iter->ended_muxer_upstream_msg_iters) {
		BT_COMP_LOGD_STR("Destroying muxer's ended upstream message iterator wrappers.");
		g_ptr_array_free(
			muxer_msg_iter->ended_muxer_upstream_msg_iters, TRUE);
	}

	g_free(muxer_msg_iter);
}

static
bt_message_iterator_class_initialize_method_status
muxer_msg_iter_init_upstream_iterators(struct muxer_comp *muxer_comp,
		struct muxer_msg_iter *muxer_msg_iter,
		struct bt_self_message_iterator_configuration *config)
{
	int64_t count;
	int64_t i;
	bt_message_iterator_class_initialize_method_status status;
	bool can_seek_forward = true;

	count = bt_component_filter_get_input_port_count(
		bt_self_component_filter_as_component_filter(
			muxer_comp->self_comp_flt));
	if (count < 0) {
		BT_COMP_LOGD("No input port to initialize for muxer component's message iterator: "
			"muxer-comp-addr=%p, muxer-msg-iter-addr=%p",
			muxer_comp, muxer_msg_iter);
		status = BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_OK;
		goto end;
	}

	for (i = 0; i < count; i++) {
		bt_message_iterator *upstream_msg_iter;
		bt_self_component_port_input *self_port =
			bt_self_component_filter_borrow_input_port_by_index(
				muxer_comp->self_comp_flt, i);
		const bt_port *port;
		bt_message_iterator_create_from_message_iterator_status
			msg_iter_status;
		int int_status;

		BT_ASSERT(self_port);
		port = bt_self_component_port_as_port(
			bt_self_component_port_input_as_self_component_port(
				self_port));
		BT_ASSERT(port);

		if (!bt_port_is_connected(port)) {
			/* Skip non-connected port */
			continue;
		}

		msg_iter_status = create_msg_iter_on_input_port(muxer_comp,
			muxer_msg_iter, self_port, &upstream_msg_iter);
		if (msg_iter_status != BT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_OK) {
			/* create_msg_iter_on_input_port() logs errors */
			status = (int) msg_iter_status;
			goto end;
		}

		int_status = muxer_msg_iter_add_upstream_msg_iter(muxer_msg_iter,
			upstream_msg_iter);
		bt_message_iterator_put_ref(
			upstream_msg_iter);
		if (int_status) {
			status = BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
			/* muxer_msg_iter_add_upstream_msg_iter() logs errors */
			goto end;
		}

		can_seek_forward = can_seek_forward &&
			bt_message_iterator_can_seek_forward(
				upstream_msg_iter);
	}

	/*
	 * This iterator can seek forward if all of its iterators can seek
	 * forward.
	 */
	bt_self_message_iterator_configuration_set_can_seek_forward(
		config, can_seek_forward);

	status = BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_OK;

end:
	return status;
}

BT_HIDDEN
bt_message_iterator_class_initialize_method_status muxer_msg_iter_init(
		bt_self_message_iterator *self_msg_iter,
		bt_self_message_iterator_configuration *config,
		bt_self_component_port_output *port)
{
	struct muxer_comp *muxer_comp = NULL;
	struct muxer_msg_iter *muxer_msg_iter = NULL;
	bt_message_iterator_class_initialize_method_status status;
	bt_self_component *self_comp =
		bt_self_message_iterator_borrow_component(self_msg_iter);

	muxer_comp = bt_self_component_get_data(self_comp);
	BT_ASSERT(muxer_comp);
	BT_COMP_LOGD("Initializing muxer component's message iterator: "
		"comp-addr=%p, muxer-comp-addr=%p, msg-iter-addr=%p",
		self_comp, muxer_comp, self_msg_iter);

	if (muxer_comp->initializing_muxer_msg_iter) {
		/*
		 * Weird, unhandled situation detected: downstream
		 * creates a muxer message iterator while creating
		 * another muxer message iterator (same component).
		 */
		BT_COMP_LOGE("Recursive initialization of muxer component's message iterator: "
			"comp-addr=%p, muxer-comp-addr=%p, msg-iter-addr=%p",
			self_comp, muxer_comp, self_msg_iter);
		status = BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
		goto error;
	}

	muxer_comp->initializing_muxer_msg_iter = true;
	muxer_msg_iter = g_new0(struct muxer_msg_iter, 1);
	if (!muxer_msg_iter) {
		BT_COMP_LOGE_STR("Failed to allocate one muxer component's message iterator.");
		status = BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		goto error;
	}

	muxer_msg_iter->muxer_comp = muxer_comp;
	muxer_msg_iter->self_msg_iter = self_msg_iter;
	muxer_msg_iter->last_returned_ts_ns = INT64_MIN;
	muxer_msg_iter->active_muxer_upstream_msg_iters =
		g_ptr_array_new_with_free_func(
			(GDestroyNotify) destroy_muxer_upstream_msg_iter);
	if (!muxer_msg_iter->active_muxer_upstream_msg_iters) {
		BT_COMP_LOGE_STR("Failed to allocate a GPtrArray.");
		status = BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		goto error;
	}

	muxer_msg_iter->ended_muxer_upstream_msg_iters =
		g_ptr_array_new_with_free_func(
			(GDestroyNotify) destroy_muxer_upstream_msg_iter);
	if (!muxer_msg_iter->ended_muxer_upstream_msg_iters) {
		BT_COMP_LOGE_STR("Failed to allocate a GPtrArray.");
		status = BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		goto error;
	}

	status = muxer_msg_iter_init_upstream_iterators(muxer_comp,
		muxer_msg_iter, config);
	if (status) {
		BT_COMP_LOGE("Cannot initialize connected input ports for muxer component's message iterator: "
			"comp-addr=%p, muxer-comp-addr=%p, "
			"muxer-msg-iter-addr=%p, msg-iter-addr=%p, ret=%d",
			self_comp, muxer_comp, muxer_msg_iter,
			self_msg_iter, status);
		goto error;
	}

	bt_self_message_iterator_set_data(self_msg_iter, muxer_msg_iter);
	BT_COMP_LOGD("Initialized muxer component's message iterator: "
		"comp-addr=%p, muxer-comp-addr=%p, muxer-msg-iter-addr=%p, "
		"msg-iter-addr=%p",
		self_comp, muxer_comp, muxer_msg_iter, self_msg_iter);
	goto end;

error:
	destroy_muxer_msg_iter(muxer_msg_iter);
	bt_self_message_iterator_set_data(self_msg_iter, NULL);

end:
	muxer_comp->initializing_muxer_msg_iter = false;
	return status;
}

BT_HIDDEN
void muxer_msg_iter_finalize(bt_self_message_iterator *self_msg_iter)
{
	struct muxer_msg_iter *muxer_msg_iter =
		bt_self_message_iterator_get_data(self_msg_iter);
	bt_self_component *self_comp = NULL;
	struct muxer_comp *muxer_comp = NULL;

	self_comp = bt_self_message_iterator_borrow_component(
		self_msg_iter);
	BT_ASSERT(self_comp);
	muxer_comp = bt_self_component_get_data(self_comp);
	BT_COMP_LOGD("Finalizing muxer component's message iterator: "
		"comp-addr=%p, muxer-comp-addr=%p, muxer-msg-iter-addr=%p, "
		"msg-iter-addr=%p",
		self_comp, muxer_comp, muxer_msg_iter, self_msg_iter);

	if (muxer_msg_iter) {
		destroy_muxer_msg_iter(muxer_msg_iter);
	}
}

BT_HIDDEN
bt_message_iterator_class_next_method_status muxer_msg_iter_next(
		bt_self_message_iterator *self_msg_iter,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	bt_message_iterator_class_next_method_status status;
	struct muxer_msg_iter *muxer_msg_iter =
		bt_self_message_iterator_get_data(self_msg_iter);
	bt_self_component *self_comp = NULL;
	struct muxer_comp *muxer_comp = NULL;

	BT_ASSERT_DBG(muxer_msg_iter);
	self_comp = bt_self_message_iterator_borrow_component(
		self_msg_iter);
	BT_ASSERT_DBG(self_comp);
	muxer_comp = bt_self_component_get_data(self_comp);
	BT_ASSERT_DBG(muxer_comp);
	BT_COMP_LOGT("Muxer component's message iterator's \"next\" method called: "
		"comp-addr=%p, muxer-comp-addr=%p, muxer-msg-iter-addr=%p, "
		"msg-iter-addr=%p",
		self_comp, muxer_comp, muxer_msg_iter, self_msg_iter);

	status = muxer_msg_iter_do_next(muxer_comp, muxer_msg_iter,
		msgs, capacity, count);
	if (status < 0) {
		BT_COMP_LOGE("Cannot get next message: "
			"comp-addr=%p, muxer-comp-addr=%p, muxer-msg-iter-addr=%p, "
			"msg-iter-addr=%p, status=%s",
			self_comp, muxer_comp, muxer_msg_iter, self_msg_iter,
			bt_common_func_status_string(status));
	} else {
		BT_COMP_LOGT("Returning from muxer component's message iterator's \"next\" method: "
			"status=%s",
			bt_common_func_status_string(status));
	}

	return status;
}

BT_HIDDEN
bt_component_class_port_connected_method_status muxer_input_port_connected(
		bt_self_component_filter *self_comp,
		bt_self_component_port_input *self_port,
		const bt_port_output *other_port)
{
	bt_component_class_port_connected_method_status status =
		BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_OK;
	bt_self_component_add_port_status add_port_status;
	struct muxer_comp *muxer_comp = bt_self_component_get_data(
		bt_self_component_filter_as_self_component(self_comp));

	add_port_status = add_available_input_port(self_comp);
	if (add_port_status) {
		BT_COMP_LOGE("Cannot add one muxer component's input port: "
			"status=%s",
			bt_common_func_status_string(status));

		if (add_port_status ==
				BT_SELF_COMPONENT_ADD_PORT_STATUS_MEMORY_ERROR) {
			status = BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_MEMORY_ERROR;
		} else {
			status = BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_ERROR;
		}

		goto end;
	}

end:
	return status;
}

static inline
bt_message_iterator_class_can_seek_beginning_method_status
muxer_upstream_msg_iters_can_all_seek_beginning(
		GPtrArray *muxer_upstream_msg_iters, bt_bool *can_seek)
{
	bt_message_iterator_class_can_seek_beginning_method_status status =
		BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_OK;
	uint64_t i;

	for (i = 0; i < muxer_upstream_msg_iters->len; i++) {
		struct muxer_upstream_msg_iter *upstream_msg_iter =
			muxer_upstream_msg_iters->pdata[i];
		status = (int) bt_message_iterator_can_seek_beginning(
			upstream_msg_iter->msg_iter, can_seek);
		if (status != BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_OK) {
			goto end;
		}

		if (!*can_seek) {
			goto end;
		}
	}

	*can_seek = BT_TRUE;

end:
	return status;
}

BT_HIDDEN
bt_message_iterator_class_can_seek_beginning_method_status
muxer_msg_iter_can_seek_beginning(
		bt_self_message_iterator *self_msg_iter, bt_bool *can_seek)
{
	struct muxer_msg_iter *muxer_msg_iter =
		bt_self_message_iterator_get_data(self_msg_iter);
	bt_message_iterator_class_can_seek_beginning_method_status status;

	status = muxer_upstream_msg_iters_can_all_seek_beginning(
		muxer_msg_iter->active_muxer_upstream_msg_iters, can_seek);
	if (status != BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_OK) {
		goto end;
	}

	if (!*can_seek) {
		goto end;
	}

	status = muxer_upstream_msg_iters_can_all_seek_beginning(
		muxer_msg_iter->ended_muxer_upstream_msg_iters, can_seek);

end:
	return status;
}

BT_HIDDEN
bt_message_iterator_class_seek_beginning_method_status muxer_msg_iter_seek_beginning(
		bt_self_message_iterator *self_msg_iter)
{
	struct muxer_msg_iter *muxer_msg_iter =
		bt_self_message_iterator_get_data(self_msg_iter);
	bt_message_iterator_class_seek_beginning_method_status status =
		BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_OK;
	bt_message_iterator_seek_beginning_status seek_beg_status;
	uint64_t i;

	/* Seek all ended upstream iterators first */
	for (i = 0; i < muxer_msg_iter->ended_muxer_upstream_msg_iters->len;
			i++) {
		struct muxer_upstream_msg_iter *upstream_msg_iter =
			muxer_msg_iter->ended_muxer_upstream_msg_iters->pdata[i];

		seek_beg_status = bt_message_iterator_seek_beginning(
			upstream_msg_iter->msg_iter);
		if (seek_beg_status != BT_MESSAGE_ITERATOR_SEEK_BEGINNING_STATUS_OK) {
			status = (int) seek_beg_status;
			goto end;
		}

		empty_message_queue(upstream_msg_iter);
	}

	/* Seek all previously active upstream iterators */
	for (i = 0; i < muxer_msg_iter->active_muxer_upstream_msg_iters->len;
			i++) {
		struct muxer_upstream_msg_iter *upstream_msg_iter =
			muxer_msg_iter->active_muxer_upstream_msg_iters->pdata[i];

		seek_beg_status = bt_message_iterator_seek_beginning(
			upstream_msg_iter->msg_iter);
		if (seek_beg_status != BT_MESSAGE_ITERATOR_SEEK_BEGINNING_STATUS_OK) {
			status = (int) seek_beg_status;
			goto end;
		}

		empty_message_queue(upstream_msg_iter);
	}

	/* Make them all active */
	for (i = 0; i < muxer_msg_iter->ended_muxer_upstream_msg_iters->len;
			i++) {
		struct muxer_upstream_msg_iter *upstream_msg_iter =
			muxer_msg_iter->ended_muxer_upstream_msg_iters->pdata[i];

		g_ptr_array_add(muxer_msg_iter->active_muxer_upstream_msg_iters,
			upstream_msg_iter);
		muxer_msg_iter->ended_muxer_upstream_msg_iters->pdata[i] = NULL;
	}

	/*
	 * GLib < 2.48.0 asserts when g_ptr_array_remove_range() is
	 * called on an empty array.
	 */
	if (muxer_msg_iter->ended_muxer_upstream_msg_iters->len > 0) {
		g_ptr_array_remove_range(muxer_msg_iter->ended_muxer_upstream_msg_iters,
			0, muxer_msg_iter->ended_muxer_upstream_msg_iters->len);
	}
	muxer_msg_iter->last_returned_ts_ns = INT64_MIN;
	muxer_msg_iter->clock_class_expectation =
		MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_ANY;

end:
	return status;
}
