#ifndef BABELTRACE_PLUGIN_DEBUG_INFO_TRACE_IR_MAPPING_H
#define BABELTRACE_PLUGIN_DEBUG_INFO_TRACE_IR_MAPPING_H
/*
 * Copyright 2019 Francis Deslauriers francis.deslauriers@efficios.com>
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

#include <glib.h>

#include <babeltrace/assert-internal.h>
#include <babeltrace/babeltrace.h>

#include "debug-info.h"

/* Used to resolve field paths for dynamic arrays and variant field classes. */
struct field_class_resolving_context {
	/* Weak reference. Owned by input stream class. */
	const bt_field_class *packet_context;
	/* Weak reference. Owned by input stream class. */
	const bt_field_class *event_common_context;
	/* Weak reference. Owned by input event class. */
	const bt_field_class *event_specific_context;
	/* Weak reference. Owned by input event class. */
	const bt_field_class *event_payload;
};

struct trace_ir_metadata_maps {
	const bt_trace_class *input_trace_class;
	bt_trace_class *output_trace_class;

	/*
	 * Map between input stream class and its corresponding output stream
	 * class.
	 * input stream class: weak reference. Owned by an upstream
	 * component.
	 * output stream class: owned by this structure.
	 */
	GHashTable *stream_class_map;

	/*
	 * Map between input event class and its corresponding output event
	 * class.
	 * input event class: weak reference. Owned by an upstream component.
	 * output event class: owned by this structure.
	 */
	GHashTable *event_class_map;

	/*
	 * Map between input field class and its corresponding output field
	 * class.
	 * input field class: weak reference. Owned by an upstream component.
	 * output field class: owned by this structure.
	 */
	GHashTable *field_class_map;

	/*
	 * Map between input clock class and its corresponding output clock
	 * class.
	 * input clock class: weak reference. Owned by an upstream component.
	 * output clock class: owned by this structure.
	 */
	GHashTable *clock_class_map;

	struct field_class_resolving_context *fc_resolving_ctx;

	uint64_t destruction_listener_id;
};

struct trace_ir_data_maps {
	const bt_trace *input_trace;
	bt_trace *output_trace;

	/*
	 * Map between input stream its corresponding output stream.
	 * input stream: weak reference. Owned by an upstream component.
	 * output stream: owned by this structure.
	 */
	GHashTable *stream_map;

	/*
	 * Map between input packet its corresponding output packet.
	 * input packet: weak reference. Owned by an upstream packet component.
	 * output packet: owned by this structure.
	 */
	GHashTable *packet_map;

	uint64_t destruction_listener_id;
};

struct trace_ir_maps {
	/*
	 * input trace -> trace_ir_data_maps.
	 * input trace: weak reference. Owned by an upstream component.
	 * trace_ir_data_maps: Owned by this structure.
	 */
	GHashTable *data_maps;

	/*
	 * input trace class -> trace_ir_metadata_maps.
	 * input trace class: weak reference. Owned by an upstream component.
	 * trace_ir_metadata_maps: Owned by this structure.
	 */
	GHashTable *metadata_maps;

	char *debug_info_field_class_name;

	bt_self_component *self_comp;
};

BT_HIDDEN
struct trace_ir_maps *trace_ir_maps_create(bt_self_component *self_comp,
		const char *debug_info_field_name);

BT_HIDDEN
void trace_ir_maps_clear(struct trace_ir_maps *maps);

BT_HIDDEN
void trace_ir_maps_destroy(struct trace_ir_maps *maps);

BT_HIDDEN
struct trace_ir_data_maps *trace_ir_data_maps_create(
		struct trace_ir_maps *ir_maps,
		const bt_trace *in_trace);

BT_HIDDEN
void trace_ir_data_maps_destroy(struct trace_ir_data_maps *d_maps);

BT_HIDDEN
struct trace_ir_metadata_maps *trace_ir_metadata_maps_create(
		struct trace_ir_maps *ir_maps,
		const bt_trace_class *in_trace_class);

BT_HIDDEN
void trace_ir_metadata_maps_destroy(struct trace_ir_metadata_maps *md_maps);

BT_HIDDEN
bt_stream *trace_ir_mapping_create_new_mapped_stream(
		struct trace_ir_maps *ir_maps,
		const bt_stream *in_stream);

BT_HIDDEN
bt_stream *trace_ir_mapping_borrow_mapped_stream(
		struct trace_ir_maps *ir_maps,
		const bt_stream *in_stream);

BT_HIDDEN
void trace_ir_mapping_remove_mapped_stream(
		struct trace_ir_maps *ir_maps,
		const bt_stream *in_stream);

BT_HIDDEN
bt_event_class *trace_ir_mapping_create_new_mapped_event_class(
		struct trace_ir_maps *ir_maps,
		const bt_event_class *in_event_class);

BT_HIDDEN
bt_event_class *trace_ir_mapping_borrow_mapped_event_class(
		struct trace_ir_maps *ir_maps,
		const bt_event_class *in_event_class);

BT_HIDDEN
bt_packet *trace_ir_mapping_create_new_mapped_packet(
		struct trace_ir_maps *ir_maps,
		const bt_packet *in_packet);

BT_HIDDEN
bt_packet *trace_ir_mapping_borrow_mapped_packet(
		struct trace_ir_maps *ir_maps,
		const bt_packet *in_packet);

BT_HIDDEN
void trace_ir_mapping_remove_mapped_packet(
		struct trace_ir_maps *ir_maps,
		const bt_packet *in_packet);

static inline
struct trace_ir_data_maps *borrow_data_maps_from_input_trace(
		struct trace_ir_maps *ir_maps, const bt_trace *in_trace)
{
	BT_ASSERT(ir_maps);
	BT_ASSERT(in_trace);

	struct trace_ir_data_maps *d_maps =
		g_hash_table_lookup(ir_maps->data_maps, (gpointer) in_trace);
	if (!d_maps) {
		d_maps = trace_ir_data_maps_create(ir_maps, in_trace);
		g_hash_table_insert(ir_maps->data_maps, (gpointer) in_trace, d_maps);
	}

	return d_maps;
}

static inline
struct trace_ir_data_maps *borrow_data_maps_from_input_stream(
		struct trace_ir_maps *ir_maps, const bt_stream *in_stream)
{
	BT_ASSERT(ir_maps);
	BT_ASSERT(in_stream);

	return borrow_data_maps_from_input_trace(ir_maps,
			bt_stream_borrow_trace_const(in_stream));
}

static inline
struct trace_ir_data_maps *borrow_data_maps_from_input_packet(
		struct trace_ir_maps *ir_maps, const bt_packet *in_packet)
{
	BT_ASSERT(ir_maps);
	BT_ASSERT(in_packet);

	return borrow_data_maps_from_input_stream(ir_maps,
			bt_packet_borrow_stream_const(in_packet));
}

static inline
struct trace_ir_metadata_maps *borrow_metadata_maps_from_input_trace_class(
		struct trace_ir_maps *ir_maps,
		const bt_trace_class *in_trace_class)
{
	BT_ASSERT(ir_maps);
	BT_ASSERT(in_trace_class);

	struct trace_ir_metadata_maps *md_maps =
		g_hash_table_lookup(ir_maps->metadata_maps,
				(gpointer) in_trace_class);
	if (!md_maps) {
		md_maps = trace_ir_metadata_maps_create(ir_maps, in_trace_class);
		g_hash_table_insert(ir_maps->metadata_maps,
				(gpointer) in_trace_class, md_maps);
	}

	return md_maps;
}

static inline
struct trace_ir_metadata_maps *borrow_metadata_maps_from_input_stream_class(
		struct trace_ir_maps *ir_maps,
		const bt_stream_class *in_stream_class) {

	BT_ASSERT(in_stream_class);

	return borrow_metadata_maps_from_input_trace_class(ir_maps,
			bt_stream_class_borrow_trace_class_const(in_stream_class));
}

static inline
struct trace_ir_metadata_maps *borrow_metadata_maps_from_input_event_class(
		struct trace_ir_maps *ir_maps,
		const bt_event_class *in_event_class) {

	BT_ASSERT(in_event_class);

	return borrow_metadata_maps_from_input_stream_class(ir_maps,
			bt_event_class_borrow_stream_class_const(in_event_class));
}

#endif /* BABELTRACE_PLUGIN_DEBUG_INFO_TRACE_IR_MAPPING_H */
