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

#define BT_LOG_TAG "PLUGIN-UTILS-MUXER-FLT"
#include "logging.h"

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/compat/uuid-internal.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/value-internal.h>
#include <babeltrace/graph/component-internal.h>
#include <babeltrace/graph/message-iterator-internal.h>
#include <babeltrace/graph/connection-internal.h>
#include <plugins-common.h>
#include <glib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/common-internal.h>
#include <stdlib.h>
#include <string.h>

#define ASSUME_ABSOLUTE_CLOCK_CLASSES_PARAM_NAME	"assume-absolute-clock-classes"

struct muxer_comp {
	/*
	 * Array of struct
	 * bt_self_message_iterator *
	 * (weak refs)
	 */
	GPtrArray *muxer_msg_iters;

	/* Weak ref */
	bt_self_component_filter *self_comp;

	unsigned int next_port_num;
	size_t available_input_ports;
	bool initializing_muxer_msg_iter;
	bool assume_absolute_clock_classes;
};

struct muxer_upstream_msg_iter {
	/* Owned by this, NULL if ended */
	bt_self_component_port_input_message_iterator *msg_iter;

	/* Contains `const bt_message *`, owned by this */
	GQueue *msgs;
};

enum muxer_msg_iter_clock_class_expectation {
	MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_ANY = 0,
	MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_ABSOLUTE,
	MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_NOT_ABS_SPEC_UUID,
	MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_NOT_ABS_NO_UUID,
};

struct muxer_msg_iter {
	/*
	 * Array of struct muxer_upstream_msg_iter * (owned by this).
	 *
	 * NOTE: This array is searched in linearly to find the youngest
	 * current message. Keep this until benchmarks confirm that
	 * another data structure is faster than this for our typical
	 * use cases.
	 */
	GPtrArray *muxer_upstream_msg_iters;

	/*
	 * List of "recently" connected input ports (weak) to
	 * handle by this muxer message iterator.
	 * muxer_port_connected() adds entries to this list, and the
	 * entries are removed when a message iterator is created
	 * on the port's connection and put into
	 * muxer_upstream_msg_iters above by
	 * muxer_msg_iter_handle_newly_connected_ports().
	 */
	GList *newly_connected_self_ports;

	/* Last time returned in a message */
	int64_t last_returned_ts_ns;

	/* Clock class expectation state */
	enum muxer_msg_iter_clock_class_expectation clock_class_expectation;

	/*
	 * Expected clock class UUID, only valid when
	 * clock_class_expectation is
	 * MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_NOT_ABS_SPEC_UUID.
	 */
	unsigned char expected_clock_class_uuid[BABELTRACE_UUID_LEN];
};

static
void destroy_muxer_upstream_msg_iter(
		struct muxer_upstream_msg_iter *muxer_upstream_msg_iter)
{
	if (!muxer_upstream_msg_iter) {
		return;
	}

	BT_LOGD("Destroying muxer's upstream message iterator wrapper: "
		"addr=%p, msg-iter-addr=%p, queue-len=%u",
		muxer_upstream_msg_iter,
		muxer_upstream_msg_iter->msg_iter,
		muxer_upstream_msg_iter->msgs->length);
	bt_self_component_port_input_message_iterator_put_ref(muxer_upstream_msg_iter->msg_iter);

	if (muxer_upstream_msg_iter->msgs) {
		const bt_message *msg;

		while ((msg = g_queue_pop_head(
				muxer_upstream_msg_iter->msgs))) {
			bt_message_put_ref(msg);
		}

		g_queue_free(muxer_upstream_msg_iter->msgs);
	}

	g_free(muxer_upstream_msg_iter);
}

static
struct muxer_upstream_msg_iter *muxer_msg_iter_add_upstream_msg_iter(
		struct muxer_msg_iter *muxer_msg_iter,
		bt_self_component_port_input_message_iterator *self_msg_iter)
{
	struct muxer_upstream_msg_iter *muxer_upstream_msg_iter =
		g_new0(struct muxer_upstream_msg_iter, 1);

	if (!muxer_upstream_msg_iter) {
		BT_LOGE_STR("Failed to allocate one muxer's upstream message iterator wrapper.");
		goto end;
	}

	muxer_upstream_msg_iter->msg_iter = self_msg_iter;
	bt_self_component_port_input_message_iterator_get_ref(muxer_upstream_msg_iter->msg_iter);
	muxer_upstream_msg_iter->msgs = g_queue_new();
	if (!muxer_upstream_msg_iter->msgs) {
		BT_LOGE_STR("Failed to allocate a GQueue.");
		goto end;
	}

	g_ptr_array_add(muxer_msg_iter->muxer_upstream_msg_iters,
		muxer_upstream_msg_iter);
	BT_LOGD("Added muxer's upstream message iterator wrapper: "
		"addr=%p, muxer-msg-iter-addr=%p, msg-iter-addr=%p",
		muxer_upstream_msg_iter, muxer_msg_iter,
		self_msg_iter);

end:
	return muxer_upstream_msg_iter;
}

static
enum bt_self_component_status ensure_available_input_port(
		bt_self_component_filter *self_comp)
{
	struct muxer_comp *muxer_comp = bt_self_component_get_data(
		bt_self_component_filter_as_self_component(self_comp));
	enum bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	GString *port_name = NULL;

	BT_ASSERT(muxer_comp);

	if (muxer_comp->available_input_ports >= 1) {
		goto end;
	}

	port_name = g_string_new("in");
	if (!port_name) {
		BT_LOGE_STR("Failed to allocate a GString.");
		status = BT_SELF_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	g_string_append_printf(port_name, "%u", muxer_comp->next_port_num);
	status = bt_self_component_filter_add_input_port(
		self_comp, port_name->str, NULL, NULL);
	if (status != BT_SELF_COMPONENT_STATUS_OK) {
		BT_LOGE("Cannot add input port to muxer component: "
			"port-name=\"%s\", comp-addr=%p, status=%s",
			port_name->str, self_comp,
			bt_self_component_status_string(status));
		goto end;
	}

	muxer_comp->available_input_ports++;
	muxer_comp->next_port_num++;
	BT_LOGD("Added one input port to muxer component: "
		"port-name=\"%s\", comp-addr=%p",
		port_name->str, self_comp);
end:
	if (port_name) {
		g_string_free(port_name, TRUE);
	}

	return status;
}

static
enum bt_self_component_status create_output_port(
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

	BT_LOGD("Destroying muxer component: muxer-comp-addr=%p, "
		"muxer-msg-iter-count=%u", muxer_comp,
		muxer_comp->muxer_msg_iters ?
			muxer_comp->muxer_msg_iters->len : 0);

	if (muxer_comp->muxer_msg_iters) {
		g_ptr_array_free(muxer_comp->muxer_msg_iters, TRUE);
	}

	g_free(muxer_comp);
}

static
bt_value *get_default_params(void)
{
	bt_value *params;
	int ret;

	params = bt_value_map_create();
	if (!params) {
		BT_LOGE_STR("Cannot create a map value object.");
		goto error;
	}

	ret = bt_value_map_insert_bool_entry(params,
		ASSUME_ABSOLUTE_CLOCK_CLASSES_PARAM_NAME, false);
	if (ret) {
		BT_LOGE_STR("Cannot add boolean value to map value object.");
		goto error;
	}

	goto end;

error:
	BT_VALUE_PUT_REF_AND_RESET(params);

end:
	return params;
}

static
int configure_muxer_comp(struct muxer_comp *muxer_comp,
		const bt_value *params)
{
	bt_value *default_params = NULL;
	bt_value *real_params = NULL;
	const bt_value *assume_absolute_clock_classes = NULL;
	int ret = 0;
	bt_bool bool_val;

	default_params = get_default_params();
	if (!default_params) {
		BT_LOGE("Cannot get default parameters: "
			"muxer-comp-addr=%p", muxer_comp);
		goto error;
	}

	ret = bt_value_map_extend(default_params, params, &real_params);
	if (ret) {
		BT_LOGE("Cannot extend default parameters map value: "
			"muxer-comp-addr=%p, def-params-addr=%p, "
			"params-addr=%p", muxer_comp, default_params,
			params);
		goto error;
	}

	assume_absolute_clock_classes = bt_value_map_borrow_entry_value(real_params,
									ASSUME_ABSOLUTE_CLOCK_CLASSES_PARAM_NAME);
	if (assume_absolute_clock_classes &&
			!bt_value_is_bool(assume_absolute_clock_classes)) {
		BT_LOGE("Expecting a boolean value for the `%s` parameter: "
			"muxer-comp-addr=%p, value-type=%s",
			ASSUME_ABSOLUTE_CLOCK_CLASSES_PARAM_NAME, muxer_comp,
			bt_common_value_type_string(
				bt_value_get_type(assume_absolute_clock_classes)));
		goto error;
	}

	bool_val = bt_value_bool_get(assume_absolute_clock_classes);
	muxer_comp->assume_absolute_clock_classes = (bool) bool_val;
	BT_LOGD("Configured muxer component: muxer-comp-addr=%p, "
		"assume-absolute-clock-classes=%d",
		muxer_comp, muxer_comp->assume_absolute_clock_classes);
	goto end;

error:
	ret = -1;

end:
	bt_value_put_ref(default_params);
	bt_value_put_ref(real_params);
	return ret;
}

BT_HIDDEN
enum bt_self_component_status muxer_init(
		bt_self_component_filter *self_comp,
		bt_value *params, void *init_data)
{
	int ret;
	enum bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	struct muxer_comp *muxer_comp = g_new0(struct muxer_comp, 1);

	BT_LOGD("Initializing muxer component: "
		"comp-addr=%p, params-addr=%p", self_comp, params);

	if (!muxer_comp) {
		BT_LOGE_STR("Failed to allocate one muxer component.");
		goto error;
	}

	ret = configure_muxer_comp(muxer_comp, params);
	if (ret) {
		BT_LOGE("Cannot configure muxer component: "
			"muxer-comp-addr=%p, params-addr=%p",
			muxer_comp, params);
		goto error;
	}

	muxer_comp->muxer_msg_iters = g_ptr_array_new();
	if (!muxer_comp->muxer_msg_iters) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
		goto error;
	}

	muxer_comp->self_comp = self_comp;
	bt_self_component_set_data(
		bt_self_component_filter_as_self_component(self_comp),
		muxer_comp);
	status = ensure_available_input_port(self_comp);
	if (status != BT_SELF_COMPONENT_STATUS_OK) {
		BT_LOGE("Cannot ensure that at least one muxer component's input port is available: "
			"muxer-comp-addr=%p, status=%s",
			muxer_comp,
			bt_self_component_status_string(status));
		goto error;
	}

	status = create_output_port(self_comp);
	if (status) {
		BT_LOGE("Cannot create muxer component's output port: "
			"muxer-comp-addr=%p, status=%s",
			muxer_comp,
			bt_self_component_status_string(status));
		goto error;
	}

	BT_LOGD("Initialized muxer component: "
		"comp-addr=%p, params-addr=%p, muxer-comp-addr=%p",
		self_comp, params, muxer_comp);

	goto end;

error:
	destroy_muxer_comp(muxer_comp);
	bt_self_component_set_data(
		bt_self_component_filter_as_self_component(self_comp),
		NULL);

	if (status == BT_SELF_COMPONENT_STATUS_OK) {
		status = BT_SELF_COMPONENT_STATUS_ERROR;
	}

end:
	return status;
}

BT_HIDDEN
void muxer_finalize(bt_self_component_filter *self_comp)
{
	struct muxer_comp *muxer_comp = bt_self_component_get_data(
		bt_self_component_filter_as_self_component(self_comp));

	BT_LOGD("Finalizing muxer component: comp-addr=%p",
		self_comp);
	destroy_muxer_comp(muxer_comp);
}

static
bt_self_component_port_input_message_iterator *
create_msg_iter_on_input_port(
		bt_self_component_port_input *self_port, int *ret)
{
	const bt_port *port = bt_self_component_port_as_port(
		bt_self_component_port_input_as_self_component_port(
			self_port));
	bt_self_component_port_input_message_iterator *msg_iter =
		NULL;

	BT_ASSERT(ret);
	*ret = 0;
	BT_ASSERT(port);
	BT_ASSERT(bt_port_is_connected(port));

	// TODO: Advance the iterator to >= the time of the latest
	//       returned message by the muxer message
	//       iterator which creates it.
	msg_iter = bt_self_component_port_input_message_iterator_create(
		self_port);
	if (!msg_iter) {
		BT_LOGE("Cannot create upstream message iterator on input port: "
			"port-addr=%p, port-name=\"%s\"",
			port, bt_port_get_name(port));
		*ret = -1;
		goto end;
	}

	BT_LOGD("Created upstream message iterator on input port: "
		"port-addr=%p, port-name=\"%s\", msg-iter-addr=%p",
		port, bt_port_get_name(port), msg_iter);

end:
	return msg_iter;
}

static
enum bt_message_iterator_status muxer_upstream_msg_iter_next(
		struct muxer_upstream_msg_iter *muxer_upstream_msg_iter)
{
	enum bt_message_iterator_status status;
	bt_message_array_const msgs;
	uint64_t i;
	uint64_t count;

	BT_LOGV("Calling upstream message iterator's \"next\" method: "
		"muxer-upstream-msg-iter-wrap-addr=%p, msg-iter-addr=%p",
		muxer_upstream_msg_iter,
		muxer_upstream_msg_iter->msg_iter);
	status = bt_self_component_port_input_message_iterator_next(
		muxer_upstream_msg_iter->msg_iter, &msgs, &count);
	BT_LOGV("Upstream message iterator's \"next\" method returned: "
		"status=%s", bt_message_iterator_status_string(status));

	switch (status) {
	case BT_MESSAGE_ITERATOR_STATUS_OK:
		/*
		 * Message iterator's current message is
		 * valid: it must be considered for muxing operations.
		 */
		BT_LOGV_STR("Validated upstream message iterator wrapper.");
		BT_ASSERT(count > 0);

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
		break;
	case BT_MESSAGE_ITERATOR_STATUS_AGAIN:
		/*
		 * Message iterator's current message is not
		 * valid anymore. Return
		 * BT_MESSAGE_ITERATOR_STATUS_AGAIN immediately.
		 */
		break;
	case BT_MESSAGE_ITERATOR_STATUS_END:	/* Fall-through. */
	case BT_MESSAGE_ITERATOR_STATUS_CANCELED:
		/*
		 * Message iterator reached the end: release it. It
		 * won't be considered again to find the youngest
		 * message.
		 */
		BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_PUT_REF_AND_RESET(muxer_upstream_msg_iter->msg_iter);
		status = BT_MESSAGE_ITERATOR_STATUS_OK;
		break;
	default:
		/* Error or unsupported status code */
		BT_LOGE("Error or unsupported status code: "
			"status-code=%d", status);
		status = BT_MESSAGE_ITERATOR_STATUS_ERROR;
		break;
	}

	return status;
}

static
int muxer_msg_iter_handle_newly_connected_ports(
		struct muxer_msg_iter *muxer_msg_iter)
{
	int ret = 0;

	BT_LOGV("Handling newly connected ports: "
		"muxer-msg-iter-addr=%p", muxer_msg_iter);

	/*
	 * Here we create one upstream message iterator for each
	 * newly connected port. We do NOT perform an initial "next" on
	 * those new upstream message iterators: they are
	 * invalidated, to be validated later. The list of newly
	 * connected ports to handle here is updated by
	 * muxer_port_connected().
	 */
	while (true) {
		GList *node = muxer_msg_iter->newly_connected_self_ports;
		bt_self_component_port_input *self_port;
		const bt_port *port;
		bt_self_component_port_input_message_iterator *
			upstream_msg_iter = NULL;
		struct muxer_upstream_msg_iter *muxer_upstream_msg_iter;

		if (!node) {
			break;
		}

		self_port = node->data;
		port = bt_self_component_port_as_port(
			bt_self_component_port_input_as_self_component_port(
				(self_port)));
		BT_ASSERT(port);

		if (!bt_port_is_connected(port)) {
			/*
			 * Looks like this port is not connected
			 * anymore: we can't create an upstream
			 * message iterator on its (non-existing)
			 * connection in this case.
			 */
			goto remove_node;
		}

		upstream_msg_iter = create_msg_iter_on_input_port(
			self_port, &ret);
		if (ret) {
			/* create_msg_iter_on_input_port() logs errors */
			BT_ASSERT(!upstream_msg_iter);
			goto error;
		}

		muxer_upstream_msg_iter =
			muxer_msg_iter_add_upstream_msg_iter(
				muxer_msg_iter, upstream_msg_iter);
		BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_PUT_REF_AND_RESET(upstream_msg_iter);
		if (!muxer_upstream_msg_iter) {
			/*
			 * muxer_msg_iter_add_upstream_msg_iter()
			 * logs errors.
			 */
			goto error;
		}

remove_node:
		bt_self_component_port_input_message_iterator_put_ref(upstream_msg_iter);
		muxer_msg_iter->newly_connected_self_ports =
			g_list_delete_link(
				muxer_msg_iter->newly_connected_self_ports,
				node);
	}

	goto end;

error:
	if (ret >= 0) {
		ret = -1;
	}

end:
	return ret;
}

static
int get_msg_ts_ns(struct muxer_comp *muxer_comp,
		struct muxer_msg_iter *muxer_msg_iter,
		const bt_message *msg, int64_t last_returned_ts_ns,
		int64_t *ts_ns)
{
	const bt_clock_class *clock_class = NULL;
	const bt_clock_snapshot *clock_snapshot = NULL;
	const bt_event *event = NULL;
	int ret = 0;
	const unsigned char *cc_uuid;
	const char *cc_name;
	enum bt_clock_snapshot_status cv_status = BT_CLOCK_SNAPSHOT_STATUS_KNOWN;

	BT_ASSERT(msg);
	BT_ASSERT(ts_ns);

	BT_LOGV("Getting message's timestamp: "
		"muxer-msg-iter-addr=%p, msg-addr=%p, "
		"last-returned-ts=%" PRId64,
		muxer_msg_iter, msg, last_returned_ts_ns);

	switch (bt_message_get_type(msg)) {
	case BT_MESSAGE_TYPE_EVENT:
		event = bt_message_event_borrow_event_const(msg);
		BT_ASSERT(event);
		cv_status = bt_event_borrow_default_clock_snapshot_const(event,
			&clock_snapshot);
		break;

	case BT_MESSAGE_TYPE_INACTIVITY:
		clock_snapshot =
			bt_message_inactivity_borrow_default_clock_snapshot_const(
				msg);
		break;
	default:
		/* All the other messages have a higher priority */
		BT_LOGV_STR("Message has no timestamp: using the last returned timestamp.");
		*ts_ns = last_returned_ts_ns;
		goto end;
	}

	if (cv_status != BT_CLOCK_SNAPSHOT_STATUS_KNOWN) {
		BT_LOGE_STR("Unsupported unknown clock snapshot.");
		ret = -1;
		goto end;
	}

	/*
	 * If the clock snapshot is missing, then we consider that this
	 * message has no time. In this case it's always the
	 * youngest.
	 */
	if (!clock_snapshot) {
		BT_LOGV_STR("Message's default clock snapshot is missing: "
			"using the last returned timestamp.");
		*ts_ns = last_returned_ts_ns;
		goto end;
	}

	clock_class = bt_clock_snapshot_borrow_clock_class_const(clock_snapshot);
	BT_ASSERT(clock_class);
	cc_uuid = bt_clock_class_get_uuid(clock_class);
	cc_name = bt_clock_class_get_name(clock_class);

	if (muxer_msg_iter->clock_class_expectation ==
			MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_ANY) {
		/*
		 * This is the first clock class that this muxer
		 * message iterator encounters. Its properties
		 * determine what to expect for the whole lifetime of
		 * the iterator without a true
		 * `assume-absolute-clock-classes` parameter.
		 */
		if (bt_clock_class_is_absolute(clock_class)) {
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
				memcpy(muxer_msg_iter->expected_clock_class_uuid,
					cc_uuid, BABELTRACE_UUID_LEN);
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

	if (!muxer_comp->assume_absolute_clock_classes) {
		switch (muxer_msg_iter->clock_class_expectation) {
		case MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_ABSOLUTE:
			if (!bt_clock_class_is_absolute(clock_class)) {
				BT_LOGE("Expecting an absolute clock class, "
					"but got a non-absolute one: "
					"clock-class-addr=%p, clock-class-name=\"%s\"",
					clock_class, cc_name);
				goto error;
			}
			break;
		case MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_NOT_ABS_NO_UUID:
			if (bt_clock_class_is_absolute(clock_class)) {
				BT_LOGE("Expecting a non-absolute clock class with no UUID, "
					"but got an absolute one: "
					"clock-class-addr=%p, clock-class-name=\"%s\"",
					clock_class, cc_name);
				goto error;
			}

			if (cc_uuid) {
				BT_LOGE("Expecting a non-absolute clock class with no UUID, "
					"but got one with a UUID: "
					"clock-class-addr=%p, clock-class-name=\"%s\", "
					"uuid=\"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\"",
					clock_class, cc_name,
					(unsigned int) cc_uuid[0],
					(unsigned int) cc_uuid[1],
					(unsigned int) cc_uuid[2],
					(unsigned int) cc_uuid[3],
					(unsigned int) cc_uuid[4],
					(unsigned int) cc_uuid[5],
					(unsigned int) cc_uuid[6],
					(unsigned int) cc_uuid[7],
					(unsigned int) cc_uuid[8],
					(unsigned int) cc_uuid[9],
					(unsigned int) cc_uuid[10],
					(unsigned int) cc_uuid[11],
					(unsigned int) cc_uuid[12],
					(unsigned int) cc_uuid[13],
					(unsigned int) cc_uuid[14],
					(unsigned int) cc_uuid[15]);
				goto error;
			}
			break;
		case MUXER_MSG_ITER_CLOCK_CLASS_EXPECTATION_NOT_ABS_SPEC_UUID:
			if (bt_clock_class_is_absolute(clock_class)) {
				BT_LOGE("Expecting a non-absolute clock class with a specific UUID, "
					"but got an absolute one: "
					"clock-class-addr=%p, clock-class-name=\"%s\"",
					clock_class, cc_name);
				goto error;
			}

			if (!cc_uuid) {
				BT_LOGE("Expecting a non-absolute clock class with a specific UUID, "
					"but got one with no UUID: "
					"clock-class-addr=%p, clock-class-name=\"%s\"",
					clock_class, cc_name);
				goto error;
			}

			if (memcmp(muxer_msg_iter->expected_clock_class_uuid,
					cc_uuid, BABELTRACE_UUID_LEN) != 0) {
				BT_LOGE("Expecting a non-absolute clock class with a specific UUID, "
					"but got one with different UUID: "
					"clock-class-addr=%p, clock-class-name=\"%s\", "
					"expected-uuid=\"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\", "
					"uuid=\"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\"",
					clock_class, cc_name,
					(unsigned int) muxer_msg_iter->expected_clock_class_uuid[0],
					(unsigned int) muxer_msg_iter->expected_clock_class_uuid[1],
					(unsigned int) muxer_msg_iter->expected_clock_class_uuid[2],
					(unsigned int) muxer_msg_iter->expected_clock_class_uuid[3],
					(unsigned int) muxer_msg_iter->expected_clock_class_uuid[4],
					(unsigned int) muxer_msg_iter->expected_clock_class_uuid[5],
					(unsigned int) muxer_msg_iter->expected_clock_class_uuid[6],
					(unsigned int) muxer_msg_iter->expected_clock_class_uuid[7],
					(unsigned int) muxer_msg_iter->expected_clock_class_uuid[8],
					(unsigned int) muxer_msg_iter->expected_clock_class_uuid[9],
					(unsigned int) muxer_msg_iter->expected_clock_class_uuid[10],
					(unsigned int) muxer_msg_iter->expected_clock_class_uuid[11],
					(unsigned int) muxer_msg_iter->expected_clock_class_uuid[12],
					(unsigned int) muxer_msg_iter->expected_clock_class_uuid[13],
					(unsigned int) muxer_msg_iter->expected_clock_class_uuid[14],
					(unsigned int) muxer_msg_iter->expected_clock_class_uuid[15],
					(unsigned int) cc_uuid[0],
					(unsigned int) cc_uuid[1],
					(unsigned int) cc_uuid[2],
					(unsigned int) cc_uuid[3],
					(unsigned int) cc_uuid[4],
					(unsigned int) cc_uuid[5],
					(unsigned int) cc_uuid[6],
					(unsigned int) cc_uuid[7],
					(unsigned int) cc_uuid[8],
					(unsigned int) cc_uuid[9],
					(unsigned int) cc_uuid[10],
					(unsigned int) cc_uuid[11],
					(unsigned int) cc_uuid[12],
					(unsigned int) cc_uuid[13],
					(unsigned int) cc_uuid[14],
					(unsigned int) cc_uuid[15]);
				goto error;
			}
			break;
		default:
			/* Unexpected */
			BT_LOGF("Unexpected clock class expectation: "
				"expectation-code=%d",
				muxer_msg_iter->clock_class_expectation);
			abort();
		}
	}

	ret = bt_clock_snapshot_get_ns_from_origin(clock_snapshot, ts_ns);
	if (ret) {
		BT_LOGE("Cannot get nanoseconds from Epoch of clock snapshot: "
			"clock-snapshot-addr=%p", clock_snapshot);
		goto error;
	}

	goto end;

error:
	ret = -1;

end:
	if (ret == 0) {
		BT_LOGV("Found message's timestamp: "
			"muxer-msg-iter-addr=%p, msg-addr=%p, "
			"last-returned-ts=%" PRId64 ", ts=%" PRId64,
			muxer_msg_iter, msg, last_returned_ts_ns,
			*ts_ns);
	}

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
 * * Check for newly connected ports.
 * * Check the upstream message iterators to retry.
 *
 * On sucess, this function sets *muxer_upstream_msg_iter to the
 * upstream message iterator of which the current message is
 * the youngest, and sets *ts_ns to its time.
 */
static
enum bt_message_iterator_status
muxer_msg_iter_youngest_upstream_msg_iter(
		struct muxer_comp *muxer_comp,
		struct muxer_msg_iter *muxer_msg_iter,
		struct muxer_upstream_msg_iter **muxer_upstream_msg_iter,
		int64_t *ts_ns)
{
	size_t i;
	int ret;
	int64_t youngest_ts_ns = INT64_MAX;
	enum bt_message_iterator_status status =
		BT_MESSAGE_ITERATOR_STATUS_OK;

	BT_ASSERT(muxer_comp);
	BT_ASSERT(muxer_msg_iter);
	BT_ASSERT(muxer_upstream_msg_iter);
	*muxer_upstream_msg_iter = NULL;

	for (i = 0; i < muxer_msg_iter->muxer_upstream_msg_iters->len; i++) {
		const bt_message *msg;
		struct muxer_upstream_msg_iter *cur_muxer_upstream_msg_iter =
			g_ptr_array_index(muxer_msg_iter->muxer_upstream_msg_iters, i);
		int64_t msg_ts_ns;

		if (!cur_muxer_upstream_msg_iter->msg_iter) {
			/* This upstream message iterator is ended */
			BT_LOGV("Skipping ended upstream message iterator: "
				"muxer-upstream-msg-iter-wrap-addr=%p",
				cur_muxer_upstream_msg_iter);
			continue;
		}

		BT_ASSERT(cur_muxer_upstream_msg_iter->msgs->length > 0);
		msg = g_queue_peek_head(cur_muxer_upstream_msg_iter->msgs);
		BT_ASSERT(msg);
		ret = get_msg_ts_ns(muxer_comp, muxer_msg_iter, msg,
			muxer_msg_iter->last_returned_ts_ns, &msg_ts_ns);
		if (ret) {
			/* get_msg_ts_ns() logs errors */
			*muxer_upstream_msg_iter = NULL;
			status = BT_MESSAGE_ITERATOR_STATUS_ERROR;
			goto end;
		}

		if (msg_ts_ns <= youngest_ts_ns) {
			*muxer_upstream_msg_iter =
				cur_muxer_upstream_msg_iter;
			youngest_ts_ns = msg_ts_ns;
			*ts_ns = youngest_ts_ns;
		}
	}

	if (!*muxer_upstream_msg_iter) {
		status = BT_MESSAGE_ITERATOR_STATUS_END;
		*ts_ns = INT64_MIN;
	}

end:
	return status;
}

static
enum bt_message_iterator_status validate_muxer_upstream_msg_iter(
	struct muxer_upstream_msg_iter *muxer_upstream_msg_iter)
{
	enum bt_message_iterator_status status =
		BT_MESSAGE_ITERATOR_STATUS_OK;

	BT_LOGV("Validating muxer's upstream message iterator wrapper: "
		"muxer-upstream-msg-iter-wrap-addr=%p",
		muxer_upstream_msg_iter);

	if (muxer_upstream_msg_iter->msgs->length > 0 ||
			!muxer_upstream_msg_iter->msg_iter) {
		BT_LOGV("Already valid or not considered: "
			"queue-len=%u, upstream-msg-iter-addr=%p",
			muxer_upstream_msg_iter->msgs->length,
			muxer_upstream_msg_iter->msg_iter);
		goto end;
	}

	/* muxer_upstream_msg_iter_next() logs details/errors */
	status = muxer_upstream_msg_iter_next(muxer_upstream_msg_iter);

end:
	return status;
}

static
enum bt_message_iterator_status validate_muxer_upstream_msg_iters(
		struct muxer_msg_iter *muxer_msg_iter)
{
	enum bt_message_iterator_status status =
		BT_MESSAGE_ITERATOR_STATUS_OK;
	size_t i;

	BT_LOGV("Validating muxer's upstream message iterator wrappers: "
		"muxer-msg-iter-addr=%p", muxer_msg_iter);

	for (i = 0; i < muxer_msg_iter->muxer_upstream_msg_iters->len; i++) {
		struct muxer_upstream_msg_iter *muxer_upstream_msg_iter =
			g_ptr_array_index(
				muxer_msg_iter->muxer_upstream_msg_iters,
				i);

		status = validate_muxer_upstream_msg_iter(
			muxer_upstream_msg_iter);
		if (status != BT_MESSAGE_ITERATOR_STATUS_OK) {
			if (status < 0) {
				BT_LOGE("Cannot validate muxer's upstream message iterator wrapper: "
					"muxer-msg-iter-addr=%p, "
					"muxer-upstream-msg-iter-wrap-addr=%p",
					muxer_msg_iter,
					muxer_upstream_msg_iter);
			} else {
				BT_LOGV("Cannot validate muxer's upstream message iterator wrapper: "
					"muxer-msg-iter-addr=%p, "
					"muxer-upstream-msg-iter-wrap-addr=%p",
					muxer_msg_iter,
					muxer_upstream_msg_iter);
			}

			goto end;
		}

		/*
		 * Remove this muxer upstream message iterator
		 * if it's ended or canceled.
		 */
		if (!muxer_upstream_msg_iter->msg_iter) {
			/*
			 * Use g_ptr_array_remove_fast() because the
			 * order of those elements is not important.
			 */
			BT_LOGV("Removing muxer's upstream message iterator wrapper: ended or canceled: "
				"muxer-msg-iter-addr=%p, "
				"muxer-upstream-msg-iter-wrap-addr=%p",
				muxer_msg_iter, muxer_upstream_msg_iter);
			g_ptr_array_remove_index_fast(
				muxer_msg_iter->muxer_upstream_msg_iters,
				i);
			i--;
		}
	}

end:
	return status;
}

static inline
enum bt_message_iterator_status muxer_msg_iter_do_next_one(
		struct muxer_comp *muxer_comp,
		struct muxer_msg_iter *muxer_msg_iter,
		const bt_message **msg)
{
	enum bt_message_iterator_status status =
		BT_MESSAGE_ITERATOR_STATUS_OK;
	struct muxer_upstream_msg_iter *muxer_upstream_msg_iter = NULL;
	int64_t next_return_ts;

	while (true) {
		int ret = muxer_msg_iter_handle_newly_connected_ports(
			muxer_msg_iter);

		if (ret) {
			BT_LOGE("Cannot handle newly connected input ports for muxer's message iterator: "
				"muxer-comp-addr=%p, muxer-msg-iter-addr=%p, "
				"ret=%d",
				muxer_comp, muxer_msg_iter, ret);
			status = BT_MESSAGE_ITERATOR_STATUS_ERROR;
			goto end;
		}

		status = validate_muxer_upstream_msg_iters(muxer_msg_iter);
		if (status != BT_MESSAGE_ITERATOR_STATUS_OK) {
			/* validate_muxer_upstream_msg_iters() logs details */
			goto end;
		}

		/*
		 * At this point, we know that all the existing upstream
		 * message iterators are valid. However the
		 * operations to validate them (during
		 * validate_muxer_upstream_msg_iters()) may have
		 * connected new ports. If no ports were connected
		 * during this operation, exit the loop.
		 */
		if (!muxer_msg_iter->newly_connected_self_ports) {
			BT_LOGV("Not breaking this loop: muxer's message iterator still has newly connected input ports to handle: "
				"muxer-comp-addr=%p", muxer_comp);
			break;
		}
	}

	BT_ASSERT(!muxer_msg_iter->newly_connected_self_ports);

	/*
	 * At this point we know that all the existing upstream
	 * message iterators are valid. We can find the one,
	 * amongst those, of which the current message is the
	 * youngest.
	 */
	status = muxer_msg_iter_youngest_upstream_msg_iter(muxer_comp,
			muxer_msg_iter, &muxer_upstream_msg_iter,
			&next_return_ts);
	if (status < 0 || status == BT_MESSAGE_ITERATOR_STATUS_END ||
			status == BT_MESSAGE_ITERATOR_STATUS_CANCELED) {
		if (status < 0) {
			BT_LOGE("Cannot find the youngest upstream message iterator wrapper: "
				"status=%s",
				bt_message_iterator_status_string(status));
		} else {
			BT_LOGV("Cannot find the youngest upstream message iterator wrapper: "
				"status=%s",
				bt_message_iterator_status_string(status));
		}

		goto end;
	}

	if (next_return_ts < muxer_msg_iter->last_returned_ts_ns) {
		BT_LOGE("Youngest upstream message iterator wrapper's timestamp is less than muxer's message iterator's last returned timestamp: "
			"muxer-msg-iter-addr=%p, ts=%" PRId64 ", "
			"last-returned-ts=%" PRId64,
			muxer_msg_iter, next_return_ts,
			muxer_msg_iter->last_returned_ts_ns);
		status = BT_MESSAGE_ITERATOR_STATUS_ERROR;
		goto end;
	}

	BT_LOGV("Found youngest upstream message iterator wrapper: "
		"muxer-msg-iter-addr=%p, "
		"muxer-upstream-msg-iter-wrap-addr=%p, "
		"ts=%" PRId64,
		muxer_msg_iter, muxer_upstream_msg_iter, next_return_ts);
	BT_ASSERT(status == BT_MESSAGE_ITERATOR_STATUS_OK);
	BT_ASSERT(muxer_upstream_msg_iter);

	/*
	 * Consume from the queue's head: other side
	 * (muxer_upstream_msg_iter_next()) writes to the tail.
	 */
	*msg = g_queue_pop_head(muxer_upstream_msg_iter->msgs);
	BT_ASSERT(*msg);
	muxer_msg_iter->last_returned_ts_ns = next_return_ts;

end:
	return status;
}

static
enum bt_message_iterator_status muxer_msg_iter_do_next(
		struct muxer_comp *muxer_comp,
		struct muxer_msg_iter *muxer_msg_iter,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	enum bt_message_iterator_status status =
		BT_MESSAGE_ITERATOR_STATUS_OK;
	uint64_t i = 0;

	while (i < capacity && status == BT_MESSAGE_ITERATOR_STATUS_OK) {
		status = muxer_msg_iter_do_next_one(muxer_comp,
			muxer_msg_iter, &msgs[i]);
		if (status == BT_MESSAGE_ITERATOR_STATUS_OK) {
			i++;
		}
	}

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
		*count = i;
		status = BT_MESSAGE_ITERATOR_STATUS_OK;
	}

	return status;
}

static
void destroy_muxer_msg_iter(struct muxer_msg_iter *muxer_msg_iter)
{
	if (!muxer_msg_iter) {
		return;
	}

	BT_LOGD("Destroying muxer component's message iterator: "
		"muxer-msg-iter-addr=%p", muxer_msg_iter);

	if (muxer_msg_iter->muxer_upstream_msg_iters) {
		BT_LOGD_STR("Destroying muxer's upstream message iterator wrappers.");
		g_ptr_array_free(
			muxer_msg_iter->muxer_upstream_msg_iters, TRUE);
	}

	g_list_free(muxer_msg_iter->newly_connected_self_ports);
	g_free(muxer_msg_iter);
}

static
int muxer_msg_iter_init_newly_connected_ports(struct muxer_comp *muxer_comp,
		struct muxer_msg_iter *muxer_msg_iter)
{
	int64_t count;
	int64_t i;
	int ret = 0;

	/*
	 * Add the connected input ports to this muxer message
	 * iterator's list of newly connected ports. They will be
	 * handled by muxer_msg_iter_handle_newly_connected_ports().
	 */
	count = bt_component_filter_get_input_port_count(
		bt_self_component_filter_as_component_filter(
			muxer_comp->self_comp));
	if (count < 0) {
		BT_LOGD("No input port to initialize for muxer component's message iterator: "
			"muxer-comp-addr=%p, muxer-msg-iter-addr=%p",
			muxer_comp, muxer_msg_iter);
		goto end;
	}

	for (i = 0; i < count; i++) {
		bt_self_component_port_input *self_port =
			bt_self_component_filter_borrow_input_port_by_index(
				muxer_comp->self_comp, i);
		const bt_port *port;

		BT_ASSERT(self_port);
		port = bt_self_component_port_as_port(
			bt_self_component_port_input_as_self_component_port(
				self_port));
		BT_ASSERT(port);

		if (!bt_port_is_connected(port)) {
			BT_LOGD("Skipping input port: not connected: "
				"muxer-comp-addr=%p, port-addr=%p, port-name\"%s\"",
				muxer_comp, port, bt_port_get_name(port));
			continue;
		}

		muxer_msg_iter->newly_connected_self_ports =
			g_list_append(
				muxer_msg_iter->newly_connected_self_ports,
				self_port);
		if (!muxer_msg_iter->newly_connected_self_ports) {
			BT_LOGE("Cannot append port to muxer's message iterator list of newly connected input ports: "
				"port-addr=%p, port-name=\"%s\", "
				"muxer-msg-iter-addr=%p", port,
				bt_port_get_name(port), muxer_msg_iter);
			ret = -1;
			goto end;
		}

		BT_LOGD("Appended port to muxer's message iterator list of newly connected input ports: "
			"port-addr=%p, port-name=\"%s\", "
			"muxer-msg-iter-addr=%p", port,
			bt_port_get_name(port), muxer_msg_iter);
	}

end:
	return ret;
}

BT_HIDDEN
enum bt_self_message_iterator_status muxer_msg_iter_init(
		bt_self_message_iterator *self_msg_iter,
		bt_self_component_filter *self_comp,
		bt_self_component_port_output *port)
{
	struct muxer_comp *muxer_comp = NULL;
	struct muxer_msg_iter *muxer_msg_iter = NULL;
	enum bt_self_message_iterator_status status =
		BT_MESSAGE_ITERATOR_STATUS_OK;
	int ret;

	muxer_comp = bt_self_component_get_data(
		bt_self_component_filter_as_self_component(self_comp));
	BT_ASSERT(muxer_comp);
	BT_LOGD("Initializing muxer component's message iterator: "
		"comp-addr=%p, muxer-comp-addr=%p, msg-iter-addr=%p",
		self_comp, muxer_comp, self_msg_iter);

	if (muxer_comp->initializing_muxer_msg_iter) {
		/*
		 * Weird, unhandled situation detected: downstream
		 * creates a muxer message iterator while creating
		 * another muxer message iterator (same component).
		 */
		BT_LOGE("Recursive initialization of muxer component's message iterator: "
			"comp-addr=%p, muxer-comp-addr=%p, msg-iter-addr=%p",
			self_comp, muxer_comp, self_msg_iter);
		goto error;
	}

	muxer_comp->initializing_muxer_msg_iter = true;
	muxer_msg_iter = g_new0(struct muxer_msg_iter, 1);
	if (!muxer_msg_iter) {
		BT_LOGE_STR("Failed to allocate one muxer component's message iterator.");
		goto error;
	}

	muxer_msg_iter->last_returned_ts_ns = INT64_MIN;
	muxer_msg_iter->muxer_upstream_msg_iters =
		g_ptr_array_new_with_free_func(
			(GDestroyNotify) destroy_muxer_upstream_msg_iter);
	if (!muxer_msg_iter->muxer_upstream_msg_iters) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
		goto error;
	}

	/*
	 * Add the muxer message iterator to the component's array
	 * of muxer message iterators here because
	 * muxer_msg_iter_init_newly_connected_ports() can cause
	 * muxer_port_connected() to be called, which adds the newly
	 * connected port to each muxer message iterator's list of
	 * newly connected ports.
	 */
	g_ptr_array_add(muxer_comp->muxer_msg_iters, muxer_msg_iter);
	ret = muxer_msg_iter_init_newly_connected_ports(muxer_comp,
		muxer_msg_iter);
	if (ret) {
		BT_LOGE("Cannot initialize newly connected input ports for muxer component's message iterator: "
			"comp-addr=%p, muxer-comp-addr=%p, "
			"muxer-msg-iter-addr=%p, msg-iter-addr=%p, ret=%d",
			self_comp, muxer_comp, muxer_msg_iter,
			self_msg_iter, ret);
		goto error;
	}

	bt_self_message_iterator_set_data(self_msg_iter,
		muxer_msg_iter);
	BT_LOGD("Initialized muxer component's message iterator: "
		"comp-addr=%p, muxer-comp-addr=%p, muxer-msg-iter-addr=%p, "
		"msg-iter-addr=%p",
		self_comp, muxer_comp, muxer_msg_iter, self_msg_iter);
	goto end;

error:
	if (g_ptr_array_index(muxer_comp->muxer_msg_iters,
			muxer_comp->muxer_msg_iters->len - 1) == muxer_msg_iter) {
		g_ptr_array_remove_index(muxer_comp->muxer_msg_iters,
			muxer_comp->muxer_msg_iters->len - 1);
	}

	destroy_muxer_msg_iter(muxer_msg_iter);
	bt_self_message_iterator_set_data(self_msg_iter,
		NULL);
	status = BT_MESSAGE_ITERATOR_STATUS_ERROR;

end:
	muxer_comp->initializing_muxer_msg_iter = false;
	return status;
}

BT_HIDDEN
void muxer_msg_iter_finalize(
		bt_self_message_iterator *self_msg_iter)
{
	struct muxer_msg_iter *muxer_msg_iter =
		bt_self_message_iterator_get_data(self_msg_iter);
	bt_self_component *self_comp = NULL;
	struct muxer_comp *muxer_comp = NULL;

	self_comp = bt_self_message_iterator_borrow_component(
		self_msg_iter);
	BT_ASSERT(self_comp);
	muxer_comp = bt_self_component_get_data(self_comp);
	BT_LOGD("Finalizing muxer component's message iterator: "
		"comp-addr=%p, muxer-comp-addr=%p, muxer-msg-iter-addr=%p, "
		"msg-iter-addr=%p",
		self_comp, muxer_comp, muxer_msg_iter, self_msg_iter);

	if (muxer_comp) {
		(void) g_ptr_array_remove_fast(muxer_comp->muxer_msg_iters,
			muxer_msg_iter);
		destroy_muxer_msg_iter(muxer_msg_iter);
	}
}

BT_HIDDEN
enum bt_message_iterator_status muxer_msg_iter_next(
		bt_self_message_iterator *self_msg_iter,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	enum bt_message_iterator_status status;
	struct muxer_msg_iter *muxer_msg_iter =
		bt_self_message_iterator_get_data(self_msg_iter);
	bt_self_component *self_comp = NULL;
	struct muxer_comp *muxer_comp = NULL;

	BT_ASSERT(muxer_msg_iter);
	self_comp = bt_self_message_iterator_borrow_component(
		self_msg_iter);
	BT_ASSERT(self_comp);
	muxer_comp = bt_self_component_get_data(self_comp);
	BT_ASSERT(muxer_comp);
	BT_LOGV("Muxer component's message iterator's \"next\" method called: "
		"comp-addr=%p, muxer-comp-addr=%p, muxer-msg-iter-addr=%p, "
		"msg-iter-addr=%p",
		self_comp, muxer_comp, muxer_msg_iter, self_msg_iter);

	status = muxer_msg_iter_do_next(muxer_comp, muxer_msg_iter,
		msgs, capacity, count);
	if (status < 0) {
		BT_LOGE("Cannot get next message: "
			"comp-addr=%p, muxer-comp-addr=%p, muxer-msg-iter-addr=%p, "
			"msg-iter-addr=%p, status=%s",
			self_comp, muxer_comp, muxer_msg_iter, self_msg_iter,
			bt_message_iterator_status_string(status));
	} else {
		BT_LOGV("Returning from muxer component's message iterator's \"next\" method: "
			"status=%s",
			bt_message_iterator_status_string(status));
	}

	return status;
}

BT_HIDDEN
enum bt_self_component_status muxer_input_port_connected(
		bt_self_component_filter *self_comp,
		bt_self_component_port_input *self_port,
		const bt_port_output *other_port)
{
	enum bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	const bt_port *port = bt_self_component_port_as_port(
		bt_self_component_port_input_as_self_component_port(
			self_port));
	struct muxer_comp *muxer_comp =
		bt_self_component_get_data(
			bt_self_component_filter_as_self_component(
				self_comp));
	size_t i;
	int ret;

	BT_ASSERT(port);
	BT_ASSERT(muxer_comp);
	BT_LOGD("Port connected: "
		"comp-addr=%p, muxer-comp-addr=%p, "
		"port-addr=%p, port-name=\"%s\", "
		"other-port-addr=%p, other-port-name=\"%s\"",
		self_comp, muxer_comp, self_port, bt_port_get_name(port),
		other_port,
		bt_port_get_name(bt_port_output_as_port_const(other_port)));

	for (i = 0; i < muxer_comp->muxer_msg_iters->len; i++) {
		struct muxer_msg_iter *muxer_msg_iter =
			g_ptr_array_index(muxer_comp->muxer_msg_iters, i);

		/*
		 * Add this port to the list of newly connected ports
		 * for this muxer message iterator. We append at
		 * the end of this list while
		 * muxer_msg_iter_handle_newly_connected_ports()
		 * removes the nodes from the beginning.
		 */
		muxer_msg_iter->newly_connected_self_ports =
			g_list_append(
				muxer_msg_iter->newly_connected_self_ports,
				self_port);
		if (!muxer_msg_iter->newly_connected_self_ports) {
			BT_LOGE("Cannot append port to muxer's message iterator list of newly connected input ports: "
				"port-addr=%p, port-name=\"%s\", "
				"muxer-msg-iter-addr=%p", self_port,
				bt_port_get_name(port), muxer_msg_iter);
			status = BT_SELF_COMPONENT_STATUS_ERROR;
			goto end;
		}

		BT_LOGD("Appended port to muxer's message iterator list of newly connected input ports: "
			"port-addr=%p, port-name=\"%s\", "
			"muxer-msg-iter-addr=%p", self_port,
			bt_port_get_name(port), muxer_msg_iter);
	}

	/* One less available input port */
	muxer_comp->available_input_ports--;
	ret = ensure_available_input_port(self_comp);
	if (ret) {
		/*
		 * Only way to report an error later since this
		 * method does not return anything.
		 */
		BT_LOGE("Cannot ensure that at least one muxer component's input port is available: "
			"muxer-comp-addr=%p, status=%s",
			muxer_comp, bt_self_component_status_string(ret));
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

end:
	return status;
}

BT_HIDDEN
void muxer_input_port_disconnected(
		bt_self_component_filter *self_component,
		bt_self_component_port_input *self_port)
{
	struct muxer_comp *muxer_comp =
		bt_self_component_get_data(
			bt_self_component_filter_as_self_component(
				self_component));
	const bt_port *port =
		bt_self_component_port_as_port(
			bt_self_component_port_input_as_self_component_port(
				self_port));

	BT_ASSERT(port);
	BT_ASSERT(muxer_comp);

	/* One more available input port */
	muxer_comp->available_input_ports++;
	BT_LOGD("Leaving disconnected input port available for future connections: "
		"comp-addr=%p, muxer-comp-addr=%p, port-addr=%p, "
		"port-name=\"%s\", avail-input-port-count=%zu",
		self_component, muxer_comp, port, bt_port_get_name(port),
		muxer_comp->available_input_ports);
}
