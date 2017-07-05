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
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-ir/event.h>
#include <babeltrace/graph/clock-class-priority-map.h>
#include <babeltrace/graph/component-filter.h>
#include <babeltrace/graph/component.h>
#include <babeltrace/graph/component-internal.h>
#include <babeltrace/graph/notification-event.h>
#include <babeltrace/graph/notification-inactivity.h>
#include <babeltrace/graph/notification-iterator.h>
#include <babeltrace/graph/notification-iterator-internal.h>
#include <babeltrace/graph/notification.h>
#include <babeltrace/graph/port.h>
#include <babeltrace/graph/private-component-filter.h>
#include <babeltrace/graph/private-component.h>
#include <babeltrace/graph/private-component.h>
#include <babeltrace/graph/private-connection.h>
#include <babeltrace/graph/private-notification-iterator.h>
#include <babeltrace/graph/private-port.h>
#include <babeltrace/graph/connection.h>
#include <babeltrace/graph/connection-internal.h>
#include <babeltrace/values-internal.h>
#include <plugins-common.h>
#include <glib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define ASSUME_ABSOLUTE_CLOCK_CLASSES_PARAM_NAME	"assume-absolute-clock-classes"

struct muxer_comp {
	/* Array of struct bt_private_notification_iterator * (weak refs) */
	GPtrArray *muxer_notif_iters;

	/* Weak ref */
	struct bt_private_component *priv_comp;
	unsigned int next_port_num;
	size_t available_input_ports;
	bool error;
	bool initializing_muxer_notif_iter;
	bool assume_absolute_clock_classes;
};

struct muxer_upstream_notif_iter {
	/* Owned by this, NULL if ended */
	struct bt_notification_iterator *notif_iter;

	/*
	 * This flag is true if the upstream notification iterator's
	 * current notification must be considered for the multiplexing
	 * operations. If the upstream iterator returns
	 * BT_NOTIFICATION_ITERATOR_STATUS_AGAIN, then this object
	 * is considered invalid, because its current notification is
	 * still the previous one, but we already took it into account.
	 *
	 * The value of this flag is not important if notif_iter above
	 * is NULL (which means the upstream iterator is finished).
	 */
	bool is_valid;
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
	GList *newly_connected_priv_ports;

	/* Next thing to return by the "next" method */
	struct bt_notification_iterator_next_return next_next_return;

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
		"addr=%p, notif-iter-addr=%p, is-valid=%d",
		muxer_upstream_notif_iter,
		muxer_upstream_notif_iter->notif_iter,
		muxer_upstream_notif_iter->is_valid);
	bt_put(muxer_upstream_notif_iter->notif_iter);
	g_free(muxer_upstream_notif_iter);
}

static
struct muxer_upstream_notif_iter *muxer_notif_iter_add_upstream_notif_iter(
		struct muxer_notif_iter *muxer_notif_iter,
		struct bt_notification_iterator *notif_iter,
		struct bt_private_port *priv_port)
{
	struct muxer_upstream_notif_iter *muxer_upstream_notif_iter =
		g_new0(struct muxer_upstream_notif_iter, 1);

	if (!muxer_upstream_notif_iter) {
		BT_LOGE_STR("Failed to allocate one muxer's upstream notification iterator wrapper.");
		goto end;
	}

	muxer_upstream_notif_iter->notif_iter = bt_get(notif_iter);
	muxer_upstream_notif_iter->is_valid = false;
	g_ptr_array_add(muxer_notif_iter->muxer_upstream_notif_iters,
		muxer_upstream_notif_iter);
	BT_LOGD("Added muxer's upstream notification iterator wrapper: "
		"addr=%p, muxer-notif-iter-addr=%p, notif-iter-addr=%p",
		muxer_upstream_notif_iter, muxer_notif_iter,
		notif_iter);

end:
	return muxer_upstream_notif_iter;
}

static
enum bt_component_status ensure_available_input_port(
		struct bt_private_component *priv_comp)
{
	struct muxer_comp *muxer_comp =
		bt_private_component_get_user_data(priv_comp);
	enum bt_component_status status = BT_COMPONENT_STATUS_OK;
	GString *port_name = NULL;

	assert(muxer_comp);

	if (muxer_comp->available_input_ports >= 1) {
		goto end;
	}

	port_name = g_string_new("in");
	if (!port_name) {
		BT_LOGE_STR("Failed to allocate a GString.");
		status = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	g_string_append_printf(port_name, "%u", muxer_comp->next_port_num);
	status = bt_private_component_filter_add_input_private_port(
		priv_comp, port_name->str, NULL, NULL);
	if (status != BT_COMPONENT_STATUS_OK) {
		BT_LOGE("Cannot add input port to muxer component: "
			"port-name=\"%s\", comp-addr=%p, status=%s",
			port_name->str, priv_comp,
			bt_component_status_string(status));
		goto end;
	}

	muxer_comp->available_input_ports++;
	muxer_comp->next_port_num++;
	BT_LOGD("Added one input port to muxer component: "
		"port-name=\"%s\", comp-addr=%p",
		port_name->str, priv_comp);
end:
	if (port_name) {
		g_string_free(port_name, TRUE);
	}

	return status;
}

static
enum bt_component_status create_output_port(
		struct bt_private_component *priv_comp)
{
	return bt_private_component_filter_add_output_private_port(
		priv_comp, "out", NULL, NULL);
}

static
void destroy_muxer_comp(struct muxer_comp *muxer_comp)
{
	if (!muxer_comp) {
		return;
	}

	BT_LOGD("Destroying muxer component: muxer-comp-addr=%p, "
		"muxer-notif-iter-count=%u", muxer_comp,
		muxer_comp->muxer_notif_iters->len);

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

	ret = bt_value_map_insert_bool(params,
		ASSUME_ABSOLUTE_CLOCK_CLASSES_PARAM_NAME, false);
	if (ret) {
		BT_LOGE_STR("Cannot add boolean value to map value object.");
		goto error;
	}

	goto end;

error:
	BT_PUT(params);

end:
	return params;
}

static
int configure_muxer_comp(struct muxer_comp *muxer_comp, struct bt_value *params)
{
	struct bt_value *default_params = NULL;
	struct bt_value *real_params = NULL;
	struct bt_value *assume_absolute_clock_classes = NULL;
	int ret = 0;
	bt_bool bool_val;

	default_params = get_default_params();
	if (!default_params) {
		BT_LOGE("Cannot get default parameters: "
			"muxer-comp-addr=%p", muxer_comp);
		goto error;
	}

	real_params = bt_value_map_extend(default_params, params);
	if (!real_params) {
		BT_LOGE("Cannot extend default parameters map value: "
			"muxer-comp-addr=%p, def-params-addr=%p, "
			"params-addr=%p", muxer_comp, default_params,
			params);
		goto error;
	}

	assume_absolute_clock_classes = bt_value_map_get(real_params,
		ASSUME_ABSOLUTE_CLOCK_CLASSES_PARAM_NAME);
	if (!bt_value_is_bool(assume_absolute_clock_classes)) {
		BT_LOGE("Expecting a boolean value for the `%s` parameter: "
			"muxer-comp-addr=%p, value-type=%s",
			ASSUME_ABSOLUTE_CLOCK_CLASSES_PARAM_NAME, muxer_comp,
			bt_value_type_string(
				bt_value_get_type(assume_absolute_clock_classes)));
		goto error;
	}

	ret = bt_value_bool_get(assume_absolute_clock_classes, &bool_val);
	assert(ret == 0);
	muxer_comp->assume_absolute_clock_classes = (bool) bool_val;
	BT_LOGD("Configured muxer component: muxer-comp-addr=%p, "
		"assume-absolute-clock-classes=%d",
		muxer_comp, muxer_comp->assume_absolute_clock_classes);
	goto end;

error:
	ret = -1;

end:
	bt_put(default_params);
	bt_put(real_params);
	bt_put(assume_absolute_clock_classes);
	return ret;
}

BT_HIDDEN
enum bt_component_status muxer_init(
		struct bt_private_component *priv_comp,
		struct bt_value *params, void *init_data)
{
	int ret;
	enum bt_component_status status = BT_COMPONENT_STATUS_OK;
	struct muxer_comp *muxer_comp = g_new0(struct muxer_comp, 1);

	BT_LOGD("Initializing muxer component: "
		"comp-addr=%p, params-addr=%p", priv_comp, params);

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

	muxer_comp->priv_comp = priv_comp;
	ret = bt_private_component_set_user_data(priv_comp, muxer_comp);
	assert(ret == 0);
	status = ensure_available_input_port(priv_comp);
	if (status != BT_COMPONENT_STATUS_OK) {
		BT_LOGE("Cannot ensure that at least one muxer component's input port is available: "
			"muxer-comp-addr=%p, status=%s",
			muxer_comp,
			bt_component_status_string(status));
		goto error;
	}

	status = create_output_port(priv_comp);
	if (status) {
		BT_LOGE("Cannot create muxer component's output port: "
			"muxer-comp-addr=%p, status=%s",
			muxer_comp,
			bt_component_status_string(status));
		goto error;
	}

	BT_LOGD("Initialized muxer component: "
		"comp-addr=%p, params-addr=%p, muxer-comp-addr=%p",
		priv_comp, params, muxer_comp);

	goto end;

error:
	destroy_muxer_comp(muxer_comp);
	ret = bt_private_component_set_user_data(priv_comp, NULL);
	assert(ret == 0);

	if (status == BT_COMPONENT_STATUS_OK) {
		status = BT_COMPONENT_STATUS_ERROR;
	}

end:
	return status;
}

BT_HIDDEN
void muxer_finalize(struct bt_private_component *priv_comp)
{
	struct muxer_comp *muxer_comp =
		bt_private_component_get_user_data(priv_comp);

	BT_LOGD("Finalizing muxer component: comp-addr=%p",
		priv_comp);
	destroy_muxer_comp(muxer_comp);
}

static
struct bt_notification_iterator *create_notif_iter_on_input_port(
		struct bt_private_port *priv_port, int *ret)
{
	struct bt_port *port = bt_port_from_private_port(priv_port);
	struct bt_notification_iterator *notif_iter = NULL;
	struct bt_private_connection *priv_conn = NULL;
	enum bt_connection_status conn_status;

	assert(ret);
	*ret = 0;
	assert(port);
	assert(bt_port_is_connected(port));
	priv_conn = bt_private_port_get_private_connection(priv_port);
	assert(priv_conn);

	// TODO: Advance the iterator to >= the time of the latest
	//       returned notification by the muxer notification
	//       iterator which creates it.
	conn_status = bt_private_connection_create_notification_iterator(
		priv_conn, NULL, &notif_iter);
	if (conn_status != BT_CONNECTION_STATUS_OK) {
		BT_LOGE("Cannot create upstream notification iterator on input port's connection: "
			"port-addr=%p, port-name=\"%s\", conn-addr=%p, "
			"status=%s",
			port, bt_port_get_name(port), priv_conn,
			bt_connection_status_string(conn_status));
		*ret = -1;
		goto end;
	}

	BT_LOGD("Created upstream notification iterator on input port's connection: "
		"port-addr=%p, port-name=\"%s\", conn-addr=%p, "
		"notif-iter-addr=%p",
		port, bt_port_get_name(port), priv_conn, notif_iter);

end:
	bt_put(port);
	bt_put(priv_conn);
	return notif_iter;
}

static
enum bt_notification_iterator_status muxer_upstream_notif_iter_next(
		struct muxer_upstream_notif_iter *muxer_upstream_notif_iter)
{
	enum bt_notification_iterator_status status;

	BT_LOGV("Calling upstream notification iterator's \"next\" method: "
		"muxer-upstream-notif-iter-wrap-addr=%p, notif-iter-addr=%p",
		muxer_upstream_notif_iter,
		muxer_upstream_notif_iter->notif_iter);
	status = bt_notification_iterator_next(
		muxer_upstream_notif_iter->notif_iter);
	BT_LOGV("Upstream notification iterator's \"next\" method returned: "
		"status=%s", bt_notification_iterator_status_string(status));

	switch (status) {
	case BT_NOTIFICATION_ITERATOR_STATUS_OK:
		/*
		 * Notification iterator's current notification is valid:
		 * it must be considered for muxing operations.
		 */
		BT_LOGV_STR("Validated upstream notification iterator wrapper.");
		muxer_upstream_notif_iter->is_valid = true;
		break;
	case BT_NOTIFICATION_ITERATOR_STATUS_AGAIN:
		/*
		 * Notification iterator's current notification is not
		 * valid anymore. Return
		 * BT_NOTIFICATION_ITERATOR_STATUS_AGAIN
		 * immediately.
		 */
		BT_LOGV_STR("Invalidated upstream notification iterator wrapper because of BT_NOTIFICATION_ITERATOR_STATUS_AGAIN.");
		muxer_upstream_notif_iter->is_valid = false;
		break;
	case BT_NOTIFICATION_ITERATOR_STATUS_END:	/* Fall-through. */
	case BT_NOTIFICATION_ITERATOR_STATUS_CANCELED:
		/*
		 * Notification iterator reached the end: release it. It
		 * won't be considered again to find the youngest
		 * notification.
		 */
		BT_LOGV_STR("Invalidated upstream notification iterator wrapper because of BT_NOTIFICATION_ITERATOR_STATUS_END or BT_NOTIFICATION_ITERATOR_STATUS_CANCELED.");
		BT_PUT(muxer_upstream_notif_iter->notif_iter);
		muxer_upstream_notif_iter->is_valid = false;
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
	 * newly connected port. We do not perform an initial "next" on
	 * those new upstream notification iterators: they are
	 * invalidated, to be validated later. The list of newly
	 * connected ports to handle here is updated by
	 * muxer_port_connected().
	 */
	while (true) {
		GList *node = muxer_notif_iter->newly_connected_priv_ports;
		struct bt_private_port *priv_port;
		struct bt_port *port;
		struct bt_notification_iterator *upstream_notif_iter = NULL;
		struct muxer_upstream_notif_iter *muxer_upstream_notif_iter;

		if (!node) {
			break;
		}

		priv_port = node->data;
		port = bt_port_from_private_port(priv_port);
		assert(port);

		if (!bt_port_is_connected(port)) {
			/*
			 * Looks like this port is not connected
			 * anymore: we can't create an upstream
			 * notification iterator on its (non-existing)
			 * connection in this case.
			 */
			goto remove_node;
		}

		BT_PUT(port);
		upstream_notif_iter = create_notif_iter_on_input_port(priv_port,
			&ret);
		if (ret) {
			/* create_notif_iter_on_input_port() logs errors */
			assert(!upstream_notif_iter);
			goto error;
		}

		muxer_upstream_notif_iter =
			muxer_notif_iter_add_upstream_notif_iter(
				muxer_notif_iter, upstream_notif_iter,
				priv_port);
		BT_PUT(upstream_notif_iter);
		if (!muxer_upstream_notif_iter) {
			/*
			 * muxer_notif_iter_add_upstream_notif_iter()
			 * logs errors.
			 */
			goto error;
		}

remove_node:
		bt_put(upstream_notif_iter);
		bt_put(port);
		muxer_notif_iter->newly_connected_priv_ports =
			g_list_delete_link(
				muxer_notif_iter->newly_connected_priv_ports,
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
		struct bt_notification *notif, int64_t last_returned_ts_ns,
		int64_t *ts_ns)
{
	struct bt_clock_class_priority_map *cc_prio_map = NULL;
	struct bt_ctf_clock_class *clock_class = NULL;
	struct bt_ctf_clock_value *clock_value = NULL;
	struct bt_ctf_event *event = NULL;
	int ret = 0;
	const unsigned char *cc_uuid;
	const char *cc_name;

	assert(notif);
	assert(ts_ns);

	BT_LOGV("Getting notification's timestamp: "
		"muxer-notif-iter-addr=%p, notif-addr=%p, "
		"last-returned-ts=%" PRId64,
		muxer_notif_iter, notif, last_returned_ts_ns);

	switch (bt_notification_get_type(notif)) {
	case BT_NOTIFICATION_TYPE_EVENT:
		cc_prio_map =
			bt_notification_event_get_clock_class_priority_map(
				notif);
		break;

	case BT_NOTIFICATION_TYPE_INACTIVITY:
		cc_prio_map =
			bt_notification_inactivity_get_clock_class_priority_map(
				notif);
		break;
	default:
		/* All the other notifications have a higher priority */
		BT_LOGV_STR("Notification has no timestamp: using the last returned timestamp.");
		*ts_ns = last_returned_ts_ns;
		goto end;
	}

	if (!cc_prio_map) {
		BT_LOGE("Cannot get notification's clock class priority map: "
			"notif-addr=%p", notif);
		goto error;
	}

	/*
	 * If the clock class priority map is empty, then we consider
	 * that this notification has no time. In this case it's always
	 * the youngest.
	 */
	if (bt_clock_class_priority_map_get_clock_class_count(cc_prio_map) == 0) {
		BT_LOGV_STR("Notification's clock class priorty map contains 0 clock classes: "
			"using the last returned timestamp.");
		*ts_ns = last_returned_ts_ns;
		goto end;
	}

	clock_class =
		bt_clock_class_priority_map_get_highest_priority_clock_class(
			cc_prio_map);
	if (!clock_class) {
		BT_LOGE("Cannot get the clock class with the highest priority from clock class priority map: "
			"cc-prio-map-addr=%p", cc_prio_map);
		goto error;
	}

	cc_uuid = bt_ctf_clock_class_get_uuid(clock_class);
	cc_name = bt_ctf_clock_class_get_name(clock_class);

	if (muxer_notif_iter->clock_class_expectation ==
			MUXER_NOTIF_ITER_CLOCK_CLASS_EXPECTATION_ANY) {
		/*
		 * This is the first clock class that this muxer
		 * notification iterator encounters. Its properties
		 * determine what to expect for the whole lifetime of
		 * the iterator without a true
		 * `assume-absolute-clock-classes` parameter.
		 */
		if (bt_ctf_clock_class_is_absolute(clock_class)) {
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
			if (!bt_ctf_clock_class_is_absolute(clock_class)) {
				BT_LOGE("Expecting an absolute clock class, "
					"but got a non-absolute one: "
					"clock-class-addr=%p, clock-class-name=\"%s\"",
					clock_class, cc_name);
				goto error;
			}
			break;
		case MUXER_NOTIF_ITER_CLOCK_CLASS_EXPECTATION_NOT_ABS_NO_UUID:
			if (bt_ctf_clock_class_is_absolute(clock_class)) {
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
			if (bt_ctf_clock_class_is_absolute(clock_class)) {
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

	switch (bt_notification_get_type(notif)) {
	case BT_NOTIFICATION_TYPE_EVENT:
		event = bt_notification_event_get_event(notif);
		assert(event);
		clock_value = bt_ctf_event_get_clock_value(event,
			clock_class);
		break;
	case BT_NOTIFICATION_TYPE_INACTIVITY:
		clock_value = bt_notification_inactivity_get_clock_value(
			notif, clock_class);
		break;
	default:
		BT_LOGF("Unexpected notification type at this point: "
			"type=%d", bt_notification_get_type(notif));
		abort();
	}

	if (!clock_value) {
		BT_LOGE("Cannot get notification's clock value for clock class: "
			"clock-class-addr=%p, clock-class-name=\"%s\"",
			clock_class, cc_name);
		goto error;
	}

	ret = bt_ctf_clock_value_get_value_ns_from_epoch(clock_value, ts_ns);
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

	bt_put(cc_prio_map);
	bt_put(event);
	bt_put(clock_class);
	bt_put(clock_value);
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

	assert(muxer_comp);
	assert(muxer_notif_iter);
	assert(muxer_upstream_notif_iter);
	*muxer_upstream_notif_iter = NULL;

	for (i = 0; i < muxer_notif_iter->muxer_upstream_notif_iters->len; i++) {
		struct bt_notification *notif;
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

		assert(cur_muxer_upstream_notif_iter->is_valid);
		notif = bt_notification_iterator_get_notification(
			cur_muxer_upstream_notif_iter->notif_iter);
		assert(notif);
		ret = get_notif_ts_ns(muxer_comp, muxer_notif_iter, notif,
			muxer_notif_iter->last_returned_ts_ns, &notif_ts_ns);
		bt_put(notif);
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

	if (muxer_upstream_notif_iter->is_valid ||
			!muxer_upstream_notif_iter->notif_iter) {
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

static
struct bt_notification_iterator_next_return muxer_notif_iter_do_next(
		struct muxer_comp *muxer_comp,
		struct muxer_notif_iter *muxer_notif_iter)
{
	struct muxer_upstream_notif_iter *muxer_upstream_notif_iter = NULL;
	struct bt_notification_iterator_next_return next_return = {
		.notification = NULL,
		.status = BT_NOTIFICATION_ITERATOR_STATUS_OK,
	};
	int64_t next_return_ts;

	while (true) {
		int ret = muxer_notif_iter_handle_newly_connected_ports(
			muxer_notif_iter);

		if (ret) {
			BT_LOGE("Cannot handle newly connected input ports for muxer's notification iterator: "
				"muxer-comp-addr=%p, muxer-notif-iter-addr=%p, "
				"ret=%d",
				muxer_comp, muxer_notif_iter, ret);
			next_return.status =
				BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
			goto end;
		}

		next_return.status =
			validate_muxer_upstream_notif_iters(muxer_notif_iter);
		if (next_return.status != BT_NOTIFICATION_ITERATOR_STATUS_OK) {
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
		if (!muxer_notif_iter->newly_connected_priv_ports) {
			BT_LOGV("Not breaking this loop: muxer's notification iterator still has newly connected input ports to handle: "
				"muxer-comp-addr=%p", muxer_comp);
			break;
		}
	}

	assert(!muxer_notif_iter->newly_connected_priv_ports);

	/*
	 * At this point we know that all the existing upstream
	 * notification iterators are valid. We can find the one,
	 * amongst those, of which the current notification is the
	 * youngest.
	 */
	next_return.status =
		muxer_notif_iter_youngest_upstream_notif_iter(muxer_comp,
			muxer_notif_iter, &muxer_upstream_notif_iter,
			&next_return_ts);
	if (next_return.status < 0 ||
			next_return.status == BT_NOTIFICATION_ITERATOR_STATUS_END ||
			next_return.status == BT_NOTIFICATION_ITERATOR_STATUS_CANCELED) {
		if (next_return.status < 0) {
			BT_LOGE("Cannot find the youngest upstream notification iterator wrapper: "
				"status=%s",
				bt_notification_iterator_status_string(next_return.status));
		} else {
			BT_LOGV("Cannot find the youngest upstream notification iterator wrapper: "
				"status=%s",
				bt_notification_iterator_status_string(next_return.status));
		}

		goto end;
	}

	if (next_return_ts < muxer_notif_iter->last_returned_ts_ns) {
		BT_LOGE("Youngest upstream notification iterator wrapper's timestamp is less than muxer's notification iterator's last returned timestamp: "
			"muxer-notif-iter-addr=%p, ts=%" PRId64 ", "
			"last-returned-ts=%" PRId64,
			muxer_notif_iter, next_return_ts,
			muxer_notif_iter->last_returned_ts_ns);
		next_return.status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
		goto end;
	}

	BT_LOGV("Found youngest upstream notification iterator wrapper: "
		"muxer-notif-iter-addr=%p, "
		"muxer-upstream-notif-iter-wrap-addr=%p, "
		"ts=%" PRId64,
		muxer_notif_iter, muxer_upstream_notif_iter, next_return_ts);
	assert(next_return.status == BT_NOTIFICATION_ITERATOR_STATUS_OK);
	assert(muxer_upstream_notif_iter);
	next_return.notification = bt_notification_iterator_get_notification(
		muxer_upstream_notif_iter->notif_iter);
	assert(next_return.notification);

	/*
	 * We invalidate the upstream notification iterator so that, the
	 * next time this function is called,
	 * validate_muxer_upstream_notif_iters() will make it valid.
	 */
	muxer_upstream_notif_iter->is_valid = false;
	muxer_notif_iter->last_returned_ts_ns = next_return_ts;

end:
	return next_return;
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

	g_list_free(muxer_notif_iter->newly_connected_priv_ports);
	g_free(muxer_notif_iter);
}

static
int muxer_notif_iter_init_newly_connected_ports(struct muxer_comp *muxer_comp,
		struct muxer_notif_iter *muxer_notif_iter)
{
	struct bt_component *comp;
	int64_t count;
	int64_t i;
	int ret = 0;

	/*
	 * Add the connected input ports to this muxer notification
	 * iterator's list of newly connected ports. They will be
	 * handled by muxer_notif_iter_handle_newly_connected_ports().
	 */
	comp = bt_component_from_private_component(muxer_comp->priv_comp);
	assert(comp);
	count = bt_component_filter_get_input_port_count(comp);
	if (count < 0) {
		BT_LOGD("No input port to initialize for muxer component's notification iterator: "
			"muxer-comp-addr=%p, muxer-notif-iter-addr=%p",
			muxer_comp, muxer_notif_iter);
		goto end;
	}

	for (i = 0; i < count; i++) {
		struct bt_private_port *priv_port =
			bt_private_component_filter_get_input_private_port_by_index(
				muxer_comp->priv_comp, i);
		struct bt_port *port;

		assert(priv_port);
		port = bt_port_from_private_port(priv_port);
		assert(port);

		if (!bt_port_is_connected(port)) {
			BT_LOGD("Skipping input port: not connected: "
				"muxer-comp-addr=%p, port-addr=%p, port-name\"%s\"",
				muxer_comp, port, bt_port_get_name(port));
			bt_put(priv_port);
			bt_put(port);
			continue;
		}

		bt_put(port);
		bt_put(priv_port);
		muxer_notif_iter->newly_connected_priv_ports =
			g_list_append(
				muxer_notif_iter->newly_connected_priv_ports,
				priv_port);
		if (!muxer_notif_iter->newly_connected_priv_ports) {
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
	bt_put(comp);
	return ret;
}

BT_HIDDEN
enum bt_notification_iterator_status muxer_notif_iter_init(
		struct bt_private_notification_iterator *priv_notif_iter,
		struct bt_private_port *output_priv_port)
{
	struct muxer_comp *muxer_comp = NULL;
	struct muxer_notif_iter *muxer_notif_iter = NULL;
	struct bt_private_component *priv_comp = NULL;
	enum bt_notification_iterator_status status =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;
	int ret;

	priv_comp = bt_private_notification_iterator_get_private_component(
		priv_notif_iter);
	assert(priv_comp);
	muxer_comp = bt_private_component_get_user_data(priv_comp);
	assert(muxer_comp);
	BT_LOGD("Initializing muxer component's notification iterator: "
		"comp-addr=%p, muxer-comp-addr=%p, notif-iter-addr=%p",
		priv_comp, muxer_comp, priv_notif_iter);

	if (muxer_comp->initializing_muxer_notif_iter) {
		/*
		 * Weird, unhandled situation detected: downstream
		 * creates a muxer notification iterator while creating
		 * another muxer notification iterator (same component).
		 */
		BT_LOGE("Recursive initialization of muxer component's notification iterator: "
			"comp-addr=%p, muxer-comp-addr=%p, notif-iter-addr=%p",
			priv_comp, muxer_comp, priv_notif_iter);
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
			priv_comp, muxer_comp, muxer_notif_iter,
			priv_notif_iter, ret);
		goto error;
	}

	ret = bt_private_notification_iterator_set_user_data(priv_notif_iter,
		muxer_notif_iter);
	assert(ret == 0);
	BT_LOGD("Initialized muxer component's notification iterator: "
		"comp-addr=%p, muxer-comp-addr=%p, muxer-notif-iter-addr=%p, "
		"notif-iter-addr=%p",
		priv_comp, muxer_comp, muxer_notif_iter, priv_notif_iter);
	goto end;

error:
	if (g_ptr_array_index(muxer_comp->muxer_notif_iters,
			muxer_comp->muxer_notif_iters->len - 1) == muxer_notif_iter) {
		g_ptr_array_remove_index(muxer_comp->muxer_notif_iters,
			muxer_comp->muxer_notif_iters->len - 1);
	}

	destroy_muxer_notif_iter(muxer_notif_iter);
	ret = bt_private_notification_iterator_set_user_data(priv_notif_iter,
		NULL);
	assert(ret == 0);
	status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;

end:
	muxer_comp->initializing_muxer_notif_iter = false;
	bt_put(priv_comp);
	return status;
}

BT_HIDDEN
void muxer_notif_iter_finalize(
		struct bt_private_notification_iterator *priv_notif_iter)
{
	struct muxer_notif_iter *muxer_notif_iter =
		bt_private_notification_iterator_get_user_data(priv_notif_iter);
	struct bt_private_component *priv_comp = NULL;
	struct muxer_comp *muxer_comp = NULL;

	priv_comp = bt_private_notification_iterator_get_private_component(
		priv_notif_iter);
	assert(priv_comp);
	muxer_comp = bt_private_component_get_user_data(priv_comp);
	BT_LOGD("Finalizing muxer component's notification iterator: "
		"comp-addr=%p, muxer-comp-addr=%p, muxer-notif-iter-addr=%p, "
		"notif-iter-addr=%p",
		priv_comp, muxer_comp, muxer_notif_iter, priv_notif_iter);

	if (muxer_comp) {
		(void) g_ptr_array_remove_fast(muxer_comp->muxer_notif_iters,
			muxer_notif_iter);
		destroy_muxer_notif_iter(muxer_notif_iter);
	}

	bt_put(priv_comp);
}

BT_HIDDEN
struct bt_notification_iterator_next_return muxer_notif_iter_next(
		struct bt_private_notification_iterator *priv_notif_iter)
{
	struct bt_notification_iterator_next_return next_ret;
	struct muxer_notif_iter *muxer_notif_iter =
		bt_private_notification_iterator_get_user_data(priv_notif_iter);
	struct bt_private_component *priv_comp = NULL;
	struct muxer_comp *muxer_comp = NULL;

	assert(muxer_notif_iter);
	priv_comp = bt_private_notification_iterator_get_private_component(
		priv_notif_iter);
	assert(priv_comp);
	muxer_comp = bt_private_component_get_user_data(priv_comp);
	assert(muxer_comp);

	BT_LOGV("Muxer component's notification iterator's \"next\" method called: "
		"comp-addr=%p, muxer-comp-addr=%p, muxer-notif-iter-addr=%p, "
		"notif-iter-addr=%p",
		priv_comp, muxer_comp, muxer_notif_iter, priv_notif_iter);

	/* Are we in an error state set elsewhere? */
	if (unlikely(muxer_comp->error)) {
		BT_LOGE("Muxer component is already in an error state: returning BT_NOTIFICATION_ITERATOR_STATUS_ERROR: "
			"comp-addr=%p, muxer-comp-addr=%p, muxer-notif-iter-addr=%p, "
			"notif-iter-addr=%p",
			priv_comp, muxer_comp, muxer_notif_iter, priv_notif_iter);
		next_ret.notification = NULL;
		next_ret.status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
		goto end;
	}

	next_ret = muxer_notif_iter_do_next(muxer_comp, muxer_notif_iter);
	if (next_ret.status < 0) {
		BT_LOGE("Cannot get next notification: "
			"comp-addr=%p, muxer-comp-addr=%p, muxer-notif-iter-addr=%p, "
			"notif-iter-addr=%p, status=%s",
			priv_comp, muxer_comp, muxer_notif_iter, priv_notif_iter,
			bt_notification_iterator_status_string(next_ret.status));
	} else {
		BT_LOGV("Returning from muxer component's notification iterator's \"next\" method: "
			"status=%s, notif-addr=%p",
			bt_notification_iterator_status_string(next_ret.status),
			next_ret.notification);
	}

end:
	bt_put(priv_comp);
	return next_ret;
}

BT_HIDDEN
void muxer_port_connected(
		struct bt_private_component *priv_comp,
		struct bt_private_port *self_private_port,
		struct bt_port *other_port)
{
	struct bt_port *self_port =
		bt_port_from_private_port(self_private_port);
	struct muxer_comp *muxer_comp =
		bt_private_component_get_user_data(priv_comp);
	size_t i;
	int ret;

	assert(self_port);
	assert(muxer_comp);
	BT_LOGD("Port connected: "
		"comp-addr=%p, muxer-comp-addr=%p, "
		"port-addr=%p, port-name=\"%s\", "
		"other-port-addr=%p, other-port-name=\"%s\"",
		priv_comp, muxer_comp, self_port, bt_port_get_name(self_port),
		other_port, bt_port_get_name(other_port));

	if (bt_port_get_type(self_port) == BT_PORT_TYPE_OUTPUT) {
		goto end;
	}

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
		muxer_notif_iter->newly_connected_priv_ports =
			g_list_append(
				muxer_notif_iter->newly_connected_priv_ports,
				self_private_port);
		if (!muxer_notif_iter->newly_connected_priv_ports) {
			BT_LOGE("Cannot append port to muxer's notification iterator list of newly connected input ports: "
				"port-addr=%p, port-name=\"%s\", "
				"muxer-notif-iter-addr=%p", self_port,
				bt_port_get_name(self_port), muxer_notif_iter);
			muxer_comp->error = true;
			goto end;
		}

		BT_LOGD("Appended port to muxer's notification iterator list of newly connected input ports: "
			"port-addr=%p, port-name=\"%s\", "
			"muxer-notif-iter-addr=%p", self_port,
			bt_port_get_name(self_port), muxer_notif_iter);
	}

	/* One less available input port */
	muxer_comp->available_input_ports--;
	ret = ensure_available_input_port(priv_comp);
	if (ret) {
		/*
		 * Only way to report an error later since this
		 * method does not return anything.
		 */
		BT_LOGE("Cannot ensure that at least one muxer component's input port is available: "
			"muxer-comp-addr=%p, status=%s",
			muxer_comp, bt_component_status_string(ret));
		muxer_comp->error = true;
		goto end;
	}

end:
	bt_put(self_port);
}

BT_HIDDEN
void muxer_port_disconnected(struct bt_private_component *priv_comp,
		struct bt_private_port *priv_port)
{
	struct bt_port *port = bt_port_from_private_port(priv_port);
	struct muxer_comp *muxer_comp =
		bt_private_component_get_user_data(priv_comp);

	assert(port);
	assert(muxer_comp);
	BT_LOGD("Port disconnected: "
		"comp-addr=%p, muxer-comp-addr=%p, port-addr=%p, "
		"port-name=\"%s\"", priv_comp, muxer_comp,
		port, bt_port_get_name(port));

	/*
	 * There's nothing special to do when a port is disconnected
	 * because this component deals with upstream notification
	 * iterators which were already created thanks to connected
	 * ports. The fact that the port is disconnected does not cancel
	 * the upstream notification iterators created using its
	 * connection: they still exist, even if the connection is dead.
	 * The only way to remove an upstream notification iterator is
	 * for its "next" operation to return
	 * BT_NOTIFICATION_ITERATOR_STATUS_END.
	 */
	if (bt_port_get_type(port) == BT_PORT_TYPE_INPUT) {
		/* One more available input port */
		muxer_comp->available_input_ports++;
		BT_LOGD("Leaving disconnected input port available for future connections: "
			"comp-addr=%p, muxer-comp-addr=%p, port-addr=%p, "
			"port-name=\"%s\", avail-input-port-count=%zu",
			priv_comp, muxer_comp, port, bt_port_get_name(port),
			muxer_comp->available_input_ports);
	}

	bt_put(port);
}
