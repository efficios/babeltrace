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
struct bt_field_type *ctf_field_type_to_ir(struct ctf_field_type *ft,
		struct ctf_trace_class *tc,
		struct ctf_stream_class *sc,
		struct ctf_event_class *ec);

static inline
void ctf_field_type_int_set_props(struct ctf_field_type_int *ft,
		struct bt_field_type *ir_ft)
{
	int ret;

	ret = bt_field_type_integer_set_field_value_range(ir_ft, ft->base.size);
	BT_ASSERT(ret == 0);
	ret = bt_field_type_integer_set_preferred_display_base(ir_ft,
		ft->disp_base);
	BT_ASSERT(ret == 0);
}

static inline
struct bt_field_type *ctf_field_type_int_to_ir(struct ctf_field_type_int *ft)
{
	struct bt_field_type *ir_ft;

	if (ft->is_signed) {
		ir_ft = bt_field_type_signed_integer_create();
	} else {
		ir_ft = bt_field_type_unsigned_integer_create();
	}

	BT_ASSERT(ir_ft);
	ctf_field_type_int_set_props(ft, ir_ft);
	return ir_ft;
}

static inline
struct bt_field_type *ctf_field_type_enum_to_ir(struct ctf_field_type_enum *ft)
{
	int ret;
	struct bt_field_type *ir_ft;
	uint64_t i;

	if (ft->base.is_signed) {
		ir_ft = bt_field_type_signed_enumeration_create();
	} else {
		ir_ft = bt_field_type_unsigned_enumeration_create();
	}

	BT_ASSERT(ir_ft);
	ctf_field_type_int_set_props((void *) ft, ir_ft);

	for (i = 0; i < ft->mappings->len; i++) {
		struct ctf_field_type_enum_mapping *mapping =
			ctf_field_type_enum_borrow_mapping_by_index(ft, i);

		if (ft->base.is_signed) {
			ret = bt_field_type_signed_enumeration_map_range(
				ir_ft, mapping->label->str,
				mapping->range.lower.i, mapping->range.upper.i);
		} else {
			ret = bt_field_type_unsigned_enumeration_map_range(
				ir_ft, mapping->label->str,
				mapping->range.lower.u, mapping->range.upper.u);
		}

		BT_ASSERT(ret == 0);
	}

	return ir_ft;
}

static inline
struct bt_field_type *ctf_field_type_float_to_ir(
		struct ctf_field_type_float *ft)
{
	struct bt_field_type *ir_ft;
	int ret;

	ir_ft = bt_field_type_real_create();
	BT_ASSERT(ir_ft);

	if (ft->base.size == 32) {
		ret = bt_field_type_real_set_is_single_precision(ir_ft,
			BT_TRUE);
		BT_ASSERT(ret == 0);
	}

	return ir_ft;
}

static inline
struct bt_field_type *ctf_field_type_string_to_ir(
		struct ctf_field_type_string *ft)
{
	struct bt_field_type *ir_ft = bt_field_type_string_create();

	BT_ASSERT(ir_ft);
	return ir_ft;
}

static inline
struct bt_field_type *ctf_field_type_struct_to_ir(
		struct ctf_field_type_struct *ft,
		struct ctf_trace_class *tc,
		struct ctf_stream_class *sc,
		struct ctf_event_class *ec)
{
	int ret;
	struct bt_field_type *ir_ft = bt_field_type_structure_create();
	uint64_t i;

	BT_ASSERT(ir_ft);

	for (i = 0; i < ft->members->len; i++) {
		struct ctf_named_field_type *named_ft =
			ctf_field_type_struct_borrow_member_by_index(ft, i);
		struct bt_field_type *member_ir_ft;

		if (!named_ft->ft->in_ir) {
			continue;
		}

		member_ir_ft = ctf_field_type_to_ir(named_ft->ft, tc, sc, ec);
		BT_ASSERT(member_ir_ft);
		ret = bt_field_type_structure_append_member(ir_ft,
			named_ft->name->str, member_ir_ft);
		BT_ASSERT(ret == 0);
		bt_put(member_ir_ft);
	}

	return ir_ft;
}

static inline
struct bt_field_type *borrow_ir_ft_from_field_path(
		struct ctf_field_path *field_path,
		struct ctf_trace_class *tc,
		struct ctf_stream_class *sc,
		struct ctf_event_class *ec)
{
	struct bt_field_type *ir_ft = NULL;
	struct ctf_field_type *ft = ctf_field_path_borrow_field_type(
		field_path, tc, sc, ec);

	BT_ASSERT(ft);

	if (ft->in_ir) {
		ir_ft = ft->ir_ft;
	}

	return ir_ft;
}

static inline
struct bt_field_type *ctf_field_type_variant_to_ir(
		struct ctf_field_type_variant *ft,
		struct ctf_trace_class *tc,
		struct ctf_stream_class *sc,
		struct ctf_event_class *ec)
{
	int ret;
	struct bt_field_type *ir_ft = bt_field_type_variant_create();
	uint64_t i;

	BT_ASSERT(ir_ft);
	ret = bt_field_type_variant_set_selector_field_type(ir_ft,
		borrow_ir_ft_from_field_path(&ft->tag_path, tc, sc, ec));
	BT_ASSERT(ret == 0);

	for (i = 0; i < ft->options->len; i++) {
		struct ctf_named_field_type *named_ft =
			ctf_field_type_variant_borrow_option_by_index(ft, i);
		struct bt_field_type *option_ir_ft;

		BT_ASSERT(named_ft->ft->in_ir);
		option_ir_ft = ctf_field_type_to_ir(named_ft->ft, tc, sc, ec);
		BT_ASSERT(option_ir_ft);
		ret = bt_field_type_variant_append_option(ir_ft,
			named_ft->name->str, option_ir_ft);
		BT_ASSERT(ret == 0);
		bt_put(option_ir_ft);
	}

	return ir_ft;
}

static inline
struct bt_field_type *ctf_field_type_array_to_ir(
		struct ctf_field_type_array *ft,
		struct ctf_trace_class *tc,
		struct ctf_stream_class *sc,
		struct ctf_event_class *ec)
{
	struct bt_field_type *ir_ft;
	struct bt_field_type *elem_ir_ft;

	if (ft->base.is_text) {
		ir_ft = bt_field_type_string_create();
		BT_ASSERT(ir_ft);
		goto end;
	}

	elem_ir_ft = ctf_field_type_to_ir(ft->base.elem_ft, tc, sc, ec);
	BT_ASSERT(elem_ir_ft);
	ir_ft = bt_field_type_static_array_create(elem_ir_ft, ft->length);
	BT_ASSERT(ir_ft);
	bt_put(elem_ir_ft);

end:
	return ir_ft;
}

static inline
struct bt_field_type *ctf_field_type_sequence_to_ir(
		struct ctf_field_type_sequence *ft,
		struct ctf_trace_class *tc,
		struct ctf_stream_class *sc,
		struct ctf_event_class *ec)
{
	int ret;
	struct bt_field_type *ir_ft;
	struct bt_field_type *elem_ir_ft;

	if (ft->base.is_text) {
		ir_ft = bt_field_type_string_create();
		BT_ASSERT(ir_ft);
		goto end;
	}

	elem_ir_ft = ctf_field_type_to_ir(ft->base.elem_ft, tc, sc, ec);
	BT_ASSERT(elem_ir_ft);
	ir_ft = bt_field_type_dynamic_array_create(elem_ir_ft);
	BT_ASSERT(ir_ft);
	bt_put(elem_ir_ft);
	BT_ASSERT(ir_ft);
	ret = bt_field_type_dynamic_array_set_length_field_type(ir_ft,
		borrow_ir_ft_from_field_path(&ft->length_path, tc, sc, ec));
	BT_ASSERT(ret == 0);

end:
	return ir_ft;
}

static inline
struct bt_field_type *ctf_field_type_to_ir(struct ctf_field_type *ft,
		struct ctf_trace_class *tc,
		struct ctf_stream_class *sc,
		struct ctf_event_class *ec)
{
	struct bt_field_type *ir_ft = NULL;

	BT_ASSERT(ft);
	BT_ASSERT(ft->in_ir);

	switch (ft->id) {
	case CTF_FIELD_TYPE_ID_INT:
		ir_ft = ctf_field_type_int_to_ir((void *) ft);
		break;
	case CTF_FIELD_TYPE_ID_ENUM:
		ir_ft = ctf_field_type_enum_to_ir((void *) ft);
		break;
	case CTF_FIELD_TYPE_ID_FLOAT:
		ir_ft = ctf_field_type_float_to_ir((void *) ft);
		break;
	case CTF_FIELD_TYPE_ID_STRING:
		ir_ft = ctf_field_type_string_to_ir((void *) ft);
		break;
	case CTF_FIELD_TYPE_ID_STRUCT:
		ir_ft = ctf_field_type_struct_to_ir((void *) ft, tc, sc, ec);
		break;
	case CTF_FIELD_TYPE_ID_ARRAY:
		ir_ft = ctf_field_type_array_to_ir((void *) ft, tc, sc, ec);
		break;
	case CTF_FIELD_TYPE_ID_SEQUENCE:
		ir_ft = ctf_field_type_sequence_to_ir((void *) ft, tc, sc, ec);
		break;
	case CTF_FIELD_TYPE_ID_VARIANT:
		ir_ft = ctf_field_type_variant_to_ir((void *) ft, tc, sc, ec);
		break;
	default:
		abort();
	}

	ft->ir_ft = ir_ft;
	return ir_ft;
}

static inline
bool ctf_field_type_struct_has_immediate_member_in_ir(
		struct ctf_field_type_struct *ft)
{
	uint64_t i;
	bool has_immediate_member_in_ir = false;

	for (i = 0; i < ft->members->len; i++) {
		struct ctf_named_field_type *named_ft =
			ctf_field_type_struct_borrow_member_by_index(ft, i);

		if (named_ft->ft->in_ir) {
			has_immediate_member_in_ir = true;
			goto end;
		}
	}

end:
	return has_immediate_member_in_ir;
}

static inline
struct bt_field_type *scope_ctf_field_type_to_ir(struct ctf_field_type *ft,
	struct ctf_trace_class *tc,
	struct ctf_stream_class *sc,
	struct ctf_event_class *ec)
{
	struct bt_field_type *ir_ft = NULL;

	if (!ft) {
		goto end;
	}

	BT_ASSERT(ft->id == CTF_FIELD_TYPE_ID_STRUCT);

	if (!ctf_field_type_struct_has_immediate_member_in_ir((void *) ft)) {
		/*
		 * Nothing for IR in this scope: typical for packet
		 * header, packet context, and event header.
		 */
		goto end;
	}

	ir_ft = ctf_field_type_to_ir(ft, tc, sc, ec);

end:
	return ir_ft;
}

static inline
struct ctf_field_type_int *borrow_named_int_field_type(
		struct ctf_field_type_struct *struct_ft, const char *name)
{
	struct ctf_named_field_type *named_ft = NULL;
	struct ctf_field_type_int *int_ft = NULL;

	if (!struct_ft) {
		goto end;
	}

	named_ft = ctf_field_type_struct_borrow_member_by_name(struct_ft, name);
	if (!named_ft) {
		goto end;
	}

	if (named_ft->ft->id != CTF_FIELD_TYPE_ID_INT &&
			named_ft->ft->id != CTF_FIELD_TYPE_ID_ENUM) {
		goto end;
	}

	int_ft = (void *) named_ft->ft;

end:
	return int_ft;
}

static inline
struct bt_event_class *ctf_event_class_to_ir(struct ctf_event_class *ec,
		struct bt_stream_class *ir_sc, struct ctf_trace_class *tc,
		struct ctf_stream_class *sc)
{
	int ret;
	struct bt_event_class *ir_ec = NULL;

	if (ec->is_translated) {
		ir_ec = bt_stream_class_borrow_event_class_by_id(
			ir_sc, ec->id);
		BT_ASSERT(ir_ec);
		goto end;
	}

	ir_ec = bt_event_class_create_with_id(ir_sc, ec->id);
	BT_ASSERT(ir_ec);
	bt_put(ir_ec);

	if (ec->spec_context_ft) {
		struct bt_field_type *ir_ft = scope_ctf_field_type_to_ir(
			ec->spec_context_ft, tc, sc, ec);

		if (ir_ft) {
			ret = bt_event_class_set_specific_context_field_type(
				ir_ec, ir_ft);
			BT_ASSERT(ret == 0);
			bt_put(ir_ft);
		}
	}

	if (ec->payload_ft) {
		struct bt_field_type *ir_ft = scope_ctf_field_type_to_ir(
			ec->payload_ft, tc, sc, ec);

		if (ir_ft) {
			ret = bt_event_class_set_payload_field_type(ir_ec,
				ir_ft);
			BT_ASSERT(ret == 0);
			bt_put(ir_ft);
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
		ret = bt_event_class_set_log_level(ir_ec, ec->log_level);
		BT_ASSERT(ret == 0);
	}

	ec->is_translated = true;
	ec->ir_ec = ir_ec;

end:
	return ir_ec;
}


static inline
struct bt_stream_class *ctf_stream_class_to_ir(struct ctf_stream_class *sc,
		struct bt_trace *ir_trace, struct ctf_trace_class *tc)
{
	int ret;
	struct bt_stream_class *ir_sc = NULL;
	struct ctf_field_type_int *int_ft;

	if (sc->is_translated) {
		ir_sc = bt_trace_borrow_stream_class_by_id(ir_trace, sc->id);
		BT_ASSERT(ir_sc);
		goto end;
	}

	ir_sc = bt_stream_class_create_with_id(ir_trace, sc->id);
	BT_ASSERT(ir_sc);
	bt_put(ir_sc);

	if (sc->packet_context_ft) {
		struct bt_field_type *ir_ft = scope_ctf_field_type_to_ir(
			sc->packet_context_ft, tc, sc, NULL);

		if (ir_ft) {
			ret = bt_stream_class_set_packet_context_field_type(
				ir_sc, ir_ft);
			BT_ASSERT(ret == 0);
			bt_put(ir_ft);
		}
	}

	if (sc->event_header_ft) {
		struct bt_field_type *ir_ft = scope_ctf_field_type_to_ir(
			sc->event_header_ft, tc, sc, NULL);

		if (ir_ft) {
			ret = bt_stream_class_set_event_header_field_type(ir_sc,
				ir_ft);
			BT_ASSERT(ret == 0);
			bt_put(ir_ft);
		}
	}

	if (sc->event_common_context_ft) {
		struct bt_field_type *ir_ft = scope_ctf_field_type_to_ir(
			sc->event_common_context_ft, tc, sc, NULL);

		if (ir_ft) {
			ret = bt_stream_class_set_event_common_context_field_type(
				ir_sc, ir_ft);
			BT_ASSERT(ret == 0);
			bt_put(ir_ft);
		}
	}

	ret = bt_stream_class_set_assigns_automatic_event_class_id(ir_sc,
		BT_FALSE);
	BT_ASSERT(ret == 0);
	ret = bt_stream_class_set_assigns_automatic_stream_id(ir_sc, BT_FALSE);
	BT_ASSERT(ret == 0);

	if (sc->default_clock_class) {
		ret = bt_stream_class_set_default_clock_class(ir_sc,
			sc->default_clock_class);
		BT_ASSERT(ret == 0);
	}

	int_ft = borrow_named_int_field_type((void *) sc->packet_context_ft,
		"events_discarded");
	if (int_ft) {
		if (int_ft->meaning == CTF_FIELD_TYPE_MEANING_DISC_EV_REC_COUNTER_SNAPSHOT) {
			ret = bt_stream_class_set_packets_have_discarded_event_counter_snapshot(
				ir_sc, BT_TRUE);
			BT_ASSERT(ret == 0);
		}
	}

	int_ft = borrow_named_int_field_type((void *) sc->packet_context_ft,
		"packet_seq_num");
	if (int_ft) {
		if (int_ft->meaning == CTF_FIELD_TYPE_MEANING_PACKET_COUNTER_SNAPSHOT) {
			ret = bt_stream_class_set_packets_have_packet_counter_snapshot(
				ir_sc, BT_TRUE);
			BT_ASSERT(ret == 0);
		}
	}

	int_ft = borrow_named_int_field_type((void *) sc->packet_context_ft,
		"timestamp_begin");
	if (int_ft) {
		if (int_ft->meaning == CTF_FIELD_TYPE_MEANING_PACKET_BEGINNING_TIME) {
			ret = bt_stream_class_set_packets_have_default_beginning_clock_value(
				ir_sc, BT_TRUE);
			BT_ASSERT(ret == 0);
		}
	}

	int_ft = borrow_named_int_field_type((void *) sc->packet_context_ft,
		"timestamp_end");
	if (int_ft) {
		if (int_ft->meaning == CTF_FIELD_TYPE_MEANING_PACKET_END_TIME) {
			ret = bt_stream_class_set_packets_have_default_end_clock_value(
				ir_sc, BT_TRUE);
			BT_ASSERT(ret == 0);
		}
	}

	sc->is_translated = true;
	sc->ir_sc = ir_sc;

end:
	return ir_sc;
}

static inline
int ctf_trace_class_to_ir(struct bt_trace *ir_trace,
		struct ctf_trace_class *tc)
{
	int ret = 0;
	uint64_t i;

	if (tc->is_translated) {
		goto end;
	}

	if (tc->packet_header_ft) {
		struct bt_field_type *ir_ft = scope_ctf_field_type_to_ir(
			tc->packet_header_ft, tc, NULL, NULL);

		if (ir_ft) {
			ret = bt_trace_set_packet_header_field_type(ir_trace,
				ir_ft);
			BT_ASSERT(ret == 0);
			bt_put(ir_ft);
		}
	}

	if (tc->name->len > 0) {
		ret = bt_trace_set_name(ir_trace, tc->name->str);
		if (ret) {
			goto end;
		}
	}

	if (tc->is_uuid_set) {
		ret = bt_trace_set_uuid(ir_trace, tc->uuid);
		if (ret) {
			goto end;
		}
	}

	for (i = 0; i < tc->env_entries->len; i++) {
		struct ctf_trace_class_env_entry *env_entry =
			ctf_trace_class_borrow_env_entry_by_index(tc, i);

		switch (env_entry->type) {
		case CTF_TRACE_CLASS_ENV_ENTRY_TYPE_INT:
			ret = bt_trace_set_environment_entry_integer(
				ir_trace, env_entry->name->str,
				env_entry->value.i);
			break;
		case CTF_TRACE_CLASS_ENV_ENTRY_TYPE_STR:
			ret = bt_trace_set_environment_entry_string(
				ir_trace, env_entry->name->str,
				env_entry->value.str->str);
			break;
		default:
			abort();
		}

		if (ret) {
			goto end;
		}
	}

	ret = bt_trace_set_assigns_automatic_stream_class_id(ir_trace,
		BT_FALSE);
	if (ret) {
		goto end;
	}

	tc->is_translated = true;
	tc->ir_tc = ir_trace;

end:
	return ret;
}

BT_HIDDEN
int ctf_trace_class_translate(struct bt_trace *ir_trace,
		struct ctf_trace_class *tc)
{
	int ret = 0;
	uint64_t i;

	ret = ctf_trace_class_to_ir(ir_trace, tc);
	if (ret) {
		goto end;
	}

	for (i = 0; i < tc->stream_classes->len; i++) {
		uint64_t j;
		struct ctf_stream_class *sc = tc->stream_classes->pdata[i];
		struct bt_stream_class *ir_sc;

		ir_sc = ctf_stream_class_to_ir(sc, ir_trace, tc);
		if (!ir_sc) {
			ret = -1;
			goto end;
		}

		for (j = 0; j < sc->event_classes->len; j++) {
			struct ctf_event_class *ec = sc->event_classes->pdata[j];
			struct bt_event_class *ir_ec;

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
