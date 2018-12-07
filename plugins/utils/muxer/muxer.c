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
#include <babeltrace/graph/notification-iterator-internal.h>
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
	 * bt_self_notification_iterator *
	 * (weak refs)
	 */
	GPtrArray *muxer_notif_iters;

	/* Weak ref */
	struct bt_self_component_filter *self_comp;

	unsigned int next_port_num;
	size_t available_input_ports;
	bool initializing_muxer_notif_iter;
	bool assume_absolute_clock_classes;
};

struct muxer_upstream_notif_iter {
	/* Owned by this, NULL if ended */
	struct bt_self_component_port_input_notification_iterator *notif_iter;

	/* Contains `const struct bt_notification *`, owned by this */
	GQueue *notifs;
};

enum muxer_notif_iter_clock_class_expectation {
	MUXER_NOTIF_ITER_CLOCK_CLASS_EXPECTATION_ANY = 0,
	MUXER_NOTIF_ITER_CLOCK_CLASS_EXPECTATION_ABSOLUTE,
	MUXER_NOTIF_ITER_CLOCK_CLASS_EXPECTATION_NOT_ABS_SPEC_UUID,
	MUXER_NOTIF_ITER_CLOCK_CLASS_EXPECTATION_NOT_ABS_NO_UUID,
};

struct muxer_notif_iter {
	/*
	 * Array of struct muxer_upstream_notif_iter * (owned by this).
	 *
	 * NOTE: This array is searched in linearly to find the youngest
	 * current notification. Keep this until benchmarks confirm that
	 * another data structure is faster than this for our typical
	 * use cases.
	 */
	GPtrArray *muxer_upstream_notif_iters;

	/*
	 * List of "recently" connected input ports (weak) to
	 * handle by this muxer notification iterator.
	 * muxer_port_connected() adds entries to this list, and the
	 * entries are removed when a notification iterator is created
	 * on the port's connection and put into
	 * muxer_upstream_notif_iters above by
	 * muxer_notif_iter_handle_newly_connected_ports().
	 */
	GList *newly_connected_self_ports;

	/* Last time returned in a notification */
	int64_t last_returned_ts_ns;

	/* Clock class expectation state */
	enum muxer_notif_iter_clock_class_expectation clock_class_expectation;

	/*
	 * Expected clock class UUID, only valid when
	 * clock_class_expectation is
	 * MUXER_NOTIF_ITER_CLOCK_CLASS_EXPECTATION_NOT_ABS_SPEC_UUID.
	 */
	unsigned char expected_clock_class_uuid[BABELTRACE_UUID_LEN];
};

static
void destroy_muxer_upstream_notif_iter(
		struct muxer_upstream_notif_iter *muxer_upstream_notif_iter)
{
	if (!muxer_upstream_notif_iter) {
		return;
	}

	BT_LOGD("Destroying muxer's upstream notification iterator wrapper: "
		"addr=%p, notif-iter-addr=%p, queue-len=%u",
		muxer_upstream_notif_iter,
		muxer_upstream_notif_iter->notif_iter,
		muxer_upstream_notif_iter->notifs->length);
	bt_self_component_port_input_notification_iterator_put_ref(muxer_upstream_notif_iter->notif_iter);

	if (muxer_upstream_notif_iter->notifs) {
		const struct bt_notification *notif;

		while ((notif = g_queue_pop_head(
				muxer_upstream_notif_iter->notifs))) {
			bt_notification_put_ref(notif);
		}

		g_queue_free(muxer_upstream_notif_iter->notifs);
	}

	g_free(muxer_upstream_notif_iter);
}

static
struct muxer_upstream_notif_iter *muxer_notif_iter_add_upstream_notif_iter(
		struct muxer_notif_iter *muxer_notif_iter,
		struct bt_self_component_port_input_notification_iterator *self_notif_iter)
{
	struct muxer_upstream_notif_iter *muxer_upstream_notif_iter =
		g_new0(struct muxer_upstream_notif_iter, 1);

	if (!muxer_upstream_notif_iter) {
		BT_LOGE_STR("Failed to allocate one muxer's upstream notification iterator wrapper.");
		goto end;
	}

	muxer_upstream_notif_iter->notif_iter = self_notif_iter;
	bt_self_component_port_input_notification_iterator_get_ref(muxer_upstream_notif_iter->notif_iter);
	muxer_upstream_notif_iter->notifs = g_queue_new();
	if (!muxer_upstream_notif_iter->notifs) {
		BT_LOGE_STR("Failed to allocate a GQueue.");
		goto end;
	}

	g_ptr_array_add(muxer_notif_iter->muxer_upstream_notif_iters,
		muxer_upstream_notif_iter);
	BT_LOGD("Added muxer's upstream notification iterator wrapper: "
		"addr=%p, muxer-notif-iter-addr=%p, notif-iter-addr=%p",
		muxer_upstream_notif_iter, muxer_notif_iter,
		self_notif_iter);

end:
	return muxer_upstream_notif_iter;
}

static
enum bt_self_component_status ensure_available_input_port(
		struct bt_self_component_filter *self_comp)
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
		struct bt_self_component_filter *self_comp)
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
		"muxer-notif-iter-count=%u", muxer_comp,
		muxer_comp->muxer_notif_iters ?
			muxer_comp->muxer_notif_iters->len : 0);

	if (muxer_comp->muxer_notif_iters) {
		g_ptr_array_free(muxer_comp->muxer_notif_iters, TRUE);
	}

	g_free(muxer_comp);
}

static
struct bt_value *get_default_params(void)
{
	struct bt_value *params;
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
		const struct bt_value *params)
{
	struct bt_value *default_params = NULL;
	struct bt_value *real_params = NULL;
	const struct bt_value *assume_absolute_clock_classes = NULL;
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
		struct bt_self_component_filter *self_comp,
		struct bt_value *params, void *init_data)
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

	muxer_comp->muxer_notif_iters = g_ptr_array_new();
	if (!muxer_comp->muxer_notif_iters) {
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
void muxer_finalize(struct bt_self_component_filter *self_comp)
{
	struct muxer_comp *muxer_comp = bt_self_component_get_data(
		bt_self_component_filter_as_self_component(self_comp));

	BT_LOGD("Finalizing muxer component: comp-addr=%p",
		self_comp);
	destroy_muxer_comp(muxer_comp);
}

static
struct bt_self_component_port_input_notification_iterator *
create_notif_iter_on_input_port(
		struct bt_self_component_port_input *self_port, int *ret)
{
	const struct bt_port *port = bt_self_component_port_as_port(
		bt_self_component_port_input_as_self_component_port(
			self_port));
	struct bt_self_component_port_input_notification_iterator *notif_iter =
		NULL;

	BT_ASSERT(ret);
	*ret = 0;
	BT_ASSERT(port);
	BT_ASSERT(bt_port_is_connected(port));

	// TODO: Advance the iterator to >= the time of the latest
	//       returned notification by the muxer notification
	//       iterator which creates it.
	notif_iter = bt_self_component_port_input_notification_iterator_create(
		self_port);
	if (!notif_iter) {
		BT_LOGE("Cannot create upstream notification iterator on input port: "
			"port-addr=%p, port-name=\"%s\"",
			port, bt_port_get_name(port));
		*ret = -1;
		goto end;
	}

	BT_LOGD("Created upstream notification iterator on input port: "
		"port-addr=%p, port-name=\"%s\", notif-iter-addr=%p",
		port, bt_port_get_name(port), notif_iter);

end:
	return notif_iter;
}

static
enum bt_notification_iterator_status muxer_upstream_notif_iter_next(
		struct muxer_upstream_notif_iter *muxer_upstream_notif_iter)
{
	enum bt_notification_iterator_status status;
	bt_notification_array_const notifs;
	uint64_t i;
	uint64_t count;

	BT_LOGV("Calling upstream notification iterator's \"next\" method: "
		"muxer-upstream-notif-iter-wrap-addr=%p, notif-iter-addr=%p",
		muxer_upstream_notif_iter,
		muxer_upstream_notif_iter->notif_iter);
	status = bt_self_component_port_input_notification_iterator_next(
		muxer_upstream_notif_iter->notif_iter, &notifs, &count);
	BT_LOGV("Upstream notification iterator's \"next\" method returned: "
		"status=%s", bt_notification_iterator_status_string(status));

	switch (status) {
	case BT_NOTIFICATION_ITERATOR_STATUS_OK:
		/*
		 * Notification iterator's current notification is
		 * valid: it must be considered for muxing operations.
		 */
		BT_LOGV_STR("Validated upstream notification iterator wrapper.");
		BT_ASSERT(count > 0);

		/* Move notifications to our queue */
		for (i = 0; i < count; i++) {
			/*
			 * Push to tail in order; other side
			 * (muxer_notif_iter_do_next_one()) consumes
			 * from the head first.
			 */
			g_queue_push_tail(muxer_upstream_notif_iter->notifs,
				(void *) notifs[i]);
		}
		break;
	case BT_NOTIFICATION_ITERATOR_STATUS_AGAIN:
		/*
		 * Notification iterator's current notification is not
		 * valid anymore. Return
		 * BT_NOTIFICATION_ITERATOR_STATUS_AGAIN immediately.
		 */
		break;
	case BT_NOTIFICATION_ITERATOR_STATUS_END:	/* Fall-through. */
	case BT_NOTIFICATION_ITERATOR_STATUS_CANCELED:
		/*
		 * Notification iterator reached the end: release it. It
		 * won't be considered again to find the youngest
		 * notification.
		 */
		BT_SELF_COMPONENT_PORT_INPUT_NOTIFICATION_ITERATOR_PUT_REF_AND_RESET(muxer_upstream_notif_iter->notif_iter);
		status = BT_NOTIFICATION_ITERATOR_STATUS_OK;
		break;
	default:
		/* Error or unsupported status code */
		BT_LOGE("Error or unsupported status code: "
			"status-code=%d", status);
		status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
		break;
	}

	return status;
}

static
int muxer_notif_iter_handle_newly_connected_ports(
		struct muxer_notif_iter *muxer_notif_iter)
{
	int ret = 0;

	BT_LOGV("Handling newly connected ports: "
		"muxer-notif-iter-addr=%p", muxer_notif_iter);

	/*
	 * Here we create one upstream notification iterator for each
	 * newly connected port. We do NOT perform an initial "next" on
	 * those new upstream notification iterators: they are
	 * invalidated, to be validated later. The list of newly
	 * connected ports to handle here is updated by
	 * muxer_port_connected().
	 */
	while (true) {
		GList *node = muxer_notif_iter->newly_connected_self_ports;
		struct bt_self_component_port_input *self_port;
		const struct bt_port *port;
		struct bt_self_component_port_input_notification_iterator *
			upstream_notif_iter = NULL;
		struct muxer_upstream_notif_iter *muxer_upstream_notif_iter;

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
			 * notification iterator on its (non-existing)
			 * connection in this case.
			 */
			goto remove_node;
		}

		upstream_notif_iter = create_notif_iter_on_input_port(
			self_port, &ret);
		if (ret) {
			/* create_notif_iter_on_input_port() logs errors */
			BT_ASSERT(!upstream_notif_iter);
			goto error;
		}

		muxer_upstream_notif_iter =
			muxer_notif_iter_add_upstream_notif_iter(
				muxer_notif_iter, upstream_notif_iter);
		BT_SELF_COMPONENT_PORT_INPUT_NOTIFICATION_ITERATOR_PUT_REF_AND_RESET(upstream_notif_iter);
		if (!muxer_upstream_notif_iter) {
			/*
			 * muxer_notif_iter_add_upstream_notif_iter()
			 * logs errors.
			 */
			goto error;
		}

remove_node:
		bt_self_component_port_input_notification_iterator_put_ref(upstream_notif_iter);
		muxer_notif_iter->newly_connected_self_ports =
			g_list_delete_link(
				muxer_notif_iter->newly_connected_self_ports,
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
int get_notif_ts_ns(struct muxer_comp *muxer_comp,
		struct muxer_notif_iter *muxer_notif_iter,
		const struct bt_notification *notif, int64_t last_returned_ts_ns,
		int64_t *ts_ns)
{
	const struct bt_clock_class *clock_class = NULL;
	const struct bt_clock_value *clock_value = NULL;
	const struct bt_event *event = NULL;
	int ret = 0;
	const unsigned char *cc_uuid;
	const char *cc_name;
	enum bt_clock_value_status cv_status = BT_CLOCK_VALUE_STATUS_KNOWN;

	BT_ASSERT(notif);
	BT_ASSERT(ts_ns);

	BT_LOGV("Getting notification's timestamp: "
		"muxer-notif-iter-addr=%p, notif-addr=%p, "
		"last-returned-ts=%" PRId64,
		muxer_notif_iter, notif, last_returned_ts_ns);

	switch (bt_notification_get_type(notif)) {
	case BT_NOTIFICATION_TYPE_EVENT:
		event = bt_notification_event_borrow_event_const(notif);
		BT_ASSERT(event);
		cv_status = bt_event_borrow_default_clock_value_const(event,
			&clock_value);
		break;

	case BT_NOTIFICATION_TYPE_INACTIVITY:
		clock_value =
			bt_notification_inactivity_borrow_default_clock_value_const(
				notif);
		break;
	default:
		/* All the other notifications have a higher priority */
		BT_LOGV_STR("Notification has no timestamp: using the last returned timestamp.");
		*ts_ns = last_returned_ts_ns;
		goto end;
	}

	if (cv_status != BT_CLOCK_VALUE_STATUS_KNOWN) {
		BT_LOGE_STR("Unsupported unknown clock value.");
		ret = -1;
		goto end;
	}

	/*
	 * If the clock value is missing, then we consider that this
	 * notification has no time. In this case it's always the
	 * youngest.
	 */
	if (!clock_value) {
		BT_LOGV_STR("Notification's default clock value is missing: "
			"using the last returned timestamp.");
		*ts_ns = last_returned_ts_ns;
		goto end;
	}

	clock_class = bt_clock_value_borrow_clock_class_const(clock_value);
	BT_ASSERT(clock_class);
	cc_uuid = bt_clock_class_get_uuid(clock_class);
	cc_name = bt_clock_class_get_name(clock_class);

	if (muxer_notif_iter->clock_class_expectation ==
			MUXER_NOTIF_ITER_CLOCK_CLASS_EXPECTATION_ANY) {
		/*
		 * This is the first clock class that this muxer
		 * notification iterator encounters. Its properties
		 * determine what to expect for the whole lifetime of
		 * the iterator without a true
		 * `assume-absolute-clock-classes` parameter.
		 */
		if (bt_clock_class_is_absolute(clock_class)) {
			/* Expect absolute clock classes */
			muxer_notif_iter->clock_class_expectation =
				MUXER_NOTIF_ITER_CLOCK_CLASS_EXPECTATION_ABSOLUTE;
		} else {
			if (cc_uuid) {
				/*
				 * Expect non-absolute clock classes
				 * with a specific UUID.
				 */
				muxer_notif_iter->clock_class_expectation =
					MUXER_NOTIF_ITER_CLOCK_CLASS_EXPECTATION_NOT_ABS_SPEC_UUID;
				memcpy(muxer_notif_iter->expected_clock_class_uuid,
					cc_uuid, BABELTRACE_UUID_LEN);
			} else {
				/*
				 * Expect non-absolute clock classes
				 * with no UUID.
				 */
				muxer_notif_iter->clock_class_expectation =
					MUXER_NOTIF_ITER_CLOCK_CLASS_EXPECTATION_NOT_ABS_NO_UUID;
			}
		}
	}

	if (!muxer_comp->assume_absolute_clock_classes) {
		switch (muxer_notif_iter->clock_class_expectation) {
		case MUXER_NOTIF_ITER_CLOCK_CLASS_EXPECTATION_ABSOLUTE:
			if (!bt_clock_class_is_absolute(clock_class)) {
				BT_LOGE("Expecting an absolute clock class, "
					"but got a non-absolute one: "
					"clock-class-addr=%p, clock-class-name=\"%s\"",
					clock_class, cc_name);
				goto error;
			}
			break;
		case MUXER_NOTIF_ITER_CLOCK_CLASS_EXPECTATION_NOT_ABS_NO_UUID:
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
		case MUXER_NOTIF_ITER_CLOCK_CLASS_EXPECTATION_NOT_ABS_SPEC_UUID:
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

			if (memcmp(muxer_notif_iter->expected_clock_class_uuid,
					cc_uuid, BABELTRACE_UUID_LEN) != 0) {
				BT_LOGE("Expecting a non-absolute clock class with a specific UUID, "
					"but got one with different UUID: "
					"clock-class-addr=%p, clock-class-name=\"%s\", "
					"expected-uuid=\"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\", "
					"uuid=\"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\"",
					clock_class, cc_name,
					(unsigned int) muxer_notif_iter->expected_clock_class_uuid[0],
					(unsigned int) muxer_notif_iter->expected_clock_class_uuid[1],
					(unsigned int) muxer_notif_iter->expected_clock_class_uuid[2],
					(unsigned int) muxer_notif_iter->expected_clock_class_uuid[3],
					(unsigned int) muxer_notif_iter->expected_clock_class_uuid[4],
					(unsigned int) muxer_notif_iter->expected_clock_class_uuid[5],
					(unsigned int) muxer_notif_iter->expected_clock_class_uuid[6],
					(unsigned int) muxer_notif_iter->expected_clock_class_uuid[7],
					(unsigned int) muxer_notif_iter->expected_clock_class_uuid[8],
					(unsigned int) muxer_notif_iter->expected_clock_class_uuid[9],
					(unsigned int) muxer_notif_iter->expected_clock_class_uuid[10],
					(unsigned int) muxer_notif_iter->expected_clock_class_uuid[11],
					(unsigned int) muxer_notif_iter->expected_clock_class_uuid[12],
					(unsigned int) muxer_notif_iter->expected_clock_class_uuid[13],
					(unsigned int) muxer_notif_iter->expected_clock_class_uuid[14],
					(unsigned int) muxer_notif_iter->expected_clock_class_uuid[15],
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
				muxer_notif_iter->clock_class_expectation);
			abort();
		}
	}

	ret = bt_clock_value_get_ns_from_origin(clock_value, ts_ns);
	if (ret) {
		BT_LOGE("Cannot get nanoseconds from Epoch of clock value: "
			"clock-value-addr=%p", clock_value);
		goto error;
	}

	goto end;

error:
	ret = -1;

end:
	if (ret == 0) {
		BT_LOGV("Found notification's timestamp: "
			"muxer-notif-iter-addr=%p, notif-addr=%p, "
			"last-returned-ts=%" PRId64 ", ts=%" PRId64,
			muxer_notif_iter, notif, last_returned_ts_ns,
			*ts_ns);
	}

	return ret;
}

/*
 * This function finds the youngest available notification amongst the
 * non-ended upstream notification iterators and returns the upstream
 * notification iterator which has it, or
 * BT_NOTIFICATION_ITERATOR_STATUS_END if there's no available
 * notification.
 *
 * This function does NOT:
 *
 * * Update any upstream notification iterator.
 * * Check for newly connected ports.
 * * Check the upstream notification iterators to retry.
 *
 * On sucess, this function sets *muxer_upstream_notif_iter to the
 * upstream notification iterator of which the current notification is
 * the youngest, and sets *ts_ns to its time.
 */
static
enum bt_notification_iterator_status
muxer_notif_iter_youngest_upstream_notif_iter(
		struct muxer_comp *muxer_comp,
		struct muxer_notif_iter *muxer_notif_iter,
		struct muxer_upstream_notif_iter **muxer_upstream_notif_iter,
		int64_t *ts_ns)
{
	size_t i;
	int ret;
	int64_t youngest_ts_ns = INT64_MAX;
	enum bt_notification_iterator_status status =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;

	BT_ASSERT(muxer_comp);
	BT_ASSERT(muxer_notif_iter);
	BT_ASSERT(muxer_upstream_notif_iter);
	*muxer_upstream_notif_iter = NULL;

	for (i = 0; i < muxer_notif_iter->muxer_upstream_notif_iters->len; i++) {
		const struct bt_notification *notif;
		struct muxer_upstream_notif_iter *cur_muxer_upstream_notif_iter =
			g_ptr_array_index(muxer_notif_iter->muxer_upstream_notif_iters, i);
		int64_t notif_ts_ns;

		if (!cur_muxer_upstream_notif_iter->notif_iter) {
			/* This upstream notification iterator is ended */
			BT_LOGV("Skipping ended upstream notification iterator: "
				"muxer-upstream-notif-iter-wrap-addr=%p",
				cur_muxer_upstream_notif_iter);
			continue;
		}

		BT_ASSERT(cur_muxer_upstream_notif_iter->notifs->length > 0);
		notif = g_queue_peek_head(cur_muxer_upstream_notif_iter->notifs);
		BT_ASSERT(notif);
		ret = get_notif_ts_ns(muxer_comp, muxer_notif_iter, notif,
			muxer_notif_iter->last_returned_ts_ns, &notif_ts_ns);
		if (ret) {
			/* get_notif_ts_ns() logs errors */
			*muxer_upstream_notif_iter = NULL;
			status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
			goto end;
		}

		if (notif_ts_ns <= youngest_ts_ns) {
			*muxer_upstream_notif_iter =
				cur_muxer_upstream_notif_iter;
			youngest_ts_ns = notif_ts_ns;
			*ts_ns = youngest_ts_ns;
		}
	}

	if (!*muxer_upstream_notif_iter) {
		status = BT_NOTIFICATION_ITERATOR_STATUS_END;
		*ts_ns = INT64_MIN;
	}

end:
	return status;
}

static
enum bt_notification_iterator_status validate_muxer_upstream_notif_iter(
	struct muxer_upstream_notif_iter *muxer_upstream_notif_iter)
{
	enum bt_notification_iterator_status status =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;

	BT_LOGV("Validating muxer's upstream notification iterator wrapper: "
		"muxer-upstream-notif-iter-wrap-addr=%p",
		muxer_upstream_notif_iter);

	if (muxer_upstream_notif_iter->notifs->length > 0 ||
			!muxer_upstream_notif_iter->notif_iter) {
		BT_LOGV("Already valid or not considered: "
			"queue-len=%u, upstream-notif-iter-addr=%p",
			muxer_upstream_notif_iter->notifs->length,
			muxer_upstream_notif_iter->notif_iter);
		goto end;
	}

	/* muxer_upstream_notif_iter_next() logs details/errors */
	status = muxer_upstream_notif_iter_next(muxer_upstream_notif_iter);

end:
	return status;
}

static
enum bt_notification_iterator_status validate_muxer_upstream_notif_iters(
		struct muxer_notif_iter *muxer_notif_iter)
{
	enum bt_notification_iterator_status status =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;
	size_t i;

	BT_LOGV("Validating muxer's upstream notification iterator wrappers: "
		"muxer-notif-iter-addr=%p", muxer_notif_iter);

	for (i = 0; i < muxer_notif_iter->muxer_upstream_notif_iters->len; i++) {
		struct muxer_upstream_notif_iter *muxer_upstream_notif_iter =
			g_ptr_array_index(
				muxer_notif_iter->muxer_upstream_notif_iters,
				i);

		status = validate_muxer_upstream_notif_iter(
			muxer_upstream_notif_iter);
		if (status != BT_NOTIFICATION_ITERATOR_STATUS_OK) {
			if (status < 0) {
				BT_LOGE("Cannot validate muxer's upstream notification iterator wrapper: "
					"muxer-notif-iter-addr=%p, "
					"muxer-upstream-notif-iter-wrap-addr=%p",
					muxer_notif_iter,
					muxer_upstream_notif_iter);
			} else {
				BT_LOGV("Cannot validate muxer's upstream notification iterator wrapper: "
					"muxer-notif-iter-addr=%p, "
					"muxer-upstream-notif-iter-wrap-addr=%p",
					muxer_notif_iter,
					muxer_upstream_notif_iter);
			}

			goto end;
		}

		/*
		 * Remove this muxer upstream notification iterator
		 * if it's ended or canceled.
		 */
		if (!muxer_upstream_notif_iter->notif_iter) {
			/*
			 * Use g_ptr_array_remove_fast() because the
			 * order of those elements is not important.
			 */
			BT_LOGV("Removing muxer's upstream notification iterator wrapper: ended or canceled: "
				"muxer-notif-iter-addr=%p, "
				"muxer-upstream-notif-iter-wrap-addr=%p",
				muxer_notif_iter, muxer_upstream_notif_iter);
			g_ptr_array_remove_index_fast(
				muxer_notif_iter->muxer_upstream_notif_iters,
				i);
			i--;
		}
	}

end:
	return status;
}

static inline
enum bt_notification_iterator_status muxer_notif_iter_do_next_one(
		struct muxer_comp *muxer_comp,
		struct muxer_notif_iter *muxer_notif_iter,
		const struct bt_notification **notif)
{
	enum bt_notification_iterator_status status =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;
	struct muxer_upstream_notif_iter *muxer_upstream_notif_iter = NULL;
	int64_t next_return_ts;

	while (true) {
		int ret = muxer_notif_iter_handle_newly_connected_ports(
			muxer_notif_iter);

		if (ret) {
			BT_LOGE("Cannot handle newly connected input ports for muxer's notification iterator: "
				"muxer-comp-addr=%p, muxer-notif-iter-addr=%p, "
				"ret=%d",
				muxer_comp, muxer_notif_iter, ret);
			status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
			goto end;
		}

		status = validate_muxer_upstream_notif_iters(muxer_notif_iter);
		if (status != BT_NOTIFICATION_ITERATOR_STATUS_OK) {
			/* validate_muxer_upstream_notif_iters() logs details */
			goto end;
		}

		/*
		 * At this point, we know that all the existing upstream
		 * notification iterators are valid. However the
		 * operations to validate them (during
		 * validate_muxer_upstream_notif_iters()) may have
		 * connected new ports. If no ports were connected
		 * during this operation, exit the loop.
		 */
		if (!muxer_notif_iter->newly_connected_self_ports) {
			BT_LOGV("Not breaking this loop: muxer's notification iterator still has newly connected input ports to handle: "
				"muxer-comp-addr=%p", muxer_comp);
			break;
		}
	}

	BT_ASSERT(!muxer_notif_iter->newly_connected_self_ports);

	/*
	 * At this point we know that all the existing upstream
	 * notification iterators are valid. We can find the one,
	 * amongst those, of which the current notification is the
	 * youngest.
	 */
	status = muxer_notif_iter_youngest_upstream_notif_iter(muxer_comp,
			muxer_notif_iter, &muxer_upstream_notif_iter,
			&next_return_ts);
	if (status < 0 || status == BT_NOTIFICATION_ITERATOR_STATUS_END ||
			status == BT_NOTIFICATION_ITERATOR_STATUS_CANCELED) {
		if (status < 0) {
			BT_LOGE("Cannot find the youngest upstream notification iterator wrapper: "
				"status=%s",
				bt_notification_iterator_status_string(status));
		} else {
			BT_LOGV("Cannot find the youngest upstream notification iterator wrapper: "
				"status=%s",
				bt_notification_iterator_status_string(status));
		}

		goto end;
	}

	if (next_return_ts < muxer_notif_iter->last_returned_ts_ns) {
		BT_LOGE("Youngest upstream notification iterator wrapper's timestamp is less than muxer's notification iterator's last returned timestamp: "
			"muxer-notif-iter-addr=%p, ts=%" PRId64 ", "
			"last-returned-ts=%" PRId64,
			muxer_notif_iter, next_return_ts,
			muxer_notif_iter->last_returned_ts_ns);
		status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
		goto end;
	}

	BT_LOGV("Found youngest upstream notification iterator wrapper: "
		"muxer-notif-iter-addr=%p, "
		"muxer-upstream-notif-iter-wrap-addr=%p, "
		"ts=%" PRId64,
		muxer_notif_iter, muxer_upstream_notif_iter, next_return_ts);
	BT_ASSERT(status == BT_NOTIFICATION_ITERATOR_STATUS_OK);
	BT_ASSERT(muxer_upstream_notif_iter);

	/*
	 * Consume from the queue's head: other side
	 * (muxer_upstream_notif_iter_next()) writes to the tail.
	 */
	*notif = g_queue_pop_head(muxer_upstream_notif_iter->notifs);
	BT_ASSERT(*notif);
	muxer_notif_iter->last_returned_ts_ns = next_return_ts;

end:
	return status;
}

static
enum bt_notification_iterator_status muxer_notif_iter_do_next(
		struct muxer_comp *muxer_comp,
		struct muxer_notif_iter *muxer_notif_iter,
		bt_notification_array_const notifs, uint64_t capacity,
		uint64_t *count)
{
	enum bt_notification_iterator_status status =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;
	uint64_t i = 0;

	while (i < capacity && status == BT_NOTIFICATION_ITERATOR_STATUS_OK) {
		status = muxer_notif_iter_do_next_one(muxer_comp,
			muxer_notif_iter, &notifs[i]);
		if (status == BT_NOTIFICATION_ITERATOR_STATUS_OK) {
			i++;
		}
	}

	if (i > 0) {
		/*
		 * Even if muxer_notif_iter_do_next_one() returned
		 * something else than
		 * BT_NOTIFICATION_ITERATOR_STATUS_OK, we accumulated
		 * notification objects in the output notification
		 * array, so we need to return
		 * BT_NOTIFICATION_ITERATOR_STATUS_OK so that they are
		 * transfered to downstream. This other status occurs
		 * again the next time muxer_notif_iter_do_next() is
		 * called, possibly without any accumulated
		 * notification, in which case we'll return it.
		 */
		*count = i;
		status = BT_NOTIFICATION_ITERATOR_STATUS_OK;
	}

	return status;
}

static
void destroy_muxer_notif_iter(struct muxer_notif_iter *muxer_notif_iter)
{
	if (!muxer_notif_iter) {
		return;
	}

	BT_LOGD("Destroying muxer component's notification iterator: "
		"muxer-notif-iter-addr=%p", muxer_notif_iter);

	if (muxer_notif_iter->muxer_upstream_notif_iters) {
		BT_LOGD_STR("Destroying muxer's upstream notification iterator wrappers.");
		g_ptr_array_free(
			muxer_notif_iter->muxer_upstream_notif_iters, TRUE);
	}

	g_list_free(muxer_notif_iter->newly_connected_self_ports);
	g_free(muxer_notif_iter);
}

static
int muxer_notif_iter_init_newly_connected_ports(struct muxer_comp *muxer_comp,
		struct muxer_notif_iter *muxer_notif_iter)
{
	int64_t count;
	int64_t i;
	int ret = 0;

	/*
	 * Add the connected input ports to this muxer notification
	 * iterator's list of newly connected ports. They will be
	 * handled by muxer_notif_iter_handle_newly_connected_ports().
	 */
	count = bt_component_filter_get_input_port_count(
		bt_self_component_filter_as_component_filter(
			muxer_comp->self_comp));
	if (count < 0) {
		BT_LOGD("No input port to initialize for muxer component's notification iterator: "
			"muxer-comp-addr=%p, muxer-notif-iter-addr=%p",
			muxer_comp, muxer_notif_iter);
		goto end;
	}

	for (i = 0; i < count; i++) {
		struct bt_self_component_port_input *self_port =
			bt_self_component_filter_borrow_input_port_by_index(
				muxer_comp->self_comp, i);
		const struct bt_port *port;

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

		muxer_notif_iter->newly_connected_self_ports =
			g_list_append(
				muxer_notif_iter->newly_connected_self_ports,
				self_port);
		if (!muxer_notif_iter->newly_connected_self_ports) {
			BT_LOGE("Cannot append port to muxer's notification iterator list of newly connected input ports: "
				"port-addr=%p, port-name=\"%s\", "
				"muxer-notif-iter-addr=%p", port,
				bt_port_get_name(port), muxer_notif_iter);
			ret = -1;
			goto end;
		}

		BT_LOGD("Appended port to muxer's notification iterator list of newly connected input ports: "
			"port-addr=%p, port-name=\"%s\", "
			"muxer-notif-iter-addr=%p", port,
			bt_port_get_name(port), muxer_notif_iter);
	}

end:
	return ret;
}

BT_HIDDEN
enum bt_self_notification_iterator_status muxer_notif_iter_init(
		struct bt_self_notification_iterator *self_notif_iter,
		struct bt_self_component_filter *self_comp,
		struct bt_self_component_port_output *port)
{
	struct muxer_comp *muxer_comp = NULL;
	struct muxer_notif_iter *muxer_notif_iter = NULL;
	enum bt_self_notification_iterator_status status =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;
	int ret;

	muxer_comp = bt_self_component_get_data(
		bt_self_component_filter_as_self_component(self_comp));
	BT_ASSERT(muxer_comp);
	BT_LOGD("Initializing muxer component's notification iterator: "
		"comp-addr=%p, muxer-comp-addr=%p, notif-iter-addr=%p",
		self_comp, muxer_comp, self_notif_iter);

	if (muxer_comp->initializing_muxer_notif_iter) {
		/*
		 * Weird, unhandled situation detected: downstream
		 * creates a muxer notification iterator while creating
		 * another muxer notification iterator (same component).
		 */
		BT_LOGE("Recursive initialization of muxer component's notification iterator: "
			"comp-addr=%p, muxer-comp-addr=%p, notif-iter-addr=%p",
			self_comp, muxer_comp, self_notif_iter);
		goto error;
	}

	muxer_comp->initializing_muxer_notif_iter = true;
	muxer_notif_iter = g_new0(struct muxer_notif_iter, 1);
	if (!muxer_notif_iter) {
		BT_LOGE_STR("Failed to allocate one muxer component's notification iterator.");
		goto error;
	}

	muxer_notif_iter->last_returned_ts_ns = INT64_MIN;
	muxer_notif_iter->muxer_upstream_notif_iters =
		g_ptr_array_new_with_free_func(
			(GDestroyNotify) destroy_muxer_upstream_notif_iter);
	if (!muxer_notif_iter->muxer_upstream_notif_iters) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
		goto error;
	}

	/*
	 * Add the muxer notification iterator to the component's array
	 * of muxer notification iterators here because
	 * muxer_notif_iter_init_newly_connected_ports() can cause
	 * muxer_port_connected() to be called, which adds the newly
	 * connected port to each muxer notification iterator's list of
	 * newly connected ports.
	 */
	g_ptr_array_add(muxer_comp->muxer_notif_iters, muxer_notif_iter);
	ret = muxer_notif_iter_init_newly_connected_ports(muxer_comp,
		muxer_notif_iter);
	if (ret) {
		BT_LOGE("Cannot initialize newly connected input ports for muxer component's notification iterator: "
			"comp-addr=%p, muxer-comp-addr=%p, "
			"muxer-notif-iter-addr=%p, notif-iter-addr=%p, ret=%d",
			self_comp, muxer_comp, muxer_notif_iter,
			self_notif_iter, ret);
		goto error;
	}

	bt_self_notification_iterator_set_data(self_notif_iter,
		muxer_notif_iter);
	BT_LOGD("Initialized muxer component's notification iterator: "
		"comp-addr=%p, muxer-comp-addr=%p, muxer-notif-iter-addr=%p, "
		"notif-iter-addr=%p",
		self_comp, muxer_comp, muxer_notif_iter, self_notif_iter);
	goto end;

error:
	if (g_ptr_array_index(muxer_comp->muxer_notif_iters,
			muxer_comp->muxer_notif_iters->len - 1) == muxer_notif_iter) {
		g_ptr_array_remove_index(muxer_comp->muxer_notif_iters,
			muxer_comp->muxer_notif_iters->len - 1);
	}

	destroy_muxer_notif_iter(muxer_notif_iter);
	bt_self_notification_iterator_set_data(self_notif_iter,
		NULL);
	status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;

end:
	muxer_comp->initializing_muxer_notif_iter = false;
	return status;
}

BT_HIDDEN
void muxer_notif_iter_finalize(
		struct bt_self_notification_iterator *self_notif_iter)
{
	struct muxer_notif_iter *muxer_notif_iter =
		bt_self_notification_iterator_get_data(self_notif_iter);
	struct bt_self_component *self_comp = NULL;
	struct muxer_comp *muxer_comp = NULL;

	self_comp = bt_self_notification_iterator_borrow_component(
		self_notif_iter);
	BT_ASSERT(self_comp);
	muxer_comp = bt_self_component_get_data(self_comp);
	BT_LOGD("Finalizing muxer component's notification iterator: "
		"comp-addr=%p, muxer-comp-addr=%p, muxer-notif-iter-addr=%p, "
		"notif-iter-addr=%p",
		self_comp, muxer_comp, muxer_notif_iter, self_notif_iter);

	if (muxer_comp) {
		(void) g_ptr_array_remove_fast(muxer_comp->muxer_notif_iters,
			muxer_notif_iter);
		destroy_muxer_notif_iter(muxer_notif_iter);
	}
}

BT_HIDDEN
enum bt_notification_iterator_status muxer_notif_iter_next(
		struct bt_self_notification_iterator *self_notif_iter,
		bt_notification_array_const notifs, uint64_t capacity,
		uint64_t *count)
{
	enum bt_notification_iterator_status status;
	struct muxer_notif_iter *muxer_notif_iter =
		bt_self_notification_iterator_get_data(self_notif_iter);
	struct bt_self_component *self_comp = NULL;
	struct muxer_comp *muxer_comp = NULL;

	BT_ASSERT(muxer_notif_iter);
	self_comp = bt_self_notification_iterator_borrow_component(
		self_notif_iter);
	BT_ASSERT(self_comp);
	muxer_comp = bt_self_component_get_data(self_comp);
	BT_ASSERT(muxer_comp);
	BT_LOGV("Muxer component's notification iterator's \"next\" method called: "
		"comp-addr=%p, muxer-comp-addr=%p, muxer-notif-iter-addr=%p, "
		"notif-iter-addr=%p",
		self_comp, muxer_comp, muxer_notif_iter, self_notif_iter);

	status = muxer_notif_iter_do_next(muxer_comp, muxer_notif_iter,
		notifs, capacity, count);
	if (status < 0) {
		BT_LOGE("Cannot get next notification: "
			"comp-addr=%p, muxer-comp-addr=%p, muxer-notif-iter-addr=%p, "
			"notif-iter-addr=%p, status=%s",
			self_comp, muxer_comp, muxer_notif_iter, self_notif_iter,
			bt_notification_iterator_status_string(status));
	} else {
		BT_LOGV("Returning from muxer component's notification iterator's \"next\" method: "
			"status=%s",
			bt_notification_iterator_status_string(status));
	}

	return status;
}

BT_HIDDEN
enum bt_self_component_status muxer_input_port_connected(
		struct bt_self_component_filter *self_comp,
		struct bt_self_component_port_input *self_port,
		const struct bt_port_output *other_port)
{
	enum bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	const struct bt_port *port = bt_self_component_port_as_port(
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

	for (i = 0; i < muxer_comp->muxer_notif_iters->len; i++) {
		struct muxer_notif_iter *muxer_notif_iter =
			g_ptr_array_index(muxer_comp->muxer_notif_iters, i);

		/*
		 * Add this port to the list of newly connected ports
		 * for this muxer notification iterator. We append at
		 * the end of this list while
		 * muxer_notif_iter_handle_newly_connected_ports()
		 * removes the nodes from the beginning.
		 */
		muxer_notif_iter->newly_connected_self_ports =
			g_list_append(
				muxer_notif_iter->newly_connected_self_ports,
				self_port);
		if (!muxer_notif_iter->newly_connected_self_ports) {
			BT_LOGE("Cannot append port to muxer's notification iterator list of newly connected input ports: "
				"port-addr=%p, port-name=\"%s\", "
				"muxer-notif-iter-addr=%p", self_port,
				bt_port_get_name(port), muxer_notif_iter);
			status = BT_SELF_COMPONENT_STATUS_ERROR;
			goto end;
		}

		BT_LOGD("Appended port to muxer's notification iterator list of newly connected input ports: "
			"port-addr=%p, port-name=\"%s\", "
			"muxer-notif-iter-addr=%p", self_port,
			bt_port_get_name(port), muxer_notif_iter);
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
		struct bt_self_component_filter *self_component,
		struct bt_self_component_port_input *self_port)
{
	struct muxer_comp *muxer_comp =
		bt_self_component_get_data(
			bt_self_component_filter_as_self_component(
				self_component));
	const struct bt_port *port =
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
