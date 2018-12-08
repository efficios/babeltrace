/*
 * Copyright 2010-2011 EfficiOS Inc. and Linux Foundation
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

#define BT_LOG_TAG "PLUGIN-TEXT-DMESG-SRC"
#include "logging.h"

#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/common-internal.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/value-internal.h>
#include <babeltrace/compat/utc-internal.h>
#include <babeltrace/compat/stdio-internal.h>
#include <glib.h>

#define NSEC_PER_USEC 1000UL
#define NSEC_PER_MSEC 1000000UL
#define NSEC_PER_SEC 1000000000ULL
#define USEC_PER_SEC 1000000UL

struct dmesg_component;

struct dmesg_msg_iter {
	struct dmesg_component *dmesg_comp;
	bt_self_message_iterator *pc_msg_iter; /* Weak */
	char *linebuf;
	size_t linebuf_len;
	FILE *fp;
	bt_message *tmp_event_msg;

	enum {
		STATE_EMIT_STREAM_BEGINNING,
		STATE_EMIT_PACKET_BEGINNING,
		STATE_EMIT_EVENT,
		STATE_EMIT_PACKET_END,
		STATE_EMIT_STREAM_END,
		STATE_DONE,
	} state;
};

struct dmesg_component {
	struct {
		GString *path;
		bt_bool read_from_stdin;
		bt_bool no_timestamp;
	} params;

	bt_trace_class *trace_class;
	bt_stream_class *stream_class;
	bt_event_class *event_class;
	bt_trace *trace;
	bt_stream *stream;
	bt_packet *packet;
	bt_clock_class *clock_class;
};

static
bt_field_class *create_event_payload_fc(void)
{
	bt_field_class *root_fc = NULL;
	bt_field_class *fc = NULL;
	int ret;

	root_fc = bt_field_class_structure_create();
	if (!root_fc) {
		BT_LOGE_STR("Cannot create an empty structure field class object.");
		goto error;
	}

	fc = bt_field_class_string_create();
	if (!fc) {
		BT_LOGE_STR("Cannot create a string field class object.");
		goto error;
	}

	ret = bt_field_class_structure_append_member(root_fc,
		"str", fc);
	if (ret) {
		BT_LOGE("Cannot add `str` member to structure field class: "
			"ret=%d", ret);
		goto error;
	}

	goto end;

error:
	BT_FIELD_CLASS_PUT_REF_AND_RESET(root_fc);

end:
	bt_field_class_put_ref(fc);
	return root_fc;
}

static
int create_meta(struct dmesg_component *dmesg_comp, bool has_ts)
{
	bt_field_class *fc = NULL;
	int ret = 0;

	dmesg_comp->trace_class = bt_trace_class_create();
	if (!dmesg_comp->trace_class) {
		BT_LOGE_STR("Cannot create an empty trace class object.");
		goto error;
	}

	dmesg_comp->stream_class = bt_stream_class_create(
		dmesg_comp->trace_class);
	if (!dmesg_comp->stream_class) {
		BT_LOGE_STR("Cannot create a stream class object.");
		goto error;
	}

	if (has_ts) {
		dmesg_comp->clock_class = bt_clock_class_create();
		if (!dmesg_comp->clock_class) {
			BT_LOGE_STR("Cannot create clock class.");
			goto error;
		}

		ret = bt_stream_class_set_default_clock_class(
			dmesg_comp->stream_class, dmesg_comp->clock_class);
		if (ret) {
			BT_LOGE_STR("Cannot set stream class's default clock class.");
			goto error;
		}
	}

	dmesg_comp->event_class = bt_event_class_create(
		dmesg_comp->stream_class);
	if (!dmesg_comp->event_class) {
		BT_LOGE_STR("Cannot create an event class object.");
		goto error;
	}

	ret = bt_event_class_set_name(dmesg_comp->event_class, "string");
	if (ret) {
		BT_LOGE_STR("Cannot set event class's name.");
		goto error;
	}

	fc = create_event_payload_fc();
	if (!fc) {
		BT_LOGE_STR("Cannot create event payload field class.");
		goto error;
	}

	ret = bt_event_class_set_payload_field_class(dmesg_comp->event_class, fc);
	if (ret) {
		BT_LOGE_STR("Cannot set event class's event payload field class.");
		goto error;
	}

	goto end;

error:
	ret = -1;

end:
	bt_field_class_put_ref(fc);
	return ret;
}

static
int handle_params(struct dmesg_component *dmesg_comp,
		const bt_value *params)
{
	const bt_value *no_timestamp = NULL;
	const bt_value *path = NULL;
	const char *path_str;
	int ret = 0;

	no_timestamp = bt_value_map_borrow_entry_value_const(params,
		"no-extract-timestamp");
	if (no_timestamp) {
		if (!bt_value_is_bool(no_timestamp)) {
			BT_LOGE("Expecting a boolean value for the `no-extract-timestamp` parameter: "
				"type=%s",
				bt_common_value_type_string(
					bt_value_get_type(no_timestamp)));
			goto error;
		}

		dmesg_comp->params.no_timestamp =
			bt_value_bool_get(no_timestamp);
	}

	path = bt_value_map_borrow_entry_value_const(params, "path");
	if (path) {
		if (dmesg_comp->params.read_from_stdin) {
			BT_LOGE_STR("Cannot specify both `read-from-stdin` and `path` parameters.");
			goto error;
		}

		if (!bt_value_is_string(path)) {
			BT_LOGE("Expecting a string value for the `path` parameter: "
				"type=%s",
				bt_common_value_type_string(
					bt_value_get_type(path)));
			goto error;
		}

		path_str = bt_value_string_get(path);
		g_string_assign(dmesg_comp->params.path, path_str);
	} else {
		dmesg_comp->params.read_from_stdin = true;
	}

	goto end;

error:
	ret = -1;

end:
	return ret;
}

static
int create_packet_and_stream_and_trace(struct dmesg_component *dmesg_comp)
{
	int ret = 0;
	const char *trace_name;
	gchar *basename = NULL;

	dmesg_comp->trace = bt_trace_create(dmesg_comp->trace_class);
	if (!dmesg_comp->trace) {
		BT_LOGE_STR("Cannot create trace object.");
		goto error;
	}

	if (dmesg_comp->params.read_from_stdin) {
		trace_name = "STDIN";
	} else {
		basename = g_path_get_basename(dmesg_comp->params.path->str);
		BT_ASSERT(basename);

		if (strcmp(basename, G_DIR_SEPARATOR_S) != 0 &&
				strcmp(basename, ".") != 0) {
			trace_name = basename;
		}
	}

	if (trace_name) {
		ret = bt_trace_set_name(dmesg_comp->trace, trace_name);
		if (ret) {
			BT_LOGE("Cannot set trace's name: name=\"%s\"",
				trace_name);
			goto error;
		}
	}

	dmesg_comp->stream = bt_stream_create(dmesg_comp->stream_class,
		dmesg_comp->trace);
	if (!dmesg_comp->stream) {
		BT_LOGE_STR("Cannot create stream object.");
		goto error;
	}

	dmesg_comp->packet = bt_packet_create(dmesg_comp->stream);
	if (!dmesg_comp->packet) {
		BT_LOGE_STR("Cannot create packet object.");
		goto error;
	}

	ret = bt_trace_make_static(dmesg_comp->trace);
	if (ret) {
		BT_LOGE_STR("Cannot make trace static.");
		goto error;
	}

	goto end;

error:
	ret = -1;

end:
	if (basename) {
		g_free(basename);
	}

	return ret;
}

static
int try_create_meta_stream_packet(struct dmesg_component *dmesg_comp,
		bool has_ts)
{
	int ret = 0;

	if (dmesg_comp->trace) {
		/* Already created */
		goto end;
	}

	ret = create_meta(dmesg_comp, has_ts);
	if (ret) {
		BT_LOGE("Cannot create metadata objects: dmesg-comp-addr=%p",
			dmesg_comp);
		goto error;
	}

	ret = create_packet_and_stream_and_trace(dmesg_comp);
	if (ret) {
		BT_LOGE("Cannot create packet and stream objects: "
			"dmesg-comp-addr=%p", dmesg_comp);
		goto error;
	}

	goto end;

error:
	ret = -1;

end:
	return ret;
}

static
void destroy_dmesg_component(struct dmesg_component *dmesg_comp)
{
	if (!dmesg_comp) {
		return;
	}

	if (dmesg_comp->params.path) {
		g_string_free(dmesg_comp->params.path, TRUE);
	}

	bt_packet_put_ref(dmesg_comp->packet);
	bt_trace_put_ref(dmesg_comp->trace);
	bt_stream_class_put_ref(dmesg_comp->stream_class);
	bt_event_class_put_ref(dmesg_comp->event_class);
	bt_stream_put_ref(dmesg_comp->stream);
	bt_clock_class_put_ref(dmesg_comp->clock_class);
	g_free(dmesg_comp);
}

static
enum bt_self_component_status create_port(
		bt_self_component_source *self_comp)
{
	return bt_self_component_source_add_output_port(self_comp,
		"out", NULL, NULL);
}

BT_HIDDEN
enum bt_self_component_status dmesg_init(
		bt_self_component_source *self_comp,
		bt_value *params, void *init_method_data)
{
	int ret = 0;
	struct dmesg_component *dmesg_comp = g_new0(struct dmesg_component, 1);
	enum bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;

	if (!dmesg_comp) {
		BT_LOGE_STR("Failed to allocate one dmesg component structure.");
		goto error;
	}

	dmesg_comp->params.path = g_string_new(NULL);
	if (!dmesg_comp->params.path) {
		BT_LOGE_STR("Failed to allocate a GString.");
		goto error;
	}

	ret = handle_params(dmesg_comp, params);
	if (ret) {
		BT_LOGE("Invalid parameters: comp-addr=%p", self_comp);
		goto error;
	}

	if (!dmesg_comp->params.read_from_stdin &&
			!g_file_test(dmesg_comp->params.path->str,
			G_FILE_TEST_IS_REGULAR)) {
		BT_LOGE("Input path is not a regular file: "
			"comp-addr=%p, path=\"%s\"", self_comp,
			dmesg_comp->params.path->str);
		goto error;
	}

	status = create_port(self_comp);
	if (status != BT_SELF_COMPONENT_STATUS_OK) {
		goto error;
	}

	bt_self_component_set_data(
		bt_self_component_source_as_self_component(self_comp),
		dmesg_comp);
	goto end;

error:
	destroy_dmesg_component(dmesg_comp);
	bt_self_component_set_data(
		bt_self_component_source_as_self_component(self_comp),
		NULL);

	if (status >= 0) {
		status = BT_SELF_COMPONENT_STATUS_ERROR;
	}

end:
	return status;
}

BT_HIDDEN
void dmesg_finalize(bt_self_component_source *self_comp)
{
	destroy_dmesg_component(bt_self_component_get_data(
		bt_self_component_source_as_self_component(self_comp)));
}

static
bt_message *create_init_event_msg_from_line(
		struct dmesg_msg_iter *msg_iter,
		const char *line, const char **new_start)
{
	bt_event *event;
	bt_message *msg = NULL;
	bool has_timestamp = false;
	unsigned long sec, usec, msec;
	unsigned int year, mon, mday, hour, min;
	uint64_t ts = 0;
	int ret = 0;
	struct dmesg_component *dmesg_comp = msg_iter->dmesg_comp;

	*new_start = line;

	if (dmesg_comp->params.no_timestamp) {
		goto skip_ts;
	}

	/* Extract time from input line */
	if (sscanf(line, "[%lu.%lu] ", &sec, &usec) == 2) {
		ts = (uint64_t) sec * USEC_PER_SEC + (uint64_t) usec;

		/*
		 * The clock class we use has a 1 GHz frequency: convert
		 * from µs to ns.
		 */
		ts *= NSEC_PER_USEC;
		has_timestamp = true;
	} else if (sscanf(line, "[%u-%u-%u %u:%u:%lu.%lu] ",
			&year, &mon, &mday, &hour, &min,
			&sec, &msec) == 7) {
		time_t ep_sec;
		struct tm ti;

		memset(&ti, 0, sizeof(ti));
		ti.tm_year = year - 1900;	/* From 1900 */
		ti.tm_mon = mon - 1;		/* 0 to 11 */
		ti.tm_mday = mday;
		ti.tm_hour = hour;
		ti.tm_min = min;
		ti.tm_sec = sec;

		ep_sec = bt_timegm(&ti);
		if (ep_sec != (time_t) -1) {
			ts = (uint64_t) ep_sec * NSEC_PER_SEC
				+ (uint64_t) msec * NSEC_PER_MSEC;
		}

		has_timestamp = true;
	}

	if (has_timestamp) {
		/* Set new start for the message portion of the line */
		*new_start = strchr(line, ']');
		BT_ASSERT(*new_start);
		(*new_start)++;

		if ((*new_start)[0] == ' ') {
			(*new_start)++;
		}
	}

skip_ts:
	/*
	 * At this point, we know if the stream class's event header
	 * field class should have a timestamp or not, so we can lazily
	 * create the metadata, stream, and packet objects.
	 */
	ret = try_create_meta_stream_packet(dmesg_comp, has_timestamp);
	if (ret) {
		/* try_create_meta_stream_packet() logs errors */
		goto error;
	}

	msg = bt_message_event_create(msg_iter->pc_msg_iter,
		dmesg_comp->event_class, dmesg_comp->packet);
	if (!msg) {
		BT_LOGE_STR("Cannot create event message.");
		goto error;
	}

	event = bt_message_event_borrow_event(msg);
	BT_ASSERT(event);

	if (dmesg_comp->clock_class) {
		bt_event_set_default_clock_value(event, ts);
	}

	goto end;

error:
	BT_MESSAGE_PUT_REF_AND_RESET(msg);

end:
	return msg;
}

static
int fill_event_payload_from_line(const char *line,
		bt_event *event)
{
	bt_field *ep_field = NULL;
	bt_field *str_field = NULL;
	size_t len;
	int ret;

	ep_field = bt_event_borrow_payload_field(event);
	BT_ASSERT(ep_field);
	str_field = bt_field_structure_borrow_member_field_by_index(
		ep_field, 0);
	if (!str_field) {
		BT_LOGE_STR("Cannot borrow `timestamp` field from event payload structure field.");
		goto error;
	}

	len = strlen(line);
	if (line[len - 1] == '\n') {
		/* Do not include the newline character in the payload */
		len--;
	}

	ret = bt_field_string_clear(str_field);
	if (ret) {
		BT_LOGE_STR("Cannot clear string field object.");
		goto error;
	}

	ret = bt_field_string_append_with_length(str_field, line, len);
	if (ret) {
		BT_LOGE("Cannot append value to string field object: "
			"len=%zu", len);
		goto error;
	}

	goto end;

error:
	ret = -1;

end:
	return ret;
}

static
bt_message *create_msg_from_line(
		struct dmesg_msg_iter *dmesg_msg_iter, const char *line)
{
	bt_event *event = NULL;
	bt_message *msg = NULL;
	const char *new_start;
	int ret;

	msg = create_init_event_msg_from_line(dmesg_msg_iter,
		line, &new_start);
	if (!msg) {
		BT_LOGE_STR("Cannot create and initialize event message from line.");
		goto error;
	}

	event = bt_message_event_borrow_event(msg);
	BT_ASSERT(event);
	ret = fill_event_payload_from_line(new_start, event);
	if (ret) {
		BT_LOGE("Cannot fill event payload field from line: "
			"ret=%d", ret);
		goto error;
	}

	goto end;

error:
	BT_MESSAGE_PUT_REF_AND_RESET(msg);

end:
	return msg;
}

static
void destroy_dmesg_msg_iter(struct dmesg_msg_iter *dmesg_msg_iter)
{
	if (!dmesg_msg_iter) {
		return;
	}

	if (dmesg_msg_iter->fp && dmesg_msg_iter->fp != stdin) {
		if (fclose(dmesg_msg_iter->fp)) {
			BT_LOGE_ERRNO("Cannot close input file", ".");
		}
	}

	bt_message_put_ref(dmesg_msg_iter->tmp_event_msg);
	free(dmesg_msg_iter->linebuf);
	g_free(dmesg_msg_iter);
}

BT_HIDDEN
enum bt_self_message_iterator_status dmesg_msg_iter_init(
		bt_self_message_iterator *self_msg_iter,
		bt_self_component_source *self_comp,
		bt_self_component_port_output *self_port)
{
	struct dmesg_component *dmesg_comp;
	struct dmesg_msg_iter *dmesg_msg_iter =
		g_new0(struct dmesg_msg_iter, 1);
	enum bt_self_message_iterator_status status =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;

	if (!dmesg_msg_iter) {
		BT_LOGE_STR("Failed to allocate on dmesg message iterator structure.");
		goto error;
	}

	dmesg_comp = bt_self_component_get_data(
		bt_self_component_source_as_self_component(self_comp));
	BT_ASSERT(dmesg_comp);
	dmesg_msg_iter->dmesg_comp = dmesg_comp;
	dmesg_msg_iter->pc_msg_iter = self_msg_iter;

	if (dmesg_comp->params.read_from_stdin) {
		dmesg_msg_iter->fp = stdin;
	} else {
		dmesg_msg_iter->fp = fopen(dmesg_comp->params.path->str, "r");
		if (!dmesg_msg_iter->fp) {
			BT_LOGE_ERRNO("Cannot open input file in read mode", ": path=\"%s\"",
				dmesg_comp->params.path->str);
			goto error;
		}
	}

	bt_self_message_iterator_set_data(self_msg_iter,
		dmesg_msg_iter);
	goto end;

error:
	destroy_dmesg_msg_iter(dmesg_msg_iter);
	bt_self_message_iterator_set_data(self_msg_iter, NULL);
	if (status >= 0) {
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
	}

end:
	return status;
}

BT_HIDDEN
void dmesg_msg_iter_finalize(
		bt_self_message_iterator *priv_msg_iter)
{
	destroy_dmesg_msg_iter(bt_self_message_iterator_get_data(
		priv_msg_iter));
}

static
enum bt_self_message_iterator_status dmesg_msg_iter_next_one(
		struct dmesg_msg_iter *dmesg_msg_iter,
		bt_message **msg)
{
	ssize_t len;
	struct dmesg_component *dmesg_comp;
	enum bt_self_message_iterator_status status =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;

	BT_ASSERT(dmesg_msg_iter);
	dmesg_comp = dmesg_msg_iter->dmesg_comp;
	BT_ASSERT(dmesg_comp);

	if (dmesg_msg_iter->state == STATE_DONE) {
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_END;
		goto end;
	}

	if (dmesg_msg_iter->tmp_event_msg ||
			dmesg_msg_iter->state == STATE_EMIT_PACKET_END ||
			dmesg_msg_iter->state == STATE_EMIT_STREAM_END) {
		goto handle_state;
	}

	while (true) {
		const char *ch;
		bool only_spaces = true;

		len = bt_getline(&dmesg_msg_iter->linebuf,
			&dmesg_msg_iter->linebuf_len, dmesg_msg_iter->fp);
		if (len < 0) {
			if (errno == EINVAL) {
				status = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
			} else if (errno == ENOMEM) {
				status =
					BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
			} else {
				if (dmesg_msg_iter->state == STATE_EMIT_STREAM_BEGINNING) {
					/* Stream did not even begin */
					status = BT_SELF_MESSAGE_ITERATOR_STATUS_END;
					goto end;
				} else {
					/* End current packet now */
					dmesg_msg_iter->state =
						STATE_EMIT_PACKET_END;
					goto handle_state;
				}
			}

			goto end;
		}

		BT_ASSERT(dmesg_msg_iter->linebuf);

		/* Ignore empty lines, once trimmed */
		for (ch = dmesg_msg_iter->linebuf; *ch != '\0'; ch++) {
			if (!isspace(*ch)) {
				only_spaces = false;
				break;
			}
		}

		if (!only_spaces) {
			break;
		}
	}

	dmesg_msg_iter->tmp_event_msg = create_msg_from_line(
		dmesg_msg_iter, dmesg_msg_iter->linebuf);
	if (!dmesg_msg_iter->tmp_event_msg) {
		BT_LOGE("Cannot create event message from line: "
			"dmesg-comp-addr=%p, line=\"%s\"", dmesg_comp,
			dmesg_msg_iter->linebuf);
		goto end;
	}

handle_state:
	BT_ASSERT(dmesg_comp->trace);

	switch (dmesg_msg_iter->state) {
	case STATE_EMIT_STREAM_BEGINNING:
		BT_ASSERT(dmesg_msg_iter->tmp_event_msg);
		*msg = bt_message_stream_beginning_create(
			dmesg_msg_iter->pc_msg_iter, dmesg_comp->stream);
		dmesg_msg_iter->state = STATE_EMIT_PACKET_BEGINNING;
		break;
	case STATE_EMIT_PACKET_BEGINNING:
		BT_ASSERT(dmesg_msg_iter->tmp_event_msg);
		*msg = bt_message_packet_beginning_create(
			dmesg_msg_iter->pc_msg_iter, dmesg_comp->packet);
		dmesg_msg_iter->state = STATE_EMIT_EVENT;
		break;
	case STATE_EMIT_EVENT:
		BT_ASSERT(dmesg_msg_iter->tmp_event_msg);
		*msg = dmesg_msg_iter->tmp_event_msg;
		dmesg_msg_iter->tmp_event_msg = NULL;
		break;
	case STATE_EMIT_PACKET_END:
		*msg = bt_message_packet_end_create(
			dmesg_msg_iter->pc_msg_iter, dmesg_comp->packet);
		dmesg_msg_iter->state = STATE_EMIT_STREAM_END;
		break;
	case STATE_EMIT_STREAM_END:
		*msg = bt_message_stream_end_create(
			dmesg_msg_iter->pc_msg_iter, dmesg_comp->stream);
		dmesg_msg_iter->state = STATE_DONE;
		break;
	default:
		break;
	}

	if (!*msg) {
		BT_LOGE("Cannot create message: dmesg-comp-addr=%p",
			dmesg_comp);
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
	}

end:
	return status;
}

BT_HIDDEN
enum bt_self_message_iterator_status dmesg_msg_iter_next(
		bt_self_message_iterator *self_msg_iter,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	struct dmesg_msg_iter *dmesg_msg_iter =
		bt_self_message_iterator_get_data(
			self_msg_iter);
	enum bt_self_message_iterator_status status =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
	uint64_t i = 0;

	while (i < capacity &&
			status == BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
		bt_message *priv_msg = NULL;

		status = dmesg_msg_iter_next_one(dmesg_msg_iter,
			&priv_msg);
		msgs[i] = priv_msg;
		if (status == BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
			i++;
		}
	}

	if (i > 0) {
		/*
		 * Even if dmesg_msg_iter_next_one() returned
		 * something else than
		 * BT_SELF_MESSAGE_ITERATOR_STATUS_OK, we
		 * accumulated message objects in the output
		 * message array, so we need to return
		 * BT_SELF_MESSAGE_ITERATOR_STATUS_OK so that they
		 * are transfered to downstream. This other status
		 * occurs again the next time muxer_msg_iter_do_next()
		 * is called, possibly without any accumulated
		 * message, in which case we'll return it.
		 */
		*count = i;
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
	}

	return status;
}
