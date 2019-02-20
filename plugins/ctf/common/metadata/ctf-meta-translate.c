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

struct ctx {
	bt_trace_class *ir_tc;
	bt_stream_class *ir_sc;
	struct ctf_trace_class *tc;
	struct ctf_stream_class *sc;
	struct ctf_event_class *ec;
	enum ctf_scope scope;
};

static inline
bt_field_class *ctf_field_class_to_ir(struct ctx *ctx,
		struct ctf_field_class *fc);

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
bt_field_class *ctf_field_class_int_to_ir(struct ctx *ctx,
		struct ctf_field_class_int *fc)
{
	bt_field_class *ir_fc;

	if (fc->is_signed) {
		ir_fc = bt_field_class_signed_integer_create(ctx->ir_tc);
	} else {
		ir_fc = bt_field_class_unsigned_integer_create(ctx->ir_tc);
	}

	BT_ASSERT(ir_fc);
	ctf_field_class_int_set_props(fc, ir_fc);
	return ir_fc;
}

static inline
bt_field_class *ctf_field_class_enum_to_ir(struct ctx *ctx,
		struct ctf_field_class_enum *fc)
{
	int ret;
	bt_field_class *ir_fc;
	uint64_t i;

	if (fc->base.is_signed) {
		ir_fc = bt_field_class_signed_enumeration_create(ctx->ir_tc);
	} else {
		ir_fc = bt_field_class_unsigned_enumeration_create(ctx->ir_tc);
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
bt_field_class *ctf_field_class_float_to_ir(struct ctx *ctx,
		struct ctf_field_class_float *fc)
{
	bt_field_class *ir_fc;

	ir_fc = bt_field_class_real_create(ctx->ir_tc);
	BT_ASSERT(ir_fc);

	if (fc->base.size == 32) {
		bt_field_class_real_set_is_single_precision(ir_fc,
			BT_TRUE);
	}

	return ir_fc;
}

static inline
bt_field_class *ctf_field_class_string_to_ir(struct ctx *ctx,
		struct ctf_field_class_string *fc)
{
	bt_field_class *ir_fc = bt_field_class_string_create(ctx->ir_tc);

	BT_ASSERT(ir_fc);
	return ir_fc;
}

static inline
void translate_struct_field_class_members(struct ctx *ctx,
		struct ctf_field_class_struct *fc, bt_field_class *ir_fc,
		bool with_header_prefix,
		struct ctf_field_class_struct *context_fc)
{
	uint64_t i;
	int ret;

	for (i = 0; i < fc->members->len; i++) {
		struct ctf_named_field_class *named_fc =
			ctf_field_class_struct_borrow_member_by_index(fc, i);
		bt_field_class *member_ir_fc;
		const char *name = named_fc->name->str;

		if (!named_fc->fc->in_ir) {
			continue;
		}

		member_ir_fc = ctf_field_class_to_ir(ctx, named_fc->fc);
		BT_ASSERT(member_ir_fc);
		ret = bt_field_class_structure_append_member(ir_fc, name,
			member_ir_fc);
		BT_ASSERT(ret == 0);
		bt_field_class_put_ref(member_ir_fc);
	}
}

static inline
bt_field_class *ctf_field_class_struct_to_ir(struct ctx *ctx,
		struct ctf_field_class_struct *fc)
{
	bt_field_class *ir_fc = bt_field_class_structure_create(ctx->ir_tc);

	BT_ASSERT(ir_fc);
	translate_struct_field_class_members(ctx, fc, ir_fc, false, NULL);
	return ir_fc;
}

static inline
bt_field_class *borrow_ir_fc_from_field_path(struct ctx *ctx,
		struct ctf_field_path *field_path)
{
	bt_field_class *ir_fc = NULL;
	struct ctf_field_class *fc = ctf_field_path_borrow_field_class(
		field_path, ctx->tc, ctx->sc, ctx->ec);

	BT_ASSERT(fc);

	if (fc->in_ir) {
		ir_fc = fc->ir_fc;
	}

	return ir_fc;
}

static inline
bt_field_class *ctf_field_class_variant_to_ir(struct ctx *ctx,
		struct ctf_field_class_variant *fc)
{
	int ret;
	bt_field_class *ir_fc = bt_field_class_variant_create(ctx->ir_tc);
	uint64_t i;

	BT_ASSERT(ir_fc);

	if (fc->tag_path.root != CTF_SCOPE_PACKET_HEADER &&
			fc->tag_path.root != CTF_SCOPE_EVENT_HEADER) {
		ret = bt_field_class_variant_set_selector_field_class(
			ir_fc, borrow_ir_fc_from_field_path(ctx,
				&fc->tag_path));
		BT_ASSERT(ret == 0);
	}

	for (i = 0; i < fc->options->len; i++) {
		struct ctf_named_field_class *named_fc =
			ctf_field_class_variant_borrow_option_by_index(fc, i);
		bt_field_class *option_ir_fc;

		BT_ASSERT(named_fc->fc->in_ir);
		option_ir_fc = ctf_field_class_to_ir(ctx, named_fc->fc);
		BT_ASSERT(option_ir_fc);
		ret = bt_field_class_variant_append_option(
			ir_fc, named_fc->name->str, option_ir_fc);
		BT_ASSERT(ret == 0);
		bt_field_class_put_ref(option_ir_fc);
	}

	return ir_fc;
}

static inline
bt_field_class *ctf_field_class_array_to_ir(struct ctx *ctx,
		struct ctf_field_class_array *fc)
{
	bt_field_class *ir_fc;
	bt_field_class *elem_ir_fc;

	if (fc->base.is_text) {
		ir_fc = bt_field_class_string_create(ctx->ir_tc);
		BT_ASSERT(ir_fc);
		goto end;
	}

	elem_ir_fc = ctf_field_class_to_ir(ctx, fc->base.elem_fc);
	BT_ASSERT(elem_ir_fc);
	ir_fc = bt_field_class_static_array_create(ctx->ir_tc, elem_ir_fc,
		fc->length);
	BT_ASSERT(ir_fc);
	bt_field_class_put_ref(elem_ir_fc);

end:
	return ir_fc;
}

static inline
bt_field_class *ctf_field_class_sequence_to_ir(struct ctx *ctx,
		struct ctf_field_class_sequence *fc)
{
	int ret;
	bt_field_class *ir_fc;
	bt_field_class *elem_ir_fc;

	if (fc->base.is_text) {
		ir_fc = bt_field_class_string_create(ctx->ir_tc);
		BT_ASSERT(ir_fc);
		goto end;
	}

	elem_ir_fc = ctf_field_class_to_ir(ctx, fc->base.elem_fc);
	BT_ASSERT(elem_ir_fc);
	ir_fc = bt_field_class_dynamic_array_create(ctx->ir_tc, elem_ir_fc);
	BT_ASSERT(ir_fc);
	bt_field_class_put_ref(elem_ir_fc);
	BT_ASSERT(ir_fc);

	if (fc->length_path.root != CTF_SCOPE_PACKET_HEADER &&
			fc->length_path.root != CTF_SCOPE_EVENT_HEADER) {
		ret = bt_field_class_dynamic_array_set_length_field_class(
			ir_fc, borrow_ir_fc_from_field_path(ctx, &fc->length_path));
		BT_ASSERT(ret == 0);
	}

end:
	return ir_fc;
}

static inline
bt_field_class *ctf_field_class_to_ir(struct ctx *ctx,
		struct ctf_field_class *fc)
{
	bt_field_class *ir_fc = NULL;

	BT_ASSERT(fc);
	BT_ASSERT(fc->in_ir);

	switch (fc->type) {
	case CTF_FIELD_CLASS_TYPE_INT:
		ir_fc = ctf_field_class_int_to_ir(ctx, (void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_ENUM:
		ir_fc = ctf_field_class_enum_to_ir(ctx, (void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_FLOAT:
		ir_fc = ctf_field_class_float_to_ir(ctx, (void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_STRING:
		ir_fc = ctf_field_class_string_to_ir(ctx, (void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_STRUCT:
		ir_fc = ctf_field_class_struct_to_ir(ctx, (void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_ARRAY:
		ir_fc = ctf_field_class_array_to_ir(ctx, (void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_SEQUENCE:
		ir_fc = ctf_field_class_sequence_to_ir(ctx, (void *) fc);
		break;
	case CTF_FIELD_CLASS_TYPE_VARIANT:
		ir_fc = ctf_field_class_variant_to_ir(ctx, (void *) fc);
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
bt_field_class *scope_ctf_field_class_to_ir(struct ctx *ctx)
{
	bt_field_class *ir_fc = NULL;
	struct ctf_field_class *fc = NULL;

	switch (ctx->scope) {
	case CTF_SCOPE_PACKET_CONTEXT:
		fc = ctx->sc->packet_context_fc;
		break;
	case CTF_SCOPE_EVENT_COMMON_CONTEXT:
		fc = ctx->sc->event_common_context_fc;
		break;
	case CTF_SCOPE_EVENT_SPECIFIC_CONTEXT:
		fc = ctx->ec->spec_context_fc;
		break;
	case CTF_SCOPE_EVENT_PAYLOAD:
		fc = ctx->ec->payload_fc;
		break;
	default:
		abort();
	}

	if (fc && ctf_field_class_struct_has_immediate_member_in_ir(
			(void *) fc)) {
		ir_fc = ctf_field_class_to_ir(ctx, fc);
	}

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
void ctf_event_class_to_ir(struct ctx *ctx)
{
	int ret;
	bt_event_class *ir_ec = NULL;
	bt_field_class *ir_fc;

	BT_ASSERT(ctx->ec);

	if (ctx->ec->is_translated) {
		ir_ec = bt_stream_class_borrow_event_class_by_id(
			ctx->ir_sc, ctx->ec->id);
		BT_ASSERT(ir_ec);
		goto end;
	}

	ir_ec = bt_event_class_create_with_id(ctx->ir_sc, ctx->ec->id);
	BT_ASSERT(ir_ec);
	bt_event_class_put_ref(ir_ec);
	ctx->scope = CTF_SCOPE_EVENT_SPECIFIC_CONTEXT;
	ir_fc = scope_ctf_field_class_to_ir(ctx);
	if (ir_fc) {
		ret = bt_event_class_set_specific_context_field_class(
			ir_ec, ir_fc);
		BT_ASSERT(ret == 0);
		bt_field_class_put_ref(ir_fc);
	}

	ctx->scope = CTF_SCOPE_EVENT_PAYLOAD;
	ir_fc = scope_ctf_field_class_to_ir(ctx);
	if (ir_fc) {
		ret = bt_event_class_set_payload_field_class(ir_ec,
			ir_fc);
		BT_ASSERT(ret == 0);
		bt_field_class_put_ref(ir_fc);
	}

	if (ctx->ec->name->len > 0) {
		ret = bt_event_class_set_name(ir_ec, ctx->ec->name->str);
		BT_ASSERT(ret == 0);
	}

	if (ctx->ec->emf_uri->len > 0) {
		ret = bt_event_class_set_emf_uri(ir_ec, ctx->ec->emf_uri->str);
		BT_ASSERT(ret == 0);
	}

	if (ctx->ec->log_level != -1) {
		bt_event_class_set_log_level(ir_ec, ctx->ec->log_level);
	}

	ctx->ec->is_translated = true;
	ctx->ec->ir_ec = ir_ec;

end:
	return;
}


static inline
void ctf_stream_class_to_ir(struct ctx *ctx)
{
	int ret;
	bt_field_class *ir_fc;

	BT_ASSERT(ctx->sc);

	if (ctx->sc->is_translated) {
		ctx->ir_sc = bt_trace_class_borrow_stream_class_by_id(
			ctx->ir_tc, ctx->sc->id);
		BT_ASSERT(ctx->ir_sc);
		goto end;
	}

	ctx->ir_sc = bt_stream_class_create_with_id(ctx->ir_tc, ctx->sc->id);
	BT_ASSERT(ctx->ir_sc);
	bt_stream_class_put_ref(ctx->ir_sc);
	ctx->scope = CTF_SCOPE_PACKET_CONTEXT;
	ir_fc = scope_ctf_field_class_to_ir(ctx);
	if (ir_fc) {
		ret = bt_stream_class_set_packet_context_field_class(
			ctx->ir_sc, ir_fc);
		BT_ASSERT(ret == 0);
		bt_field_class_put_ref(ir_fc);
	}

	ctx->scope = CTF_SCOPE_EVENT_COMMON_CONTEXT;
	ir_fc = scope_ctf_field_class_to_ir(ctx);
	if (ir_fc) {
		ret = bt_stream_class_set_event_common_context_field_class(
			ctx->ir_sc, ir_fc);
		BT_ASSERT(ret == 0);
		bt_field_class_put_ref(ir_fc);
	}

	bt_stream_class_set_assigns_automatic_event_class_id(ctx->ir_sc,
		BT_FALSE);
	bt_stream_class_set_assigns_automatic_stream_id(ctx->ir_sc, BT_FALSE);

	if (ctx->sc->default_clock_class) {
		BT_ASSERT(ctx->sc->default_clock_class->ir_cc);
		ret = bt_stream_class_set_default_clock_class(ctx->ir_sc,
			ctx->sc->default_clock_class->ir_cc);
		BT_ASSERT(ret == 0);
	}

	ctx->sc->is_translated = true;
	ctx->sc->ir_sc = ctx->ir_sc;

end:
	return;
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

	bt_clock_class_set_origin_is_unix_epoch(ir_cc, cc->is_absolute);
}

static inline
int ctf_trace_class_to_ir(struct ctx *ctx)
{
	int ret = 0;
	uint64_t i;

	BT_ASSERT(ctx->tc);
	BT_ASSERT(ctx->ir_tc);

	if (ctx->tc->is_translated) {
		goto end;
	}

	if (ctx->tc->is_uuid_set) {
		bt_trace_class_set_uuid(ctx->ir_tc, ctx->tc->uuid);
	}

	for (i = 0; i < ctx->tc->env_entries->len; i++) {
		struct ctf_trace_class_env_entry *env_entry =
			ctf_trace_class_borrow_env_entry_by_index(ctx->tc, i);

		switch (env_entry->type) {
		case CTF_TRACE_CLASS_ENV_ENTRY_TYPE_INT:
			ret = bt_trace_class_set_environment_entry_integer(
				ctx->ir_tc, env_entry->name->str,
				env_entry->value.i);
			break;
		case CTF_TRACE_CLASS_ENV_ENTRY_TYPE_STR:
			ret = bt_trace_class_set_environment_entry_string(
				ctx->ir_tc, env_entry->name->str,
				env_entry->value.str->str);
			break;
		default:
			abort();
		}

		if (ret) {
			goto end;
		}
	}

	for (i = 0; i < ctx->tc->clock_classes->len; i++) {
		struct ctf_clock_class *cc = ctx->tc->clock_classes->pdata[i];

		cc->ir_cc = bt_clock_class_create(ctx->ir_tc);
		ctf_clock_class_to_ir(cc->ir_cc, cc);
	}

	bt_trace_class_set_assigns_automatic_stream_class_id(ctx->ir_tc,
		BT_FALSE);
	ctx->tc->is_translated = true;
	ctx->tc->ir_tc = ctx->ir_tc;

end:
	return ret;
}

BT_HIDDEN
int ctf_trace_class_translate(bt_trace_class *ir_tc,
		struct ctf_trace_class *tc)
{
	int ret = 0;
	uint64_t i;
	struct ctx ctx = { 0 };

	ctx.tc = tc;
	ctx.ir_tc = ir_tc;
	ret = ctf_trace_class_to_ir(&ctx);
	if (ret) {
		goto end;
	}

	for (i = 0; i < tc->stream_classes->len; i++) {
		uint64_t j;
		ctx.sc = tc->stream_classes->pdata[i];

		ctf_stream_class_to_ir(&ctx);

		for (j = 0; j < ctx.sc->event_classes->len; j++) {
			ctx.ec = ctx.sc->event_classes->pdata[j];

			ctf_event_class_to_ir(&ctx);
			ctx.ec = NULL;
		}

		ctx.sc = NULL;
	}

end:
	return ret;
}
