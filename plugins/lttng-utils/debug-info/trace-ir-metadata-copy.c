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

#define BT_LOG_TAG "PLUGIN-LTTNG-UTILS-DEBUG-INFO-TRACE-IR-METADATA-COPY"
#include "logging.h"

#include <inttypes.h>
#include <stdint.h>

#include <babeltrace/assert-internal.h>

#include "trace-ir-metadata-copy.h"
#include "trace-ir-metadata-field-class-copy.h"
#include "utils.h"

BT_HIDDEN
int copy_trace_class_content(const bt_trace_class *in_trace_class,
		bt_trace_class *out_trace_class)
{
	int ret = 0;
	uint64_t i, env_field_count;
	const char *in_trace_class_name;

	BT_LOGD("Copying content of trace class: in-tc-addr=%p, out-tc-addr=%p",
			in_trace_class, out_trace_class);

	/* Use the same stream class ids as in the origin trace class. */
	bt_trace_class_set_assigns_automatic_stream_class_id(out_trace_class,
			BT_FALSE);

	in_trace_class_name = bt_trace_class_get_name(in_trace_class);
	if (in_trace_class_name) {
		bt_trace_class_set_name(out_trace_class, in_trace_class_name);
	}

	/*
	 * Do not copy the trace class UUID as it may be modified and should no
	 * longer have the same UUID.
	 */

	/*
	 * Go over all the entries in the environment section of the trace class
	 * and copy the content to the new trace class.
	 */
	env_field_count = bt_trace_class_get_environment_entry_count(in_trace_class);
	for (i = 0; i < env_field_count; i++) {
		const char *value_name;
		const bt_value *value = NULL;
		bt_trace_class_status trace_class_status;

		bt_trace_class_borrow_environment_entry_by_index_const(
			in_trace_class, i, &value_name, &value);

		BT_LOGD("Copying trace class environnement entry: "
			"index=%" PRId64 ", value-addr=%p, value-name=%s",
			i, value, value_name);

		BT_ASSERT(value_name);
		BT_ASSERT(value);

		if (bt_value_is_integer(value)) {
			trace_class_status =
				bt_trace_class_set_environment_entry_integer(
						out_trace_class, value_name,
						bt_value_integer_get(value));
		} else if (bt_value_is_string(value)) {
			trace_class_status =
				bt_trace_class_set_environment_entry_string(
					out_trace_class, value_name,
					bt_value_string_get(value));
		} else {
			abort();
		}

		if (trace_class_status != BT_TRACE_CLASS_STATUS_OK) {
			ret = -1;
			goto error;
		}
	}

	BT_LOGD("Copied content of trace class: in-tc-addr=%p, out-tc-addr=%p",
			in_trace_class, out_trace_class);
error:
	return ret;
}

static
int copy_clock_class_content(const bt_clock_class *in_clock_class,
		bt_clock_class *out_clock_class)
{
	bt_clock_class_status status;
	const char *clock_class_name, *clock_class_description;
	int64_t seconds;
	uint64_t cycles;
	bt_uuid in_uuid;
	int ret = 0;

	BT_LOGD("Copying content of clock class: in-cc-addr=%p, out-cc-addr=%p",
			in_clock_class, out_clock_class);

	clock_class_name = bt_clock_class_get_name(in_clock_class);

	if (clock_class_name) {
		status = bt_clock_class_set_name(out_clock_class, clock_class_name);
		if (status != BT_CLOCK_CLASS_STATUS_OK) {
			BT_LOGE("Error setting clock class' name cc-addr=%p, name=%p",
				out_clock_class, clock_class_name);
			out_clock_class = NULL;
			ret = -1;
			goto error;
		}
	}

	clock_class_description = bt_clock_class_get_description(in_clock_class);

	if (clock_class_description) {
		status = bt_clock_class_set_description(out_clock_class,
				clock_class_description);
		if (status != BT_CLOCK_CLASS_STATUS_OK) {
			BT_LOGE("Error setting clock class' description cc-addr=%p, "
				"name=%p", out_clock_class, clock_class_description);
			out_clock_class = NULL;
			ret = -1;
			goto error;
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

	BT_LOGD("Copied content of clock class: in-cc-addr=%p, out-cc-addr=%p",
			in_clock_class, out_clock_class);

error:
	return ret;
}

static
bt_clock_class *borrow_mapped_clock_class(
		struct trace_ir_metadata_maps *md_maps,
		const bt_clock_class *in_clock_class)
{
	BT_ASSERT(md_maps);
	BT_ASSERT(in_clock_class);

	return g_hash_table_lookup(md_maps->clock_class_map,
			(gpointer) in_clock_class);
}

static
bt_clock_class *create_new_mapped_clock_class(
		bt_self_component *self_comp,
		struct trace_ir_metadata_maps *md_maps,
		const bt_clock_class *in_clock_class)
{
	bt_clock_class *out_clock_class;
	int ret;

	BT_LOGD("Creating new mapped clock class: in-cc-addr=%p",
			in_clock_class);

	BT_ASSERT(md_maps);
	BT_ASSERT(in_clock_class);

	BT_ASSERT(!borrow_mapped_clock_class(md_maps, in_clock_class));

	out_clock_class = bt_clock_class_create(self_comp);
	if (!out_clock_class) {
		BT_LOGE_STR("Cannot create clock class");
		goto end;
	}
	/* If not, create a new one and add it to the mapping. */
	ret = copy_clock_class_content(in_clock_class, out_clock_class);
	if (ret) {
		BT_LOGE_STR("Cannot copy clock class");
		goto end;
	}

	g_hash_table_insert(md_maps->clock_class_map,
			(gpointer) in_clock_class, out_clock_class);

	BT_LOGD("Created new mapped clock class: in-cc-addr=%p, out-cc-addr=%p",
			in_clock_class, out_clock_class);
end:
	return out_clock_class;
}

BT_HIDDEN
int copy_stream_class_content(struct trace_ir_maps *ir_maps,
		const bt_stream_class *in_stream_class,
		bt_stream_class *out_stream_class)
{
	struct trace_ir_metadata_maps *md_maps;
	const bt_clock_class *in_clock_class;
	bt_clock_class *out_clock_class;
	const bt_field_class *in_packet_context_fc, *in_common_context_fc;
	bt_field_class *out_packet_context_fc, *out_common_context_fc;
	bt_stream_class_status status;
	const char *in_name;
	int ret = 0;

	BT_LOGD("Copying content of stream class: in-sc-addr=%p, out-sc-addr=%p",
			in_stream_class, out_stream_class);

	md_maps = borrow_metadata_maps_from_input_stream_class(ir_maps, in_stream_class);
	in_clock_class = bt_stream_class_borrow_default_clock_class_const(
				in_stream_class);

	if (in_clock_class) {
		/* Copy the clock class. */
		out_clock_class =
			borrow_mapped_clock_class(md_maps, in_clock_class);
		if (!out_clock_class) {
			out_clock_class = create_new_mapped_clock_class(
					ir_maps->self_comp, md_maps,
					in_clock_class);
		}
		bt_stream_class_set_default_clock_class(out_stream_class,
				out_clock_class);

	}

	in_name = bt_stream_class_get_name(in_stream_class);
	if (in_name) {
		status = bt_stream_class_set_name(out_stream_class, in_name);
		if (status != BT_STREAM_CLASS_STATUS_OK) {
			BT_LOGE("Error set stream class name: out-sc-addr=%p, "
				"name=%s", out_stream_class, in_name);
			ret = -1;
			goto error;
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
		/* Copy packet context. */
		out_packet_context_fc = create_field_class_copy(
				md_maps, in_packet_context_fc);

		ret = copy_field_class_content(md_maps,
			in_packet_context_fc, out_packet_context_fc);
		if (ret) {
			ret = -1;
			goto error;
		}

		status = bt_stream_class_set_packet_context_field_class(
				out_stream_class, out_packet_context_fc);
		if (status !=  BT_STREAM_CLASS_STATUS_OK) {
			BT_LOGE("Error setting stream class' packet context "
				"field class: sc-addr=%p, packet-fc-addr=%p",
				out_stream_class, out_packet_context_fc);
			ret = -1;
			goto error;
		}
	}

	/*
	 * Add the input common context field class to the context to
	 * resolution in the further steps.
	 */
	in_common_context_fc =
		bt_stream_class_borrow_event_common_context_field_class_const(
				in_stream_class);
	md_maps->fc_resolving_ctx->event_common_context =
		in_common_context_fc;

	if (in_common_context_fc) {
		/* Copy common context. */
		/* TODO: I find it a bit awkward to have this special function
		 * here to add the debug-info field class. I would like to
		 * abstract that.*/
		out_common_context_fc = create_field_class_copy(
				md_maps, in_common_context_fc);

		ret = copy_event_common_context_field_class_content(
				md_maps, ir_maps->debug_info_field_class_name,
				in_common_context_fc, out_common_context_fc);
		if (ret) {
			goto error;
		}

		status = bt_stream_class_set_event_common_context_field_class(
				out_stream_class, out_common_context_fc);
		if (status !=  BT_STREAM_CLASS_STATUS_OK) {
			BT_LOGE("Error setting stream class' packet context "
				"field class: sc-addr=%p, packet-fc-addr=%p",
				out_stream_class, out_common_context_fc);
			ret = -1;
			goto error;
		}
	}

	/* Set packet snapshot boolean fields. */
	BT_LOGD("Copied content of stream class: in-sc-addr=%p, out-sc-addr=%p",
			in_stream_class, out_stream_class);
error:
	return ret;
}

BT_HIDDEN
int copy_event_class_content(struct trace_ir_maps *ir_maps,
		const bt_event_class *in_event_class,
		bt_event_class *out_event_class)
{
	struct trace_ir_metadata_maps *md_maps;
	const char *in_event_class_name, *in_emf_uri;
	bt_property_availability prop_avail;
	bt_event_class_log_level log_level;
	bt_event_class_status status;
	bt_field_class *out_specific_context_fc, *out_payload_fc;
	const bt_field_class *in_event_specific_context, *in_event_payload;
	int ret = 0;

	BT_LOGD("Copying content of event class: in-ec-addr=%p, out-ec-addr=%p",
			in_event_class, out_event_class);

	/* Copy event class name. */
	in_event_class_name = bt_event_class_get_name(in_event_class);
	if (in_event_class_name) {
		status = bt_event_class_set_name(out_event_class, in_event_class_name);
		if (status != BT_EVENT_CLASS_STATUS_OK) {
			BT_LOGE("Error setting event class' name: ec-addr=%p, "
				"name=%s", out_event_class, in_event_class_name);
			ret = -1;
			goto error;
		}
	}

	/* Copy event class loglevel. */
	prop_avail = bt_event_class_get_log_level(in_event_class, &log_level);
	if (prop_avail == BT_PROPERTY_AVAILABILITY_AVAILABLE) {
		bt_event_class_set_log_level(out_event_class,
				log_level);
	}

	/* Copy event class emf uri. */
	in_emf_uri = bt_event_class_get_emf_uri(in_event_class);
	if (in_emf_uri) {
		status = bt_event_class_set_emf_uri(out_event_class, in_emf_uri);
		if (status != BT_EVENT_CLASS_STATUS_OK) {
			BT_LOGE("Error setting event class' emf uri: ec-addr=%p, "
				"emf uri=%s", out_event_class, in_emf_uri);
			ret = -1;
			goto error;
		}
	}

	md_maps = borrow_metadata_maps_from_input_event_class(ir_maps, in_event_class);
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
		/* Copy the specific context of this event class. */
		out_specific_context_fc = create_field_class_copy(md_maps,
				in_event_specific_context);

		copy_field_class_content(md_maps,
				in_event_specific_context, out_specific_context_fc);
		if (ret) {
			goto error;
		}
		/*
		 * Add the output specific context to the output event
		 * class.
		 */
		status = bt_event_class_set_specific_context_field_class(
				out_event_class, out_specific_context_fc);
		if (status != BT_EVENT_CLASS_STATUS_OK) {
			BT_LOGE("Error setting event class' specific context "
				"field class: ec-addr=%p, ctx-fc-addr=%p",
				out_event_class, out_specific_context_fc);
			ret = -1;
			goto error;
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
	/* Copy the payload of this event class. */
		out_payload_fc = create_field_class_copy(md_maps,
				in_event_payload);
		copy_field_class_content(md_maps,
				in_event_payload, out_payload_fc);
		if (ret) {
			goto error;
		}

		/* Add the output payload to the output event class. */
		status = bt_event_class_set_payload_field_class(
				out_event_class, out_payload_fc);
		if (status != BT_EVENT_CLASS_STATUS_OK) {
			BT_LOGE("Error setting event class' payload "
				"field class: ec-addr=%p, payload-fc-addr=%p",
				out_event_class, out_payload_fc);
			ret = -1;
			goto error;
		}
	}

	BT_LOGD("Copied content of event class: in-ec-addr=%p, out-ec-addr=%p",
			in_event_class, out_event_class);
error:
	return ret;
}

BT_HIDDEN
int copy_event_common_context_field_class_content(
		struct trace_ir_metadata_maps *md_maps,
		const char *debug_info_fc_name,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	bt_field_class_status status;
	bt_field_class *debug_field_class = NULL, *bin_field_class = NULL,
		       *func_field_class = NULL, *src_field_class = NULL;
	int ret = 0;

	BT_LOGD("Copying content of event common context field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);

	/* Copy the content of the input common context. */
	ret = copy_field_class_content(md_maps, in_field_class, out_field_class);
	if (ret) {
		goto error;
	}

	/*
	 * If this event common context has the necessary fields to compute the
	 * debug information append the debug-info field class to the event
	 * common context.
	 */
	if (is_event_common_ctx_dbg_info_compatible(in_field_class, debug_info_fc_name)) {
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
			BT_LOGE_STR("Failed to create debug_info structure.");
			ret = -1;
			goto error;
		}

		bin_field_class = bt_field_class_string_create(
				md_maps->output_trace_class);
		if (!bin_field_class) {
			BT_LOGE_STR("Failed to create string for field=bin.");
			ret = -1;
			goto error;
		}

		func_field_class = bt_field_class_string_create(
				md_maps->output_trace_class);
		if (!func_field_class) {
			BT_LOGE_STR("Failed to create string for field=func.");
			ret = -1;
			goto error;
		}

		src_field_class = bt_field_class_string_create(
				md_maps->output_trace_class);
		if (!src_field_class) {
			BT_LOGE_STR("Failed to create string for field=src.");
			ret = -1;
			goto error;
		}

		status = bt_field_class_structure_append_member(
				debug_field_class, "bin", bin_field_class);
		if (status != BT_FIELD_CLASS_STATUS_OK) {
			BT_LOGE_STR("Failed to add a field to debug_info "
					"struct: field=bin.");
			ret = -1;
			goto error;
		}
		BT_FIELD_CLASS_PUT_REF_AND_RESET(bin_field_class);

		status = bt_field_class_structure_append_member(
				debug_field_class, "func", func_field_class);
		if (status != BT_FIELD_CLASS_STATUS_OK) {
			BT_LOGE_STR("Failed to add a field to debug_info "
					"struct: field=func.");
			ret = -1;
			goto error;
		}
		BT_FIELD_CLASS_PUT_REF_AND_RESET(func_field_class);

		status = bt_field_class_structure_append_member(
				debug_field_class, "src", src_field_class);
		if (status != BT_FIELD_CLASS_STATUS_OK) {
			BT_LOGE_STR("Failed to add a field to debug_info "
					"struct: field=src.");
			ret = -1;
			goto error;
		}
		BT_FIELD_CLASS_PUT_REF_AND_RESET(src_field_class);

		/*Add the filled debug-info field class to the common context. */
		status = bt_field_class_structure_append_member(out_field_class,
				debug_info_fc_name,
				debug_field_class);
		if (status != BT_FIELD_CLASS_STATUS_OK) {
			BT_LOGE_STR("Failed to add debug_info field to "
					"event common context.");
			ret = -1;
			goto error;
		}
		BT_FIELD_CLASS_PUT_REF_AND_RESET(debug_field_class);
	}
	BT_LOGD("Copied content of event common context field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);
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
	return ret;
}

BT_HIDDEN
bt_field_class *create_field_class_copy(struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class)
{
	return create_field_class_copy_internal(md_maps, in_field_class);
}

BT_HIDDEN
int copy_field_class_content(struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	return copy_field_class_content_internal(md_maps, in_field_class,
			out_field_class);
}
