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

#define BT_LOG_TAG "PLUGIN-TEXT-DMESG-SRC"
#include "logging.h"

#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/values-internal.h>
#include <babeltrace/compat/utc-internal.h>
#include <babeltrace/compat/stdio-internal.h>
#include <glib.h>

#define NSEC_PER_USEC 1000UL
#define NSEC_PER_MSEC 1000000UL
#define NSEC_PER_SEC 1000000000ULL
#define USEC_PER_SEC 1000000UL

struct dmesg_component;

struct dmesg_notif_iter {
	struct dmesg_component *dmesg_comp;
	char *linebuf;
	size_t linebuf_len;
	FILE *fp;
};

struct dmesg_component {
	struct {
		GString *path;
		bt_bool read_from_stdin;
		bt_bool no_timestamp;
	} params;

	struct bt_trace *trace;
	struct bt_stream_class *stream_class;
	struct bt_event_class *event_class;
	struct bt_stream *stream;
	struct bt_packet *packet;
	struct bt_clock_class *clock_class;
	struct bt_clock_class_priority_map *cc_prio_map;
};

static
struct bt_field_type *create_packet_header_ft(void)
{
	struct bt_field_type *root_ft = NULL;
	struct bt_field_type *ft = NULL;
	int ret;

	root_ft = bt_field_type_structure_create();
	if (!root_ft) {
		BT_LOGE_STR("Cannot create an empty structure field type object.");
		goto error;
	}

	ft = bt_field_type_integer_create(32);
	if (!ft) {
		BT_LOGE_STR("Cannot create an integer field type object.");
		goto error;
	}

	ret = bt_field_type_structure_add_field(root_ft, ft, "magic");
	if (ret) {
		BT_LOGE("Cannot add `magic` field type to structure field type: "
			"ret=%d", ret);
		goto error;
	}

	BT_PUT(ft);
	ft = bt_field_type_integer_create(8);
	if (!ft) {
		BT_LOGE_STR("Cannot create an integer field type object.");
		goto error;
	}

	goto end;

error:
	BT_PUT(root_ft);

end:
	bt_put(ft);
	return root_ft;
}

static
struct bt_field_type *create_packet_context_ft(void)
{
	struct bt_field_type *root_ft = NULL;
	struct bt_field_type *ft = NULL;
	int ret;

	root_ft = bt_field_type_structure_create();
	if (!root_ft) {
		BT_LOGE_STR("Cannot create an empty structure field type object.");
		goto error;
	}

	ft = bt_field_type_integer_create(64);
	if (!ft) {
		BT_LOGE_STR("Cannot create an integer field type object.");
		goto error;
	}

	ret = bt_field_type_structure_add_field(root_ft,
		ft, "content_size");
	if (ret) {
		BT_LOGE("Cannot add `content_size` field type to structure field type: "
			"ret=%d", ret);
		goto error;
	}

	BT_PUT(ft);
	ft = bt_field_type_integer_create(64);
	if (!ft) {
		BT_LOGE_STR("Cannot create an integer field type object.");
		goto error;
	}

	ret = bt_field_type_structure_add_field(root_ft,
		ft, "packet_size");
	if (ret) {
		BT_LOGE("Cannot add `packet_size` field type to structure field type: "
			"ret=%d", ret);
		goto error;
	}

	goto end;

error:
	BT_PUT(root_ft);

end:
	bt_put(ft);
	return root_ft;
}

static
struct bt_field_type *create_event_header_ft(
		struct bt_clock_class *clock_class)
{
	struct bt_field_type *root_ft = NULL;
	struct bt_field_type *ft = NULL;
	int ret;

	root_ft = bt_field_type_structure_create();
	if (!root_ft) {
		BT_LOGE_STR("Cannot create an empty structure field type object.");
		goto error;
	}

	ft = bt_field_type_integer_create(64);
	if (!ft) {
		BT_LOGE_STR("Cannot create an integer field type object.");
		goto error;
	}

	ret = bt_field_type_integer_set_mapped_clock_class(ft, clock_class);
	if (ret) {
		BT_LOGE("Cannot map integer field type to clock class: "
			"ret=%d", ret);
		goto error;
	}

	ret = bt_field_type_structure_add_field(root_ft,
		ft, "timestamp");
	if (ret) {
		BT_LOGE("Cannot add `timestamp` field type to structure field type: "
			"ret=%d", ret);
		goto error;
	}

	goto end;

error:
	BT_PUT(root_ft);

end:
	bt_put(ft);
	return root_ft;
}

static
struct bt_field_type *create_event_payload_ft(void)
{
	struct bt_field_type *root_ft = NULL;
	struct bt_field_type *ft = NULL;
	int ret;

	root_ft = bt_field_type_structure_create();
	if (!root_ft) {
		BT_LOGE_STR("Cannot create an empty structure field type object.");
		goto error;
	}

	ft = bt_field_type_string_create();
	if (!ft) {
		BT_LOGE_STR("Cannot create a string field type object.");
		goto error;
	}

	ret = bt_field_type_structure_add_field(root_ft,
		ft, "str");
	if (ret) {
		BT_LOGE("Cannot add `str` field type to structure field type: "
			"ret=%d", ret);
		goto error;
	}

	goto end;

error:
	BT_PUT(root_ft);

end:
	bt_put(ft);
	return root_ft;
}

static
struct bt_clock_class *create_clock_class(void)
{
	return bt_clock_class_create("the_clock", 1000000000);
}

static
int create_meta(struct dmesg_component *dmesg_comp, bool has_ts)
{
	struct bt_field_type *ft = NULL;
	const char *trace_name = NULL;
	gchar *basename = NULL;
	int ret = 0;

	dmesg_comp->trace = bt_trace_create();
	if (!dmesg_comp->trace) {
		BT_LOGE_STR("Cannot create an empty trace object.");
		goto error;
	}

	ft = create_packet_header_ft();
	if (!ft) {
		BT_LOGE_STR("Cannot create packet header field type.");
		goto error;
	}

	ret = bt_trace_set_packet_header_type(dmesg_comp->trace, ft);
	if (ret) {
		BT_LOGE_STR("Cannot set trace's packet header field type.");
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
			BT_LOGE("Cannot set trace's name: name=\"%s\"", trace_name);
			goto error;
		}
	}

	dmesg_comp->stream_class = bt_stream_class_create_empty(NULL);
	if (!dmesg_comp->stream_class) {
		BT_LOGE_STR("Cannot create an empty stream class object.");
		goto error;
	}

	bt_put(ft);
	ft = create_packet_context_ft();
	if (!ft) {
		BT_LOGE_STR("Cannot create packet context field type.");
		goto error;
	}

	ret = bt_stream_class_set_packet_context_type(
		dmesg_comp->stream_class, ft);
	if (ret) {
		BT_LOGE_STR("Cannot set stream class's packet context field type.");
		goto error;
	}

	dmesg_comp->cc_prio_map = bt_clock_class_priority_map_create();
	if (!dmesg_comp->cc_prio_map) {
		BT_LOGE_STR("Cannot create empty clock class priority map.");
		goto error;
	}

	if (has_ts) {
		dmesg_comp->clock_class = create_clock_class();
		if (!dmesg_comp->clock_class) {
			BT_LOGE_STR("Cannot create clock class.");
			goto error;
		}

		ret = bt_trace_add_clock_class(dmesg_comp->trace,
			dmesg_comp->clock_class);
		if (ret) {
			BT_LOGE_STR("Cannot add clock class to trace.");
			goto error;
		}

		ret = bt_clock_class_priority_map_add_clock_class(
			dmesg_comp->cc_prio_map, dmesg_comp->clock_class, 0);
		if (ret) {
			BT_LOGE_STR("Cannot add clock class to clock class priority map.");
			goto error;
		}

		bt_put(ft);
		ft = create_event_header_ft(dmesg_comp->clock_class);
		if (!ft) {
			BT_LOGE_STR("Cannot create event header field type.");
			goto error;
		}

		ret = bt_stream_class_set_event_header_type(
			dmesg_comp->stream_class, ft);
		if (ret) {
			BT_LOGE_STR("Cannot set stream class's event header field type.");
			goto error;
		}
	}

	dmesg_comp->event_class = bt_event_class_create("string");
	if (!dmesg_comp->event_class) {
		BT_LOGE_STR("Cannot create an empty event class object.");
		goto error;
	}

	bt_put(ft);
	ft = create_event_payload_ft();
	if (!ft) {
		BT_LOGE_STR("Cannot create event payload field type.");
		goto error;
	}

	ret = bt_event_class_set_payload_type(dmesg_comp->event_class, ft);
	if (ret) {
		BT_LOGE_STR("Cannot set event class's event payload field type.");
		goto error;
	}

	ret = bt_stream_class_add_event_class(dmesg_comp->stream_class,
		dmesg_comp->event_class);
	if (ret) {
		BT_LOGE("Cannot add event class to stream class: ret=%d", ret);
		goto error;
	}

	ret = bt_trace_add_stream_class(dmesg_comp->trace,
		dmesg_comp->stream_class);
	if (ret) {
		BT_LOGE("Cannot add event class to stream class: ret=%d", ret);
		goto error;
	}

	goto end;

error:
	ret = -1;

end:
	bt_put(ft);

	if (basename) {
		g_free(basename);
	}

	return ret;
}

static
int handle_params(struct dmesg_component *dmesg_comp, struct bt_value *params)
{
	struct bt_value *read_from_stdin = NULL;
	struct bt_value *no_timestamp = NULL;
	struct bt_value *path = NULL;
	const char *path_str;
	int ret = 0;

	no_timestamp = bt_value_map_get(params, "no-extract-timestamp");
	if (no_timestamp) {
		if (!bt_value_is_bool(no_timestamp)) {
			BT_LOGE("Expecting a boolean value for the `no-extract-timestamp` parameter: "
				"type=%s",
				bt_value_type_string(
					bt_value_get_type(no_timestamp)));
			goto error;
		}

		ret = bt_value_bool_get(no_timestamp,
			&dmesg_comp->params.no_timestamp);
		BT_ASSERT(ret == 0);
	}

	path = bt_value_map_get(params, "path");
	if (path) {
		if (dmesg_comp->params.read_from_stdin) {
			BT_LOGE_STR("Cannot specify both `read-from-stdin` and `path` parameters.");
			goto error;
		}

		if (!bt_value_is_string(path)) {
			BT_LOGE("Expecting a string value for the `path` parameter: "
				"type=%s",
				bt_value_type_string(
					bt_value_get_type(path)));
			goto error;
		}

		ret = bt_value_string_get(path, &path_str);
		BT_ASSERT(ret == 0);
		g_string_assign(dmesg_comp->params.path, path_str);
	} else {
		dmesg_comp->params.read_from_stdin = true;
	}

	goto end;

error:
	ret = -1;

end:
	bt_put(read_from_stdin);
	bt_put(path);
	bt_put(no_timestamp);
	return ret;
}

static
struct bt_field *create_packet_header_field(struct bt_field_type *ft)
{
	struct bt_field *ph = NULL;
	struct bt_field *magic = NULL;
	int ret;

	ph = bt_field_create(ft);
	if (!ph) {
		BT_LOGE_STR("Cannot create field object.");
		goto error;
	}

	magic = bt_field_structure_get_field_by_name(ph, "magic");
	if (!magic) {
		BT_LOGE_STR("Cannot get `magic` field from structure field.");
		goto error;
	}

	ret = bt_field_unsigned_integer_set_value(magic, 0xc1fc1fc1);
	if (ret) {
		BT_LOGE_STR("Cannot set integer field's value.");
		goto error;
	}

	goto end;

error:
	BT_PUT(ph);

end:
	bt_put(magic);
	return ph;
}

static
struct bt_field *create_packet_context_field(struct bt_field_type *ft)
{
	struct bt_field *pc = NULL;
	struct bt_field *field = NULL;
	int ret;

	pc = bt_field_create(ft);
	if (!pc) {
		BT_LOGE_STR("Cannot create field object.");
		goto error;
	}

	field = bt_field_structure_get_field_by_name(pc, "content_size");
	if (!field) {
		BT_LOGE_STR("Cannot get `content_size` field from structure field.");
		goto error;
	}

	ret = bt_field_unsigned_integer_set_value(field, 0);
	if (ret) {
		BT_LOGE_STR("Cannot set integer field's value.");
		goto error;
	}

	bt_put(field);
	field = bt_field_structure_get_field_by_name(pc, "packet_size");
	if (!field) {
		BT_LOGE_STR("Cannot get `packet_size` field from structure field.");
		goto error;
	}

	ret = bt_field_unsigned_integer_set_value(field, 0);
	if (ret) {
		BT_LOGE_STR("Cannot set integer field's value.");
		goto error;
	}

	goto end;

error:
	BT_PUT(pc);

end:
	bt_put(field);
	return pc;
}

static
int create_packet_and_stream(struct dmesg_component *dmesg_comp)
{
	int ret = 0;
	struct bt_field_type *ft = NULL;
	struct bt_field *field = NULL;

	dmesg_comp->stream = bt_stream_create(dmesg_comp->stream_class,
		NULL);
	if (!dmesg_comp->stream) {
		BT_LOGE_STR("Cannot create stream object.");
		goto error;
	}

	dmesg_comp->packet = bt_packet_create(dmesg_comp->stream);
	if (!dmesg_comp->packet) {
		BT_LOGE_STR("Cannot create packet object.");
		goto error;
	}

	ft = bt_trace_get_packet_header_type(dmesg_comp->trace);
	BT_ASSERT(ft);
	field = create_packet_header_field(ft);
	if (!field) {
		BT_LOGE_STR("Cannot create packet header field.");
		goto error;
	}

	ret = bt_packet_set_header(dmesg_comp->packet, field);
	if (ret) {
		BT_LOGE_STR("Cannot set packet's header field.");
		goto error;
	}

	bt_put(ft);
	bt_put(field);
	ft = bt_stream_class_get_packet_context_type(
		dmesg_comp->stream_class);
	BT_ASSERT(ft);
	field = create_packet_context_field(ft);
	if (!field) {
		BT_LOGE_STR("Cannot create packet context field.");
		goto error;
	}

	ret = bt_packet_set_context(dmesg_comp->packet, field);
	if (ret) {
		BT_LOGE_STR("Cannot set packet's context field.");
		goto error;
	}

	ret = bt_trace_set_is_static(dmesg_comp->trace);
	if (ret) {
		BT_LOGE_STR("Cannot make trace static.");
		goto error;
	}

	goto end;

error:
	ret = -1;

end:
	bt_put(field);
	bt_put(ft);
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

	ret = create_packet_and_stream(dmesg_comp);
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

	bt_put(dmesg_comp->packet);
	bt_put(dmesg_comp->trace);
	bt_put(dmesg_comp->stream_class);
	bt_put(dmesg_comp->event_class);
	bt_put(dmesg_comp->stream);
	bt_put(dmesg_comp->clock_class);
	bt_put(dmesg_comp->cc_prio_map);
	g_free(dmesg_comp);
}

static
enum bt_component_status create_port(struct bt_private_component *priv_comp)
{
	return bt_private_component_source_add_output_private_port(priv_comp,
		"out", NULL, NULL);
}

BT_HIDDEN
enum bt_component_status dmesg_init(struct bt_private_component *priv_comp,
		struct bt_value *params, void *init_method_data)
{
	int ret = 0;
	struct dmesg_component *dmesg_comp = g_new0(struct dmesg_component, 1);
	enum bt_component_status status = BT_COMPONENT_STATUS_OK;

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
		BT_LOGE("Invalid parameters: comp-addr=%p", priv_comp);
		goto error;
	}

	if (!dmesg_comp->params.read_from_stdin &&
			!g_file_test(dmesg_comp->params.path->str,
			G_FILE_TEST_IS_REGULAR)) {
		BT_LOGE("Input path is not a regular file: "
			"comp-addr=%p, path=\"%s\"", priv_comp,
			dmesg_comp->params.path->str);
		goto error;
	}

	status = create_port(priv_comp);
	if (status != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	(void) bt_private_component_set_user_data(priv_comp, dmesg_comp);
	goto end;

error:
	destroy_dmesg_component(dmesg_comp);
	(void) bt_private_component_set_user_data(priv_comp, NULL);

	if (status >= 0) {
		status = BT_COMPONENT_STATUS_ERROR;
	}

end:
	return status;
}

BT_HIDDEN
void dmesg_finalize(struct bt_private_component *priv_comp)
{
	void *data = bt_private_component_get_user_data(priv_comp);

	destroy_dmesg_component(data);
}

static
int create_event_header_from_line(
		struct dmesg_component *dmesg_comp,
		const char *line, const char **new_start,
		struct bt_field **user_field,
		struct bt_clock_value **user_clock_value)
{
	bool has_timestamp = false;
	unsigned long sec, usec, msec;
	unsigned int year, mon, mday, hour, min;
	uint64_t ts = 0;
	struct bt_clock_value *clock_value = NULL;
	struct bt_field_type *ft = NULL;
	struct bt_field *eh_field = NULL;
	struct bt_field *ts_field = NULL;
	int ret = 0;

	BT_ASSERT(user_clock_value);
	BT_ASSERT(user_field);
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
	 * field type should have a timestamp or not, so we can lazily
	 * create the metadata, stream, and packet objects.
	 */
	ret = try_create_meta_stream_packet(dmesg_comp, has_timestamp);
	if (ret) {
		/* try_create_meta_stream_packet() logs errors */
		goto error;
	}

	if (dmesg_comp->clock_class) {
		clock_value = bt_clock_value_create(dmesg_comp->clock_class,
			ts);
		if (!clock_value) {
			BT_LOGE_STR("Cannot create clock value object.");
			goto error;
		}

		ft = bt_stream_class_get_event_header_type(
			dmesg_comp->stream_class);
		BT_ASSERT(ft);
		eh_field = bt_field_create(ft);
		if (!eh_field) {
			BT_LOGE_STR("Cannot create event header field object.");
			goto error;
		}

		ts_field = bt_field_structure_get_field_by_name(eh_field,
			"timestamp");
		if (!ts_field) {
			BT_LOGE_STR("Cannot get `timestamp` field from structure field.");
			goto error;
		}

		ret = bt_field_unsigned_integer_set_value(ts_field, ts);
		if (ret) {
			BT_LOGE_STR("Cannot set integer field's value.");
			goto error;
		}

		*user_clock_value = clock_value;
		clock_value = NULL;
		*user_field = eh_field;
		eh_field = NULL;
	}

	goto end;

error:
	ret = -1;

end:
	bt_put(ft);
	bt_put(ts_field);
	bt_put(clock_value);
	bt_put(eh_field);
	return ret;
}

static
int create_event_payload_from_line(
		struct dmesg_component *dmesg_comp,
		const char *line, struct bt_field **user_field)
{
	struct bt_field_type *ft = NULL;
	struct bt_field *ep_field = NULL;
	struct bt_field *str_field = NULL;
	size_t len;
	int ret;

	BT_ASSERT(user_field);
	ft = bt_event_class_get_payload_type(dmesg_comp->event_class);
	BT_ASSERT(ft);
	ep_field = bt_field_create(ft);
	if (!ep_field) {
		BT_LOGE_STR("Cannot create event payload field object.");
		goto error;
	}

	str_field = bt_field_structure_get_field_by_name(ep_field, "str");
	if (!str_field) {
		BT_LOGE_STR("Cannot get `timestamp` field from structure field.");
		goto error;
	}

	len = strlen(line);
	if (line[len - 1] == '\n') {
		/* Do not include the newline character in the payload */
		len--;
	}

	ret = bt_field_string_append_len(str_field, line, len);
	if (ret) {
		BT_LOGE("Cannot append value to string field object: "
			"len=%zu", len);
		goto error;
	}

	*user_field = ep_field;
	ep_field = NULL;
	goto end;

error:
	ret = -1;

end:
	bt_put(ft);
	bt_put(ep_field);
	bt_put(str_field);
	return ret;
}

static
struct bt_notification *create_notif_from_line(
		struct dmesg_component *dmesg_comp, const char *line)
{
	struct bt_field *eh_field = NULL;
	struct bt_field *ep_field = NULL;
	struct bt_clock_value *clock_value = NULL;
	struct bt_event *event = NULL;
	struct bt_notification *notif = NULL;
	const char *new_start;
	int ret;

	ret = create_event_header_from_line(dmesg_comp, line, &new_start,
		&eh_field, &clock_value);
	if (ret) {
		BT_LOGE("Cannot create event header field from line: "
			"ret=%d", ret);
		goto error;
	}

	ret = create_event_payload_from_line(dmesg_comp, new_start,
		&ep_field);
	if (ret) {
		BT_LOGE("Cannot create event payload field from line: "
			"ret=%d", ret);
		goto error;
	}

	BT_ASSERT(ep_field);
	event = bt_event_create(dmesg_comp->event_class);
	if (!event) {
		BT_LOGE_STR("Cannot create event object.");
		goto error;
	}

	ret = bt_event_set_packet(event, dmesg_comp->packet);
	if (ret) {
		BT_LOGE_STR("Cannot set event's packet.");
		goto error;
	}

	if (eh_field) {
		ret = bt_event_set_header(event, eh_field);
		if (ret) {
			BT_LOGE_STR("Cannot set event's header field.");
			goto error;
		}
	}

	ret = bt_event_set_event_payload(event, ep_field);
	if (ret) {
		BT_LOGE_STR("Cannot set event's payload field.");
		goto error;
	}

	if (clock_value) {
		ret = bt_event_set_clock_value(event, clock_value);
		if (ret) {
			BT_LOGE_STR("Cannot set event's clock value.");
			goto error;
		}
	}

	notif = bt_notification_event_create(event, dmesg_comp->cc_prio_map);
	if (!notif) {
		BT_LOGE_STR("Cannot create event notification.");
		goto error;
	}

	goto end;

error:
	BT_PUT(notif);

end:
	bt_put(eh_field);
	bt_put(ep_field);
	bt_put(clock_value);
	bt_put(event);
	return notif;
}

static
void destroy_dmesg_notif_iter(struct dmesg_notif_iter *dmesg_notif_iter)
{
	if (!dmesg_notif_iter) {
		return;
	}

	if (dmesg_notif_iter->fp && dmesg_notif_iter->fp != stdin) {
		if (fclose(dmesg_notif_iter->fp)) {
			BT_LOGE_ERRNO("Cannot close input file", ".");
		}
	}

	free(dmesg_notif_iter->linebuf);
	g_free(dmesg_notif_iter);
}

BT_HIDDEN
enum bt_notification_iterator_status dmesg_notif_iter_init(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter,
		struct bt_private_port *priv_port)
{
	struct bt_private_component *priv_comp = NULL;
	struct dmesg_component *dmesg_comp;
	struct dmesg_notif_iter *dmesg_notif_iter =
		g_new0(struct dmesg_notif_iter, 1);
	enum bt_notification_iterator_status status =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;

	if (!dmesg_notif_iter) {
		BT_LOGE_STR("Failed to allocate on dmesg notification iterator structure.");
		goto error;
	}

	priv_comp = bt_private_connection_private_notification_iterator_get_private_component(
		priv_notif_iter);
	BT_ASSERT(priv_comp);
	dmesg_comp = bt_private_component_get_user_data(priv_comp);
	BT_ASSERT(dmesg_comp);
	dmesg_notif_iter->dmesg_comp = dmesg_comp;

	if (dmesg_comp->params.read_from_stdin) {
		dmesg_notif_iter->fp = stdin;
	} else {
		dmesg_notif_iter->fp = fopen(dmesg_comp->params.path->str, "r");
		if (!dmesg_notif_iter->fp) {
			BT_LOGE_ERRNO("Cannot open input file in read mode", ": path=\"%s\"",
				dmesg_comp->params.path->str);
			goto error;
		}
	}

	(void) bt_private_connection_private_notification_iterator_set_user_data(priv_notif_iter,
		dmesg_notif_iter);
	goto end;

error:
	destroy_dmesg_notif_iter(dmesg_notif_iter);
	(void) bt_private_connection_private_notification_iterator_set_user_data(priv_notif_iter,
		NULL);
	if (status >= 0) {
		status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
	}

end:
	bt_put(priv_comp);
	return status;
}

BT_HIDDEN
void dmesg_notif_iter_finalize(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter)
{
	destroy_dmesg_notif_iter(bt_private_connection_private_notification_iterator_get_user_data(
		priv_notif_iter));
}

BT_HIDDEN
struct bt_notification_iterator_next_method_return dmesg_notif_iter_next(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter)
{
	ssize_t len;
	struct dmesg_notif_iter *dmesg_notif_iter =
		bt_private_connection_private_notification_iterator_get_user_data(
			priv_notif_iter);
	struct dmesg_component *dmesg_comp;
	struct bt_notification_iterator_next_method_return next_ret = {
		.status = BT_NOTIFICATION_ITERATOR_STATUS_OK,
		.notification = NULL
	};

	BT_ASSERT(dmesg_notif_iter);
	dmesg_comp = dmesg_notif_iter->dmesg_comp;
	BT_ASSERT(dmesg_comp);

	while (true) {
		const char *ch;
		bool only_spaces = true;

		len = bt_getline(&dmesg_notif_iter->linebuf,
			&dmesg_notif_iter->linebuf_len, dmesg_notif_iter->fp);
		if (len < 0) {
			if (errno == EINVAL) {
				next_ret.status =
					BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
			} else if (errno == ENOMEM) {
				next_ret.status =
					BT_NOTIFICATION_ITERATOR_STATUS_NOMEM;
			} else {
				next_ret.status =
					BT_NOTIFICATION_ITERATOR_STATUS_END;
			}

			goto end;
		}

		BT_ASSERT(dmesg_notif_iter->linebuf);

		/* Ignore empty lines, once trimmed */
		for (ch = dmesg_notif_iter->linebuf; *ch != '\0'; ch++) {
			if (!isspace(*ch)) {
				only_spaces = false;
				break;
			}
		}

		if (!only_spaces) {
			break;
		}
	}

	next_ret.notification = create_notif_from_line(dmesg_comp,
		dmesg_notif_iter->linebuf);
	if (!next_ret.notification) {
		BT_LOGE("Cannot create event notification from line: "
			"dmesg-comp-addr=%p, line=\"%s\"", dmesg_comp,
			dmesg_notif_iter->linebuf);
	}

end:
	return next_ret;
}
