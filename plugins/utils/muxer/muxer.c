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

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-ir/event.h>
#include <babeltrace/graph/clock-class-priority-map.h>
#include <babeltrace/graph/component-filter.h>
#include <babeltrace/graph/component.h>
#include <babeltrace/graph/notification-event.h>
#include <babeltrace/graph/notification-inactivity.h>
#include <babeltrace/graph/notification-iterator.h>
#include <babeltrace/graph/notification.h>
#include <babeltrace/graph/port.h>
#include <babeltrace/graph/private-component-filter.h>
#include <babeltrace/graph/private-component.h>
#include <babeltrace/graph/private-component.h>
#include <babeltrace/graph/private-connection.h>
#include <babeltrace/graph/private-notification-iterator.h>
#include <babeltrace/graph/private-port.h>
#include <plugins-common.h>
#include <glib.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>

#define IGNORE_ABSOLUTE_PARAM_NAME	"ignore-absolute"

struct muxer_comp {
	/* Array of struct bt_private_notification_iterator * (weak refs) */
	GPtrArray *muxer_notif_iters;

	/* Weak ref */
	struct bt_private_component *priv_comp;
	unsigned int next_port_num;
	size_t available_input_ports;
	bool error;
	bool initializing_muxer_notif_iter;
	bool ignore_absolute;
};

struct muxer_upstream_notif_iter {
	/* Owned by this, NULL if ended */
	struct bt_notification_iterator *notif_iter;

	/* Weak */
	struct bt_private_port *priv_port;

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
};

static
void destroy_muxer_upstream_notif_iter(
		struct muxer_upstream_notif_iter *muxer_upstream_notif_iter)
{
	if (!muxer_upstream_notif_iter) {
		return;
	}

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
		goto end;
	}

	muxer_upstream_notif_iter->notif_iter = bt_get(notif_iter);
	muxer_upstream_notif_iter->priv_port = priv_port;
	muxer_upstream_notif_iter->is_valid = false;
	g_ptr_array_add(muxer_notif_iter->muxer_upstream_notif_iters,
		muxer_upstream_notif_iter);

end:
	return muxer_upstream_notif_iter;
}

static
int ensure_available_input_port(struct bt_private_component *priv_comp)
{
	struct muxer_comp *muxer_comp =
		bt_private_component_get_user_data(priv_comp);
	int ret = 0;
	GString *port_name = NULL;
	void *priv_port = NULL;

	assert(muxer_comp);

	if (muxer_comp->available_input_ports >= 1) {
		goto end;
	}

	port_name = g_string_new("in");
	if (!port_name) {
		ret = -1;
		goto end;
	}

	g_string_append_printf(port_name, "%u", muxer_comp->next_port_num);
	priv_port = bt_private_component_filter_add_input_private_port(
		priv_comp, port_name->str, NULL);
	if (!priv_port) {
		ret = -1;
		goto end;
	}

	muxer_comp->available_input_ports++;
	muxer_comp->next_port_num++;

end:
	if (port_name) {
		g_string_free(port_name, TRUE);
	}

	bt_put(priv_port);
	return ret;
}

static
int create_output_port(struct bt_private_component *priv_comp)
{
	void *priv_port;
	int ret = 0;

	priv_port = bt_private_component_filter_add_output_private_port(
		priv_comp, "out", NULL);
	if (!priv_port) {
		ret = -1;
	}

	bt_put(priv_port);
	return ret;
}

static
void destroy_muxer_comp(struct muxer_comp *muxer_comp)
{
	if (!muxer_comp) {
		return;
	}

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
		goto error;
	}

	ret = bt_value_map_insert_bool(params, IGNORE_ABSOLUTE_PARAM_NAME,
		false);
	if (ret) {
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
	struct bt_value *ignore_absolute = NULL;
	int ret = 0;
	bt_bool bool_val;

	default_params = get_default_params();
	if (!default_params) {
		goto error;
	}

	real_params = bt_value_map_extend(default_params, params);
	if (!real_params) {
		goto error;
	}

	ignore_absolute = bt_value_map_get(real_params,
		IGNORE_ABSOLUTE_PARAM_NAME);
	if (!bt_value_is_bool(ignore_absolute)) {
		goto error;
	}

	if (bt_value_bool_get(ignore_absolute, &bool_val)) {
		goto error;
	}

	muxer_comp->ignore_absolute = (bool) bool_val;

	goto end;

error:
	ret = -1;

end:
	bt_put(default_params);
	bt_put(real_params);
	bt_put(ignore_absolute);
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

	if (!muxer_comp) {
		goto error;
	}

	ret = configure_muxer_comp(muxer_comp, params);
	if (ret) {
		goto error;
	}

	muxer_comp->muxer_notif_iters = g_ptr_array_new();
	if (!muxer_comp->muxer_notif_iters) {
		goto error;
	}

	muxer_comp->priv_comp = priv_comp;
	ret = bt_private_component_set_user_data(priv_comp, muxer_comp);
	assert(ret == 0);
	ret = ensure_available_input_port(priv_comp);
	if (ret) {
		goto error;
	}

	ret = create_output_port(priv_comp);
	if (ret) {
		goto error;
	}

	goto end;

error:
	destroy_muxer_comp(muxer_comp);
	ret = bt_private_component_set_user_data(priv_comp, NULL);
	assert(ret == 0);
	status = BT_COMPONENT_STATUS_ERROR;

end:
	return status;
}

BT_HIDDEN
void muxer_finalize(struct bt_private_component *priv_comp)
{
	struct muxer_comp *muxer_comp =
		bt_private_component_get_user_data(priv_comp);

	destroy_muxer_comp(muxer_comp);
}

static
struct bt_notification_iterator *create_notif_iter_on_input_port(
		struct bt_private_port *priv_port, int *ret)
{
	struct bt_port *port = bt_port_from_private_port(priv_port);
	struct bt_notification_iterator *notif_iter = NULL;
	struct bt_private_connection *priv_conn = NULL;

	assert(ret);
	*ret = 0;
	assert(port);

	assert(bt_port_is_connected(port));
	priv_conn = bt_private_port_get_private_connection(priv_port);
	if (!priv_conn) {
		*ret = -1;
		goto end;
	}

	// TODO: Advance the iterator to >= the time of the latest
	//       returned notification by the muxer notification
	//       iterator which creates it.
	notif_iter = bt_private_connection_create_notification_iterator(
		priv_conn, NULL);
	if (!notif_iter) {
		*ret = -1;
		goto end;
	}

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

	status = bt_notification_iterator_next(
		muxer_upstream_notif_iter->notif_iter);

	switch (status) {
	case BT_NOTIFICATION_ITERATOR_STATUS_OK:
		/*
		 * Notification iterator's current notification is valid:
		 * it must be considered for muxing operations.
		 */
		muxer_upstream_notif_iter->is_valid = true;
		break;
	case BT_NOTIFICATION_ITERATOR_STATUS_AGAIN:
		/*
		 * Notification iterator's current notification is not
		 * valid anymore. Return
		 * BT_NOTIFICATION_ITERATOR_STATUS_AGAIN
		 * immediately.
		 */
		muxer_upstream_notif_iter->is_valid = false;
		break;
	case BT_NOTIFICATION_ITERATOR_STATUS_END:	/* Fall-through. */
	case BT_NOTIFICATION_ITERATOR_STATUS_CANCELED:
		/*
		 * Notification iterator reached the end: release it. It
		 * won't be considered again to find the youngest
		 * notification.
		 */
		BT_PUT(muxer_upstream_notif_iter->notif_iter);
		muxer_upstream_notif_iter->is_valid = false;
		status = BT_NOTIFICATION_ITERATOR_STATUS_OK;
		break;
	default:
		/* Error or unsupported status code */
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
			assert(!upstream_notif_iter);
			goto error;
		}

		muxer_upstream_notif_iter =
			muxer_notif_iter_add_upstream_notif_iter(
				muxer_notif_iter, upstream_notif_iter,
				priv_port);
		BT_PUT(upstream_notif_iter);
		if (!muxer_upstream_notif_iter) {
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
		struct bt_notification *notif, int64_t last_returned_ts_ns,
		int64_t *ts_ns)
{
	struct bt_clock_class_priority_map *cc_prio_map = NULL;
	struct bt_ctf_clock_class *clock_class = NULL;
	struct bt_ctf_clock_value *clock_value = NULL;
	struct bt_ctf_event *event = NULL;
	int ret = 0;

	assert(notif);
	assert(ts_ns);

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
		*ts_ns = last_returned_ts_ns;
		goto end;
	}

	if (!cc_prio_map) {
		goto error;
	}

	/*
	 * If the clock class priority map is empty, then we consider
	 * that this notification has no time. In this case it's always
	 * the youngest.
	 */
	if (bt_clock_class_priority_map_get_clock_class_count(cc_prio_map) == 0) {
		*ts_ns = last_returned_ts_ns;
		goto end;
	}

	clock_class =
		bt_clock_class_priority_map_get_highest_priority_clock_class(
			cc_prio_map);
	if (!clock_class) {
		goto error;
	}

	if (!muxer_comp->ignore_absolute &&
			!bt_ctf_clock_class_is_absolute(clock_class)) {
		goto error;
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
		abort();
	}

	if (!clock_value) {
		goto error;
	}

	ret = bt_ctf_clock_value_get_value_ns_from_epoch(clock_value, ts_ns);
	if (ret) {
		goto error;
	}

	goto end;

error:
	ret = -1;

end:
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
			continue;
		}

		assert(cur_muxer_upstream_notif_iter->is_valid);
		notif = bt_notification_iterator_get_notification(
			cur_muxer_upstream_notif_iter->notif_iter);
		assert(notif);
		ret = get_notif_ts_ns(muxer_comp, notif,
			muxer_notif_iter->last_returned_ts_ns, &notif_ts_ns);
		bt_put(notif);
		if (ret) {
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

	if (muxer_upstream_notif_iter->is_valid ||
			!muxer_upstream_notif_iter->notif_iter) {
		goto end;
	}

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

	for (i = 0; i < muxer_notif_iter->muxer_upstream_notif_iters->len; i++) {
		struct muxer_upstream_notif_iter *muxer_upstream_notif_iter =
			g_ptr_array_index(
				muxer_notif_iter->muxer_upstream_notif_iters,
				i);

		status = validate_muxer_upstream_notif_iter(
			muxer_upstream_notif_iter);
		if (status != BT_NOTIFICATION_ITERATOR_STATUS_OK) {
			goto end;
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
			next_return.status =
				BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
			goto end;
		}

		next_return.status =
			validate_muxer_upstream_notif_iters(muxer_notif_iter);
		if (next_return.status != BT_NOTIFICATION_ITERATOR_STATUS_OK) {
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
		goto end;
	}

	if (next_return_ts < muxer_notif_iter->last_returned_ts_ns) {
		next_return.status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
		goto end;
	}

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

	if (muxer_notif_iter->muxer_upstream_notif_iters) {
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
			ret = -1;
			goto end;
		}
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

	if (muxer_comp->initializing_muxer_notif_iter) {
		/*
		 * Weird, unhandled situation detected: downstream
		 * creates a muxer notification iterator while creating
		 * another muxer notification iterator (same component).
		 */
		goto error;
	}

	muxer_comp->initializing_muxer_notif_iter = true;
	muxer_notif_iter = g_new0(struct muxer_notif_iter, 1);
	if (!muxer_notif_iter) {
		goto error;
	}

	muxer_notif_iter->last_returned_ts_ns = INT64_MIN;
	muxer_notif_iter->muxer_upstream_notif_iters =
		g_ptr_array_new_with_free_func(
			(GDestroyNotify) destroy_muxer_upstream_notif_iter);
	if (!muxer_notif_iter->muxer_upstream_notif_iters) {
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
		goto error;
	}

	ret = bt_private_notification_iterator_set_user_data(priv_notif_iter,
		muxer_notif_iter);
	assert(ret == 0);
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

	/* Are we in an error state set elsewhere? */
	if (unlikely(muxer_comp->error)) {
		next_ret.notification = NULL;
		next_ret.status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
		goto end;
	}

	next_ret = muxer_notif_iter_do_next(muxer_comp, muxer_notif_iter);

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
		 *
		 * The list node owns the private port.
		 */
		muxer_notif_iter->newly_connected_priv_ports =
			g_list_append(
				muxer_notif_iter->newly_connected_priv_ports,
				self_private_port);
		if (!muxer_notif_iter->newly_connected_priv_ports) {
			/* Put reference taken by bt_get() above */
			muxer_comp->error = true;
			goto end;
		}
	}

	/* One less available input port */
	muxer_comp->available_input_ports--;
	ret = ensure_available_input_port(priv_comp);
	if (ret) {
		/*
		 * Only way to report an error later since this
		 * method does not return anything.
		 */
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
	}

	bt_put(port);
}
