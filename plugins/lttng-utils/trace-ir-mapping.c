/*
 * Babeltrace - Mapping of IR metadata and data object between input and output
 *		trace
 *
 * Copyright (c) 2015 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright (c) 2019 Francis Deslauriers <francis.deslauriers@efficios.com>
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

#define BT_LOG_TAG "PLUGIN-LTTNG-UTILS-DEBUG-INFO-TRACE-IR-MAPPING"
#include "logging.h"

#include <stdbool.h>

#include <babeltrace/assert-internal.h>
#include <babeltrace/babeltrace.h>
/* For bt_property_availability */
#include <babeltrace/property.h>

#include "debug-info.h"
#include "trace-ir-data-copy.h"
#include "trace-ir-mapping.h"
#include "trace-ir-metadata-copy.h"

static
bt_trace_class *create_new_mapped_trace_class(struct trace_ir_maps *ir_maps,
		const bt_trace_class *in_trace_class)
{
	int ret;
	bt_trace_class *out_trace_class;

	BT_LOGD("Creating new mapped trace class: in-tc-addr=%p", in_trace_class);

	BT_ASSERT(ir_maps);
	BT_ASSERT(in_trace_class);

	/* Create the ouput trace class. */
	out_trace_class = bt_trace_class_create(ir_maps->self_comp);
	if (!out_trace_class) {
		BT_LOGE_STR("Error create output trace class");
		goto end;
	}

	/* If not, create a new one and add it to the mapping. */
	ret = copy_trace_class_content(in_trace_class, out_trace_class);
	if (ret) {
		BT_LOGE_STR("Error copy content to output trace class");
		out_trace_class = NULL;
		goto end;
	}

	BT_LOGD("Created new mapped trace class: in-tc-addr=%p, out-tc-addr=%p",
			in_trace_class, out_trace_class);

end:
	return out_trace_class;
}

static
bt_trace *create_new_mapped_trace(struct trace_ir_maps *ir_maps,
		const bt_trace *in_trace)
{
	bt_trace *out_trace;
	const bt_trace_class *in_trace_class;
	struct trace_ir_metadata_maps *metadata_maps;

	BT_LOGD("Creating new mapped trace: in-t-addr=%p", in_trace);
	BT_ASSERT(ir_maps);
	BT_ASSERT(in_trace);

	in_trace_class = bt_trace_borrow_class_const(in_trace);
	metadata_maps = borrow_metadata_maps_from_input_trace_class(ir_maps,
			in_trace_class);

	if (!metadata_maps->output_trace_class) {
		metadata_maps->output_trace_class =
			create_new_mapped_trace_class(ir_maps, in_trace_class);
		if (!metadata_maps->output_trace_class) {
			out_trace = NULL;
			goto end;
		}
	}

	out_trace = bt_trace_create(metadata_maps->output_trace_class);
	if (!out_trace) {
		BT_LOGE_STR("Error create output trace");
		goto end;
	}

	/* If not, create a new one and add it to the mapping. */
	copy_trace_content(in_trace, out_trace);

	BT_LOGD("Created new mapped trace: in-t-addr=%p, out-t-addr=%p",
			in_trace, out_trace);
end:
	return out_trace;
}

static
bt_stream_class *borrow_mapped_stream_class(struct trace_ir_metadata_maps *md_maps,
		const bt_stream_class *in_stream_class)
{
	BT_ASSERT(md_maps);
	BT_ASSERT(in_stream_class);

	return g_hash_table_lookup(md_maps->stream_class_map,
			(gpointer) in_stream_class);
}

static
bt_stream_class *create_new_mapped_stream_class(struct trace_ir_maps *ir_maps,
		const bt_stream_class *in_stream_class)
{
	int ret;
	bt_stream_class *out_stream_class;
	struct trace_ir_metadata_maps *md_maps;

	BT_LOGD("Creating new mapped stream class: in-sc-addr=%p",
			in_stream_class);

	md_maps = borrow_metadata_maps_from_input_stream_class(ir_maps,
			in_stream_class);

	BT_ASSERT(md_maps);
	BT_ASSERT(in_stream_class);
	BT_ASSERT(!borrow_mapped_stream_class(md_maps, in_stream_class));

	/* Create an out_stream_class. */
	out_stream_class = bt_stream_class_create_with_id(
			md_maps->output_trace_class,
			bt_stream_class_get_id(in_stream_class));
	if (!out_stream_class) {
		BT_LOGE_STR("Error create output stream class");
		goto end;
	}

	/* If not, create a new one and add it to the mapping. */
	ret = copy_stream_class_content(ir_maps, in_stream_class,
			out_stream_class);
	if (ret) {
		BT_LOGE_STR("Error copy content to output stream class");
		out_stream_class = NULL;
		goto end;
	}

	g_hash_table_insert(md_maps->stream_class_map,
			(gpointer) in_stream_class, out_stream_class);

	BT_LOGD("Created new mapped stream class: in-sc-addr=%p, out-sc-addr=%p",
			in_stream_class, out_stream_class);

end:
	return out_stream_class;
}

static
bt_stream *borrow_mapped_stream(struct trace_ir_data_maps *d_maps,
		const bt_stream *in_stream)
{
	BT_ASSERT(d_maps);
	BT_ASSERT(in_stream);

	return g_hash_table_lookup(d_maps->stream_map, (gpointer) in_stream);
}

BT_HIDDEN
bt_stream *trace_ir_mapping_create_new_mapped_stream(
		struct trace_ir_maps *ir_maps,
		const bt_stream *in_stream)
{
	struct trace_ir_data_maps *d_maps;
	struct trace_ir_metadata_maps *md_maps;
	const bt_stream_class *in_stream_class;
	const bt_trace *in_trace;
	bt_stream_class *out_stream_class;
	bt_stream *out_stream = NULL;

	BT_LOGD("Creating new mapped stream: in-s-addr=%p", in_stream);

	BT_ASSERT(ir_maps);
	BT_ASSERT(in_stream);

	in_trace = bt_stream_borrow_trace_const(in_stream);

	d_maps = borrow_data_maps_from_input_trace(ir_maps, in_trace);
	if (!d_maps->output_trace) {
		d_maps->output_trace = create_new_mapped_trace(ir_maps, in_trace);
		if (!d_maps->output_trace) {
			goto end;
		}
	}

	BT_ASSERT(d_maps->output_trace);
	BT_ASSERT(!borrow_mapped_stream(d_maps, in_stream));

	in_stream_class = bt_stream_borrow_class_const(in_stream);
	if (bt_stream_class_default_clock_is_always_known(in_stream_class)
			== BT_FALSE) {
		BT_LOGE("Stream class default clock class is not always "
			"known: in-sc-addr=%p", in_stream_class);
		goto end;
	}

	md_maps = borrow_metadata_maps_from_input_stream_class(ir_maps, in_stream_class);
	out_stream_class = borrow_mapped_stream_class(md_maps, in_stream_class);
	if (!out_stream_class) {
		out_stream_class = create_new_mapped_stream_class(ir_maps,
				in_stream_class);
		if (!out_stream_class) {
			goto end;
		}
	}
	BT_ASSERT(out_stream_class);

	out_stream = bt_stream_create_with_id(out_stream_class,
			d_maps->output_trace, bt_stream_get_id(in_stream));
	if (!out_stream) {
		BT_LOGE_STR("Error creating output stream");
		goto end;
	}
	/*
	 * Release our ref since the trace object will be managing the life
	 * time of the stream objects.
	 */

	copy_stream_content(in_stream, out_stream);

	g_hash_table_insert(d_maps->stream_map, (gpointer) in_stream,
			out_stream);

	BT_LOGD("Created new mapped stream: in-s-addr=%p, out-s-addr=%p",
			in_stream, out_stream);

end:
	return out_stream;
}

BT_HIDDEN
bt_stream *trace_ir_mapping_borrow_mapped_stream(struct trace_ir_maps *ir_maps,
		const bt_stream *in_stream)
{
	BT_ASSERT(ir_maps);
	BT_ASSERT(in_stream);
	struct trace_ir_data_maps *d_maps;

	d_maps = borrow_data_maps_from_input_stream(ir_maps, in_stream);
	/* Return the mapped stream. */
	return borrow_mapped_stream(d_maps, in_stream);
}

static inline
bt_event_class *borrow_mapped_event_class(struct trace_ir_metadata_maps *md_maps,
		const bt_event_class *in_event_class)
{
	return g_hash_table_lookup(md_maps->event_class_map,
			(gpointer) in_event_class);
}

BT_HIDDEN
bt_event_class *trace_ir_mapping_create_new_mapped_event_class(
		struct trace_ir_maps *ir_maps,
		const bt_event_class *in_event_class)
{
	bt_event_class *out_event_class;
	const bt_trace_class *in_trace_class;
	const bt_stream_class *in_stream_class;
	bt_stream_class *out_stream_class;
	struct trace_ir_metadata_maps *md_maps;
	int ret;

	BT_LOGD("Creating new mapped event class: in-ec-addr=%p",
			in_event_class);

	BT_ASSERT(ir_maps);
	BT_ASSERT(in_event_class);

	in_trace_class = bt_stream_class_borrow_trace_class_const(
				bt_event_class_borrow_stream_class_const(
					in_event_class));

	md_maps = borrow_metadata_maps_from_input_trace_class(ir_maps, in_trace_class);

	BT_ASSERT(!borrow_mapped_event_class(md_maps, in_event_class));

	in_stream_class =
		bt_event_class_borrow_stream_class_const(in_event_class);
	BT_ASSERT(in_stream_class);

	/* Get the right output stream class to add the new event class to. */
	out_stream_class = borrow_mapped_stream_class(md_maps, in_stream_class);
	BT_ASSERT(out_stream_class);

	/* Create an output event class. */
	out_event_class = bt_event_class_create_with_id(out_stream_class,
			bt_event_class_get_id(in_event_class));
	if (!out_event_class) {
		BT_LOGE_STR("Error creating output event class");
		goto end;
	}

	/* If not, create a new one and add it to the mapping. */
	ret = copy_event_class_content(ir_maps, in_event_class,
			out_event_class);
	if (ret) {
		BT_LOGE_STR("Error copy content to output event class");
		out_event_class = NULL;
		goto end;
	}

	g_hash_table_insert(md_maps->event_class_map,
			(gpointer) in_event_class, out_event_class);

	BT_LOGD("Created new mapped event class: in-ec-addr=%p, out-ec-addr=%p",
			in_event_class, out_event_class);

end:
	return out_event_class;
}

BT_HIDDEN
bt_event_class *trace_ir_mapping_borrow_mapped_event_class(
		struct trace_ir_maps *ir_maps,
		const bt_event_class *in_event_class)
{
	struct trace_ir_metadata_maps *md_maps;

	BT_ASSERT(ir_maps);
	BT_ASSERT(in_event_class);

	md_maps = borrow_metadata_maps_from_input_event_class(ir_maps, in_event_class);

	/* Return the mapped event_class. */
	return borrow_mapped_event_class(md_maps, in_event_class);
}

static inline
bt_packet *borrow_mapped_packet(struct trace_ir_data_maps *d_maps,
		const bt_packet *in_packet)
{
	BT_ASSERT(d_maps);
	BT_ASSERT(in_packet);

	return g_hash_table_lookup(d_maps->packet_map,
			(gpointer) in_packet);
}

BT_HIDDEN
bt_packet *trace_ir_mapping_create_new_mapped_packet(
		struct trace_ir_maps *ir_maps,
		const bt_packet *in_packet)
{
	struct trace_ir_data_maps *d_maps;
	const bt_trace *in_trace;
	const bt_stream *in_stream;
	bt_packet *out_packet;
	bt_stream *out_stream;

	BT_LOGD("Creating new mapped packet: in-p-addr=%p", in_packet);

	in_stream = bt_packet_borrow_stream_const(in_packet);
	in_trace = bt_stream_borrow_trace_const(in_stream);
	d_maps = borrow_data_maps_from_input_trace(ir_maps, in_trace);

	/* There should never be a mapped packet. */
	BT_ASSERT(!borrow_mapped_packet(d_maps, in_packet));

	BT_ASSERT(in_stream);

	/* Get output stream corresponding to this input stream. */
	out_stream = borrow_mapped_stream(d_maps, in_stream);
	BT_ASSERT(out_stream);

	/* Create the output packet. */
	out_packet = bt_packet_create(out_stream);
	if (!out_packet) {
		BT_LOGE_STR("Error create output packet");
		goto end;
	}

	/*
	 * Release our ref since the stream object will be managing the life
	 * time of the packet objects.
	 */
	copy_packet_content(in_packet, out_packet);

	g_hash_table_insert(d_maps->packet_map,
			(gpointer) in_packet, out_packet);

	BT_LOGD("Created new mapped packet: in-p-addr=%p, out-p-addr=%p",
			in_packet, out_packet);

end:
	return out_packet;
}

BT_HIDDEN
bt_packet *trace_ir_mapping_borrow_mapped_packet(struct trace_ir_maps *ir_maps,
		const bt_packet *in_packet)
{
	struct trace_ir_data_maps *d_maps;
	BT_ASSERT(ir_maps);
	BT_ASSERT(in_packet);

	d_maps = borrow_data_maps_from_input_packet(ir_maps, in_packet);

	return borrow_mapped_packet(d_maps, in_packet);
}

BT_HIDDEN
void trace_ir_mapping_remove_mapped_packet(struct trace_ir_maps *ir_maps,
		const bt_packet *in_packet)
{
	gboolean ret;

	struct trace_ir_data_maps *d_maps;
	BT_ASSERT(ir_maps);
	BT_ASSERT(in_packet);

	d_maps = borrow_data_maps_from_input_packet(ir_maps, in_packet);

	ret = g_hash_table_remove(d_maps->packet_map, in_packet);

	BT_ASSERT(ret);
}

BT_HIDDEN
void trace_ir_mapping_remove_mapped_stream(struct trace_ir_maps *ir_maps,
		const bt_stream *in_stream)
{
	gboolean ret;
	struct trace_ir_data_maps *d_maps;

	BT_ASSERT(ir_maps);
	BT_ASSERT(in_stream);

	d_maps = borrow_data_maps_from_input_stream(ir_maps, in_stream);

	ret = g_hash_table_remove(d_maps->stream_map, in_stream);

	BT_ASSERT(ret);
}

static
void trace_ir_metadata_maps_remove_func(const bt_trace_class *in_trace_class,
		void *data)
{
	struct trace_ir_maps *maps = (struct trace_ir_maps *) data;
	if (maps->metadata_maps) {
		gboolean ret;
		ret = g_hash_table_remove(maps->metadata_maps,
				(gpointer) in_trace_class);
		BT_ASSERT(ret);
	}
}

static
void trace_ir_data_maps_remove_func(const bt_trace *in_trace, void *data)
{
	struct trace_ir_maps *maps = (struct trace_ir_maps *) data;
	if (maps->data_maps) {
		gboolean ret;
		ret = g_hash_table_remove(maps->data_maps, (gpointer) in_trace);
		BT_ASSERT(ret);
	}
}

struct trace_ir_data_maps *trace_ir_data_maps_create(struct trace_ir_maps *ir_maps,
		const bt_trace *in_trace)
{
	struct trace_ir_data_maps *d_maps =
		g_new0(struct trace_ir_data_maps, 1);
	if (!d_maps) {
		BT_LOGE_STR("Error allocating trace_ir_maps");
		goto error;
	}

	d_maps->input_trace = in_trace;

	/* Create the hashtables used to map data objects. */
	d_maps->stream_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL,(GDestroyNotify) bt_stream_put_ref);
	d_maps->packet_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL,(GDestroyNotify) bt_packet_put_ref);

	bt_trace_add_destruction_listener(in_trace, trace_ir_data_maps_remove_func,
			ir_maps, &d_maps->destruction_listener_id);
error:
	return d_maps;
}

struct trace_ir_metadata_maps *trace_ir_metadata_maps_create(
		struct trace_ir_maps *ir_maps,
		const bt_trace_class *in_trace_class)
{
	struct trace_ir_metadata_maps *md_maps =
		g_new0(struct trace_ir_metadata_maps, 1);
	if (!md_maps) {
		BT_LOGE_STR("Error allocating trace_ir_maps");
		goto error;
	}

	md_maps->input_trace_class = in_trace_class;
	/*
	 * Create the field class resolving context. This is needed to keep
	 * track of the field class already copied in order to do the field
	 * path resolution correctly.
	 */
	md_maps->fc_resolving_ctx =
		g_new0(struct field_class_resolving_context, 1);
	if (!md_maps->fc_resolving_ctx) {
		BT_LOGE_STR("Error allocating field_class_resolving_context");
		goto error;
	}

	/* Create the hashtables used to map metadata objects. */
	md_maps->stream_class_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) bt_stream_class_put_ref);
	md_maps->event_class_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) bt_event_class_put_ref);
	md_maps->field_class_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) bt_field_class_put_ref);
	md_maps->clock_class_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) bt_clock_class_put_ref);

	bt_trace_class_add_destruction_listener(in_trace_class,
			trace_ir_metadata_maps_remove_func,
			ir_maps, &md_maps->destruction_listener_id);
error:
	return md_maps;
}

BT_HIDDEN
void trace_ir_data_maps_destroy(struct trace_ir_data_maps *maps)
{
	bt_trace_status status;
	if (!maps) {
		return;
	}

	if (maps->packet_map) {
		g_hash_table_destroy(maps->packet_map);
	}

	if (maps->stream_map) {
		g_hash_table_destroy(maps->stream_map);
	}

	if (maps->output_trace) {
		bt_trace_put_ref(maps->output_trace);
	}

	status = bt_trace_remove_destruction_listener(maps->input_trace,
			maps->destruction_listener_id);
	if (status != BT_TRACE_STATUS_OK) {
		BT_LOGD("Trace destruction listener removal failed.");
	}

	g_free(maps);
}

BT_HIDDEN
void trace_ir_metadata_maps_destroy(struct trace_ir_metadata_maps *maps)
{
	bt_trace_class_status status;
	if (!maps) {
		return;
	}

	if (maps->stream_class_map) {
		g_hash_table_destroy(maps->stream_class_map);
	}

	if (maps->event_class_map) {
		g_hash_table_destroy(maps->event_class_map);
	}

	if (maps->field_class_map) {
		g_hash_table_destroy(maps->field_class_map);
	}

	if (maps->clock_class_map) {
		g_hash_table_destroy(maps->clock_class_map);
	}

	if (maps->fc_resolving_ctx) {
		g_free(maps->fc_resolving_ctx);
	}

	if (maps->output_trace_class) {
		bt_trace_class_put_ref(maps->output_trace_class);
	}

	status = bt_trace_class_remove_destruction_listener(maps->input_trace_class,
			maps->destruction_listener_id);
	if (status != BT_TRACE_CLASS_STATUS_OK) {
		BT_LOGD("Trace destruction listener removal failed.");
	}

	g_free(maps);
}

void trace_ir_maps_clear(struct trace_ir_maps *maps)
{
	if (maps->data_maps) {
		g_hash_table_remove_all(maps->data_maps);
	}

	if (maps->metadata_maps) {
		g_hash_table_remove_all(maps->metadata_maps);
	}
}

BT_HIDDEN
void trace_ir_maps_destroy(struct trace_ir_maps *maps)
{
	if (!maps) {
		return;
	}

	if (maps->debug_info_field_class_name) {
		g_free(maps->debug_info_field_class_name);
	}

	if (maps->data_maps) {
		g_hash_table_destroy(maps->data_maps);
		maps->data_maps = NULL;
	}

	if (maps->metadata_maps) {
		g_hash_table_destroy(maps->metadata_maps);
		maps->metadata_maps = NULL;
	}

	g_free(maps);
}

BT_HIDDEN
struct trace_ir_maps *trace_ir_maps_create(bt_self_component *self_comp,
		const char *debug_info_field_name)
{
	struct trace_ir_maps *trace_ir_maps =
		g_new0(struct trace_ir_maps, 1);
	if (!trace_ir_maps) {
		BT_LOGE_STR("Error allocating trace_ir_maps");
		goto error;
	}

	/* Copy debug info field name received from the user. */
	trace_ir_maps->debug_info_field_class_name =
		g_strdup(debug_info_field_name);
	if (!trace_ir_maps->debug_info_field_class_name) {
		BT_LOGE_STR("Cannot copy debug info field name");
		goto error;
	}

	trace_ir_maps->self_comp = self_comp;

	trace_ir_maps->data_maps = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, (GDestroyNotify) NULL,
			(GDestroyNotify) trace_ir_data_maps_destroy);

	trace_ir_maps->metadata_maps = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, (GDestroyNotify) NULL,
			(GDestroyNotify) trace_ir_metadata_maps_destroy);

	goto end;
error:
	trace_ir_maps_destroy(trace_ir_maps);
	trace_ir_maps = NULL;
end:
	return trace_ir_maps;
}
