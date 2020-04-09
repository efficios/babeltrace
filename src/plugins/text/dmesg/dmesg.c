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

#define BT_COMP_LOG_SELF_COMP (dmesg_comp->self_comp)
#define BT_LOG_OUTPUT_LEVEL (dmesg_comp->log_level)
#define BT_LOG_TAG "PLUGIN/SRC.TEXT.DMESG"
#include "logging/comp-logging.h"

#include "dmesg.h"

#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "common/common.h"
#include "common/assert.h"
#include <babeltrace2/babeltrace.h>
#include "compat/utc.h"
#include "compat/stdio.h"
#include <glib.h>
#include "plugins/common/param-validation/param-validation.h"

#define NSEC_PER_USEC 1000UL
#define NSEC_PER_MSEC 1000000UL
#define NSEC_PER_SEC 1000000000ULL
#define USEC_PER_SEC 1000000UL

struct dmesg_component;

struct dmesg_msg_iter {
	struct dmesg_component *dmesg_comp;

	/* Weak */
	bt_self_message_iterator *self_msg_iter;

	char *linebuf;
	size_t linebuf_len;
	FILE *fp;
	bt_message *tmp_event_msg;
	uint64_t last_clock_value;

	enum {
		STATE_EMIT_STREAM_BEGINNING,
		STATE_EMIT_EVENT,
		STATE_EMIT_STREAM_END,
		STATE_DONE,
	} state;
};

struct dmesg_component {
	bt_logging_level log_level;

	struct {
		GString *path;
		bt_bool read_from_stdin;
		bt_bool no_timestamp;
	} params;

	bt_self_component_source *self_comp_src;
	bt_self_component *self_comp;
	bt_trace_class *trace_class;
	bt_stream_class *stream_class;
	bt_event_class *event_class;
	bt_trace *trace;
	bt_stream *stream;
	bt_clock_class *clock_class;
};

static
bt_field_class *create_event_payload_fc(struct dmesg_component *dmesg_comp,
		bt_trace_class *trace_class)
{
	bt_field_class *root_fc = NULL;
	bt_field_class *fc = NULL;
	int ret;

	root_fc = bt_field_class_structure_create(trace_class);
	if (!root_fc) {
		BT_COMP_LOGE_STR("Cannot create an empty structure field class object.");
		goto error;
	}

	fc = bt_field_class_string_create(trace_class);
	if (!fc) {
		BT_COMP_LOGE_STR("Cannot create a string field class object.");
		goto error;
	}

	ret = bt_field_class_structure_append_member(root_fc,
		"str", fc);
	if (ret) {
		BT_COMP_LOGE("Cannot add `str` member to structure field class: "
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

	dmesg_comp->trace_class = bt_trace_class_create(dmesg_comp->self_comp);
	if (!dmesg_comp->trace_class) {
		BT_COMP_LOGE_STR("Cannot create an empty trace class object.");
		goto error;
	}

	dmesg_comp->stream_class = bt_stream_class_create(
		dmesg_comp->trace_class);
	if (!dmesg_comp->stream_class) {
		BT_COMP_LOGE_STR("Cannot create a stream class object.");
		goto error;
	}

	if (has_ts) {
		dmesg_comp->clock_class = bt_clock_class_create(
			dmesg_comp->self_comp);
		if (!dmesg_comp->clock_class) {
			BT_COMP_LOGE_STR("Cannot create clock class.");
			goto error;
		}

		/*
		 * The `dmesg` timestamp's origin is not the Unix epoch,
		 * it's the boot time.
		 */
		bt_clock_class_set_origin_is_unix_epoch(dmesg_comp->clock_class,
			BT_FALSE);

		ret = bt_stream_class_set_default_clock_class(
			dmesg_comp->stream_class, dmesg_comp->clock_class);
		if (ret) {
			BT_COMP_LOGE_STR("Cannot set stream class's default clock class.");
			goto error;
		}
	}

	dmesg_comp->event_class = bt_event_class_create(
		dmesg_comp->stream_class);
	if (!dmesg_comp->event_class) {
		BT_COMP_LOGE_STR("Cannot create an event class object.");
		goto error;
	}

	ret = bt_event_class_set_name(dmesg_comp->event_class, "string");
	if (ret) {
		BT_COMP_LOGE_STR("Cannot set event class's name.");
		goto error;
	}

	fc = create_event_payload_fc(dmesg_comp, dmesg_comp->trace_class);
	if (!fc) {
		BT_COMP_LOGE_STR("Cannot create event payload field class.");
		goto error;
	}

	ret = bt_event_class_set_payload_field_class(dmesg_comp->event_class, fc);
	if (ret) {
		BT_COMP_LOGE_STR("Cannot set event class's event payload field class.");
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
struct bt_param_validation_map_value_entry_descr dmesg_params[] = {
	{ "no-extract-timestamp", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	{ "path", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_STRING } },
	BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
};

static
bt_component_class_initialize_method_status handle_params(
		struct dmesg_component *dmesg_comp,
		const bt_value *params)
{
	const bt_value *no_timestamp = NULL;
	const bt_value *path = NULL;
	bt_component_class_initialize_method_status status;
	enum bt_param_validation_status validation_status;
	gchar *validate_error = NULL;

	validation_status = bt_param_validation_validate(params,
		dmesg_params, &validate_error);
	if (validation_status == BT_PARAM_VALIDATION_STATUS_MEMORY_ERROR) {
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		goto end;
	} else if (validation_status == BT_PARAM_VALIDATION_STATUS_VALIDATION_ERROR) {
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
		BT_COMP_LOGE_APPEND_CAUSE(dmesg_comp->self_comp,
			"%s", validate_error);
		goto end;
	}

	no_timestamp = bt_value_map_borrow_entry_value_const(params,
		"no-extract-timestamp");
	if (no_timestamp) {
		dmesg_comp->params.no_timestamp =
			bt_value_bool_get(no_timestamp);
	}

	path = bt_value_map_borrow_entry_value_const(params, "path");
	if (path) {
		const char *path_str = bt_value_string_get(path);

		g_string_assign(dmesg_comp->params.path, path_str);
	} else {
		dmesg_comp->params.read_from_stdin = true;
	}

	status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;
end:
	g_free(validate_error);

	return status;
}

static
int create_stream_and_trace(struct dmesg_component *dmesg_comp)
{
	int ret = 0;
	const char *trace_name = NULL;
	gchar *basename = NULL;

	dmesg_comp->trace = bt_trace_create(dmesg_comp->trace_class);
	if (!dmesg_comp->trace) {
		BT_COMP_LOGE_STR("Cannot create trace object.");
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
			BT_COMP_LOGE("Cannot set trace's name: name=\"%s\"",
				trace_name);
			goto error;
		}
	}

	dmesg_comp->stream = bt_stream_create(dmesg_comp->stream_class,
		dmesg_comp->trace);
	if (!dmesg_comp->stream) {
		BT_COMP_LOGE_STR("Cannot create stream object.");
		goto error;
	}

	goto end;

error:
	ret = -1;

end:
	g_free(basename);

	return ret;
}

static
int try_create_meta_stream(struct dmesg_component *dmesg_comp, bool has_ts)
{
	int ret = 0;

	if (dmesg_comp->trace) {
		/* Already created */
		goto end;
	}

	ret = create_meta(dmesg_comp, has_ts);
	if (ret) {
		BT_COMP_LOGE("Cannot create metadata objects: dmesg-comp-addr=%p",
			dmesg_comp);
		goto error;
	}

	ret = create_stream_and_trace(dmesg_comp);
	if (ret) {
		BT_COMP_LOGE("Cannot create stream object: "
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

	bt_trace_put_ref(dmesg_comp->trace);
	bt_stream_class_put_ref(dmesg_comp->stream_class);
	bt_event_class_put_ref(dmesg_comp->event_class);
	bt_stream_put_ref(dmesg_comp->stream);
	bt_clock_class_put_ref(dmesg_comp->clock_class);
	bt_trace_class_put_ref(dmesg_comp->trace_class);
	g_free(dmesg_comp);
}

static
bt_component_class_initialize_method_status create_port(
		bt_self_component_source *self_comp)
{
	bt_component_class_initialize_method_status status;
	bt_self_component_add_port_status add_port_status;

	add_port_status = bt_self_component_source_add_output_port(self_comp,
		"out", NULL, NULL);
	switch (add_port_status) {
	case BT_SELF_COMPONENT_ADD_PORT_STATUS_OK:
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;
		break;
	case BT_SELF_COMPONENT_ADD_PORT_STATUS_ERROR:
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
		break;
	case BT_SELF_COMPONENT_ADD_PORT_STATUS_MEMORY_ERROR:
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		break;
	default:
		bt_common_abort();
	}

	return status;
}

BT_HIDDEN
bt_component_class_initialize_method_status dmesg_init(
		bt_self_component_source *self_comp_src,
		bt_self_component_source_configuration *config,
		const bt_value *params, void *init_method_data)
{
	struct dmesg_component *dmesg_comp = g_new0(struct dmesg_component, 1);
	bt_component_class_initialize_method_status status;
	bt_self_component *self_comp =
		bt_self_component_source_as_self_component(self_comp_src);
	const bt_component *comp = bt_self_component_as_component(self_comp);
	bt_logging_level log_level = bt_component_get_logging_level(comp);

	if (!dmesg_comp) {
		/* Implicit log level is not available here */
		BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, log_level, self_comp,
			"Failed to allocate one dmesg component structure.");
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
		goto error;
	}

	dmesg_comp->log_level = log_level;
	dmesg_comp->self_comp = self_comp;
	dmesg_comp->self_comp_src = self_comp_src;
	dmesg_comp->params.path = g_string_new(NULL);
	if (!dmesg_comp->params.path) {
		BT_COMP_LOGE_STR("Failed to allocate a GString.");
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
		goto error;
	}

	status = handle_params(dmesg_comp, params);
	if (status != BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK) {
		BT_COMP_LOGE("Invalid parameters: comp-addr=%p", self_comp);
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
		goto error;
	}

	if (!dmesg_comp->params.read_from_stdin &&
			!g_file_test(dmesg_comp->params.path->str,
			G_FILE_TEST_IS_REGULAR)) {
		BT_COMP_LOGE("Input path is not a regular file: "
			"comp-addr=%p, path=\"%s\"", self_comp,
			dmesg_comp->params.path->str);
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
		goto error;
	}

	status = create_port(self_comp_src);
	if (status != BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK) {
		goto error;
	}

	bt_self_component_set_data(self_comp, dmesg_comp);
	BT_COMP_LOGI_STR("Component initialized.");

	status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;
	goto end;

error:
	destroy_dmesg_component(dmesg_comp);
	bt_self_component_set_data(self_comp, NULL);

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
		BT_ASSERT_DBG(*new_start);
		(*new_start)++;

		if ((*new_start)[0] == ' ') {
			(*new_start)++;
		}
	}

skip_ts:
	/*
	 * At this point, we know if the stream class's event header
	 * field class should have a timestamp or not, so we can lazily
	 * create the metadata and stream objects.
	 */
	ret = try_create_meta_stream(dmesg_comp, has_timestamp);
	if (ret) {
		/* try_create_meta_stream() logs errors */
		goto error;
	}

	if (dmesg_comp->clock_class) {
		msg = bt_message_event_create_with_default_clock_snapshot(
			msg_iter->self_msg_iter,
			dmesg_comp->event_class, dmesg_comp->stream, ts);
		msg_iter->last_clock_value = ts;
	} else {
		msg = bt_message_event_create(msg_iter->self_msg_iter,
			dmesg_comp->event_class, dmesg_comp->stream);
	}

	if (!msg) {
		BT_COMP_LOGE_STR("Cannot create event message.");
		goto error;
	}

	event = bt_message_event_borrow_event(msg);
	BT_ASSERT_DBG(event);
	goto end;

error:
	BT_MESSAGE_PUT_REF_AND_RESET(msg);

end:
	return msg;
}

static
int fill_event_payload_from_line(struct dmesg_component *dmesg_comp,
		const char *line, bt_event *event)
{
	bt_field *ep_field = NULL;
	bt_field *str_field = NULL;
	size_t len;
	int ret;

	ep_field = bt_event_borrow_payload_field(event);
	BT_ASSERT_DBG(ep_field);
	str_field = bt_field_structure_borrow_member_field_by_index(
		ep_field, 0);
	if (!str_field) {
		BT_COMP_LOGE_STR("Cannot borrow `timestamp` field from event payload structure field.");
		goto error;
	}

	len = strlen(line);
	if (line[len - 1] == '\n') {
		/* Do not include the newline character in the payload */
		len--;
	}

	bt_field_string_clear(str_field);
	ret = bt_field_string_append_with_length(str_field, line, len);
	if (ret) {
		BT_COMP_LOGE("Cannot append value to string field object: "
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
	struct dmesg_component *dmesg_comp = dmesg_msg_iter->dmesg_comp;
	bt_event *event = NULL;
	bt_message *msg = NULL;
	const char *new_start;
	int ret;

	msg = create_init_event_msg_from_line(dmesg_msg_iter,
		line, &new_start);
	if (!msg) {
		BT_COMP_LOGE_STR("Cannot create and initialize event message from line.");
		goto error;
	}

	event = bt_message_event_borrow_event(msg);
	BT_ASSERT_DBG(event);
	ret = fill_event_payload_from_line(dmesg_comp, new_start, event);
	if (ret) {
		BT_COMP_LOGE("Cannot fill event payload field from line: "
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
	struct dmesg_component *dmesg_comp;

	if (!dmesg_msg_iter) {
		return;
	}

	dmesg_comp = dmesg_msg_iter->dmesg_comp;

	if (dmesg_msg_iter->fp && dmesg_msg_iter->fp != stdin) {
		if (fclose(dmesg_msg_iter->fp)) {
			BT_COMP_LOGE_ERRNO("Cannot close input file", ".");
		}
	}

	bt_message_put_ref(dmesg_msg_iter->tmp_event_msg);
	free(dmesg_msg_iter->linebuf);
	g_free(dmesg_msg_iter);
}



BT_HIDDEN
bt_message_iterator_class_initialize_method_status dmesg_msg_iter_init(
		bt_self_message_iterator *self_msg_iter,
		bt_self_message_iterator_configuration *config,
		bt_self_component_port_output *self_port)
{
	bt_self_component *self_comp =
		bt_self_message_iterator_borrow_component(self_msg_iter);
	struct dmesg_component *dmesg_comp = bt_self_component_get_data(self_comp);
	struct dmesg_msg_iter *dmesg_msg_iter =
		g_new0(struct dmesg_msg_iter, 1);
	bt_message_iterator_class_initialize_method_status status =
		BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_OK;

	if (!dmesg_msg_iter) {
		BT_COMP_LOGE_STR("Failed to allocate on dmesg message iterator structure.");
		goto error;
	}

	BT_ASSERT(dmesg_comp);
	dmesg_msg_iter->dmesg_comp = dmesg_comp;
	dmesg_msg_iter->self_msg_iter = self_msg_iter;

	if (dmesg_comp->params.read_from_stdin) {
		dmesg_msg_iter->fp = stdin;
	} else {
		dmesg_msg_iter->fp = fopen(dmesg_comp->params.path->str, "r");
		if (!dmesg_msg_iter->fp) {
			BT_COMP_LOGE_ERRNO("Cannot open input file in read mode", ": path=\"%s\"",
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
		status = BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
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
bt_message_iterator_class_next_method_status dmesg_msg_iter_next_one(
		struct dmesg_msg_iter *dmesg_msg_iter,
		bt_message **msg)
{
	ssize_t len;
	struct dmesg_component *dmesg_comp;
	bt_message_iterator_class_next_method_status status;

	BT_ASSERT_DBG(dmesg_msg_iter);
	dmesg_comp = dmesg_msg_iter->dmesg_comp;
	BT_ASSERT_DBG(dmesg_comp);

	if (dmesg_msg_iter->state == STATE_DONE) {
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END;
		goto end;
	}

	if (dmesg_msg_iter->tmp_event_msg ||
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
				status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
				goto end;
			} else if (errno == ENOMEM) {
				status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_MEMORY_ERROR;
				goto end;
			} else {
				if (dmesg_msg_iter->state == STATE_EMIT_STREAM_BEGINNING) {
					/* Stream did not even begin */
					status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END;
					goto end;
				} else {
					/* End stream now */
					dmesg_msg_iter->state =
						STATE_EMIT_STREAM_END;
					goto handle_state;
				}
			}
		}

		BT_ASSERT_DBG(dmesg_msg_iter->linebuf);

		/* Ignore empty lines, once trimmed */
		for (ch = dmesg_msg_iter->linebuf; *ch != '\0'; ch++) {
			if (!isspace((unsigned char) *ch)) {
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
		BT_COMP_LOGE("Cannot create event message from line: "
			"dmesg-comp-addr=%p, line=\"%s\"", dmesg_comp,
			dmesg_msg_iter->linebuf);
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
		goto end;
	}

handle_state:
	BT_ASSERT_DBG(dmesg_comp->trace);

	switch (dmesg_msg_iter->state) {
	case STATE_EMIT_STREAM_BEGINNING:
		BT_ASSERT_DBG(dmesg_msg_iter->tmp_event_msg);
		*msg = bt_message_stream_beginning_create(
			dmesg_msg_iter->self_msg_iter, dmesg_comp->stream);
		dmesg_msg_iter->state = STATE_EMIT_EVENT;
		break;
	case STATE_EMIT_EVENT:
		BT_ASSERT_DBG(dmesg_msg_iter->tmp_event_msg);
		*msg = dmesg_msg_iter->tmp_event_msg;
		dmesg_msg_iter->tmp_event_msg = NULL;
		break;
	case STATE_EMIT_STREAM_END:
		*msg = bt_message_stream_end_create(
			dmesg_msg_iter->self_msg_iter, dmesg_comp->stream);
		dmesg_msg_iter->state = STATE_DONE;
		break;
	default:
		break;
	}

	if (!*msg) {
		BT_COMP_LOGE("Cannot create message: dmesg-comp-addr=%p",
			dmesg_comp);
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
		goto end;
	}

	status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
end:
	return status;
}

BT_HIDDEN
bt_message_iterator_class_next_method_status dmesg_msg_iter_next(
		bt_self_message_iterator *self_msg_iter,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	struct dmesg_msg_iter *dmesg_msg_iter =
		bt_self_message_iterator_get_data(
			self_msg_iter);
	bt_message_iterator_class_next_method_status status =
		BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
	uint64_t i = 0;

	while (i < capacity &&
			status == BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK) {
		bt_message *priv_msg = NULL;

		status = dmesg_msg_iter_next_one(dmesg_msg_iter,
			&priv_msg);
		msgs[i] = priv_msg;
		if (status == BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK) {
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
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
	}

	return status;
}

BT_HIDDEN
bt_message_iterator_class_can_seek_beginning_method_status
dmesg_msg_iter_can_seek_beginning(
		bt_self_message_iterator *self_msg_iter, bt_bool *can_seek)
{
	struct dmesg_msg_iter *dmesg_msg_iter =
		bt_self_message_iterator_get_data(self_msg_iter);

	/* Can't seek the beginning of the standard input stream */
	*can_seek = !dmesg_msg_iter->dmesg_comp->params.read_from_stdin;

	return BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_OK;
}

BT_HIDDEN
bt_message_iterator_class_seek_beginning_method_status
dmesg_msg_iter_seek_beginning(
		bt_self_message_iterator *self_msg_iter)
{
	struct dmesg_msg_iter *dmesg_msg_iter =
		bt_self_message_iterator_get_data(self_msg_iter);

	BT_ASSERT(!dmesg_msg_iter->dmesg_comp->params.read_from_stdin);

	BT_MESSAGE_PUT_REF_AND_RESET(dmesg_msg_iter->tmp_event_msg);
	dmesg_msg_iter->last_clock_value = 0;
	dmesg_msg_iter->state = STATE_EMIT_STREAM_BEGINNING;
	return BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_OK;
}
