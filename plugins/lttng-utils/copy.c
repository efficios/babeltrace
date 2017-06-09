/*
 * copy.c
 *
 * Babeltrace Copy Trace Structure
 *
 * Copyright 2017 Julien Desfossez <jdesfossez@efficios.com>
 *
 * Author: Julien Desfossez <jdesfossez@efficios.com>
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

#include <inttypes.h>
#include <babeltrace/ctf-ir/event.h>
#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/ctf-ir/event-class.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/ctf-writer/stream-class.h>
#include <babeltrace/ctf-writer/stream.h>

#include <ctfcopytrace.h>
#include "debug-info.h"

static
struct bt_ctf_stream *insert_new_stream(
		struct debug_info_iterator *debug_it,
		struct bt_ctf_stream *stream,
		struct debug_info_trace *di_trace);

static
void unref_stream(struct bt_ctf_stream *stream)
{
	bt_put(stream);
}

static
void unref_packet(struct bt_ctf_packet *packet)
{
	bt_put(packet);
}

static
void unref_stream_class(struct bt_ctf_stream_class *stream_class)
{
	bt_put(stream_class);
}

static
void unref_debug_info(struct debug_info *debug_info)
{
	debug_info_destroy(debug_info);
}

static
void destroy_stream_state_key(gpointer key)
{
	g_free((enum fs_writer_stream_state *) key);
}

static
struct bt_ctf_field *get_payload_field(FILE *err,
		struct bt_ctf_event *event, const char *field_name)
{
	struct bt_ctf_field *field = NULL, *sec = NULL;
	struct bt_ctf_field_type *sec_type = NULL;

	sec = bt_ctf_event_get_payload(event, NULL);
	if (!sec) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}

	sec_type = bt_ctf_field_get_type(sec);
	if (!sec_type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}

	if (bt_ctf_field_type_get_type_id(sec_type) != BT_CTF_FIELD_TYPE_ID_STRUCT) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}

	field = bt_ctf_field_structure_get_field(sec, field_name);

end:
	bt_put(sec_type);
	bt_put(sec);
	return field;
}

static
struct bt_ctf_field *get_stream_event_context_field(FILE *err,
		struct bt_ctf_event *event, const char *field_name)
{
	struct bt_ctf_field *field = NULL, *sec = NULL;
	struct bt_ctf_field_type *sec_type = NULL;

	sec = bt_ctf_event_get_stream_event_context(event);
	if (!sec) {
		goto end;
	}

	sec_type = bt_ctf_field_get_type(sec);
	if (!sec_type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}

	if (bt_ctf_field_type_get_type_id(sec_type) != BT_CTF_FIELD_TYPE_ID_STRUCT) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}

	field = bt_ctf_field_structure_get_field(sec, field_name);

end:
	bt_put(sec_type);
	bt_put(sec);
	return field;
}

BT_HIDDEN
int get_stream_event_context_unsigned_int_field_value(FILE *err,
		struct bt_ctf_event *event, const char *field_name,
		uint64_t *value)
{
	int ret;
	struct bt_ctf_field *field = NULL;
	struct bt_ctf_field_type *field_type = NULL;

	field = get_stream_event_context_field(err, event, field_name);
	if (!field) {
		goto error;
	}

	field_type = bt_ctf_field_get_type(field);
	if (!field_type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	if (bt_ctf_field_type_get_type_id(field_type) != BT_CTF_FIELD_TYPE_ID_INTEGER) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	if (bt_ctf_field_type_integer_get_signed(field_type) != 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	ret = bt_ctf_field_unsigned_integer_get_value(field, value);
	goto end;

error:
	ret = -1;
end:
	bt_put(field_type);
	bt_put(field);
	return ret;
}

BT_HIDDEN
int get_stream_event_context_int_field_value(FILE *err, struct bt_ctf_event *event,
		const char *field_name, int64_t *value)
{
	struct bt_ctf_field *field = NULL;
	struct bt_ctf_field_type *field_type = NULL;
	int ret;

	field = get_stream_event_context_field(err, event, field_name);
	if (!field) {
		goto error;
	}

	field_type = bt_ctf_field_get_type(field);
	if (!field_type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	if (bt_ctf_field_type_get_type_id(field_type) != BT_CTF_FIELD_TYPE_ID_INTEGER) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	if (bt_ctf_field_type_integer_get_signed(field_type) != 1) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	ret = bt_ctf_field_signed_integer_get_value(field, value);
	goto end;

error:
	ret = -1;
end:
	bt_put(field_type);
	bt_put(field);
	return ret;
}

BT_HIDDEN
int get_payload_unsigned_int_field_value(FILE *err,
		struct bt_ctf_event *event, const char *field_name,
		uint64_t *value)
{
	struct bt_ctf_field *field = NULL;
	struct bt_ctf_field_type *field_type = NULL;
	int ret;

	field = get_payload_field(err, event, field_name);
	if (!field) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	field_type = bt_ctf_field_get_type(field);
	if (!field_type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	if (bt_ctf_field_type_get_type_id(field_type) != BT_CTF_FIELD_TYPE_ID_INTEGER) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	if (bt_ctf_field_type_integer_get_signed(field_type) != 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	ret = bt_ctf_field_unsigned_integer_get_value(field, value);
	goto end;

error:
	ret = -1;
end:
	bt_put(field_type);
	bt_put(field);
	return ret;
}

BT_HIDDEN
int get_payload_int_field_value(FILE *err, struct bt_ctf_event *event,
		const char *field_name, int64_t *value)
{
	struct bt_ctf_field *field = NULL;
	struct bt_ctf_field_type *field_type = NULL;
	int ret;

	field = get_payload_field(err, event, field_name);
	if (!field) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	field_type = bt_ctf_field_get_type(field);
	if (!field_type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	if (bt_ctf_field_type_get_type_id(field_type) != BT_CTF_FIELD_TYPE_ID_INTEGER) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	if (bt_ctf_field_type_integer_get_signed(field_type) != 1) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	ret = bt_ctf_field_signed_integer_get_value(field, value);
	goto end;

error:
	ret = -1;
end:
	bt_put(field_type);
	bt_put(field);
	return ret;
}

BT_HIDDEN
int get_payload_string_field_value(FILE *err,
		struct bt_ctf_event *event, const char *field_name,
		const char **value)
{
	struct bt_ctf_field *field = NULL;
	struct bt_ctf_field_type *field_type = NULL;
	int ret;

	/*
	 * The field might not exist, no error here.
	 */
	field = get_payload_field(err, event, field_name);
	if (!field) {
		goto error;
	}

	field_type = bt_ctf_field_get_type(field);
	if (!field_type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	if (bt_ctf_field_type_get_type_id(field_type) != BT_CTF_FIELD_TYPE_ID_STRING) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	*value = bt_ctf_field_string_get_value(field);
	if (!*value) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	ret = 0;
	goto end;

error:
	ret = -1;
end:
	bt_put(field_type);
	bt_put(field);
	return ret;
}

BT_HIDDEN
int get_payload_build_id_field_value(FILE *err,
		struct bt_ctf_event *event, const char *field_name,
		uint8_t **build_id, uint64_t *build_id_len)
{
	struct bt_ctf_field *field = NULL, *seq_len = NULL;
	struct bt_ctf_field_type *field_type = NULL;
	struct bt_ctf_field *seq_field = NULL;
	uint64_t i;
	int ret;

	*build_id = NULL;

	field = get_payload_field(err, event, field_name);
	if (!field) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	field_type = bt_ctf_field_get_type(field);
	if (!field_type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	if (bt_ctf_field_type_get_type_id(field_type) != BT_CTF_FIELD_TYPE_ID_SEQUENCE) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}
	BT_PUT(field_type);

	seq_len = bt_ctf_field_sequence_get_length(field);
	if (!seq_len) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	ret = bt_ctf_field_unsigned_integer_get_value(seq_len, build_id_len);
	if (ret) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}
	BT_PUT(seq_len);

	*build_id = g_new0(uint8_t, *build_id_len);
	if (!*build_id) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	for (i = 0; i < *build_id_len; i++) {
		uint64_t tmp;

		seq_field = bt_ctf_field_sequence_get_field(field, i);
		if (!seq_field) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}

		ret = bt_ctf_field_unsigned_integer_get_value(seq_field, &tmp);
		if (ret) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		BT_PUT(seq_field);
		(*build_id)[i] = (uint8_t) tmp;
	}
	ret = 0;
	goto end;

error:
	g_free(*build_id);
	ret = -1;
end:
	bt_put(field_type);
	bt_put(field);
	return ret;
}

static
struct debug_info *lookup_trace_debug_info(struct debug_info_iterator *debug_it,
		struct bt_ctf_trace *writer_trace,
		struct debug_info_trace *di_trace)
{
	return (struct debug_info *) g_hash_table_lookup(
			di_trace->trace_debug_map,
			(gpointer) writer_trace);
}

static
struct debug_info *insert_new_debug_info(struct debug_info_iterator *debug_it,
		struct bt_ctf_trace *writer_trace,
		struct debug_info_trace *di_trace)
{
	struct debug_info *debug_info = NULL;
	struct bt_value *field = NULL;
	const char *str_value;
	enum bt_value_status ret;

	field = bt_ctf_trace_get_environment_field_value_by_name(writer_trace,
			"domain");
	/* No domain field, no debug info */
	if (!field) {
		goto end;
	}
	ret = bt_value_string_get(field, &str_value);
	if (ret != BT_VALUE_STATUS_OK) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}
	/* Domain not ust, no debug info */
	if (strcmp(str_value, "ust") != 0) {
		goto end;
	}
	BT_PUT(field);

	/* No tracer_name, no debug info */
	field = bt_ctf_trace_get_environment_field_value_by_name(writer_trace,
			"tracer_name");
	/* No tracer_name, no debug info */
	if (!field) {
		goto end;
	}
	ret = bt_value_string_get(field, &str_value);
	if (ret != BT_VALUE_STATUS_OK) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}
	/* Tracer_name not lttng-ust, no debug info */
	if (strcmp(str_value, "lttng-ust") != 0) {
		goto end;
	}
	BT_PUT(field);

	debug_info = debug_info_create(debug_it->debug_info_component);
	if (!debug_info) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}

	g_hash_table_insert(di_trace->trace_debug_map, (gpointer) writer_trace,
			debug_info);

end:
	bt_put(field);
	return debug_info;
}

static
struct debug_info *get_trace_debug_info(struct debug_info_iterator *debug_it,
		struct bt_ctf_trace *writer_trace,
		struct debug_info_trace *di_trace)
{
	struct debug_info *debug_info;

	debug_info = lookup_trace_debug_info(debug_it, writer_trace, di_trace);
	if (debug_info) {
		goto end;
	}

	debug_info = insert_new_debug_info(debug_it, writer_trace, di_trace);

end:
	return debug_info;
}

static
struct debug_info_trace *lookup_trace(struct debug_info_iterator *debug_it,
		struct bt_ctf_trace *trace)
{
	return (struct debug_info_trace *) g_hash_table_lookup(
			debug_it->trace_map,
			(gpointer) trace);
}

static
enum debug_info_stream_state *insert_new_stream_state(
		struct debug_info_iterator *debug_it,
		struct debug_info_trace *di_trace, struct bt_ctf_stream *stream)
{
	enum debug_info_stream_state *v = NULL;

	v = g_new0(enum debug_info_stream_state, 1);
	if (!v) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}
	*v = DEBUG_INFO_UNKNOWN_STREAM;

	g_hash_table_insert(di_trace->stream_states, stream, v);

end:
	return v;
}

static
void check_completed_trace(gpointer key, gpointer value, gpointer user_data)
{
	enum debug_info_stream_state *state = value;
	int *trace_completed = user_data;

	if (*state != DEBUG_INFO_COMPLETED_STREAM) {
		*trace_completed = 0;
	}
}

static
gboolean empty_ht(gpointer key, gpointer value, gpointer user_data)
{
	return TRUE;
}

BT_HIDDEN
void debug_info_close_trace(struct debug_info_iterator *debug_it,
		struct debug_info_trace *di_trace)
{
	if (di_trace->static_listener_id >= 0) {
		bt_ctf_trace_remove_is_static_listener(di_trace->trace,
				di_trace->static_listener_id);
	}

	/* Empty the stream class HT. */
	g_hash_table_foreach_remove(di_trace->stream_class_map,
			empty_ht, NULL);
	g_hash_table_destroy(di_trace->stream_class_map);

	/* Empty the stream HT. */
	g_hash_table_foreach_remove(di_trace->stream_map,
			empty_ht, NULL);
	g_hash_table_destroy(di_trace->stream_map);

	/* Empty the stream state HT. */
	g_hash_table_foreach_remove(di_trace->stream_states,
			empty_ht, NULL);
	g_hash_table_destroy(di_trace->stream_states);

	/* Empty the packet HT. */
	g_hash_table_foreach_remove(di_trace->packet_map,
			empty_ht, NULL);
	g_hash_table_destroy(di_trace->packet_map);

	/* Empty the trace_debug HT. */
	g_hash_table_foreach_remove(di_trace->trace_debug_map,
			empty_ht, NULL);
	g_hash_table_destroy(di_trace->trace_debug_map);
}

static
int sync_event_classes(struct debug_info_iterator *debug_it,
		struct bt_ctf_stream *stream,
		struct bt_ctf_stream *writer_stream)
{
	int int_ret;
	struct bt_ctf_stream_class *stream_class = NULL,
				   *writer_stream_class = NULL;
	enum bt_component_status ret;

	stream_class = bt_ctf_stream_get_class(stream);
	if (!stream_class) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	writer_stream_class = bt_ctf_stream_get_class(writer_stream);
	if (!writer_stream_class) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	ret = ctf_copy_event_classes(debug_it->err, stream_class,
			writer_stream_class);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	int_ret = 0;
	goto end;

error:
	int_ret = -1;
end:
	bt_put(stream_class);
	bt_put(writer_stream_class);
	return int_ret;
}

static
void trace_is_static_listener(struct bt_ctf_trace *trace, void *data)
{
	struct debug_info_trace *di_trace = data;
	struct debug_info_iterator *debug_it = di_trace->debug_it;
	int trace_completed = 1, ret, nr_stream, i;
	struct bt_ctf_stream *stream = NULL, *writer_stream = NULL;
	struct bt_ctf_trace *writer_trace = di_trace->writer_trace;

	/*
	 * When the trace becomes static, make sure that we have all
	 * the event classes in our stream_class copies before setting it
	 * static as well.
	 */
	nr_stream = bt_ctf_trace_get_stream_count(trace);
	for (i = 0; i < nr_stream; i++) {
		stream = bt_ctf_trace_get_stream_by_index(trace, i);
		if (!stream) {
			fprintf(debug_it->err,
					"[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		writer_stream = bt_ctf_trace_get_stream_by_index(writer_trace, i);
		if (!writer_stream) {
			fprintf(debug_it->err,
					"[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		ret = sync_event_classes(di_trace->debug_it, stream, writer_stream);
		if (ret) {
			fprintf(debug_it->err,
					"[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		BT_PUT(stream);
		BT_PUT(writer_stream);
	}

	bt_ctf_trace_set_is_static(di_trace->writer_trace);
	di_trace->trace_static = 1;

	g_hash_table_foreach(di_trace->stream_states,
			check_completed_trace, &trace_completed);
	if (trace_completed) {
		debug_info_close_trace(di_trace->debug_it, di_trace);
		g_hash_table_remove(di_trace->debug_it->trace_map,
				di_trace->trace);
	}

error:
	bt_put(writer_stream);
	bt_put(stream);
}

static
struct debug_info_trace *insert_new_trace(struct debug_info_iterator *debug_it,
		struct bt_ctf_stream *stream) {
	struct bt_ctf_trace *writer_trace = NULL;
	struct debug_info_trace *di_trace = NULL;
	struct bt_ctf_trace *trace = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_stream *writer_stream = NULL;
	int ret, nr_stream, i;

	writer_trace = bt_ctf_trace_create();
	if (!writer_trace) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	stream_class = bt_ctf_stream_get_class(stream);
	if (!stream_class) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	trace = bt_ctf_stream_class_get_trace(stream_class);
	if (!trace) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	ret = ctf_copy_trace(debug_it->err, trace, writer_trace);
	if (ret != BT_COMPONENT_STATUS_OK) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	di_trace = g_new0(struct debug_info_trace, 1);
	if (!di_trace) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	di_trace->trace = trace;
	di_trace->writer_trace = writer_trace;
	di_trace->debug_info_component = debug_it->debug_info_component;
	di_trace->debug_it = debug_it;
	di_trace->stream_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) unref_stream);
	di_trace->stream_class_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) unref_stream_class);
	di_trace->packet_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) unref_packet);
	di_trace->trace_debug_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) unref_debug_info);
	di_trace->stream_states = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, destroy_stream_state_key);
	g_hash_table_insert(debug_it->trace_map, (gpointer) trace, di_trace);

	/* Set all the existing streams in the unknown state. */
	nr_stream = bt_ctf_trace_get_stream_count(trace);
	for (i = 0; i < nr_stream; i++) {
		stream = bt_ctf_trace_get_stream_by_index(trace, i);
		if (!stream) {
			fprintf(debug_it->err,
					"[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		insert_new_stream_state(debug_it, di_trace, stream);
		writer_stream = insert_new_stream(debug_it, stream, di_trace);
		if (!writer_stream) {
			fprintf(debug_it->err,
					"[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		bt_get(writer_stream);
		ret = sync_event_classes(debug_it, stream, writer_stream);
		if (ret) {
			fprintf(debug_it->err,
					"[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		BT_PUT(writer_stream);
		BT_PUT(stream);
	}

	/* Check if the trace is already static or register a listener. */
	if (bt_ctf_trace_is_static(trace)) {
		di_trace->trace_static = 1;
		di_trace->static_listener_id = -1;
		bt_ctf_trace_set_is_static(writer_trace);
	} else {
		ret = bt_ctf_trace_add_is_static_listener(trace,
				trace_is_static_listener, di_trace);
		if (ret < 0) {
			fprintf(debug_it->err,
					"[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			goto error;
		}
		di_trace->static_listener_id = ret;
	}


	goto end;

error:
	BT_PUT(writer_trace);
	g_free(di_trace);
	di_trace = NULL;
end:
	bt_put(stream);
	bt_put(writer_stream);
	bt_put(stream_class);
	bt_put(trace);
	return di_trace;
}

static
struct bt_ctf_packet *lookup_packet(struct debug_info_iterator *debug_it,
		struct bt_ctf_packet *packet,
		struct debug_info_trace *di_trace)
{
	return (struct bt_ctf_packet *) g_hash_table_lookup(
			di_trace->packet_map,
			(gpointer) packet);
}

static
struct bt_ctf_packet *insert_new_packet(struct debug_info_iterator *debug_it,
		struct bt_ctf_packet *packet,
		struct bt_ctf_stream *writer_stream,
		struct debug_info_trace *di_trace)
{
	struct bt_ctf_packet *writer_packet;

	writer_packet = bt_ctf_packet_create(writer_stream);
	if (!writer_packet) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}
	g_hash_table_insert(di_trace->packet_map, (gpointer) packet, writer_packet);

end:
	return writer_packet;
}

static
int add_debug_info_fields(FILE *err,
		struct bt_ctf_field_type *writer_event_context_type,
		struct debug_info_component *component)
{
	struct bt_ctf_field_type *ip_field = NULL, *debug_field_type = NULL,
				 *bin_field_type = NULL, *func_field_type = NULL,
				 *src_field_type = NULL;
	int ret = 0;

	ip_field = bt_ctf_field_type_structure_get_field_type_by_name(
			writer_event_context_type, "_ip");
	/* No ip field, so no debug info. */
	if (!ip_field) {
		goto end;
	}
	BT_PUT(ip_field);

	debug_field_type = bt_ctf_field_type_structure_get_field_type_by_name(
			writer_event_context_type,
			component->arg_debug_info_field_name);
	/* Already existing debug_info field, no need to add it. */
	if (debug_field_type) {
		goto end;
	}

	debug_field_type = bt_ctf_field_type_structure_create();
	if (!debug_field_type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	bin_field_type = bt_ctf_field_type_string_create();
	if (!bin_field_type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	func_field_type = bt_ctf_field_type_string_create();
	if (!func_field_type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	src_field_type = bt_ctf_field_type_string_create();
	if (!src_field_type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret = bt_ctf_field_type_structure_add_field(debug_field_type,
			bin_field_type, "bin");
	if (ret) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret = bt_ctf_field_type_structure_add_field(debug_field_type,
			func_field_type, "func");
	if (ret) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret = bt_ctf_field_type_structure_add_field(debug_field_type,
			src_field_type, "src");
	if (ret) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret = bt_ctf_field_type_structure_add_field(writer_event_context_type,
			debug_field_type, component->arg_debug_info_field_name);
	if (ret) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret = 0;
	goto end;

error:
	BT_PUT(debug_field_type);
	ret = -1;
end:
	bt_put(src_field_type);
	bt_put(func_field_type);
	bt_put(bin_field_type);
	bt_put(debug_field_type);
	return ret;
}

static
int create_debug_info_event_context_type(FILE *err,
		struct bt_ctf_field_type *event_context_type,
		struct bt_ctf_field_type *writer_event_context_type,
		struct debug_info_component *component)
{
	int ret, nr_fields, i;

	nr_fields = bt_ctf_field_type_structure_get_field_count(event_context_type);
	for (i = 0; i < nr_fields; i++) {
		struct bt_ctf_field_type *field_type = NULL;
		const char *field_name;

		if (bt_ctf_field_type_structure_get_field(event_context_type,
					&field_name, &field_type, i) < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}

		ret = bt_ctf_field_type_structure_add_field(writer_event_context_type,
				field_type, field_name);
		BT_PUT(field_type);
		if (ret) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			goto error;
		}
	}

	ret = add_debug_info_fields(err, writer_event_context_type,
			component);
	goto end;

error:
	ret = -1;
end:
	return ret;
}

static
struct bt_ctf_stream_class *copy_stream_class_debug_info(FILE *err,
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_trace *writer_trace,
		struct debug_info_component *component)
{
	struct bt_ctf_field_type *type = NULL;
	struct bt_ctf_stream_class *writer_stream_class = NULL;
	struct bt_ctf_field_type *writer_event_context_type = NULL;
	int ret_int;
	const char *name = bt_ctf_stream_class_get_name(stream_class);

	if (strlen(name) == 0) {
		name = NULL;
	}

	writer_stream_class = bt_ctf_stream_class_create(name);
	if (!writer_stream_class) {
		fprintf(err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	type = bt_ctf_stream_class_get_packet_context_type(stream_class);
	if (!type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret_int = bt_ctf_stream_class_set_packet_context_type(
			writer_stream_class, type);
	if (ret_int < 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}
	BT_PUT(type);

	type = bt_ctf_stream_class_get_event_header_type(stream_class);
	if (!type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret_int = bt_ctf_stream_class_set_event_header_type(
			writer_stream_class, type);
	if (ret_int < 0) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}
	BT_PUT(type);

	type = bt_ctf_stream_class_get_event_context_type(stream_class);
	if (type) {
		writer_event_context_type = bt_ctf_field_type_structure_create();
		if (!writer_event_context_type) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			goto error;
		}
		ret_int = create_debug_info_event_context_type(err, type,
				writer_event_context_type, component);
		if (ret_int) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			goto error;
		}
		BT_PUT(type);

		ret_int = bt_ctf_stream_class_set_event_context_type(
				writer_stream_class, writer_event_context_type);
		if (ret_int < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			goto error;
		}
		BT_PUT(writer_event_context_type);
	}

	goto end;

error:
	BT_PUT(writer_stream_class);
end:
	bt_put(writer_event_context_type);
	bt_put(type);
	return writer_stream_class;
}

/*
 * Add the original clock classes to the new trace, we do not need to copy
 * them, and if we did, we would have to manually inspect the stream class
 * to update the integers mapping to a clock.
 */
static
int add_clock_classes(FILE *err, struct bt_ctf_trace *writer_trace,
		struct bt_ctf_stream_class *writer_stream_class,
		struct bt_ctf_trace *trace)
{
	int ret, clock_class_count, i;

	clock_class_count = bt_ctf_trace_get_clock_class_count(trace);

	for (i = 0; i < clock_class_count; i++) {
		struct bt_ctf_clock_class *clock_class =
			bt_ctf_trace_get_clock_class_by_index(trace, i);

		if (!clock_class) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			goto error;
		}

		ret = bt_ctf_trace_add_clock_class(writer_trace, clock_class);
		BT_PUT(clock_class);
		if (ret != 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			goto error;
		}
	}

	ret = 0;
	goto end;

error:
	ret = -1;
end:
	return ret;

}

static
struct bt_ctf_stream_class *insert_new_stream_class(
		struct debug_info_iterator *debug_it,
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_stream_class *writer_stream_class = NULL;
	struct bt_ctf_trace *trace, *writer_trace = NULL;
	struct debug_info_trace *di_trace;
	enum bt_component_status ret;
	int int_ret;

	trace = bt_ctf_stream_class_get_trace(stream_class);
	if (!trace) {
		fprintf(debug_it->err,
				"[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	di_trace = lookup_trace(debug_it, trace);
	if (!di_trace) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto error;
	}
	writer_trace = di_trace->writer_trace;
	bt_get(writer_trace);

	writer_stream_class = copy_stream_class_debug_info(debug_it->err, stream_class,
			writer_trace, debug_it->debug_info_component);
	if (!writer_stream_class) {
		fprintf(debug_it->err, "[error] Failed to copy stream class\n");
		fprintf(debug_it->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	int_ret = bt_ctf_trace_add_stream_class(writer_trace, writer_stream_class);
	if (int_ret) {
		fprintf(debug_it->err,
				"[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret = add_clock_classes(debug_it->err, writer_trace,
			writer_stream_class, trace);
	if (ret != BT_COMPONENT_STATUS_OK) {
		fprintf(debug_it->err,
				"[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	g_hash_table_insert(di_trace->stream_class_map,
			(gpointer) stream_class, writer_stream_class);

	goto end;

error:
	BT_PUT(writer_stream_class);
end:
	bt_put(trace);
	bt_put(writer_trace);
	return writer_stream_class;
}

static
struct bt_ctf_stream *insert_new_stream(
		struct debug_info_iterator *debug_it,
		struct bt_ctf_stream *stream,
		struct debug_info_trace *di_trace)
{
	struct bt_ctf_stream *writer_stream = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_stream_class *writer_stream_class = NULL;

	stream_class = bt_ctf_stream_get_class(stream);
	if (!stream_class) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	writer_stream_class = g_hash_table_lookup(
			di_trace->stream_class_map,
			(gpointer) stream_class);

	if (!writer_stream_class) {
		writer_stream_class = insert_new_stream_class(debug_it,
				stream_class);
		if (!writer_stream_class) {
			fprintf(debug_it->err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
			goto error;
		}
	}
	bt_get(writer_stream_class);

	writer_stream = bt_ctf_stream_create(writer_stream_class,
			bt_ctf_stream_get_name(stream));
	if (!writer_stream) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	g_hash_table_insert(di_trace->stream_map, (gpointer) stream,
			writer_stream);

	goto end;

error:
	BT_PUT(writer_stream);
end:
	bt_put(stream_class);
	bt_put(writer_stream_class);
	return writer_stream;
}

static
struct bt_ctf_stream *lookup_stream(struct debug_info_iterator *debug_it,
		struct bt_ctf_stream *stream,
		struct debug_info_trace *di_trace)
{
	return (struct bt_ctf_stream *) g_hash_table_lookup(
			di_trace->stream_map, (gpointer) stream);
}

static
struct bt_ctf_event_class *get_event_class(struct debug_info_iterator *debug_it,
		struct bt_ctf_stream_class *writer_stream_class,
		struct bt_ctf_event_class *event_class)
{
	return bt_ctf_stream_class_get_event_class_by_id(writer_stream_class,
			bt_ctf_event_class_get_id(event_class));
}

static
struct debug_info_trace *lookup_di_trace_from_stream(
		struct debug_info_iterator *debug_it,
		struct bt_ctf_stream *stream)
{
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_trace *trace = NULL;
	struct debug_info_trace *di_trace = NULL;

	stream_class = bt_ctf_stream_get_class(stream);
	if (!stream_class) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end;
	}

	trace = bt_ctf_stream_class_get_trace(stream_class);
	if (!trace) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end;
	}

	di_trace = (struct debug_info_trace *) g_hash_table_lookup(
			debug_it->trace_map, (gpointer) trace);

end:
	BT_PUT(stream_class);
	BT_PUT(trace);
	return di_trace;
}

static
struct bt_ctf_stream *get_writer_stream(
		struct debug_info_iterator *debug_it,
		struct bt_ctf_packet *packet, struct bt_ctf_stream *stream)
{
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_stream *writer_stream = NULL;
	struct debug_info_trace *di_trace = NULL;

	stream_class = bt_ctf_stream_get_class(stream);
	if (!stream_class) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	di_trace = lookup_di_trace_from_stream(debug_it, stream);
	if (!di_trace) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	writer_stream = lookup_stream(debug_it, stream, di_trace);
	if (!writer_stream) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}
	bt_get(writer_stream);

	goto end;

error:
	BT_PUT(writer_stream);
end:
	bt_put(stream_class);
	return writer_stream;
}

BT_HIDDEN
struct bt_ctf_packet *debug_info_new_packet(
		struct debug_info_iterator *debug_it,
		struct bt_ctf_packet *packet)
{
	struct bt_ctf_stream *stream = NULL, *writer_stream = NULL;
	struct bt_ctf_field *writer_packet_context = NULL;
	struct bt_ctf_packet *writer_packet = NULL;
	struct debug_info_trace *di_trace;
	int int_ret;

	stream = bt_ctf_packet_get_stream(packet);
	if (!stream) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	writer_stream = get_writer_stream(debug_it, packet, stream);
	if (!writer_stream) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	di_trace = lookup_di_trace_from_stream(debug_it, stream);
	if (!di_trace) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	/*
	 * If a packet was already opened, close it and remove it from
	 * the HT.
	 */
	writer_packet = lookup_packet(debug_it, packet, di_trace);
	if (writer_packet) {
		g_hash_table_remove(di_trace->packet_map, packet);
		BT_PUT(writer_packet);
	}

	writer_packet = insert_new_packet(debug_it, packet, writer_stream,
			di_trace);
	if (!writer_packet) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	writer_packet_context = ctf_copy_packet_context(debug_it->err, packet,
			writer_stream);
	if (!writer_packet_context) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	int_ret = bt_ctf_packet_set_context(writer_packet, writer_packet_context);
	if (int_ret) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	bt_get(writer_packet);
	goto end;

error:

end:
	bt_put(writer_packet_context);
	bt_put(writer_stream);
	bt_put(stream);
	return writer_packet;
}

BT_HIDDEN
struct bt_ctf_packet *debug_info_close_packet(
		struct debug_info_iterator *debug_it,
		struct bt_ctf_packet *packet)
{
	struct bt_ctf_packet *writer_packet = NULL;
	struct bt_ctf_stream *stream = NULL;
	struct debug_info_trace *di_trace;

	stream = bt_ctf_packet_get_stream(packet);
	if (!stream) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}

	di_trace = lookup_di_trace_from_stream(debug_it, stream);
	if (!di_trace) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}

	writer_packet = lookup_packet(debug_it, packet, di_trace);
	if (!writer_packet) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end;
	}
	bt_get(writer_packet);
	g_hash_table_remove(di_trace->packet_map, packet);

end:
	bt_put(stream);
	return writer_packet;
}

BT_HIDDEN
struct bt_ctf_stream *debug_info_stream_begin(
		struct debug_info_iterator *debug_it,
		struct bt_ctf_stream *stream)
{
	struct bt_ctf_stream *writer_stream = NULL;
	enum debug_info_stream_state *state;
	struct debug_info_trace *di_trace = NULL;

	di_trace = lookup_di_trace_from_stream(debug_it, stream);
	if (!di_trace) {
		di_trace = insert_new_trace(debug_it, stream);
		if (!di_trace) {
			fprintf(debug_it->err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
			goto error;
		}
	}

	/* Set the stream as active */
	state = g_hash_table_lookup(di_trace->stream_states, stream);
	if (!state) {
		if (di_trace->trace_static) {
			fprintf(debug_it->err, "[error] Adding a new stream "
					"on a static trace\n");
			goto error;
		}
		state = insert_new_stream_state(debug_it, di_trace,
				stream);
		if (!state) {
			fprintf(debug_it->err, "[error] Adding a new stream "
					"on a static trace\n");
			goto error;
		}
	}
	if (*state != DEBUG_INFO_UNKNOWN_STREAM) {
		fprintf(debug_it->err, "[error] Unexpected stream state %d\n",
				*state);
		goto error;
	}
	*state = DEBUG_INFO_ACTIVE_STREAM;

	writer_stream = lookup_stream(debug_it, stream, di_trace);
	if (!writer_stream) {
		writer_stream = insert_new_stream(debug_it, stream, di_trace);
	}
	bt_get(writer_stream);

	goto end;

error:
	BT_PUT(writer_stream);
end:
	return writer_stream;
}

BT_HIDDEN
struct bt_ctf_stream *debug_info_stream_end(struct debug_info_iterator *debug_it,
		struct bt_ctf_stream *stream)
{
	struct bt_ctf_stream *writer_stream = NULL;
	struct debug_info_trace *di_trace = NULL;
	enum debug_info_stream_state *state;

	di_trace = lookup_di_trace_from_stream(debug_it, stream);
	if (!di_trace) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	writer_stream = lookup_stream(debug_it, stream, di_trace);
	if (!writer_stream) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}
	/*
	 * Take the ref on the stream and keep it until the notification
	 * is created.
	 */
	bt_get(writer_stream);

	state = g_hash_table_lookup(di_trace->stream_states, stream);
	if (*state != DEBUG_INFO_ACTIVE_STREAM) {
		fprintf(debug_it->err, "[error] Unexpected stream "
				"state %d\n", *state);
		goto error;
	}
	*state = DEBUG_INFO_COMPLETED_STREAM;

	g_hash_table_remove(di_trace->stream_map, stream);

	if (di_trace->trace_static) {
		int trace_completed = 1;

		g_hash_table_foreach(di_trace->stream_states,
				check_completed_trace, &trace_completed);
		if (trace_completed) {
			debug_info_close_trace(debug_it, di_trace);
			g_hash_table_remove(debug_it->trace_map,
					di_trace->trace);
		}
	}

	goto end;

error:
	BT_PUT(writer_stream);

end:
	return writer_stream;
}

static
struct debug_info_source *lookup_debug_info(FILE *err,
		struct bt_ctf_event *event,
		struct debug_info *debug_info)
{
	int64_t vpid;
	uint64_t ip;
	struct debug_info_source *dbg_info_src = NULL;
	int ret;

	ret = get_stream_event_context_int_field_value(err, event,
			"_vpid", &vpid);
	if (ret) {
		goto end;
	}

	ret = get_stream_event_context_unsigned_int_field_value(err, event,
			"_ip", &ip);
	if (ret) {
		goto end;
	}

	/* Get debug info for this context. */
	dbg_info_src = debug_info_query(debug_info, vpid, ip);

end:
	return dbg_info_src;
}

static
int set_debug_info_field(FILE *err, struct bt_ctf_field *debug_field,
		struct debug_info_source *dbg_info_src,
		struct debug_info_component *component)
{
	int i, nr_fields, ret = 0;
	struct bt_ctf_field_type *debug_field_type = NULL;
	struct bt_ctf_field *field = NULL;
	struct bt_ctf_field_type *field_type = NULL;

	debug_field_type = bt_ctf_field_get_type(debug_field);
	if (!debug_field_type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	nr_fields = bt_ctf_field_type_structure_get_field_count(debug_field_type);
	for (i = 0; i < nr_fields; i++) {
		const char *field_name;

		if (bt_ctf_field_type_structure_get_field(debug_field_type,
					&field_name, &field_type, i) < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		BT_PUT(field_type);

		field = bt_ctf_field_structure_get_field_by_index(debug_field, i);
		if (!strcmp(field_name, "bin")) {
			if (dbg_info_src && dbg_info_src->bin_path) {
				GString *tmp = g_string_new(NULL);

				if (component->arg_full_path) {
					g_string_printf(tmp, "%s%s",
							dbg_info_src->bin_path,
							dbg_info_src->bin_loc);
				} else {
					g_string_printf(tmp, "%s%s",
							dbg_info_src->short_bin_path,
							dbg_info_src->bin_loc);
				}
				ret = bt_ctf_field_string_set_value(field, tmp->str);
				g_string_free(tmp, true);
			} else {
				ret = bt_ctf_field_string_set_value(field, "");
			}
		} else if (!strcmp(field_name, "func")) {
			if (dbg_info_src && dbg_info_src->func) {
				ret = bt_ctf_field_string_set_value(field,
						dbg_info_src->func);
			} else {
				ret = bt_ctf_field_string_set_value(field, "");
			}
		} else if (!strcmp(field_name, "src")) {
			if (dbg_info_src && dbg_info_src->src_path) {
				GString *tmp = g_string_new(NULL);

				if (component->arg_full_path) {
					g_string_printf(tmp, "%s:%" PRId64,
							dbg_info_src->src_path,
							dbg_info_src->line_no);
				} else {
					g_string_printf(tmp, "%s:%" PRId64,
							dbg_info_src->short_src_path,
							dbg_info_src->line_no);
				}
				ret = bt_ctf_field_string_set_value(field, tmp->str);
				g_string_free(tmp, true);
			} else {
				ret = bt_ctf_field_string_set_value(field, "");
			}
		}
		BT_PUT(field);
		if (ret) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
	}
	ret = 0;
	goto end;

error:
	ret = -1;
end:
	bt_put(field_type);
	bt_put(field);
	bt_put(debug_field_type);
	return ret;
}

static
int copy_set_debug_info_stream_event_context(FILE *err,
		struct bt_ctf_field *event_context,
		struct bt_ctf_event *event,
		struct bt_ctf_event *writer_event,
		struct debug_info *debug_info,
		struct debug_info_component *component)
{
	struct bt_ctf_field_type *writer_event_context_type = NULL,
				 *event_context_type = NULL;
	struct bt_ctf_field *writer_event_context = NULL;
	struct bt_ctf_field *field = NULL, *copy_field = NULL, *debug_field = NULL;
	struct bt_ctf_field_type *field_type = NULL;
	struct debug_info_source *dbg_info_src;
	int ret, nr_fields, i;

	writer_event_context = bt_ctf_event_get_stream_event_context(writer_event);
	if (!writer_event_context) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__, __LINE__);
		goto error;
	}

	writer_event_context_type = bt_ctf_field_get_type(writer_event_context);
	if (!writer_event_context_type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	event_context_type = bt_ctf_field_get_type(event_context);
	if (!event_context_type) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	/*
	 * If it is not a structure, we did not modify it to add the debug info
	 * fields, so just assign it as is.
	 */
	if (bt_ctf_field_type_get_type_id(writer_event_context_type) != BT_CTF_FIELD_TYPE_ID_STRUCT) {
		ret = bt_ctf_event_set_event_context(writer_event, event_context);
		goto end;
	}

	dbg_info_src = lookup_debug_info(err, event, debug_info);

	nr_fields = bt_ctf_field_type_structure_get_field_count(writer_event_context_type);
	for (i = 0; i < nr_fields; i++) {
		const char *field_name;

		if (bt_ctf_field_type_structure_get_field(writer_event_context_type,
					&field_name, &field_type, i) < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}

		/*
		 * Prevent illegal access in the event_context.
		 */
		if (i < bt_ctf_field_type_structure_get_field_count(event_context_type)) {
			field = bt_ctf_field_structure_get_field_by_index(event_context, i);
		}
		/*
		 * The debug_info field, only exists in the writer event or
		 * if it was set by a earlier pass of the debug_info plugin.
		 */
		if (!strcmp(field_name, component->arg_debug_info_field_name) &&
				!field) {
			debug_field = bt_ctf_field_structure_get_field_by_index(
					writer_event_context, i);
			if (!debug_field) {
				fprintf(err, "[error] %s in %s:%d\n", __func__,
						__FILE__, __LINE__);
				goto error;
			}
			ret = set_debug_info_field(err, debug_field,
					dbg_info_src, component);
			if (ret) {
				fprintf(err, "[error] %s in %s:%d\n", __func__,
						__FILE__, __LINE__);
				goto error;
			}
			BT_PUT(debug_field);
		} else {
			copy_field = bt_ctf_field_copy(field);
			if (!copy_field) {
				fprintf(err, "[error] %s in %s:%d\n", __func__,
						__FILE__, __LINE__);
				goto error;
			}

			ret = bt_ctf_field_structure_set_field(writer_event_context,
					field_name, copy_field);
			if (ret) {
				fprintf(err, "[error] %s in %s:%d\n", __func__,
						__FILE__, __LINE__);
				goto error;
			}
			BT_PUT(copy_field);
		}
		BT_PUT(field_type);
		BT_PUT(field);
	}

	ret = 0;
	goto end;

error:
	ret = -1;
end:
	bt_put(event_context_type);
	bt_put(writer_event_context_type);
	bt_put(writer_event_context);
	bt_put(field);
	bt_put(copy_field);
	bt_put(debug_field);
	bt_put(field_type);
	return ret;
}

static
struct bt_ctf_clock_class *stream_class_get_clock_class(FILE *err,
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_trace *trace = NULL;
	struct bt_ctf_clock_class *clock_class = NULL;

	trace = bt_ctf_stream_class_get_trace(stream_class);
	if (!trace) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end;
	}

	/* FIXME multi-clock? */
	clock_class = bt_ctf_trace_get_clock_class_by_index(trace, 0);

	bt_put(trace);

end:
	return clock_class;
}

static
struct bt_ctf_clock_class *event_get_clock_class(FILE *err, struct bt_ctf_event *event)
{
	struct bt_ctf_event_class *event_class = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_clock_class *clock_class = NULL;

	event_class = bt_ctf_event_get_class(event);
	if (!event_class) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	stream_class = bt_ctf_event_class_get_stream_class(event_class);
	if (!stream_class) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	clock_class = stream_class_get_clock_class(err, stream_class);
	goto end;

error:
	BT_PUT(clock_class);
end:
	bt_put(stream_class);
	bt_put(event_class);
	return clock_class;
}

static
int set_event_clock_value(FILE *err, struct bt_ctf_event *event,
		struct bt_ctf_event *writer_event)
{
	struct bt_ctf_clock_class *clock_class = NULL;
	struct bt_ctf_clock_value *clock_value = NULL;
	int ret;

	clock_class = event_get_clock_class(err, event);
	if (!clock_class) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	clock_value = bt_ctf_event_get_clock_value(event, clock_class);
	if (!clock_value) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	/*
	 * We share the same clocks, so we can assign the clock value to the
	 * writer event.
	 */
	ret = bt_ctf_event_set_clock_value(writer_event, clock_value);
	if (ret) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret = 0;
	goto end;

error:
	ret = -1;
end:
	bt_put(clock_class);
	bt_put(clock_value);
	return ret;
}

static
struct bt_ctf_event *debug_info_copy_event(FILE *err, struct bt_ctf_event *event,
		struct bt_ctf_event_class *writer_event_class,
		struct debug_info *debug_info,
		struct debug_info_component *component)
{
	struct bt_ctf_event *writer_event = NULL;
	struct bt_ctf_field *field = NULL, *copy_field = NULL;
	int ret;

	writer_event = bt_ctf_event_create(writer_event_class);
	if (!writer_event) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret = set_event_clock_value(err, event, writer_event);
	if (ret) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	/* Optional field, so it can fail silently. */
	field = bt_ctf_event_get_header(event);
	if (field) {
		ret = ctf_copy_event_header(err, event, writer_event_class,
				writer_event, field);
		if (ret) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		BT_PUT(field);
	}

	/* Optional field, so it can fail silently. */
	field = bt_ctf_event_get_stream_event_context(event);
	if (field) {
		ret = copy_set_debug_info_stream_event_context(err,
				field, event, writer_event, debug_info,
				component);
		if (ret < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		BT_PUT(field);
	}

	/* Optional field, so it can fail silently. */
	field = bt_ctf_event_get_event_context(event);
	if (field) {
		copy_field = bt_ctf_field_copy(field);
		if (!copy_field) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		ret = bt_ctf_event_set_event_context(writer_event, copy_field);
		if (ret < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		BT_PUT(copy_field);
		BT_PUT(field);
	}

	field = bt_ctf_event_get_event_payload(event);
	if (!field) {
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}
	copy_field = bt_ctf_field_copy(field);
	if (copy_field) {
		ret = bt_ctf_event_set_event_payload(writer_event, copy_field);
		if (ret < 0) {
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		BT_PUT(copy_field);
	}
	BT_PUT(field);

	goto end;

error:
	BT_PUT(writer_event);
end:
	bt_put(copy_field);
	bt_put(field);
	return writer_event;
}

BT_HIDDEN
struct bt_ctf_event *debug_info_output_event(
		struct debug_info_iterator *debug_it,
		struct bt_ctf_event *event)
{
	struct bt_ctf_event_class *event_class = NULL, *writer_event_class = NULL;
	struct bt_ctf_stream_class *stream_class = NULL, *writer_stream_class = NULL;
	struct bt_ctf_event *writer_event = NULL;
	struct bt_ctf_packet *packet = NULL, *writer_packet = NULL;
	struct bt_ctf_trace *writer_trace = NULL;
	struct bt_ctf_stream *stream = NULL;
	struct debug_info_trace *di_trace;
	struct debug_info *debug_info;
	const char *event_name;
	int int_ret;

	event_class = bt_ctf_event_get_class(event);
	if (!event_class) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	event_name = bt_ctf_event_class_get_name(event_class);
	if (!event_name) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	stream_class = bt_ctf_event_class_get_stream_class(event_class);
	if (!stream_class) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}
	stream = bt_ctf_event_get_stream(event);
	if (!stream) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}
	di_trace = lookup_di_trace_from_stream(debug_it, stream);
	if (!di_trace) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	writer_stream_class = g_hash_table_lookup(
			di_trace->stream_class_map,
			(gpointer) stream_class);
	if (!writer_stream_class) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}
	bt_get(writer_stream_class);

	writer_event_class = get_event_class(debug_it,
			writer_stream_class, event_class);
	if (!writer_event_class) {
		writer_event_class = ctf_copy_event_class(debug_it->err,
				event_class);
		if (!writer_event_class) {
			fprintf(debug_it->err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
			goto error;
		}
		int_ret = bt_ctf_stream_class_add_event_class(
				writer_stream_class, writer_event_class);
		if (int_ret) {
			fprintf(debug_it->err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
			goto error;
		}
	}

	writer_trace = bt_ctf_stream_class_get_trace(writer_stream_class);
	if (!writer_trace) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	debug_info = get_trace_debug_info(debug_it, writer_trace, di_trace);
	if (debug_info) {
		debug_info_handle_event(debug_it->err, event, debug_info);
	}

	writer_event = debug_info_copy_event(debug_it->err, event,
			writer_event_class, debug_info,
			debug_it->debug_info_component);
	if (!writer_event) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		fprintf(debug_it->err, "[error] Failed to copy event %s\n",
				bt_ctf_event_class_get_name(writer_event_class));
		goto error;
	}

	packet = bt_ctf_event_get_packet(event);
	if (!packet) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	writer_packet = lookup_packet(debug_it, packet, di_trace);
	if (!writer_packet) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}
	bt_get(writer_packet);

	int_ret = bt_ctf_event_set_packet(writer_event, writer_packet);
	if (int_ret < 0) {
		fprintf(debug_it->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		fprintf(debug_it->err, "[error] Failed to append event %s\n",
				bt_ctf_event_class_get_name(writer_event_class));
		goto error;
	}

	/* Keep the reference on the writer event */
	goto end;

error:
	BT_PUT(writer_event);

end:
	bt_put(stream);
	bt_put(writer_trace);
	bt_put(writer_packet);
	bt_put(packet);
	bt_put(writer_event_class);
	bt_put(writer_stream_class);
	bt_put(stream_class);
	bt_put(event_class);
	return writer_event;
}
