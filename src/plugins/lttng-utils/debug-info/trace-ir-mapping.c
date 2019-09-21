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

#define BT_COMP_LOG_SELF_COMP (ir_maps->self_comp)
#define BT_LOG_OUTPUT_LEVEL (ir_maps->log_level)
#define BT_LOG_TAG "PLUGIN/FLT.LTTNG-UTILS.DEBUG-INFO/TRACE-IR-MAPPING"
#include "logging/comp-logging.h"

#include "common/assert.h"
#include <babeltrace2/babeltrace.h>

#include "debug-info.h"
#include "trace-ir-data-copy.h"
#include "trace-ir-mapping.h"
#include "trace-ir-metadata-copy.h"

static
bt_trace_class *create_new_mapped_trace_class(struct trace_ir_maps *ir_maps,
		const bt_trace_class *in_trace_class)
{
	bt_self_component *self_comp = ir_maps->self_comp;
	enum debug_info_trace_ir_mapping_status status;
	struct trace_ir_metadata_maps *metadata_maps;

	BT_COMP_LOGD("Creating new mapped trace class: in-tc-addr=%p", in_trace_class);

	BT_ASSERT(ir_maps);
	BT_ASSERT(in_trace_class);

	metadata_maps = borrow_metadata_maps_from_input_trace_class(ir_maps,
		in_trace_class);

	BT_ASSERT(!metadata_maps->output_trace_class);

	/* Create the ouput trace class. */
	metadata_maps->output_trace_class  =
		bt_trace_class_create(ir_maps->self_comp);
	if (!metadata_maps->output_trace_class) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error create output trace class");
		goto error;
	}

	/* Copy the content over and add to the mapping. */
	status = copy_trace_class_content(ir_maps, in_trace_class,
		metadata_maps->output_trace_class, ir_maps->log_level,
		ir_maps->self_comp);
	if (status != DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error copy content to output trace class"
			"in-tc-addr=%p, out-tc-addr=%p", in_trace_class,
			metadata_maps->output_trace_class);
		goto error;
	}

	BT_COMP_LOGD("Created new mapped trace class: "
		"in-tc-addr=%p, out-tc-addr=%p",
		in_trace_class, metadata_maps->output_trace_class);

	goto end;
error:
	BT_TRACE_CLASS_PUT_REF_AND_RESET(metadata_maps->output_trace_class);
end:
	return metadata_maps->output_trace_class;
}

static
bt_trace *create_new_mapped_trace(struct trace_ir_maps *ir_maps,
		const bt_trace *in_trace)
{
	bt_self_component *self_comp = ir_maps->self_comp;
	enum debug_info_trace_ir_mapping_status status;
	struct trace_ir_metadata_maps *metadata_maps;
	const bt_trace_class *in_trace_class;
	bt_trace *out_trace;

	BT_COMP_LOGD("Creating new mapped trace: in-t-addr=%p", in_trace);
	BT_ASSERT(ir_maps);
	BT_ASSERT(in_trace);

	in_trace_class = bt_trace_borrow_class_const(in_trace);
	metadata_maps = borrow_metadata_maps_from_input_trace_class(ir_maps,
		in_trace_class);

	if (!metadata_maps->output_trace_class) {
		/*
		 * If there is no output trace class yet, create a one and add
		 * it to the mapping.
		 */
		metadata_maps->output_trace_class =
			create_new_mapped_trace_class(ir_maps, in_trace_class);
		if (!metadata_maps->output_trace_class) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Error create output trace class");
			out_trace = NULL;
			goto end;
		}
	}

	/* Create the output trace from the output trace class. */
	out_trace = bt_trace_create(metadata_maps->output_trace_class);
	if (!out_trace) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error create output trace");
		goto end;
	}

	/* Copy the content over to the output trace. */
	status = copy_trace_content(in_trace, out_trace, ir_maps->log_level,
		ir_maps->self_comp);
	if (status != DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error copy content to output trace"
			"in-t-addr=%p, out-t-addr=%p", in_trace, out_trace);
		goto error;
	}

	BT_COMP_LOGD("Created new mapped trace: in-t-addr=%p, out-t-addr=%p",
		in_trace, out_trace);
	goto end;

error:
	BT_TRACE_PUT_REF_AND_RESET(out_trace);
end:
	return out_trace;
}

BT_HIDDEN
bt_stream_class *trace_ir_mapping_borrow_mapped_stream_class(
		struct trace_ir_maps *ir_maps,
		const bt_stream_class *in_stream_class)
{
	BT_ASSERT_DBG(ir_maps);
	BT_ASSERT_DBG(in_stream_class);

	struct trace_ir_metadata_maps *md_maps =
		borrow_metadata_maps_from_input_stream_class(ir_maps,
			in_stream_class);
	return g_hash_table_lookup(md_maps->stream_class_map,
		(gpointer) in_stream_class);
}

BT_HIDDEN
bt_stream_class *trace_ir_mapping_create_new_mapped_stream_class(
		struct trace_ir_maps *ir_maps,
		const bt_stream_class *in_stream_class)
{
	bt_self_component *self_comp = ir_maps->self_comp;
	enum debug_info_trace_ir_mapping_status status;
	struct trace_ir_metadata_maps *md_maps;
	bt_stream_class *out_stream_class;

	BT_COMP_LOGD("Creating new mapped stream class: in-sc-addr=%p",
		in_stream_class);

	BT_ASSERT(ir_maps);
	BT_ASSERT(in_stream_class);
	BT_ASSERT(!trace_ir_mapping_borrow_mapped_stream_class(ir_maps,
		in_stream_class));

	md_maps = borrow_metadata_maps_from_input_stream_class(ir_maps,
		in_stream_class);

	BT_ASSERT(md_maps);

	/* Create the output stream class. */
	out_stream_class = bt_stream_class_create_with_id(
		md_maps->output_trace_class,
		bt_stream_class_get_id(in_stream_class));
	if (!out_stream_class) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error create output stream class");
		goto end;
	}

	/* Add it to the mapping. The mapping now owns out_stream_class. */
	g_hash_table_insert(md_maps->stream_class_map,
		(gpointer) in_stream_class, out_stream_class);

	/* Copy the content over to the output stream class. */
	status = copy_stream_class_content(ir_maps, in_stream_class,
		out_stream_class);
	if (status != DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error copy content to output stream class: "
			"in-sc-addr=%p, out-sc-addr=%p", in_stream_class,
			out_stream_class);
		goto error;
	}

	BT_COMP_LOGD("Created new mapped stream class: "
		"in-sc-addr=%p, out-sc-addr=%p",
		in_stream_class, out_stream_class);

	goto end;
error:
	out_stream_class = NULL;
end:
	return out_stream_class;
}

static
bt_stream *borrow_mapped_stream(struct trace_ir_data_maps *d_maps,
		const bt_stream *in_stream)
{
	BT_ASSERT_DBG(d_maps);
	BT_ASSERT_DBG(in_stream);

	return g_hash_table_lookup(d_maps->stream_map, (gpointer) in_stream);
}

BT_HIDDEN
bt_stream *trace_ir_mapping_create_new_mapped_stream(
		struct trace_ir_maps *ir_maps, const bt_stream *in_stream)
{
	bt_self_component *self_comp = ir_maps->self_comp;
	enum debug_info_trace_ir_mapping_status status;
	struct trace_ir_data_maps *d_maps;
	const bt_stream_class *in_stream_class;
	const bt_trace *in_trace;
	bt_stream_class *out_stream_class;
	bt_stream *out_stream = NULL;

	BT_ASSERT(ir_maps);
	BT_ASSERT(in_stream);
	BT_COMP_LOGD("Creating new mapped stream: in-s-addr=%p", in_stream);

	in_trace = bt_stream_borrow_trace_const(in_stream);

	d_maps = borrow_data_maps_from_input_trace(ir_maps, in_trace);
	if (!d_maps->output_trace) {
		/* Create the output trace for this input trace. */
		d_maps->output_trace = create_new_mapped_trace(ir_maps, in_trace);
		if (!d_maps->output_trace) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Error creating mapped trace");
			goto error;
		}
	}

	BT_ASSERT(d_maps->output_trace);
	BT_ASSERT(!borrow_mapped_stream(d_maps, in_stream));

	in_stream_class = bt_stream_borrow_class_const(in_stream);
	out_stream_class = trace_ir_mapping_borrow_mapped_stream_class(ir_maps,
		in_stream_class);

	if (!out_stream_class) {
		/* Create the output stream class for this input stream class. */
		out_stream_class = trace_ir_mapping_create_new_mapped_stream_class(
			ir_maps, in_stream_class);
		if (!out_stream_class) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Error creating mapped stream class");
			goto error;
		}
	}
	BT_ASSERT(out_stream_class);

	/* Create the output stream for this input stream. */
	out_stream = bt_stream_create_with_id(out_stream_class,
		d_maps->output_trace, bt_stream_get_id(in_stream));
	if (!out_stream) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error creating output stream");
		goto error;
	}

	/* Add it to the mapping. The mapping now owns out_stream.*/
	g_hash_table_insert(d_maps->stream_map, (gpointer) in_stream,
		out_stream);

	/* Copy the content over to the output stream. */
	status = copy_stream_content(in_stream, out_stream, ir_maps->log_level,
		ir_maps->self_comp);
	if (status != DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error copy content to output stream: "
			"in-s-addr=%p, out-s-addr=%p", in_stream, out_stream);
		goto error;
	}

	BT_COMP_LOGD("Created new mapped stream: in-s-addr=%p, out-s-addr=%p",
		in_stream, out_stream);

	goto end;
error:
	out_stream = NULL;
end:
	return out_stream;
}

BT_HIDDEN
bt_stream *trace_ir_mapping_borrow_mapped_stream(struct trace_ir_maps *ir_maps,
		const bt_stream *in_stream)
{
	struct trace_ir_data_maps *d_maps;

	BT_ASSERT_DBG(ir_maps);
	BT_ASSERT_DBG(in_stream);

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
	bt_self_component *self_comp = ir_maps->self_comp;
	enum debug_info_trace_ir_mapping_status status;
	struct trace_ir_metadata_maps *md_maps;
	const bt_stream_class *in_stream_class;
	bt_stream_class *out_stream_class;
	bt_event_class *out_event_class;

	BT_COMP_LOGD("Creating new mapped event class: in-ec-addr=%p",
		in_event_class);

	BT_ASSERT(ir_maps);
	BT_ASSERT(in_event_class);

	in_stream_class = bt_event_class_borrow_stream_class_const(in_event_class);

	BT_ASSERT(in_stream_class);

	md_maps = borrow_metadata_maps_from_input_stream_class(ir_maps,
		in_stream_class);

	BT_ASSERT(md_maps);
	BT_ASSERT(!borrow_mapped_event_class(md_maps, in_event_class));

	/* Get the right output stream class to add the new event class to it. */
	out_stream_class = trace_ir_mapping_borrow_mapped_stream_class(
		ir_maps, in_stream_class);
	BT_ASSERT(out_stream_class);

	/* Create an output event class. */
	out_event_class = bt_event_class_create_with_id(out_stream_class,
		bt_event_class_get_id(in_event_class));
	if (!out_event_class) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error creating output event class");
		goto end;
	}

	/* Add it to the mapping. The mapping now owns out_event_class. */
	g_hash_table_insert(md_maps->event_class_map, (gpointer) in_event_class,
		out_event_class);

	/* Copy the content over to the output event class. */
	status = copy_event_class_content(ir_maps, in_event_class,
		out_event_class);
	if (status != DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error copy content to output event class: "
			"in-ec-addr=%p, out-ec-addr=%p", in_event_class,
			out_event_class);
		goto error;
	}

	BT_COMP_LOGD("Created new mapped event class: in-ec-addr=%p, out-ec-addr=%p",
		in_event_class, out_event_class);

	goto end;
error:
	out_event_class = NULL;
end:
	return out_event_class;
}

BT_HIDDEN
bt_event_class *trace_ir_mapping_borrow_mapped_event_class(
		struct trace_ir_maps *ir_maps,
		const bt_event_class *in_event_class)
{
	struct trace_ir_metadata_maps *md_maps;

	BT_ASSERT_DBG(ir_maps);
	BT_ASSERT_DBG(in_event_class);

	md_maps = borrow_metadata_maps_from_input_event_class(ir_maps,
		in_event_class);

	/* Return the mapped event_class. */
	return borrow_mapped_event_class(md_maps, in_event_class);
}

static inline
bt_packet *borrow_mapped_packet(struct trace_ir_data_maps *d_maps,
		const bt_packet *in_packet)
{
	BT_ASSERT_DBG(d_maps);
	BT_ASSERT_DBG(in_packet);

	return g_hash_table_lookup(d_maps->packet_map, (gpointer) in_packet);
}

BT_HIDDEN
bt_packet *trace_ir_mapping_create_new_mapped_packet(
		struct trace_ir_maps *ir_maps,
		const bt_packet *in_packet)
{
	bt_self_component *self_comp = ir_maps->self_comp;
	enum debug_info_trace_ir_mapping_status status;
	struct trace_ir_data_maps *d_maps;
	const bt_stream *in_stream;
	const bt_trace *in_trace;
	bt_packet *out_packet;
	bt_stream *out_stream;

	BT_COMP_LOGD("Creating new mapped packet: in-p-addr=%p", in_packet);

	in_stream = bt_packet_borrow_stream_const(in_packet);
	in_trace = bt_stream_borrow_trace_const(in_stream);
	d_maps = borrow_data_maps_from_input_trace(ir_maps, in_trace);

	/* There should never be a mapped packet already. */
	BT_ASSERT(!borrow_mapped_packet(d_maps, in_packet));
	BT_ASSERT(in_stream);

	/* Get output stream corresponding to this input stream. */
	out_stream = borrow_mapped_stream(d_maps, in_stream);
	BT_ASSERT(out_stream);

	/* Create the output packet. */
	out_packet = bt_packet_create(out_stream);
	if (!out_packet) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error create output packet");
		goto error;
	}

	/* Add it to the mapping. The mapping now owns out_packet. */
	g_hash_table_insert(d_maps->packet_map, (gpointer) in_packet,
		out_packet);

	/* Copy the content over to the output packet. */
	status = copy_packet_content(in_packet, out_packet, ir_maps->log_level,
		ir_maps->self_comp);
	if (status != DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error copy content to output packet: "
			"in-p-addr=%p, out-p-addr=%p", in_packet, out_packet);
		goto error;
	}

	BT_COMP_LOGD("Created new mapped packet: in-p-addr=%p, out-p-addr=%p",
		in_packet, out_packet);

	goto end;
error:
	out_packet = NULL;
end:
	return out_packet;
}

BT_HIDDEN
bt_packet *trace_ir_mapping_borrow_mapped_packet(struct trace_ir_maps *ir_maps,
		const bt_packet *in_packet)
{
	struct trace_ir_data_maps *d_maps;
	BT_ASSERT_DBG(ir_maps);
	BT_ASSERT_DBG(in_packet);

	d_maps = borrow_data_maps_from_input_packet(ir_maps, in_packet);

	return borrow_mapped_packet(d_maps, in_packet);
}

BT_HIDDEN
void trace_ir_mapping_remove_mapped_packet(struct trace_ir_maps *ir_maps,
		const bt_packet *in_packet)
{
	struct trace_ir_data_maps *d_maps;
	gboolean ret;

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
	struct trace_ir_data_maps *d_maps;
	gboolean ret;

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
	bt_self_component *self_comp = ir_maps->self_comp;
	bt_trace_add_listener_status add_listener_status;
	struct trace_ir_data_maps *d_maps = g_new0(struct trace_ir_data_maps, 1);

	if (!d_maps) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error allocating trace_ir_maps");
		goto error;
	}

	d_maps->log_level = ir_maps->log_level;
	d_maps->self_comp = ir_maps->self_comp;
	d_maps->input_trace = in_trace;

	/* Create the hashtables used to map data objects. */
	d_maps->stream_map = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL,(GDestroyNotify) bt_stream_put_ref);
	d_maps->packet_map = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL,(GDestroyNotify) bt_packet_put_ref);

	add_listener_status = bt_trace_add_destruction_listener(
		in_trace, trace_ir_data_maps_remove_func,
		ir_maps, &d_maps->destruction_listener_id);
	BT_ASSERT(add_listener_status == BT_TRACE_ADD_LISTENER_STATUS_OK);

error:
	return d_maps;
}

struct trace_ir_metadata_maps *trace_ir_metadata_maps_create(
		struct trace_ir_maps *ir_maps,
		const bt_trace_class *in_trace_class)
{
	bt_self_component *self_comp = ir_maps->self_comp;
	bt_trace_class_add_listener_status add_listener_status;
	struct trace_ir_metadata_maps *md_maps =
		g_new0(struct trace_ir_metadata_maps, 1);

	if (!md_maps) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error allocating trace_ir_maps");
		goto error;
	}

	md_maps->log_level = ir_maps->log_level;
	md_maps->self_comp = ir_maps->self_comp;
	md_maps->input_trace_class = in_trace_class;
	/*
	 * Create the field class resolving context. This is needed to keep
	 * track of the field class already copied in order to do the field
	 * path resolution correctly.
	 */
	md_maps->fc_resolving_ctx =
		g_new0(struct field_class_resolving_context, 1);
	if (!md_maps->fc_resolving_ctx) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error allocating field_class_resolving_context");
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

	add_listener_status = bt_trace_class_add_destruction_listener(
		in_trace_class, trace_ir_metadata_maps_remove_func,
		ir_maps, &md_maps->destruction_listener_id);
	BT_ASSERT(add_listener_status == BT_TRACE_CLASS_ADD_LISTENER_STATUS_OK);

error:
	return md_maps;
}

BT_HIDDEN
void trace_ir_data_maps_destroy(struct trace_ir_data_maps *maps)
{
	bt_trace_remove_listener_status status;

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
	if (status != BT_TRACE_REMOVE_LISTENER_STATUS_OK) {
		BT_COMP_LOG_CUR_LVL(BT_LOG_DEBUG, maps->log_level,
			maps->self_comp,
			"Trace destruction listener removal failed.");
		bt_current_thread_clear_error();
	}

	g_free(maps);
}

BT_HIDDEN
void trace_ir_metadata_maps_destroy(struct trace_ir_metadata_maps *maps)
{
	bt_trace_class_remove_listener_status status;

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

	g_free(maps->fc_resolving_ctx);

	if (maps->output_trace_class) {
		bt_trace_class_put_ref(maps->output_trace_class);
	}

	status = bt_trace_class_remove_destruction_listener(
		maps->input_trace_class, maps->destruction_listener_id);
	if (status != BT_TRACE_CLASS_REMOVE_LISTENER_STATUS_OK) {
		BT_COMP_LOG_CUR_LVL(BT_LOG_DEBUG, maps->log_level,
			maps->self_comp,
			"Trace destruction listener removal failed.");
		bt_current_thread_clear_error();
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

	g_free(maps->debug_info_field_class_name);

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
		const char *debug_info_field_name, bt_logging_level log_level)
{
	struct trace_ir_maps *ir_maps = g_new0(struct trace_ir_maps, 1);
	if (!ir_maps) {
		BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, log_level, self_comp,
			"Error allocating trace_ir_maps");
		goto error;
	}

	ir_maps->log_level = log_level;
	ir_maps->self_comp = self_comp;

	/* Copy debug info field name received from the user. */
	ir_maps->debug_info_field_class_name = g_strdup(debug_info_field_name);
	if (!ir_maps->debug_info_field_class_name) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Cannot copy debug info field name");
		goto error;
	}

	ir_maps->self_comp = self_comp;

	ir_maps->data_maps = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, (GDestroyNotify) NULL,
		(GDestroyNotify) trace_ir_data_maps_destroy);

	ir_maps->metadata_maps = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, (GDestroyNotify) NULL,
		(GDestroyNotify) trace_ir_metadata_maps_destroy);

	goto end;
error:
	trace_ir_maps_destroy(ir_maps);
	ir_maps = NULL;
end:
	return ir_maps;
}
