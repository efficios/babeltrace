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
#include <assert.h>

struct muxer_comp {
	/* Array of struct bt_private_notification_iterator * (weak refs) */
	GPtrArray *muxer_notif_iters;

	/* Weak ref */
	struct bt_private_component *priv_comp;
	unsigned int next_port_num;
	size_t available_input_ports;
	bool error;
	bool initializing_muxer_notif_iter;
};

struct muxer_upstream_notif_iter {
	/* Owned by this, NULL if ended */
	struct bt_notification_iterator *notif_iter;

	/* Owned by this*/
	struct bt_private_port *priv_port;
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

	/* Array of struct muxer_upstream_notif_iter * (weak refs) */
	GList *muxer_upstream_notif_iters_to_retry;

	/*
	 * List of "recently" connected input ports (owned by this) to
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
	int64_t next_next_return_ts_ns;

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
	bt_put(muxer_upstream_notif_iter->priv_port);
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
	muxer_upstream_notif_iter->priv_port = bt_get(priv_port);
	g_ptr_array_add(muxer_notif_iter->muxer_upstream_notif_iters,
		muxer_upstream_notif_iter);

end:
	return muxer_upstream_notif_iter;
}

static inline
bool muxer_notif_iter_has_upstream_notif_iter_to_retry(
		struct muxer_notif_iter *muxer_notif_iter)
{
	assert(muxer_notif_iter);
	return muxer_notif_iter->muxer_upstream_notif_iters_to_retry != NULL;
}

static
void muxer_notif_iter_add_upstream_notif_iter_to_retry(
		struct muxer_notif_iter *muxer_notif_iter,
		struct muxer_upstream_notif_iter *muxer_upstream_notif_iter)
{
	assert(muxer_notif_iter);
	assert(muxer_upstream_notif_iter);
	muxer_notif_iter->muxer_upstream_notif_iters_to_retry =
		g_list_append(
			muxer_notif_iter->muxer_upstream_notif_iters_to_retry,
			muxer_upstream_notif_iter);
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
		priv_comp, port_name->str);
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

	BT_PUT(priv_port);
	return ret;
}

static
int remove_default_ports(struct bt_private_component *priv_comp)
{
	struct bt_private_port *priv_port;
	int ret = 0;

	priv_port = bt_private_component_filter_get_default_input_private_port(
		priv_comp);
	if (priv_port) {
		ret = bt_private_port_remove_from_component(priv_port);
		if (ret) {
			goto end;
		}
	}

	bt_put(priv_port);
	priv_port = bt_private_component_filter_get_default_output_private_port(
		priv_comp);
	if (priv_port) {
		ret = bt_private_port_remove_from_component(priv_port);
		if (ret) {
			goto end;
		}
	}

end:
	bt_put(priv_port);
	return ret;
}

static
int create_output_port(struct bt_private_component *priv_comp)
{
	void *priv_port;
	int ret = 0;

	priv_port = bt_private_component_filter_add_output_private_port(
		priv_comp, "out");
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

	muxer_comp->muxer_notif_iters = g_ptr_array_new();
	if (!muxer_comp->muxer_notif_iters) {
		goto error;
	}

	muxer_comp->priv_comp = priv_comp;
	ret = bt_private_component_set_user_data(priv_comp, muxer_comp);
	assert(ret == 0);
	ret = remove_default_ports(priv_comp);
	if (ret) {
		goto error;
	}

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
		priv_conn);
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
int muxer_upstream_notif_iter_next(struct muxer_notif_iter *muxer_notif_iter,
		struct muxer_upstream_notif_iter *muxer_upstream_notif_iter)
{
	int ret = 0;
	enum bt_notification_iterator_status next_status;

	next_status = bt_notification_iterator_next(
		muxer_upstream_notif_iter->notif_iter);

	switch (next_status) {
	case BT_NOTIFICATION_ITERATOR_STATUS_OK:
		/* Everything okay */
		break;
	case BT_NOTIFICATION_ITERATOR_STATUS_AGAIN:
		muxer_notif_iter_add_upstream_notif_iter_to_retry(
			muxer_notif_iter, muxer_upstream_notif_iter);
		break;
	case BT_NOTIFICATION_ITERATOR_STATUS_END:
		/*
		 * Notification iterator reached the end: release it. It
		 * won't be considered again to find the youngest
		 * notification.
		 */
		BT_PUT(muxer_upstream_notif_iter->notif_iter);
		goto end;
	default:
		/* Error or unsupported status code */
		ret = next_status;
	}

end:
	return ret;
}

static
int muxer_notif_iter_handle_newly_connected_ports(struct muxer_comp *muxer_comp,
		struct muxer_notif_iter *muxer_notif_iter)
{
	struct bt_component *comp = NULL;
	int ret = 0;

	comp = bt_component_from_private_component(muxer_comp->priv_comp);
	assert(comp);

	/*
	 * Here we create one upstream notification iterator for each
	 * newly connected port. The list of newly connected ports to
	 * handle here is updated by muxer_port_connected().
	 *
	 * An initial "next" operation is performed on each new upstream
	 * notification iterator. The possible return values of this
	 * initial "next" operation are:
	 *
	 * * BT_NOTIFICATION_ITERATOR_STATUS_OK: Perfect, we have a
	 *   current notification.
	 *
	 * * BT_NOTIFICATION_ITERATOR_STATUS_AGAIN: No notification so
	 *   far, but the muxer upstream notification iterator is added
	 *   to the list of upstream notification iterators to retry
	 *   before finding the next youngest notification.
	 *
	 * * BT_NOTIFICATION_ITERATOR_STATUS_END: No notification, and
	 *   we immediately release the upstream notification iterator
	 *   because it's useless.
	 *
	 * A possible side effect of this initial "next" operation, on
	 * each notification iterator, is the connection of a new port.
	 * In this case the list of newly connected ports is updated and
	 * this loop continues.
	 *
	 * Once this loop finishes successfully, the set of upstream
	 * notification iterators is considered _stable_, that is, it is
	 * safe, if no notification iterators must be retried, to select
	 * the youngest notification amongst them to be returned by the
	 * next "next" method call.
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
			 * notification iterator on its connection in
			 * this case.
			 */
			goto remove_node;
		}

		BT_PUT(port);
		upstream_notif_iter = create_notif_iter_on_input_port(priv_port,
			&ret);
		if (ret) {
			assert(!upstream_notif_iter);
			bt_put(priv_port);
			goto error;
		}

		muxer_upstream_notif_iter =
			muxer_notif_iter_add_upstream_notif_iter(
				muxer_notif_iter, upstream_notif_iter,
				priv_port);
		BT_PUT(priv_port);
		BT_PUT(upstream_notif_iter);
		if (!muxer_upstream_notif_iter) {
			goto error;
		}

		ret = muxer_upstream_notif_iter_next(muxer_notif_iter,
			muxer_upstream_notif_iter);
		if (ret) {
			goto error;
		}

remove_node:
		bt_put(upstream_notif_iter);
		bt_put(port);
		bt_put(priv_port);
		muxer_notif_iter->newly_connected_priv_ports =
			g_list_delete_link(
				muxer_notif_iter->newly_connected_priv_ports,
				node);
	}

	goto end;

error:
	if (ret == 0) {
		ret = -1;
	}

end:
	bt_put(comp);
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
		/*
		 * All the other notifications have a higher
		 * priority.
		 */
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

	if (!bt_ctf_clock_class_is_absolute(clock_class)) {
		// TODO: Allow this with an explicit parameter
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
		assert(false);
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
int muxer_notif_iter_set_next_next_return(struct muxer_comp *muxer_comp,
		struct muxer_notif_iter *muxer_notif_iter)
{
	struct muxer_upstream_notif_iter *muxer_upstream_notif_iter;
	enum bt_notification_iterator_status notif_iter_status;
	int ret = 0;

	/*
	 * Previous operations might have connected ports. They must be
	 * considered when finding the youngest notification because
	 * their upstream notification iterator does not exist yet.
	 */
	ret = muxer_notif_iter_handle_newly_connected_ports(muxer_comp,
		muxer_notif_iter);
	if (ret) {
		muxer_notif_iter->next_next_return.status =
			BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
		BT_PUT(muxer_notif_iter->next_next_return.notification);
		goto end;
	}

	assert(!muxer_notif_iter->newly_connected_priv_ports);

	if (muxer_notif_iter_has_upstream_notif_iter_to_retry(
			muxer_notif_iter)) {
		/*
		 * At least one upstream notification iterator to retry:
		 * try again later, because we cannot find the youngest
		 * notification if we don't have the current
		 * notification of each upstream notification iterator.
		 */
		muxer_notif_iter->next_next_return.status =
			BT_NOTIFICATION_ITERATOR_STATUS_AGAIN;
		BT_PUT(muxer_notif_iter->next_next_return.notification);
		goto end;
	}

	/*
	 * At this point we know that all our connected ports have an
	 * upstream notification iterator, and that all those iterators
	 * have a current notification (stable state). It is safe to
	 * find the youngest notification. It is possible that calling
	 * "next" on its iterator will connect new ports. This will be
	 * handled by the next call to
	 * muxer_notif_iter_set_next_next_return().
	 */
	notif_iter_status =
		muxer_notif_iter_youngest_upstream_notif_iter(muxer_comp,
			muxer_notif_iter, &muxer_upstream_notif_iter,
			&muxer_notif_iter->next_next_return_ts_ns);
	if (notif_iter_status == BT_NOTIFICATION_ITERATOR_STATUS_END) {
		/* No more active upstream notification iterator */
		muxer_notif_iter->next_next_return.status =
			BT_NOTIFICATION_ITERATOR_STATUS_END;
		BT_PUT(muxer_notif_iter->next_next_return.notification);
		goto end;
	}

	if (notif_iter_status < 0) {
		ret = -1;
		goto end;
	}

	assert(notif_iter_status == BT_NOTIFICATION_ITERATOR_STATUS_OK);
	BT_PUT(muxer_notif_iter->next_next_return.notification);
	muxer_notif_iter->next_next_return.notification =
		bt_notification_iterator_get_notification(
			muxer_upstream_notif_iter->notif_iter);
	assert(muxer_notif_iter->next_next_return.notification);
	muxer_notif_iter->next_next_return.status =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;
	ret = muxer_upstream_notif_iter_next(muxer_notif_iter,
		muxer_upstream_notif_iter);
	if (ret) {
		goto end;
	}

	/*
	 * Here we have the next "next" return value. It won't change
	 * until it is returned by the next call to our "next" method.
	 * If its time is less than the time of the last notification
	 * that our "next" method returned, then fail because the
	 * muxer's output wouldn't be monotonic.
	 */
	if (muxer_notif_iter->next_next_return_ts_ns <
			muxer_notif_iter->last_returned_ts_ns) {
		ret = -1;
		goto end;
	}

	/*
	 * We are now sure that the next "next" return value will not
	 * change until it is returned by this muxer notification
	 * iterator (unless there's a fatal error). It is now safe to
	 * set the last returned time to this one.
	 */
	muxer_notif_iter->last_returned_ts_ns =
		muxer_notif_iter->next_next_return_ts_ns;

end:
	return ret;
}

static
void destroy_muxer_notif_iter(struct muxer_notif_iter *muxer_notif_iter)
{
	GList *node;

	if (!muxer_notif_iter) {
		return;
	}

	if (muxer_notif_iter->muxer_upstream_notif_iters) {
		g_ptr_array_free(
			muxer_notif_iter->muxer_upstream_notif_iters, TRUE);
	}

	if (muxer_notif_iter->muxer_upstream_notif_iters_to_retry) {
		g_list_free(muxer_notif_iter->muxer_upstream_notif_iters_to_retry);
	}

	for (node = muxer_notif_iter->newly_connected_priv_ports;
			node; node = g_list_next(node)) {
		bt_put(node->data);
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
			bt_private_component_filter_get_input_private_port_at_index(
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
		muxer_notif_iter->newly_connected_priv_ports =
			g_list_append(
				muxer_notif_iter->newly_connected_priv_ports,
				priv_port);
		if (!muxer_notif_iter->newly_connected_priv_ports) {
			bt_put(priv_port);
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
		 * Weird, unhandled situation: downstream creates a
		 * muxer notification iterator while creating another
		 * muxer notification iterator (same component).
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

	/* Set the initial "next" return value */
	ret = muxer_notif_iter_set_next_next_return(muxer_comp,
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
	struct bt_notification_iterator_next_return next_ret = {
		.notification = NULL,
	};
	struct muxer_notif_iter *muxer_notif_iter =
		bt_private_notification_iterator_get_user_data(priv_notif_iter);
	struct bt_private_component *priv_comp = NULL;
	struct muxer_comp *muxer_comp = NULL;
	GList *retry_node;
	int ret;

	assert(muxer_notif_iter);
	priv_comp = bt_private_notification_iterator_get_private_component(
		priv_notif_iter);
	assert(priv_comp);
	muxer_comp = bt_private_component_get_user_data(priv_comp);
	assert(muxer_comp);

	/* Are we in an error state set elsewhere? */
	if (unlikely(muxer_comp->error)) {
		goto error;
	}

	/*
	 * If we have upstream notification iterators to retry, retry
	 * them now. Each one we find which now has a notification or
	 * is in "end" state, we set it to NULL in this array. Then
	 * we remove all the NULL values from this array.
	 */
	retry_node = muxer_notif_iter->muxer_upstream_notif_iters_to_retry;
	while (retry_node) {
		struct muxer_upstream_notif_iter *muxer_upstream_notif_iter =
			retry_node->data;
		enum bt_notification_iterator_status status;
		GList *next_retry_node = g_list_next(retry_node);

		assert(muxer_upstream_notif_iter->notif_iter);
		status = bt_notification_iterator_next(
			muxer_upstream_notif_iter->notif_iter);
		if (status < 0) {
			goto error;
		}

		if (status == BT_NOTIFICATION_ITERATOR_STATUS_END) {
			/*
			 * This upstream notification iterator is done.
			 * Put the iterator and remove node from list.
			 */
			BT_PUT(muxer_upstream_notif_iter->notif_iter);
			muxer_notif_iter->muxer_upstream_notif_iters_to_retry =
				g_list_delete_link(
					muxer_notif_iter->muxer_upstream_notif_iters_to_retry,
					retry_node);
			retry_node = next_retry_node;
			continue;
		}

		assert(status == BT_NOTIFICATION_ITERATOR_STATUS_OK ||
			status == BT_NOTIFICATION_ITERATOR_STATUS_AGAIN);

		if (status == BT_NOTIFICATION_ITERATOR_STATUS_OK) {
			/*
			 * This upstream notification iterator now has.
			 * a notification. Remove it from this list.
			 */
			muxer_notif_iter->muxer_upstream_notif_iters_to_retry =
				g_list_delete_link(
					muxer_notif_iter->muxer_upstream_notif_iters_to_retry,
					retry_node);
		}

		retry_node = next_retry_node;
	}

	/* Take our next "next" next return value */
	next_ret = muxer_notif_iter->next_next_return;
	muxer_notif_iter->next_next_return.status =
		BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
	muxer_notif_iter->next_next_return.notification = NULL;

	/* Set the next "next" return value */
	ret = muxer_notif_iter_set_next_next_return(muxer_comp,
		muxer_notif_iter);
	if (ret) {
		goto error;
	}

	goto end;

error:
	/*
	 * Technically we already have a next "next" return value which
	 * is ready for this call, but we're failing within this call,
	 * so discard this buffer and return the error ASAP.
	 */
	BT_PUT(next_ret.notification);
	next_ret.status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;

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

	assert(self_port);
	assert(muxer_comp);

	if (bt_port_get_type(self_port) == BT_PORT_TYPE_INPUT) {
		int ret;

		/* One less available input port */
		muxer_comp->available_input_ports--;
		ret = ensure_available_input_port(priv_comp);
		if (ret) {
			muxer_comp->error = true;
			goto end;
		}
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
				bt_get(self_private_port));
		if (!muxer_notif_iter->newly_connected_priv_ports) {
			bt_put(self_private_port);
			muxer_comp->error = true;
			goto end;
		}
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
	 * connection: they still exist. The only way to remove an
	 * upstream notification iterator is for its "next" operation to
	 * return BT_NOTIFICATION_ITERATOR_STATUS_END.
	 */
	if (bt_port_get_type(port) == BT_PORT_TYPE_INPUT) {
		/* One more available input port */
		muxer_comp->available_input_ports++;
	}

	bt_put(port);
}
