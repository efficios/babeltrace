/*
 * Copyright 2010-2011 EfficiOS Inc. and Linux Foundation
 * Copyright 2017 Philippe Proulx <jeremie.galarneau@efficios.com>
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

#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/graph/component.h>
#include <babeltrace/graph/clock-class-priority-map.h>
#include <glib.h>

struct dmesg_component;

struct dmesg_notif_iter {
	struct dmesg_component *dmesg_comp;
	FILE *fp;
};

struct dmesg_component {
	struct {
		GString *path;
		bool read_from_stdin;
	} params;

	struct bt_ctf_packet *packet;
	struct bt_ctf_event_class *event_class;
	struct bt_ctf_stream *stream;
	struct bt_clock_class_priority_map *cc_prio_map;
};

static
int check_params(struct dmesg_component *dmesg_comp, struct bt_value *params)
{
	struct bt_value *read_from_stdin;
	struct bt_value *path;
	const char *path_str;
	int ret = 0;

	if (!params || !bt_value_is_map(params)) {
		fprintf(stderr, "Expecting a map value as parameters.\n");
		goto error;
	}

	read_from_stdin = bt_value_map_get(params, "read-from-stdin");
	if (read_from_stdin) {
		if (!bt_value_is_bool(read_from_stdin)) {
			fprintf(stderr, "Expecting a boolean value for `read-from-stdin` parameter.\n");
			goto error;
		}

		ret = bt_value_bool_get(read_from_stdin,
			&dmesg_comp->params.read_from_stdin);
		assert(ret == 0);
	}

	path = bt_value_map_get(params, "path");
	if (path) {
		if (dmesg_comp->params.read_from_stdin) {
			fprintf(stderr, "Cannot specify both `read-from-stdin` and `path` parameters.\n");
			goto error;
		}

		if (!bt_value_is_string(path)) {
			fprintf(stderr, "Expecting a string value for `path` parameter.\n");
			goto error;
		}

		ret = bt_value_bool_get(path, &path_str);
		assert(ret == 0);
		g_string_assign(dmesg_comp->params.path, path_str);
	} else {
		if (!dmesg_comp->params.read_from_stdin) {
			fprintf(stderr, "Expecting `path` parameter or `read-from-stdin` parameter set to true.\n");
			goto error;
		}
	}

	goto end;

error:
	ret = -1;

end:
	bt_put(read_from_stdin);
	bt_put(path);
	return ret;
}

static
int create_stream()

static
void destroy_dmesg_component(struct dmesg_component *dmesg_comp)
{
	if (!dmesg_comp) {
		return;
	}

	if (dmesg_comp->params.path) {
		g_string_free(dmesg_comp->params.path, TRUE);
	}

	bt_put(dmesg_comp->packet);
	bt_put(dmesg_comp->event_class);
	bt_put(dmesg_comp->stream);
	bt_put(dmesg_comp->cc_prio_map);
	g_free(dmesg_comp);
}

BT_HIDDEN
enum bt_component_status dmesg_init(struct bt_private_component *priv_comp,
		struct bt_value *params, void *init_method_data)
{
	int ret = 0;
	struct dmesg_component *dmesg_comp = g_new0(struct dmesg_component, 1);
	enum bt_component_status status = BT_COMPONENT_STATUS_OK;

	if (!dmesg_comp) {
		goto error;
	}

	dmesg_comp->params.path = g_string_new(NULL);
	if (!dmesg_comp->params.path) {
		goto error;
	}

	ret = check_params(dmesg_comp, params);
	if (ret) {
		goto error;
	}



	goto end;

error:
	destroy_dmesg_component(dmesg_comp);
	status = BT_COMPONENT_STATUS_ERROR;

end:
	return status;
}

BT_HIDDEN
void dmesg_finalize(struct bt_private_component *priv_comp)
{

}

BT_HIDDEN
enum bt_notification_iterator_status dmesg_notif_iter_init(
		struct bt_private_notification_iterator *priv_notif_iter,
		struct bt_private_port *priv_port)
{

}

BT_HIDDEN
void dmesg_iterator_finalize(
		struct bt_private_notification_iterator *priv_notif_iter)
{

}

BT_HIDDEN
struct bt_notification_iterator_next_return dmesg_notif_iter_next(
		struct bt_private_notification_iterator *priv_notif_iter)
{

}
