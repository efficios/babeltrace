/*
 * Copyright 2018 - Philippe Proulx <pproulx@efficios.com>
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
 */

#define BT_LOG_TAG "PLUGIN-CTF-METADATA-META-TRANSLATE"
#include "logging.h"

#include <babeltrace/babeltrace.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "ctf-meta-visitors.h"

static inline
bt_field_class *ctf_field_class_to_ir(struct ctf_field_class *fc,
		struct ctf_trace_class *tc,
		struct ctf_stream_class *sc,
		struct ctf_event_class *ec);

static inline
void ctf_field_class_int_set_props(struct ctf_field_class_int *fc,
		bt_field_class *ir_fc)
{
	bt_field_class_integer_set_field_value_range(ir_fc,
		fc->base.size);
	bt_field_class_integer_set_preferred_display_base(ir_fc,
		fc->disp_base);
}

static inline
bt_field_class *ctf_field_class_int_to_ir(
		struct ctf_field_class_int *fc)
{
	bt_field_class *ir_fc;

	if (fc->is_signed) {
		ir_fc = bt_field_class_signed_integer_create();
	} else {
		ir_fc = bt_field_class_unsigned_integer_create();
	}

	BT_ASSERT(ir_fc);
	ctf_field_class_int_set_props(fc, ir_fc);
	return ir_fc;
}

static inline
bt_field_class *ctf_field_class_enum_to_ir(
		struct ctf_field_class_enum *fc)
{
	int ret;
	bt_field_class *ir_fc;
	uint64_t i;

	if (fc->base.is_signed) {
		ir_fc = bt_field_class_signed_enumeration_create();
	} else {
		ir_fc = bt_field_class_unsigned_enumeration_create();
	}

	BT_ASSERT(ir_fc);
	ctf_field_class_int_set_props((void *) fc, ir_fc);

	for (i = 0; i < fc->mappings->len; i++) {
		struct ctf_field_class_enum_mapping *mapping =
			ctf_field_class_enum_borrow_mapping_by_index(fc, i);

		if (fc->base.is_signed) {
			ret = bt_field_class_signed_enumeration_map_range(
				ir_fc, mapping->label->str,
				mapping->range.lower.i, mapping->range.upper.i);
		} else {
			ret = bt_field_class_unsigned_enumeration_map_range(
				ir_fc, mapping->label->str,
				mapping->range.lower.u, mapping->range.upper.u);
		}

		BT_ASSERT(ret == 0);
	}

	return ir_fc;
}

static inline
bt_field_class *ctf_field_class_float_to_ir(
		struct ctf_field_class_float *fc)
{
	bt_field_class *ir_fc;

	ir_fc = bt_field_class_real_create();
	BT_ASSERT(ir_fc);

	if (fc->base.size == 32) {
		bt_field_class_real_set_is_single_precision(ir_fc,
			BT_TRUE);
	}

	return ir_fc;
}

static inline
bt_field_class *ctf_field_class_string_to_ir(
		struct ctf_field_class_string *fc)
{
	bt_field_class *ir_fc =
		bt_field_class_string_create();

	BT_ASSERT(ir_fc);
	return ir_fc;
}

static inline
bt_field_class *ctf_field_class_struct_to_ir(
		struct ctf_field_class_struct *fc,
		struct ctf_trace_class *tc,
		struct ctf_stream_class *sc,
		struct ctf_event_class *ec)
{
	int ret;
	bt_field_class *ir_fc =
		bt_field_class_structure_create();
	uint64_t i;

	BT_ASSERT(ir_fc);

	for (i = 0; i < fc->members->len; i++) {
		struct ctf_named_field_class *named_fc =
			ctf_field_class_struct_borrow_member_by_index(fc, i);
		bt_field_class *member_ir_fc;

		if (!named_fc->fc->in_ir) {
			continue;
		}

		member_ir_fc = ctf_field_class_to_ir(named_fc->fc, tc, sc, ec);
		BT_ASSERT(member_ir_fc);
		ret = bt_field_class_structure_append_member(
			ir_fc, named_fc->name->str, member_ir_fc);
		BT_ASSERT(ret == 0);
		bt_field_class_put_ref(member_ir_fc);
	}

	return ir_fc;
}

static inline
bt_field_class *borrow_ir_ft_from_field_path(
		struct ctf_field_path *field_path,
		struct ctf_trace_class *tc,
		struct ctf_stream_class *sc,
		struct ctf_event_class *ec)
{
	bt_field_class *ir_fc = NULL;
	struct ctf_field_class *fc = ctf_field_path_borrow_field_class(
		field_path, tc, sc, ec);

	BT_ASSERT(fc);

	if (fc->in_ir) {
		ir_fc = fc->ir_fc;
	}

	return ir_fc;
}

static inline
bt_field_class *ctf_field_class_variant_to_ir(
		struct ctf_field_class_variant *fc,
		struct ctf_trace_class *tc,
		struct ctf_stream_class *sc,
		struct ctf_event_class *ec)
{
	int ret;
	bt_field_class *ir_fc =
		bt_field_class_variant_create();
	uint64_t i;

	BT_ASSERT(ir_fc);
	ret = bt_field_class_variant_set_selector_field_class(
		ir_fc, borrow_ir_ft_from_field_path(&fc->tag_path, tc, sc, ec));
	BT_ASSERT(ret == 0);

	for (i = 0; i < fc->options->len; i++) {
		struct ctf_named_field_class *named_fc =
			ctf_field_class_variant_borrow_option_by_index(fc, i);
		bt_field_class *option_ir_fc;

		BT_ASSERT(named_fc->fc->in_ir);
		option_ir_fc = ctf_field_class_to_ir(named_fc->fc, tc, sc, ec);
		BT_ASSERT(option_ir_fc);
		ret = bt_field_class_variant_append_option(
			ir_fc, named_fc->name->str, option_ir_fc);
		BT_ASSERT(ret == 0);
		bt_field_class_put_ref(option_ir_fc);
	}

	return ir_fc;
}

static inline
bt_field_class *ctf_field_class_array_to_ir(
		struct ctf_field_class_array *fc,
		struct ctf_trace_class *tc,
		struct ctf_stream_class *sc,
		struct ctf_event_class *ec)
{
	bt_field_class *ir_fc;
	bt_field_class *elem_ir_fc;

	if (fc->base.is_text) {
		ir_fc = bt_field_class_string_create();
		BT_ASSERT(ir_fc);
		goto end;
	}

	elem_ir_fc = ctf_field_class_to_ir(fc->base.elem_fc, tc, sc, ec);
	BT_ASSERT(elem_ir_fc);
	ir_fc = bt_field_class_static_array_create(elem_ir_fc,
		fc->length);
	BT_ASSERT(ir_fc);
	bt_field_class_put_ref(elem_ir_fc);

end:
	return ir_fc;
}

static inline
bt_field_class *ctf_field_class_sequence_to_ir(
		struct ctf_field_class_sequence *fc,
		struct ctf_trace_class *tc,
		struct ctf_stream_class *sc,
		struct ctf_event_class *ec)
{
	int ret;
	bt_field_class *ir_fc;
	bt_field_class *elem_ir_fc;

	if (fc->base.is_text) {
		ir_fc = bt_field_class_string_create();
		BT_ASSERT(ir_fc);
		goto end;
	}

	elem_ir_fc = ctf_field_class_to_ir(fc->base.elem_fc, tc, sc, ec);
	BT_ASSERT(elem_ir_fc);
	ir_fc = bt_field_class_dynamic_array_create(elem_ir_fc);
	BT_ASSERT(ir_fc);
	bt_field_class_put_ref(elem_ir_fc);
	BT_ASSERT(ir_fc);
	ret = bt_field_class_dynamic_array_set_length_field_class(
		ir_fc,
		borrow_ir_ft_from_field_path(&fc->length_path, tc, sc, ec));
	BT_ASSERT(ret == 0);

end:
	return ir_fc;
}

static inline
bt_field_class *ctf_field_class_to_ir(struct ctf_field_class *fc,
		struct ctf_trace_class *tc,
		struct ctf_stream_class *sc,
		struct ctf_event_class *ec)
{
	bt_field_class *ir_fc = NULL;

	BT_ASSERT(fc);
	BT_ASSERT(fc->in_ir);

	switch (fc->type) {
	case CTF_FIELD_CLASS_TYPE_INT:
		ir_fc = ctf_field_class_int_to_ir((void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_ENUM:
		ir_fc = ctf_field_class_enum_to_ir((void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_FLOAT:
		ir_fc = ctf_field_class_float_to_ir((void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_STRING:
		ir_fc = ctf_field_class_string_to_ir((void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_STRUCT:
		ir_fc = ctf_field_class_struct_to_ir((void *) fc, tc, sc, ec);
		break;
	case CTF_FIELD_CLASS_TYPE_ARRAY:
		ir_fc = ctf_field_class_array_to_ir((void *) fc, tc, sc, ec);
		break;
	case CTF_FIELD_CLASS_TYPE_SEQUENCE:
		ir_fc = ctf_field_class_sequence_to_ir((void *) fc, tc, sc, ec);
		break;
	case CTF_FIELD_CLASS_TYPE_VARIANT:
		ir_fc = ctf_field_class_variant_to_ir((void *) fc, tc, sc, ec);
		break;
	default:
		abort();
	}

	fc->ir_fc = ir_fc;
	return ir_fc;
}

static inline
bool ctf_field_class_struct_has_immediate_member_in_ir(
		struct ctf_field_class_struct *fc)
{
	uint64_t i;
	bool has_immediate_member_in_ir = false;

	for (i = 0; i < fc->members->len; i++) {
		struct ctf_named_field_class *named_fc =
			ctf_field_class_struct_borrow_member_by_index(fc, i);

		if (named_fc->fc->in_ir) {
			has_immediate_member_in_ir = true;
			goto end;
		}
	}

end:
	return has_immediate_member_in_ir;
}

static inline
bt_field_class *scope_ctf_field_class_to_ir(struct ctf_field_class *fc,
	struct ctf_trace_class *tc,
	struct ctf_stream_class *sc,
	struct ctf_event_class *ec)
{
	bt_field_class *ir_fc = NULL;

	if (!fc) {
		goto end;
	}

	BT_ASSERT(fc->type == CTF_FIELD_CLASS_TYPE_STRUCT);

	if (!ctf_field_class_struct_has_immediate_member_in_ir((void *) fc)) {
		/*
		 * Nothing for IR in this scope: typical for packet
		 * header, packet context, and event header.
		 */
		goto end;
	}

	ir_fc = ctf_field_class_to_ir(fc, tc, sc, ec);

end:
	return ir_fc;
}

static inline
struct ctf_field_class_int *borrow_named_int_field_class(
		struct ctf_field_class_struct *struct_fc, const char *name)
{
	struct ctf_named_field_class *named_fc = NULL;
	struct ctf_field_class_int *int_fc = NULL;

	if (!struct_fc) {
		goto end;
	}

	named_fc = ctf_field_class_struct_borrow_member_by_name(struct_fc, name);
	if (!named_fc) {
		goto end;
	}

	if (named_fc->fc->type != CTF_FIELD_CLASS_TYPE_INT &&
			named_fc->fc->type != CTF_FIELD_CLASS_TYPE_ENUM) {
		goto end;
	}

	int_fc = (void *) named_fc->fc;

end:
	return int_fc;
}

static inline
bt_event_class *ctf_event_class_to_ir(struct ctf_event_class *ec,
		bt_stream_class *ir_sc, struct ctf_trace_class *tc,
		struct ctf_stream_class *sc)
{
	int ret;
	bt_event_class *ir_ec = NULL;

	if (ec->is_translated) {
		ir_ec = bt_stream_class_borrow_event_class_by_id(
			ir_sc, ec->id);
		BT_ASSERT(ir_ec);
		goto end;
	}

	ir_ec = bt_event_class_create_with_id(ir_sc, ec->id);
	BT_ASSERT(ir_ec);
	bt_event_class_put_ref(ir_ec);

	if (ec->spec_context_fc) {
		bt_field_class *ir_fc = scope_ctf_field_class_to_ir(
			ec->spec_context_fc, tc, sc, ec);

		if (ir_fc) {
			ret = bt_event_class_set_specific_context_field_class(
				ir_ec, ir_fc);
			BT_ASSERT(ret == 0);
			bt_field_class_put_ref(ir_fc);
		}
	}

	if (ec->payload_fc) {
		bt_field_class *ir_fc = scope_ctf_field_class_to_ir(
			ec->payload_fc, tc, sc, ec);

		if (ir_fc) {
			ret = bt_event_class_set_payload_field_class(ir_ec,
				ir_fc);
			BT_ASSERT(ret == 0);
			bt_field_class_put_ref(ir_fc);
		}
	}

	if (ec->name->len > 0) {
		ret = bt_event_class_set_name(ir_ec, ec->name->str);
		BT_ASSERT(ret == 0);
	}

	if (ec->emf_uri->len > 0) {
		ret = bt_event_class_set_emf_uri(ir_ec, ec->emf_uri->str);
		BT_ASSERT(ret == 0);
	}

	if (ec->log_level != -1) {
		bt_event_class_set_log_level(ir_ec, ec->log_level);
	}

	ec->is_translated = true;
	ec->ir_ec = ir_ec;

end:
	return ir_ec;
}


static inline
bt_stream_class *ctf_stream_class_to_ir(struct ctf_stream_class *sc,
		bt_trace_class *ir_tc, struct ctf_trace_class *tc)
{
	int ret;
	bt_stream_class *ir_sc = NULL;
	struct ctf_field_class_int *int_fc;

	if (sc->is_translated) {
		ir_sc = bt_trace_class_borrow_stream_class_by_id(ir_tc, sc->id);
		BT_ASSERT(ir_sc);
		goto end;
	}

	ir_sc = bt_stream_class_create_with_id(ir_tc, sc->id);
	BT_ASSERT(ir_sc);
	bt_stream_class_put_ref(ir_sc);

	if (sc->packet_context_fc) {
		bt_field_class *ir_fc = scope_ctf_field_class_to_ir(
			sc->packet_context_fc, tc, sc, NULL);

		if (ir_fc) {
			ret = bt_stream_class_set_packet_context_field_class(
				ir_sc, ir_fc);
			BT_ASSERT(ret == 0);
			bt_field_class_put_ref(ir_fc);
		}
	}

	if (sc->event_header_fc) {
		bt_field_class *ir_fc = scope_ctf_field_class_to_ir(
			sc->event_header_fc, tc, sc, NULL);

		if (ir_fc) {
			ret = bt_stream_class_set_event_header_field_class(
				ir_sc, ir_fc);
			BT_ASSERT(ret == 0);
			bt_field_class_put_ref(ir_fc);
		}
	}

	if (sc->event_common_context_fc) {
		bt_field_class *ir_fc = scope_ctf_field_class_to_ir(
			sc->event_common_context_fc, tc, sc, NULL);

		if (ir_fc) {
			ret = bt_stream_class_set_event_common_context_field_class(
				ir_sc, ir_fc);
			BT_ASSERT(ret == 0);
			bt_field_class_put_ref(ir_fc);
		}
	}

	bt_stream_class_set_assigns_automatic_event_class_id(ir_sc,
		BT_FALSE);
	bt_stream_class_set_assigns_automatic_stream_id(ir_sc, BT_FALSE);

	if (sc->default_clock_class) {
		BT_ASSERT(sc->default_clock_class->ir_cc);
		ret = bt_stream_class_set_default_clock_class(ir_sc,
			sc->default_clock_class->ir_cc);
		BT_ASSERT(ret == 0);
	}

	int_fc = borrow_named_int_field_class((void *) sc->packet_context_fc,
		"events_discarded");
	if (int_fc) {
		if (int_fc->meaning == CTF_FIELD_CLASS_MEANING_DISC_EV_REC_COUNTER_SNAPSHOT) {
			bt_stream_class_set_packets_have_discarded_event_counter_snapshot(
				ir_sc, BT_TRUE);
		}
	}

	int_fc = borrow_named_int_field_class((void *) sc->packet_context_fc,
		"packet_seq_num");
	if (int_fc) {
		if (int_fc->meaning == CTF_FIELD_CLASS_MEANING_PACKET_COUNTER_SNAPSHOT) {
			bt_stream_class_set_packets_have_packet_counter_snapshot(
				ir_sc, BT_TRUE);
		}
	}

	int_fc = borrow_named_int_field_class((void *) sc->packet_context_fc,
		"timestamp_begin");
	if (int_fc) {
		if (int_fc->meaning == CTF_FIELD_CLASS_MEANING_PACKET_BEGINNING_TIME) {
			bt_stream_class_set_packets_have_default_beginning_clock_snapshot(
				ir_sc, BT_TRUE);
		}
	}

	int_fc = borrow_named_int_field_class((void *) sc->packet_context_fc,
		"timestamp_end");
	if (int_fc) {
		if (int_fc->meaning == CTF_FIELD_CLASS_MEANING_PACKET_END_TIME) {
			bt_stream_class_set_packets_have_default_end_clock_snapshot(
				ir_sc, BT_TRUE);
		}
	}

	sc->is_translated = true;
	sc->ir_sc = ir_sc;

end:
	return ir_sc;
}

static inline
void ctf_clock_class_to_ir(bt_clock_class *ir_cc, struct ctf_clock_class *cc)
{
	int ret;

	if (strlen(cc->name->str) > 0) {
		ret = bt_clock_class_set_name(ir_cc, cc->name->str);
		BT_ASSERT(ret == 0);
	}

	if (strlen(cc->description->str) > 0) {
		ret = bt_clock_class_set_description(ir_cc, cc->description->str);
		BT_ASSERT(ret == 0);
	}

	bt_clock_class_set_frequency(ir_cc, cc->frequency);
	bt_clock_class_set_precision(ir_cc, cc->precision);
	bt_clock_class_set_offset(ir_cc, cc->offset_seconds, cc->offset_cycles);

	if (cc->has_uuid) {
		bt_clock_class_set_uuid(ir_cc, cc->uuid);
	}

	bt_clock_class_set_is_absolute(ir_cc, cc->is_absolute);
}

static inline
int ctf_trace_class_to_ir(bt_trace_class *ir_tc, struct ctf_trace_class *tc)
{
	int ret = 0;
	uint64_t i;

	if (tc->is_translated) {
		goto end;
	}

	if (tc->packet_header_fc) {
		bt_field_class *ir_fc = scope_ctf_field_class_to_ir(
			tc->packet_header_fc, tc, NULL, NULL);

		if (ir_fc) {
			ret = bt_trace_class_set_packet_header_field_class(
				ir_tc, ir_fc);
			BT_ASSERT(ret == 0);
			bt_field_class_put_ref(ir_fc);
		}
	}

	if (tc->is_uuid_set) {
		bt_trace_class_set_uuid(ir_tc, tc->uuid);
	}

	for (i = 0; i < tc->env_entries->len; i++) {
		struct ctf_trace_class_env_entry *env_entry =
			ctf_trace_class_borrow_env_entry_by_index(tc, i);

		switch (env_entry->type) {
		case CTF_TRACE_CLASS_ENV_ENTRY_TYPE_INT:
			ret = bt_trace_class_set_environment_entry_integer(
				ir_tc, env_entry->name->str,
				env_entry->value.i);
			break;
		case CTF_TRACE_CLASS_ENV_ENTRY_TYPE_STR:
			ret = bt_trace_class_set_environment_entry_string(
				ir_tc, env_entry->name->str,
				env_entry->value.str->str);
			break;
		default:
			abort();
		}

		if (ret) {
			goto end;
		}
	}

	for (i = 0; i < tc->clock_classes->len; i++) {
		struct ctf_clock_class *cc = tc->clock_classes->pdata[i];

		cc->ir_cc = bt_clock_class_create(ir_tc);
		ctf_clock_class_to_ir(cc->ir_cc, cc);
	}

	bt_trace_class_set_assigns_automatic_stream_class_id(ir_tc,
		BT_FALSE);
	tc->is_translated = true;
	tc->ir_tc = ir_tc;

end:
	return ret;
}

BT_HIDDEN
int ctf_trace_class_translate(bt_trace_class *ir_tc,
		struct ctf_trace_class *tc)
{
	int ret = 0;
	uint64_t i;

	ret = ctf_trace_class_to_ir(ir_tc, tc);
	if (ret) {
		goto end;
	}

	for (i = 0; i < tc->stream_classes->len; i++) {
		uint64_t j;
		struct ctf_stream_class *sc = tc->stream_classes->pdata[i];
		bt_stream_class *ir_sc;

		ir_sc = ctf_stream_class_to_ir(sc, ir_tc, tc);
		if (!ir_sc) {
			ret = -1;
			goto end;
		}

		for (j = 0; j < sc->event_classes->len; j++) {
			struct ctf_event_class *ec = sc->event_classes->pdata[j];
			bt_event_class *ir_ec;

			ir_ec = ctf_event_class_to_ir(ec, ir_sc, tc, sc);
			if (!ir_ec) {
				ret = -1;
				goto end;
			}
		}
	}

end:
	return ret;
}
