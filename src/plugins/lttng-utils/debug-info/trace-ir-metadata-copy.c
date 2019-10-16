/*
 * Babeltrace - Trace IR metadata object copy
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

#define BT_COMP_LOG_SELF_COMP self_comp
#define BT_LOG_OUTPUT_LEVEL log_level
#define BT_LOG_TAG "PLUGIN/FLT.LTTNG-UTILS.DEBUG-INFO/TRACE-IR-META-COPY"
#include "logging/comp-logging.h"

#include <inttypes.h>
#include <stdint.h>

#include "common/assert.h"

#include "trace-ir-metadata-copy.h"
#include "trace-ir-metadata-field-class-copy.h"
#include "utils.h"

BT_HIDDEN
enum debug_info_trace_ir_mapping_status copy_trace_class_content(
		struct trace_ir_maps *ir_maps,
		const bt_trace_class *in_trace_class,
		bt_trace_class *out_trace_class,
		bt_logging_level log_level, bt_self_component *self_comp)
{
	enum debug_info_trace_ir_mapping_status status;
	uint64_t sc_number, sc_idx;

	BT_COMP_LOGD("Copying content of trace class: in-tc-addr=%p, out-tc-addr=%p",
		in_trace_class, out_trace_class);

	/*
	 * Safe to use the same value object because it's frozen at this
	 * point.
	 */
	bt_trace_class_set_user_attributes(out_trace_class,
		bt_trace_class_borrow_user_attributes_const(in_trace_class));

	/* Use the same stream class ids as in the origin trace class. */
	bt_trace_class_set_assigns_automatic_stream_class_id(out_trace_class,
		BT_FALSE);

	/* Copy stream classes contained in the trace class. */
	sc_number = bt_trace_class_get_stream_class_count(in_trace_class);
	for (sc_idx = 0; sc_idx < sc_number; sc_idx++) {
		bt_stream_class *out_stream_class;
		const bt_stream_class *in_stream_class =
			bt_trace_class_borrow_stream_class_by_index_const(
				in_trace_class, sc_idx);

		out_stream_class = trace_ir_mapping_borrow_mapped_stream_class(
			ir_maps, in_stream_class);
		if (!out_stream_class) {
			out_stream_class = trace_ir_mapping_create_new_mapped_stream_class(
				ir_maps, in_stream_class);
			if (!out_stream_class) {
				status = DEBUG_INFO_TRACE_IR_MAPPING_STATUS_MEMORY_ERROR;
				goto end;
			}
		}
	}

	BT_COMP_LOGD("Copied content of trace class: in-tc-addr=%p, out-tc-addr=%p",
		in_trace_class, out_trace_class);

	status = DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK;
end:
	return status;
}

static
enum debug_info_trace_ir_mapping_status copy_clock_class_content(
		const bt_clock_class *in_clock_class,
		bt_clock_class *out_clock_class, bt_logging_level log_level,
		bt_self_component *self_comp)
{
	enum debug_info_trace_ir_mapping_status status;
	const char *clock_class_name, *clock_class_description;
	int64_t seconds;
	uint64_t cycles;
	bt_uuid in_uuid;

	BT_COMP_LOGD("Copying content of clock class: in-cc-addr=%p, out-cc-addr=%p",
		in_clock_class, out_clock_class);

	clock_class_name = bt_clock_class_get_name(in_clock_class);

	if (clock_class_name) {
		enum bt_clock_class_set_name_status set_name_status =
			bt_clock_class_set_name(out_clock_class, clock_class_name);
		if (set_name_status != BT_CLOCK_CLASS_SET_NAME_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Error setting clock class' name: "
				"cc-addr=%p, name=%s", out_clock_class,
				clock_class_name);
			status = (int) set_name_status;
			goto end;
		}
	}

	/*
	 * Safe to use the same value object because it's frozen at this
	 * point.
	 */
	bt_clock_class_set_user_attributes(out_clock_class,
		bt_clock_class_borrow_user_attributes_const(in_clock_class));

	clock_class_description = bt_clock_class_get_description(in_clock_class);

	if (clock_class_description) {
		enum bt_clock_class_set_description_status set_desc_status =
			bt_clock_class_set_description(out_clock_class, clock_class_description);
		if (set_desc_status!= BT_CLOCK_CLASS_SET_DESCRIPTION_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Error setting clock class' description: "
				"cc-addr=%p, cc-desc=%s", out_clock_class,
				clock_class_description);
			status = (int) set_desc_status;
			goto end;
		}
	}

	in_uuid = bt_clock_class_get_uuid(in_clock_class);
	if (in_uuid) {
		bt_clock_class_set_uuid(out_clock_class, in_uuid);
	}

	bt_clock_class_set_frequency(out_clock_class,
		bt_clock_class_get_frequency(in_clock_class));
	bt_clock_class_set_precision(out_clock_class,
		bt_clock_class_get_precision(in_clock_class));
	bt_clock_class_get_offset(in_clock_class, &seconds, &cycles);
	bt_clock_class_set_offset(out_clock_class, seconds, cycles);
	bt_clock_class_set_origin_is_unix_epoch(out_clock_class,
		bt_clock_class_origin_is_unix_epoch(in_clock_class));

	BT_COMP_LOGD("Copied content of clock class: in-cc-addr=%p, out-cc-addr=%p",
		in_clock_class, out_clock_class);

	status = DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK;
end:
	return status;
}

static
bt_clock_class *borrow_mapped_clock_class(
		struct trace_ir_metadata_maps *md_maps,
		const bt_clock_class *in_clock_class)
{
	BT_ASSERT_DBG(md_maps);
	BT_ASSERT_DBG(in_clock_class);

	return g_hash_table_lookup(md_maps->clock_class_map,
		(gpointer) in_clock_class);
}

static
bt_clock_class *create_new_mapped_clock_class(bt_self_component *self_comp,
		struct trace_ir_metadata_maps *md_maps,
		const bt_clock_class *in_clock_class)
{
	enum debug_info_trace_ir_mapping_status status;
	bt_clock_class *out_clock_class;
	bt_logging_level log_level = md_maps->log_level;

	BT_COMP_LOGD("Creating new mapped clock class: in-cc-addr=%p",
		in_clock_class);

	BT_ASSERT(md_maps);
	BT_ASSERT(in_clock_class);

	BT_ASSERT(!borrow_mapped_clock_class(md_maps, in_clock_class));

	out_clock_class = bt_clock_class_create(self_comp);
	if (!out_clock_class) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Cannot create clock class");
		goto end;
	}
	/* If not, create a new one and add it to the mapping. */
	status = copy_clock_class_content(in_clock_class, out_clock_class,
		log_level, self_comp);
	if (status != DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Cannot copy clock class");
		BT_CLOCK_CLASS_PUT_REF_AND_RESET(out_clock_class);
		goto end;
	}

	g_hash_table_insert(md_maps->clock_class_map,
		(gpointer) in_clock_class, out_clock_class);

	BT_COMP_LOGD("Created new mapped clock class: in-cc-addr=%p, out-cc-addr=%p",
		in_clock_class, out_clock_class);
end:
	return out_clock_class;
}

BT_HIDDEN
enum debug_info_trace_ir_mapping_status copy_stream_class_content(
		struct trace_ir_maps *ir_maps,
		const bt_stream_class *in_stream_class,
		bt_stream_class *out_stream_class)
{
	enum debug_info_trace_ir_mapping_status status;
	struct trace_ir_metadata_maps *md_maps;
	const bt_clock_class *in_clock_class;
	bt_clock_class *out_clock_class;
	const bt_field_class *in_packet_context_fc, *in_common_context_fc;
	bt_field_class *out_packet_context_fc, *out_common_context_fc;
	const char *in_name;
	uint64_t ec_number, ec_idx;
	bt_logging_level log_level = ir_maps->log_level;
	bt_self_component *self_comp = ir_maps->self_comp;

	BT_COMP_LOGD("Copying content of stream class: in-sc-addr=%p, out-sc-addr=%p",
		in_stream_class, out_stream_class);

	md_maps = borrow_metadata_maps_from_input_stream_class(ir_maps, in_stream_class);
	in_clock_class = bt_stream_class_borrow_default_clock_class_const(
			in_stream_class);

	if (in_clock_class) {
		enum bt_stream_class_set_default_clock_class_status set_def_cc_status;
		/* Copy the clock class. */
		out_clock_class = borrow_mapped_clock_class(md_maps,
			in_clock_class);
		if (!out_clock_class) {
			out_clock_class = create_new_mapped_clock_class(
				ir_maps->self_comp, md_maps, in_clock_class);
			if (!out_clock_class) {
				status = DEBUG_INFO_TRACE_IR_MAPPING_STATUS_MEMORY_ERROR;
				goto end;
			}
		}
		set_def_cc_status = bt_stream_class_set_default_clock_class(
			out_stream_class, out_clock_class);
		if (set_def_cc_status != BT_STREAM_CLASS_SET_DEFAULT_CLOCK_CLASS_STATUS_OK) {
			status = (int) set_def_cc_status;
			goto end;
		}
	}

	/*
	 * Safe to use the same value object because it's frozen at this
	 * point.
	 */
	bt_stream_class_set_user_attributes(out_stream_class,
		bt_stream_class_borrow_user_attributes_const(in_stream_class));

	bt_stream_class_set_supports_packets(
		out_stream_class,
		bt_stream_class_supports_packets(in_stream_class),
		bt_stream_class_packets_have_beginning_default_clock_snapshot(
			in_stream_class),
		bt_stream_class_packets_have_end_default_clock_snapshot(
			in_stream_class));
	bt_stream_class_set_supports_discarded_events(
		out_stream_class,
		bt_stream_class_supports_discarded_events(in_stream_class),
		bt_stream_class_discarded_events_have_default_clock_snapshots(
			in_stream_class));
	bt_stream_class_set_supports_discarded_packets(
		out_stream_class,
		bt_stream_class_supports_discarded_packets(in_stream_class),
		bt_stream_class_discarded_packets_have_default_clock_snapshots(
			in_stream_class));

	in_name = bt_stream_class_get_name(in_stream_class);
	if (in_name) {
		enum bt_stream_class_set_name_status set_name_status =
			bt_stream_class_set_name(out_stream_class, in_name);
		if (set_name_status != BT_STREAM_CLASS_SET_NAME_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Error set stream class name: "
				"out-sc-addr=%p, name=%s", out_stream_class,
				in_name);
			status = (int) set_name_status;
			goto end;
		}
	}

	bt_stream_class_set_assigns_automatic_stream_id(out_stream_class,
		BT_FALSE);
	bt_stream_class_set_assigns_automatic_event_class_id(out_stream_class,
		BT_FALSE);

	/*
	 * Add the input packet context field class to the context to
	 * resolution in the further steps.
	 */
	in_packet_context_fc =
		bt_stream_class_borrow_packet_context_field_class_const(
			in_stream_class);
	md_maps->fc_resolving_ctx->packet_context =
		in_packet_context_fc;

	if (in_packet_context_fc) {
		enum bt_stream_class_set_field_class_status set_fc_status;
		/* Copy packet context. */
		out_packet_context_fc = create_field_class_copy(md_maps,
			in_packet_context_fc);

		status = copy_field_class_content(md_maps, in_packet_context_fc,
			out_packet_context_fc);
		if (status != DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Error copying stream class' packet context field class: "
				"in-packet-ctx-fc-addr=%p, out-packet-ctx-fc-addr=%p",
				in_packet_context_fc, out_packet_context_fc);
			goto end;
		}

		set_fc_status = bt_stream_class_set_packet_context_field_class(
			out_stream_class, out_packet_context_fc);
		if (set_fc_status != BT_STREAM_CLASS_SET_FIELD_CLASS_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Error setting stream class' packet context field class: "
				"out-sc-addr=%p, out-packet-ctx-fc-addr=%p",
				out_stream_class, out_packet_context_fc);
			status = (int) set_fc_status;
			goto end;
		}
	}

	/*
	 * Add the input common context field class to the context to
	 * resolution in the further steps.
	 */
	in_common_context_fc =
		bt_stream_class_borrow_event_common_context_field_class_const(
			in_stream_class);
	md_maps->fc_resolving_ctx->event_common_context = in_common_context_fc;

	if (in_common_context_fc) {
		enum bt_stream_class_set_field_class_status set_fc_status;
		/* Copy common context. */
		/* TODO: I find it a bit awkward to have this special function
		 * here to add the debug-info field class. I would like to
		 * abstract that.*/
		out_common_context_fc = create_field_class_copy(md_maps,
			in_common_context_fc);
		status = copy_event_common_context_field_class_content(md_maps,
			ir_maps->debug_info_field_class_name,
			in_common_context_fc, out_common_context_fc);
		if (status != DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Error copying stream class' common context field class: "
				"in-comm-ctx-fc-addr=%p, out-comm-ctx-fc-addr=%p",
				in_common_context_fc, out_common_context_fc);
			goto end;
		}

		set_fc_status = bt_stream_class_set_event_common_context_field_class(
			out_stream_class, out_common_context_fc);
		if (set_fc_status != BT_STREAM_CLASS_SET_FIELD_CLASS_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Error setting stream class' common context field class: "
				"out-sc-addr=%p, out-comm-ctx-fc-addr=%p",
				out_stream_class, out_common_context_fc);
			status = (int) set_fc_status;
			goto end;
		}
	}

	/* Copy event classes contained in the stream class. */
	ec_number = bt_stream_class_get_event_class_count(in_stream_class);
	for (ec_idx = 0; ec_idx < ec_number; ec_idx++) {
		bt_event_class *out_event_class;
		const bt_event_class *in_event_class =
			bt_stream_class_borrow_event_class_by_id_const(
				in_stream_class, ec_idx);
		out_event_class = trace_ir_mapping_borrow_mapped_event_class(
			ir_maps, in_event_class);
		if (!out_event_class) {
			/*
			 * We don't need the new event_class yet. We simply
			 * want to create it and keep it in the map.
			 */
			out_event_class = trace_ir_mapping_create_new_mapped_event_class(
				ir_maps, in_event_class);
			if (!out_event_class) {
				status = DEBUG_INFO_TRACE_IR_MAPPING_STATUS_MEMORY_ERROR;
				goto end;
			}
		}
	}

	BT_COMP_LOGD("Copied content of stream class: in-sc-addr=%p, out-sc-addr=%p",
		in_stream_class, out_stream_class);

	status = DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK;
end:
	return status;
}

BT_HIDDEN
enum debug_info_trace_ir_mapping_status copy_event_class_content(
		struct trace_ir_maps *ir_maps,
		const bt_event_class *in_event_class,
		bt_event_class *out_event_class)
{
	enum debug_info_trace_ir_mapping_status status;
	struct trace_ir_metadata_maps *md_maps;
	const char *in_event_class_name, *in_emf_uri;
	bt_property_availability prop_avail;
	bt_event_class_log_level ec_log_level;
	bt_field_class *out_specific_context_fc, *out_payload_fc;
	const bt_field_class *in_event_specific_context, *in_event_payload;
	bt_logging_level log_level = ir_maps->log_level;
	bt_self_component *self_comp = ir_maps->self_comp;

	BT_COMP_LOGD("Copying content of event class: in-ec-addr=%p, out-ec-addr=%p",
		in_event_class, out_event_class);

	/* Copy event class name. */
	in_event_class_name = bt_event_class_get_name(in_event_class);
	if (in_event_class_name) {
		enum bt_event_class_set_name_status set_name_status =
			bt_event_class_set_name(out_event_class, in_event_class_name);
		if (set_name_status != BT_EVENT_CLASS_SET_NAME_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp, "Error setting event class' name: ec-addr=%p, "
				"name=%s", out_event_class, in_event_class_name);
			status = (int) set_name_status;
			goto end;
		}
	}

	/*
	 * Safe to use the same value object because it's frozen at this
	 * point.
	 */
	bt_event_class_set_user_attributes(out_event_class,
		bt_event_class_borrow_user_attributes_const(in_event_class));

	/* Copy event class loglevel. */
	prop_avail = bt_event_class_get_log_level(in_event_class,
		&ec_log_level);
	if (prop_avail == BT_PROPERTY_AVAILABILITY_AVAILABLE) {
		bt_event_class_set_log_level(out_event_class, ec_log_level);
	}

	/* Copy event class emf uri. */
	in_emf_uri = bt_event_class_get_emf_uri(in_event_class);
	if (in_emf_uri) {
		enum bt_event_class_set_emf_uri_status set_emf_status =
			bt_event_class_set_emf_uri(out_event_class, in_emf_uri);
		if (set_emf_status != BT_EVENT_CLASS_SET_EMF_URI_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Error setting event class' emf uri: "
				"out-ec-addr=%p, emf-uri=\"%s\"",
				out_event_class, in_emf_uri);
			status = (int) set_emf_status;
			goto end;
		}
	}

	md_maps = borrow_metadata_maps_from_input_event_class(ir_maps,
		in_event_class);
	/*
	 * Add the input event class' specific ctx to te
	 * context.
	 */
	in_event_specific_context =
		bt_event_class_borrow_specific_context_field_class_const(
			in_event_class);

	md_maps->fc_resolving_ctx->event_specific_context =
		in_event_specific_context;

	if (in_event_specific_context) {
		enum bt_event_class_set_field_class_status set_fc_status;
		/* Copy the specific context of this event class. */
		out_specific_context_fc = create_field_class_copy(md_maps,
			in_event_specific_context);

		status = copy_field_class_content(md_maps,
			in_event_specific_context, out_specific_context_fc);
		if (status != DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Error copying event class' specific context field class:"
				"in-spec-ctx-fc-addr=%p, out-spec-ctx-fc-addr=%p",
				in_event_specific_context, out_specific_context_fc);
			goto end;
		}
		/*
		 * Add the output specific context to the output event
		 * class.
		 */
		set_fc_status = bt_event_class_set_specific_context_field_class(
				out_event_class, out_specific_context_fc);
		if (set_fc_status != BT_EVENT_CLASS_SET_FIELD_CLASS_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Error setting event class' specific context field class:"
				"out-ec-addr=%p, out-spec-ctx-fc-addr=%p",
				out_event_class, out_specific_context_fc);
			status = (int) set_fc_status;
			goto end;
		}
	}

	/*
	 * Add the input event class' payload field class to
	 * the context.
	 */
	in_event_payload = bt_event_class_borrow_payload_field_class_const(
		in_event_class);

	md_maps->fc_resolving_ctx->event_payload = in_event_payload;

	if (in_event_payload) {
		enum bt_event_class_set_field_class_status set_fc_status;
		/* Copy the payload of this event class. */
		out_payload_fc = create_field_class_copy(md_maps,
			in_event_payload);
		status = copy_field_class_content(md_maps, in_event_payload,
			out_payload_fc);
		if (status != DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Error copying event class' specific context field class:"
				"in-payload-fc-addr=%p, out-payload-fc-addr=%p",
				in_event_payload, out_payload_fc);
			goto end;
		}

		/* Add the output payload to the output event class. */
		set_fc_status = bt_event_class_set_payload_field_class(
			out_event_class, out_payload_fc);
		if (set_fc_status != BT_EVENT_CLASS_SET_FIELD_CLASS_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp, "Error setting event class' payload field class: "
				"out-ec-addr=%p, out-payload-fc-addr=%p",
				out_event_class, out_payload_fc);
			status = (int) set_fc_status;
			goto end;
		}
	}

	BT_COMP_LOGD("Copied content of event class: in-ec-addr=%p, out-ec-addr=%p",
		in_event_class, out_event_class);

	status = DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK;
end:
	return status;
}

BT_HIDDEN
enum debug_info_trace_ir_mapping_status
copy_event_common_context_field_class_content(
		struct trace_ir_metadata_maps *md_maps,
		const char *debug_info_fc_name,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	enum debug_info_trace_ir_mapping_status status;
	bt_field_class *debug_field_class = NULL, *bin_field_class = NULL,
		       *func_field_class = NULL, *src_field_class = NULL;
	bt_logging_level log_level = md_maps->log_level;
	bt_self_component *self_comp = md_maps->self_comp;

	BT_COMP_LOGD("Copying content of event common context field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);

	/* Copy the content of the input common context. */
	status = copy_field_class_content(md_maps, in_field_class, out_field_class);
	if (status != DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error copying content of event common context field class: "
			"in-fc-addr=%p, out-fc-addr=%p", in_field_class,
			out_field_class);
		goto error;
	}

	/*
	 * If this event common context has the necessary fields to compute the
	 * debug information append the debug-info field class to the event
	 * common context.
	 */
	if (is_event_common_ctx_dbg_info_compatible(in_field_class, debug_info_fc_name)) {
		enum bt_field_class_structure_append_member_status append_member_status;
		/*
		 * The struct field and 3 sub-fields are not stored in the
		 * field class map because they don't have input equivalent.
		 * We need to put our reference each of these field classes
		 * once they are added to their respective containing field
		 * classes.
		 */
		debug_field_class = bt_field_class_structure_create(
			md_maps->output_trace_class);
		if (!debug_field_class) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Failed to create debug_info structure.");
			status = DEBUG_INFO_TRACE_IR_MAPPING_STATUS_MEMORY_ERROR;
			goto error;
		}

		bin_field_class = bt_field_class_string_create(
			md_maps->output_trace_class);
		if (!bin_field_class) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Failed to create string for field=\"bin\".");
			status = DEBUG_INFO_TRACE_IR_MAPPING_STATUS_MEMORY_ERROR;
			goto error;
		}

		func_field_class = bt_field_class_string_create(
			md_maps->output_trace_class);
		if (!func_field_class) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Failed to create string for field=\"func\".");
			status = DEBUG_INFO_TRACE_IR_MAPPING_STATUS_MEMORY_ERROR;
			goto error;
		}

		src_field_class = bt_field_class_string_create(
			md_maps->output_trace_class);
		if (!src_field_class) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Failed to create string for field=\"src\".");
			status = DEBUG_INFO_TRACE_IR_MAPPING_STATUS_MEMORY_ERROR;
			goto error;
		}

		append_member_status = bt_field_class_structure_append_member(
			debug_field_class, "bin", bin_field_class);
		if (append_member_status !=
				BT_FIELD_CLASS_STRUCTURE_APPEND_MEMBER_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Failed to add a field to debug_info struct: "
				"field=\"bin\".");
			status = (int) append_member_status;
			goto error;
		}
		BT_FIELD_CLASS_PUT_REF_AND_RESET(bin_field_class);

		append_member_status = bt_field_class_structure_append_member(
			debug_field_class, "func", func_field_class);
		if (append_member_status !=
				BT_FIELD_CLASS_STRUCTURE_APPEND_MEMBER_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Failed to add a field to debug_info struct: "
				"field=\"func\".");
			status = (int) append_member_status;
			goto error;
		}
		BT_FIELD_CLASS_PUT_REF_AND_RESET(func_field_class);

		append_member_status = bt_field_class_structure_append_member(
			debug_field_class, "src", src_field_class);
		if (append_member_status !=
				BT_FIELD_CLASS_STRUCTURE_APPEND_MEMBER_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Failed to add a field to debug_info struct: "
				"field=\"src\".");
			status = (int) append_member_status;
			goto error;
		}
		BT_FIELD_CLASS_PUT_REF_AND_RESET(src_field_class);

		/*Add the filled debug-info field class to the common context. */
		append_member_status = bt_field_class_structure_append_member(
			out_field_class, debug_info_fc_name, debug_field_class);
		if (append_member_status !=
				BT_FIELD_CLASS_STRUCTURE_APPEND_MEMBER_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Failed to add debug_info field to event common context.");
			status = (int) append_member_status;
			goto error;
		}
		BT_FIELD_CLASS_PUT_REF_AND_RESET(debug_field_class);
	}
	BT_COMP_LOGD("Copied content of event common context field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);

	status = DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK;
	goto end;

error:
	if (debug_field_class) {
		bt_field_class_put_ref(debug_field_class);
	}
	if (bin_field_class) {
		bt_field_class_put_ref(bin_field_class);
	}
	if (func_field_class) {
		bt_field_class_put_ref(func_field_class);
	}
	if (src_field_class) {
		bt_field_class_put_ref(src_field_class);
	}
end:
	return status;
}

BT_HIDDEN
bt_field_class *create_field_class_copy(struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class)
{
	return create_field_class_copy_internal(md_maps, in_field_class);
}

BT_HIDDEN
enum debug_info_trace_ir_mapping_status copy_field_class_content(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	return copy_field_class_content_internal(md_maps, in_field_class,
			out_field_class);
}
