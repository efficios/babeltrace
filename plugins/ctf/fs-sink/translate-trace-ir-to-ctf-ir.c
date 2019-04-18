/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "PLUGIN-CTF-FS-SINK-TRANSLATE-TRACE-IR-TO-CTF-IR"
#include "logging.h"

#include <babeltrace/babeltrace.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/common-internal.h>
#include <babeltrace/assert-internal.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <glib.h>

#include "fs-sink-ctf-meta.h"

struct field_path_elem {
	uint64_t index_in_parent;
	GString *name;

	/* Weak */
	const bt_field_class *ir_fc;

	/* Weak */
	struct fs_sink_ctf_field_class *parent_fc;
};

struct ctx {
	/* Weak */
	struct fs_sink_ctf_stream_class *cur_sc;

	/* Weak */
	struct fs_sink_ctf_event_class *cur_ec;

	bt_scope cur_scope;

	/*
	 * Array of `struct field_path_elem` */
	GArray *cur_path;
};

static inline
struct field_path_elem *cur_path_stack_at(struct ctx *ctx, uint64_t i)
{
	BT_ASSERT(i < ctx->cur_path->len);
	return &g_array_index(ctx->cur_path, struct field_path_elem, i);
}

static inline
struct field_path_elem *cur_path_stack_top(struct ctx *ctx)
{
	BT_ASSERT(ctx->cur_path->len > 0);
	return cur_path_stack_at(ctx, ctx->cur_path->len - 1);
}

static inline
bool is_reserved_member_name(const char *name, const char *reserved_name)
{
	bool is_reserved = false;

	if (strcmp(name, reserved_name) == 0) {
		is_reserved = true;
		goto end;
	}

	if (name[0] == '_' && strcmp(&name[1], reserved_name) == 0) {
		is_reserved = true;
		goto end;
	}

end:
	return is_reserved;
}

static inline
int cur_path_stack_push(struct ctx *ctx,
		uint64_t index_in_parent, const char *ir_name,
		const bt_field_class *ir_fc,
		struct fs_sink_ctf_field_class *parent_fc)
{
	int ret = 0;
	struct field_path_elem *field_path_elem;

	g_array_set_size(ctx->cur_path, ctx->cur_path->len + 1);
	field_path_elem = cur_path_stack_top(ctx);
	field_path_elem->index_in_parent = index_in_parent;
	field_path_elem->name = g_string_new(ir_name);

	if (ir_name) {
		if (ctx->cur_scope == BT_SCOPE_PACKET_CONTEXT) {
			if (is_reserved_member_name(ir_name, "packet_size") ||
					is_reserved_member_name(ir_name, "content_size") ||
					is_reserved_member_name(ir_name, "timestamp_begin") ||
					is_reserved_member_name(ir_name, "timestamp_end") ||
					is_reserved_member_name(ir_name, "events_discarded") ||
					is_reserved_member_name(ir_name, "packet_seq_num")) {
				BT_LOGE("Unsupported reserved TSDL structure field class member "
					"or variant field class option name: name=\"%s\"",
					ir_name);
				ret = -1;
				goto end;
			}
		}

		ret = fs_sink_ctf_protect_name(field_path_elem->name);
		if (ret) {
			BT_LOGE("Unsupported non-TSDL structure field class member "
				"or variant field class option name: name=\"%s\"",
				ir_name);
			goto end;
		}
	}

	field_path_elem->ir_fc = ir_fc;
	field_path_elem->parent_fc = parent_fc;

end:
	return ret;
}

static inline
void cur_path_stack_pop(struct ctx *ctx)
{
	struct field_path_elem *field_path_elem;

	BT_ASSERT(ctx->cur_path->len > 0);
	field_path_elem = cur_path_stack_top(ctx);

	if (field_path_elem->name) {
		g_string_free(field_path_elem->name, TRUE);
		field_path_elem->name = NULL;
	}

	g_array_set_size(ctx->cur_path, ctx->cur_path->len - 1);
}

/*
 * Creates a relative field ref (a single name) from IR field path
 * `tgt_ir_field_path`.
 *
 * This function tries to locate the target field class recursively from
 * the top to the bottom of the context's current path using only the
 * target field class's own name. This is because many CTF reading tools
 * do not support a relative field ref with more than one element, for
 * example `prev_struct.len`.
 *
 * Returns a negative value if this resolving operation failed.
 */
static
int create_relative_field_ref(struct ctx *ctx,
		const bt_field_path *tgt_ir_field_path, GString *tgt_field_ref)
{
	int ret = 0;
	struct fs_sink_ctf_field_class *tgt_fc = NULL;
	uint64_t i;
	int64_t si;
	const char *tgt_fc_name;
	struct field_path_elem *field_path_elem;

	/* Get target field class's name */
	switch (bt_field_path_get_root_scope(tgt_ir_field_path)) {
	case BT_SCOPE_PACKET_CONTEXT:
		BT_ASSERT(ctx->cur_sc);
		tgt_fc = ctx->cur_sc->packet_context_fc;
		break;
	case BT_SCOPE_EVENT_COMMON_CONTEXT:
		BT_ASSERT(ctx->cur_sc);
		tgt_fc = ctx->cur_sc->event_common_context_fc;
		break;
	case BT_SCOPE_EVENT_SPECIFIC_CONTEXT:
		BT_ASSERT(ctx->cur_ec);
		tgt_fc = ctx->cur_ec->spec_context_fc;
		break;
	case BT_SCOPE_EVENT_PAYLOAD:
		BT_ASSERT(ctx->cur_ec);
		tgt_fc = ctx->cur_ec->payload_fc;
		break;
	default:
		abort();
	}

	i = 0;

	while (i < bt_field_path_get_index_count(tgt_ir_field_path)) {
		uint64_t index = bt_field_path_get_index_by_index(
			tgt_ir_field_path, i);
		struct fs_sink_ctf_named_field_class *named_fc = NULL;

		BT_ASSERT(tgt_fc);

		switch (tgt_fc->type) {
		case FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT:
			named_fc = fs_sink_ctf_field_class_struct_borrow_member_by_index(
				(void *) tgt_fc, index);
			break;
		case FS_SINK_CTF_FIELD_CLASS_TYPE_VARIANT:
			named_fc = fs_sink_ctf_field_class_variant_borrow_option_by_index(
				(void *) tgt_fc, index);
			break;
		case FS_SINK_CTF_FIELD_CLASS_TYPE_ARRAY:
		case FS_SINK_CTF_FIELD_CLASS_TYPE_SEQUENCE:
		{
			struct fs_sink_ctf_field_class_array_base *array_base_fc =
				(void *) tgt_fc;

			tgt_fc = array_base_fc->elem_fc;
			break;
		}
		default:
			abort();
		}

		if (named_fc) {
			tgt_fc = named_fc->fc;
			tgt_fc_name = named_fc->name->str;
			i++;
		}
	}

	BT_ASSERT(tgt_fc);
	BT_ASSERT(tgt_fc->type == FS_SINK_CTF_FIELD_CLASS_TYPE_INT);
	BT_ASSERT(tgt_fc_name);

	/* Find target field class having this name in current context */
	for (si = ctx->cur_path->len - 1; si >= 0; si--) {
		struct fs_sink_ctf_field_class *fc;
		struct fs_sink_ctf_field_class_struct *struct_fc;
		struct fs_sink_ctf_field_class_variant *var_fc;
		struct fs_sink_ctf_named_field_class *named_fc;
		uint64_t len;

		field_path_elem = cur_path_stack_at(ctx, (uint64_t) si);
		fc = field_path_elem->parent_fc;
		if (!fc) {
			/* Reached stack's bottom */
			ret = -1;
			goto end;
		}

		switch (fc->type) {
		case FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT:
		case FS_SINK_CTF_FIELD_CLASS_TYPE_VARIANT:
			break;
		case FS_SINK_CTF_FIELD_CLASS_TYPE_ARRAY:
		case FS_SINK_CTF_FIELD_CLASS_TYPE_SEQUENCE:
			continue;
		default:
			/* Not supported by TSDL 1.8 */
			ret = -1;
			goto end;
		}

		if (fc->type == FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT) {
			struct_fc = (void *) fc;
			len = struct_fc->members->len;
		} else {
			var_fc = (void *) fc;
			len = var_fc->options->len;
		}

		for (i = 0; i < len; i++) {
			if (fc->type == FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT) {
				named_fc = fs_sink_ctf_field_class_struct_borrow_member_by_index(
					struct_fc, i);
			} else {
				named_fc = fs_sink_ctf_field_class_variant_borrow_option_by_index(
					var_fc, i);
			}

			if (strcmp(named_fc->name->str, tgt_fc_name) == 0) {
				if (named_fc->fc == tgt_fc) {
					g_string_assign(tgt_field_ref,
						tgt_fc_name);
				} else {
					/*
					 * Using only the target field
					 * class's name, we're not
					 * reaching the target field
					 * class. This is not supported
					 * by TSDL 1.8.
					 */
					ret = -1;
				}

				goto end;
			}
		}
	}

end:
	return ret;
}

/*
 * Creates an absolute field ref from IR field path `tgt_ir_field_path`.
 *
 * Returns a negative value if this resolving operation failed.
 */
static
int create_absolute_field_ref(struct ctx *ctx,
		const bt_field_path *tgt_ir_field_path, GString *tgt_field_ref)
{
	int ret = 0;
	struct fs_sink_ctf_field_class *fc = NULL;
	uint64_t i;

	switch (bt_field_path_get_root_scope(tgt_ir_field_path)) {
	case BT_SCOPE_PACKET_CONTEXT:
		BT_ASSERT(ctx->cur_sc);
		fc = ctx->cur_sc->packet_context_fc;
		g_string_assign(tgt_field_ref, "stream.packet.context");
		break;
	case BT_SCOPE_EVENT_COMMON_CONTEXT:
		BT_ASSERT(ctx->cur_sc);
		fc = ctx->cur_sc->event_common_context_fc;
		g_string_assign(tgt_field_ref, "stream.event.context");
		break;
	case BT_SCOPE_EVENT_SPECIFIC_CONTEXT:
		BT_ASSERT(ctx->cur_ec);
		fc = ctx->cur_ec->spec_context_fc;
		g_string_assign(tgt_field_ref, "event.context");
		break;
	case BT_SCOPE_EVENT_PAYLOAD:
		BT_ASSERT(ctx->cur_ec);
		fc = ctx->cur_ec->payload_fc;
		g_string_assign(tgt_field_ref, "event.fields");
		break;
	default:
		abort();
	}

	BT_ASSERT(fc);

	for (i = 0; i < bt_field_path_get_index_count(tgt_ir_field_path); i++) {
		uint64_t index = bt_field_path_get_index_by_index(
			tgt_ir_field_path, i);
		struct fs_sink_ctf_named_field_class *named_fc = NULL;

		switch (fc->type) {
		case FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT:
			named_fc = fs_sink_ctf_field_class_struct_borrow_member_by_index(
				(void *) fc, index);
			break;
		case FS_SINK_CTF_FIELD_CLASS_TYPE_VARIANT:
			named_fc = fs_sink_ctf_field_class_variant_borrow_option_by_index(
				(void *) fc, index);
			break;
		case FS_SINK_CTF_FIELD_CLASS_TYPE_ARRAY:
		case FS_SINK_CTF_FIELD_CLASS_TYPE_SEQUENCE:
			/* Not supported by TSDL 1.8 */
			ret = -1;
			goto end;
		default:
			abort();
		}

		BT_ASSERT(named_fc);
		g_string_append_c(tgt_field_ref, '.');
		g_string_append(tgt_field_ref, named_fc->name->str);
		fc = named_fc->fc;
	}

end:
	return ret;
}

/*
 * Resolves a target field class located at `tgt_ir_field_path`, writing
 * the resolved field ref to `tgt_field_ref` and setting
 * `*create_before` according to whether or not the target field must be
 * created immediately before (in which case `tgt_field_ref` is
 * irrelevant).
 */
static
void resolve_field_class(struct ctx *ctx,
		const bt_field_path *tgt_ir_field_path,
		GString *tgt_field_ref, bool *create_before)
{
	int ret;
	bt_scope tgt_scope;

	*create_before = false;

	if (!tgt_ir_field_path) {
		*create_before = true;
		goto end;
	}

	tgt_scope = bt_field_path_get_root_scope(tgt_ir_field_path);

	if (tgt_scope == ctx->cur_scope) {
		/*
		 * Try, in this order:
		 *
		 * 1. Use a relative path, using only the target field
		 *    class's name. This is what is the most commonly
		 *    supported by popular CTF reading tools.
		 *
		 * 2. Use an absolute path. This could fail if there's
		 *    an array field class from the current root's field
		 *    class to the target field class.
		 *
		 * 3. Create the target field class before the
		 *    requesting field class (fallback).
		 */
		ret = create_relative_field_ref(ctx, tgt_ir_field_path,
			tgt_field_ref);
		if (ret) {
			ret = create_absolute_field_ref(ctx, tgt_ir_field_path,
				tgt_field_ref);
			if (ret) {
				*create_before = true;
				ret = 0;
				goto end;
			}
		}
	} else {
		ret = create_absolute_field_ref(ctx, tgt_ir_field_path,
			tgt_field_ref);

		/* It must always work in previous scopes */
		BT_ASSERT(ret == 0);
	}

end:
	return;
}

static
int translate_field_class(struct ctx *ctx);

static inline
void append_to_parent_field_class(struct ctx *ctx,
		struct fs_sink_ctf_field_class *fc)
{
	struct fs_sink_ctf_field_class *parent_fc =
		cur_path_stack_top(ctx)->parent_fc;

	switch (parent_fc->type) {
	case FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT:
		fs_sink_ctf_field_class_struct_append_member((void *) parent_fc,
			cur_path_stack_top(ctx)->name->str, fc);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_VARIANT:
		fs_sink_ctf_field_class_variant_append_option((void *) parent_fc,
			cur_path_stack_top(ctx)->name->str, fc);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_ARRAY:
	case FS_SINK_CTF_FIELD_CLASS_TYPE_SEQUENCE:
	{
		struct fs_sink_ctf_field_class_array_base *array_base_fc =
			(void *) parent_fc;

		BT_ASSERT(!array_base_fc->elem_fc);
		array_base_fc->elem_fc = fc;
		array_base_fc->base.alignment = fc->alignment;
		break;
	}
	default:
		abort();
	}
}

static inline
void update_parent_field_class_alignment(struct ctx *ctx,
		unsigned int alignment)
{
	struct fs_sink_ctf_field_class *parent_fc =
		cur_path_stack_top(ctx)->parent_fc;

	switch (parent_fc->type) {
	case FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT:
		fs_sink_ctf_field_class_struct_align_at_least(
			(void *) parent_fc, alignment);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_ARRAY:
	case FS_SINK_CTF_FIELD_CLASS_TYPE_SEQUENCE:
	{
		struct fs_sink_ctf_field_class_array_base *array_base_fc =
			(void *) parent_fc;

		array_base_fc->base.alignment = alignment;
		break;
	}
	default:
		break;
	}
}

static inline
int translate_structure_field_class_members(struct ctx *ctx,
		struct fs_sink_ctf_field_class_struct *struct_fc,
		const bt_field_class *ir_fc)
{
	int ret = 0;
	uint64_t i;

	for (i = 0; i < bt_field_class_structure_get_member_count(ir_fc); i++) {
		const bt_field_class_structure_member *member;
		const char *name;
		const bt_field_class *memb_ir_fc;

		member =
			bt_field_class_structure_borrow_member_by_index_const(
				ir_fc, i);
		name = bt_field_class_structure_member_get_name(member);
		memb_ir_fc = bt_field_class_structure_member_borrow_field_class_const(
			member);
		ret = cur_path_stack_push(ctx, i, name, memb_ir_fc,
			(void *) struct_fc);
		if (ret) {
			BT_LOGE("Cannot translate structure field class member: "
				"name=\"%s\"", name);
			goto end;
		}

		ret = translate_field_class(ctx);
		if (ret) {
			BT_LOGE("Cannot translate structure field class member: "
				"name=\"%s\"", name);
			goto end;
		}

		cur_path_stack_pop(ctx);
	}

end:
	return ret;
}

static inline
int translate_structure_field_class(struct ctx *ctx)
{
	int ret;
	struct fs_sink_ctf_field_class_struct *fc =
		fs_sink_ctf_field_class_struct_create_empty(
			cur_path_stack_top(ctx)->ir_fc,
			cur_path_stack_top(ctx)->index_in_parent);

	BT_ASSERT(fc);
	append_to_parent_field_class(ctx, (void *) fc);
	ret = translate_structure_field_class_members(ctx, fc, fc->base.ir_fc);
	if (ret) {
		goto end;
	}

	update_parent_field_class_alignment(ctx, fc->base.alignment);

end:
	return ret;
}

static inline
int translate_variant_field_class(struct ctx *ctx)
{
	int ret = 0;
	uint64_t i;
	struct fs_sink_ctf_field_class_variant *fc =
		fs_sink_ctf_field_class_variant_create_empty(
			cur_path_stack_top(ctx)->ir_fc,
			cur_path_stack_top(ctx)->index_in_parent);

	BT_ASSERT(fc);

	/* Resolve tag field class before appending to parent */
	resolve_field_class(ctx,
		bt_field_class_variant_borrow_selector_field_path_const(
			fc->base.ir_fc), fc->tag_ref, &fc->tag_is_before);

	append_to_parent_field_class(ctx, (void *) fc);

	for (i = 0; i < bt_field_class_variant_get_option_count(fc->base.ir_fc);
			i++) {
		const bt_field_class_variant_option *opt;
		const char *name;
		const bt_field_class *opt_ir_fc;

		opt = bt_field_class_variant_borrow_option_by_index_const(
			fc->base.ir_fc, i);
		name = bt_field_class_variant_option_get_name(opt);
		opt_ir_fc = bt_field_class_variant_option_borrow_field_class_const(
			opt);
		ret = cur_path_stack_push(ctx, i, name, opt_ir_fc, (void *) fc);
		if (ret) {
			BT_LOGE("Cannot translate variant field class option: "
				"name=\"%s\"", name);
			goto end;
		}

		ret = translate_field_class(ctx);
		if (ret) {
			BT_LOGE("Cannot translate variant field class option: "
				"name=\"%s\"", name);
			goto end;
		}

		cur_path_stack_pop(ctx);
	}

end:
	return ret;
}

static inline
int translate_static_array_field_class(struct ctx *ctx)
{
	struct fs_sink_ctf_field_class_array *fc =
		fs_sink_ctf_field_class_array_create_empty(
			cur_path_stack_top(ctx)->ir_fc,
			cur_path_stack_top(ctx)->index_in_parent);
	const bt_field_class *elem_ir_fc =
		bt_field_class_array_borrow_element_field_class_const(
			fc->base.base.ir_fc);
	int ret;

	BT_ASSERT(fc);
	append_to_parent_field_class(ctx, (void *) fc);
	ret = cur_path_stack_push(ctx, UINT64_C(-1), NULL, elem_ir_fc,
		(void *) fc);
	if (ret) {
		BT_LOGE_STR("Cannot translate static array field class element.");
		goto end;
	}

	ret = translate_field_class(ctx);
	if (ret) {
		BT_LOGE_STR("Cannot translate static array field class element.");
		goto end;
	}

	cur_path_stack_pop(ctx);
	update_parent_field_class_alignment(ctx, fc->base.base.alignment);

end:
	return ret;
}

static inline
int translate_dynamic_array_field_class(struct ctx *ctx)
{
	struct fs_sink_ctf_field_class_sequence *fc =
		fs_sink_ctf_field_class_sequence_create_empty(
			cur_path_stack_top(ctx)->ir_fc,
			cur_path_stack_top(ctx)->index_in_parent);
	const bt_field_class *elem_ir_fc =
		bt_field_class_array_borrow_element_field_class_const(
			fc->base.base.ir_fc);
	int ret;

	BT_ASSERT(fc);

	/* Resolve length field class before appending to parent */
	resolve_field_class(ctx,
		bt_field_class_dynamic_array_borrow_length_field_path_const(
			fc->base.base.ir_fc),
		fc->length_ref, &fc->length_is_before);

	append_to_parent_field_class(ctx, (void *) fc);
	ret = cur_path_stack_push(ctx, UINT64_C(-1), NULL, elem_ir_fc,
		(void *) fc);
	if (ret) {
		BT_LOGE_STR("Cannot translate dynamic array field class element.");
		goto end;
	}

	ret = translate_field_class(ctx);
	if (ret) {
		BT_LOGE_STR("Cannot translate dynamic array field class element.");
		goto end;
	}

	cur_path_stack_pop(ctx);
	update_parent_field_class_alignment(ctx, fc->base.base.alignment);

end:
	return ret;
}

static inline
int translate_integer_field_class(struct ctx *ctx)
{
	struct fs_sink_ctf_field_class_int *fc =
		fs_sink_ctf_field_class_int_create(
			cur_path_stack_top(ctx)->ir_fc,
			cur_path_stack_top(ctx)->index_in_parent);

	BT_ASSERT(fc);
	append_to_parent_field_class(ctx, (void *) fc);
	return 0;
}

static inline
int translate_real_field_class(struct ctx *ctx)
{
	struct fs_sink_ctf_field_class_float *fc =
		fs_sink_ctf_field_class_float_create(
			cur_path_stack_top(ctx)->ir_fc,
			cur_path_stack_top(ctx)->index_in_parent);

	BT_ASSERT(fc);
	append_to_parent_field_class(ctx, (void *) fc);
	return 0;
}

static inline
int translate_string_field_class(struct ctx *ctx)
{
	struct fs_sink_ctf_field_class_string *fc =
		fs_sink_ctf_field_class_string_create(
			cur_path_stack_top(ctx)->ir_fc,
			cur_path_stack_top(ctx)->index_in_parent);

	BT_ASSERT(fc);
	append_to_parent_field_class(ctx, (void *) fc);
	return 0;
}

/*
 * Translates a field class, recursively.
 *
 * The field class's IR field class, parent field class, and index
 * within its parent are in the context's current path's top element
 * (cur_path_stack_top()).
 */
static
int translate_field_class(struct ctx *ctx)
{
	int ret;

	switch (bt_field_class_get_type(cur_path_stack_top(ctx)->ir_fc)) {
	case BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER:
	case BT_FIELD_CLASS_TYPE_SIGNED_INTEGER:
	case BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION:
	case BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION:
		ret = translate_integer_field_class(ctx);
		break;
	case BT_FIELD_CLASS_TYPE_REAL:
		ret = translate_real_field_class(ctx);
		break;
	case BT_FIELD_CLASS_TYPE_STRING:
		ret = translate_string_field_class(ctx);
		break;
	case BT_FIELD_CLASS_TYPE_STRUCTURE:
		ret = translate_structure_field_class(ctx);
		break;
	case BT_FIELD_CLASS_TYPE_STATIC_ARRAY:
		ret = translate_static_array_field_class(ctx);
		break;
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY:
		ret = translate_dynamic_array_field_class(ctx);
		break;
	case BT_FIELD_CLASS_TYPE_VARIANT:
		ret = translate_variant_field_class(ctx);
		break;
	default:
		abort();
	}

	return ret;
}

static
int set_field_ref(struct fs_sink_ctf_field_class *fc, const char *fc_name,
		struct fs_sink_ctf_field_class *parent_fc)
{
	int ret = 0;
	GString *field_ref = NULL;
	bool is_before;
	const char *tgt_type;
	struct fs_sink_ctf_field_class_struct *parent_struct_fc =
		(void *) parent_fc;
	uint64_t i;
	unsigned int suffix = 0;

	if (!fc_name || !parent_fc ||
			parent_fc->type != FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT) {
		/* Not supported */
		ret = -1;
		goto end;
	}

	switch (fc->type) {
	case FS_SINK_CTF_FIELD_CLASS_TYPE_SEQUENCE:
	{
		struct fs_sink_ctf_field_class_sequence *seq_fc = (void *) fc;

		field_ref = seq_fc->length_ref;
		is_before = seq_fc->length_is_before;
		tgt_type = "len";
		break;
	}
	case FS_SINK_CTF_FIELD_CLASS_TYPE_VARIANT:
	{
		struct fs_sink_ctf_field_class_variant *var_fc = (void *) fc;

		field_ref = var_fc->tag_ref;
		is_before = var_fc->tag_is_before;
		tgt_type = "tag";
		break;
	}
	default:
		abort();
	}

	BT_ASSERT(field_ref);

	if (!is_before) {
		goto end;
	}

	/* Initial field ref */
	g_string_printf(field_ref, "__%s_%s", fc_name, tgt_type);

	/*
	 * Make sure field ref does not clash with an existing field
	 * class name within the same parent structure field class.
	 */
	while (true) {
		bool name_ok = true;

		for (i = 0; i < parent_struct_fc->members->len; i++) {
			struct fs_sink_ctf_named_field_class *named_fc =
				fs_sink_ctf_field_class_struct_borrow_member_by_index(
					parent_struct_fc, i);

			if (strcmp(field_ref->str, named_fc->name->str) == 0) {
				/* Name clash */
				name_ok = false;
				break;
			}
		}

		if (name_ok) {
			/* No clash: we're done */
			break;
		}

		/* Append suffix and try again */
		g_string_printf(field_ref, "__%s_%s_%u", fc_name, tgt_type,
			suffix);
		suffix++;
	}

end:
	return ret;
}

/*
 * This function recursively sets field refs of sequence and variant
 * field classes when they are immediately before, avoiding name clashes
 * with existing field class names.
 *
 * It can fail at this point if, for example, a sequence field class of
 * which to set the length's field ref has something else than a
 * structure field class as its parent: in this case, there's no
 * location to place the length field class immediately before the
 * sequence field class.
 */
static
int set_field_refs(struct fs_sink_ctf_field_class *fc, const char *fc_name,
		struct fs_sink_ctf_field_class *parent_fc)
{
	int ret = 0;
	BT_ASSERT(fc);

	switch (fc->type) {
	case FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT:
	case FS_SINK_CTF_FIELD_CLASS_TYPE_VARIANT:
	{
		uint64_t i;
		uint64_t len;
		struct fs_sink_ctf_field_class_struct *struct_fc;
		struct fs_sink_ctf_field_class_variant *var_fc;
		struct fs_sink_ctf_named_field_class *named_fc;

		if (fc->type == FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT) {
			struct_fc = (void *) fc;
			len = struct_fc->members->len;
		} else {
			var_fc = (void *) fc;
			len = var_fc->options->len;
			ret = set_field_ref(fc, fc_name, parent_fc);
			if (ret) {
				goto end;
			}
		}

		for (i = 0; i < len; i++) {
			if (fc->type == FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT) {
				named_fc = fs_sink_ctf_field_class_struct_borrow_member_by_index(
					struct_fc, i);
			} else {
				named_fc = fs_sink_ctf_field_class_variant_borrow_option_by_index(
					var_fc, i);
			}

			ret = set_field_refs(named_fc->fc, named_fc->name->str,
				fc);
			if (ret) {
				goto end;
			}
		}

		break;
	}
	case FS_SINK_CTF_FIELD_CLASS_TYPE_ARRAY:
	case FS_SINK_CTF_FIELD_CLASS_TYPE_SEQUENCE:
	{
		struct fs_sink_ctf_field_class_array_base *array_base_fc =
			(void *) fc;

		if (fc->type == FS_SINK_CTF_FIELD_CLASS_TYPE_SEQUENCE) {
			ret = set_field_ref(fc, fc_name, parent_fc);
			if (ret) {
				goto end;
			}
		}

		ret = set_field_refs(array_base_fc->elem_fc, NULL, fc);
		if (ret) {
			goto end;
		}

		break;
	}
	default:
		break;
	}

end:
	return ret;
}

/*
 * This function translates a root scope trace IR field class to
 * a CTF IR field class.
 *
 * The resulting CTF IR field class is written to `*fc` so that it
 * exists as the parent object's (stream class or event class) true root
 * field class during the recursive translation for resolving purposes.
 * This is also why this function creates the empty structure field
 * class and then calls translate_structure_field_class_members() to
 * fill it.
 */
static
int translate_scope_field_class(struct ctx *ctx, bt_scope scope,
		struct fs_sink_ctf_field_class **fc,
		const bt_field_class *ir_fc)
{
	int ret = 0;

	if (!ir_fc) {
		goto end;
	}

	BT_ASSERT(bt_field_class_get_type(ir_fc) ==
			BT_FIELD_CLASS_TYPE_STRUCTURE);
	BT_ASSERT(fc);
	*fc = (void *) fs_sink_ctf_field_class_struct_create_empty(
		ir_fc, UINT64_C(-1));
	BT_ASSERT(*fc);
	ctx->cur_scope = scope;
	BT_ASSERT(ctx->cur_path->len == 0);
	ret = cur_path_stack_push(ctx, UINT64_C(-1), NULL, ir_fc, NULL);
	if (ret) {
		BT_LOGE("Cannot translate scope structure field class: "
			"scope=%d", scope);
		goto end;
	}

	ret = translate_structure_field_class_members(ctx, (void *) *fc, ir_fc);
	if (ret) {
		BT_LOGE("Cannot translate scope structure field class: "
			"scope=%d", scope);
		goto end;
	}

	cur_path_stack_pop(ctx);

	/* Set field refs for preceding targets */
	ret = set_field_refs(*fc, NULL, NULL);

end:
	return ret;
}

static inline
void ctx_init(struct ctx *ctx)
{
	memset(ctx, 0, sizeof(struct ctx));
	ctx->cur_path = g_array_new(FALSE, TRUE,
		sizeof(struct field_path_elem));
	BT_ASSERT(ctx->cur_path);
}

static inline
void ctx_fini(struct ctx *ctx)
{
	if (ctx->cur_path) {
		g_array_free(ctx->cur_path, TRUE);
		ctx->cur_path = NULL;
	}
}

static
int translate_event_class(struct fs_sink_ctf_stream_class *sc,
		const bt_event_class *ir_ec,
		struct fs_sink_ctf_event_class **out_ec)
{
	int ret = 0;
	struct ctx ctx;
	struct fs_sink_ctf_event_class *ec;

	BT_ASSERT(sc);
	BT_ASSERT(ir_ec);

	ctx_init(&ctx);
	ec = fs_sink_ctf_event_class_create(sc, ir_ec);
	BT_ASSERT(ec);
	ctx.cur_sc = sc;
	ctx.cur_ec = ec;
	ret = translate_scope_field_class(&ctx, BT_SCOPE_EVENT_SPECIFIC_CONTEXT,
		&ec->spec_context_fc,
		bt_event_class_borrow_specific_context_field_class_const(
			ir_ec));
	if (ret) {
		goto end;
	}

	ret = translate_scope_field_class(&ctx, BT_SCOPE_EVENT_PAYLOAD,
		&ec->payload_fc,
		bt_event_class_borrow_payload_field_class_const(ir_ec));
	if (ret) {
		goto end;
	}

end:
	ctx_fini(&ctx);
	*out_ec = ec;
	return ret;
}

BT_HIDDEN
int try_translate_event_class_trace_ir_to_ctf_ir(
		struct fs_sink_ctf_stream_class *sc,
		const bt_event_class *ir_ec,
		struct fs_sink_ctf_event_class **out_ec)
{
	int ret = 0;

	BT_ASSERT(sc);
	BT_ASSERT(ir_ec);

	/* Check in hash table first */
	*out_ec = g_hash_table_lookup(sc->event_classes_from_ir, ir_ec);
	if (likely(*out_ec)) {
		goto end;
	}

	ret = translate_event_class(sc, ir_ec, out_ec);

end:
	return ret;
}

bool default_clock_class_name_exists(struct fs_sink_ctf_trace_class *tc,
		const char *name)
{
	bool exists = false;
	uint64_t i;

	for (i = 0; i < tc->stream_classes->len; i++) {
		struct fs_sink_ctf_stream_class *sc =
			tc->stream_classes->pdata[i];

		if (sc->default_clock_class_name->len == 0) {
			/* No default clock class */
			continue;
		}

		if (strcmp(sc->default_clock_class_name->str, name) == 0) {
			exists = true;
			goto end;
		}
	}

end:
	return exists;
}

static
void make_unique_default_clock_class_name(struct fs_sink_ctf_stream_class *sc)
{
	unsigned int suffix = 0;
	char buf[16];

	g_string_assign(sc->default_clock_class_name, "");
	sprintf(buf, "default");

	while (default_clock_class_name_exists(sc->tc, buf)) {
		sprintf(buf, "default%u", suffix);
		suffix++;
	}

	g_string_assign(sc->default_clock_class_name, buf);
}

static
int translate_stream_class(struct fs_sink_ctf_trace_class *tc,
		const bt_stream_class *ir_sc,
		struct fs_sink_ctf_stream_class **out_sc)
{
	int ret = 0;
	struct ctx ctx;

	BT_ASSERT(tc);
	BT_ASSERT(ir_sc);
	ctx_init(&ctx);
	*out_sc = fs_sink_ctf_stream_class_create(tc, ir_sc);
	BT_ASSERT(*out_sc);

	/* Set default clock class's protected name, if any */
	if ((*out_sc)->default_clock_class) {
		const char *name = bt_clock_class_get_name(
			(*out_sc)->default_clock_class);

		if (!bt_stream_class_default_clock_is_always_known(ir_sc)) {
			BT_LOGE("Unsupported stream clock which can have an unknown value: "
				"sc-name=\"%s\"",
				bt_stream_class_get_name(ir_sc));
			goto error;
		}

		if (name) {
			/* Try original name, protected */
			g_string_assign((*out_sc)->default_clock_class_name,
				name);
			ret = fs_sink_ctf_protect_name(
				(*out_sc)->default_clock_class_name);
			if (ret) {
				/* Invalid: create a new name */
				make_unique_default_clock_class_name(*out_sc);
				ret = 0;
			}
		} else {
			/* No name: create a name */
			make_unique_default_clock_class_name(*out_sc);
		}
	}

	ctx.cur_sc = *out_sc;
	ret = translate_scope_field_class(&ctx, BT_SCOPE_PACKET_CONTEXT,
		&(*out_sc)->packet_context_fc,
		bt_stream_class_borrow_packet_context_field_class_const(ir_sc));
	if (ret) {
		goto error;
	}

	if ((*out_sc)->packet_context_fc) {
		/*
		 * Make sure the structure field class's alignment is
		 * enough: 8 is what we use for our own special members
		 * in the packet context.
		 */
		fs_sink_ctf_field_class_struct_align_at_least(
			(void *) (*out_sc)->packet_context_fc, 8);
	}

	ret = translate_scope_field_class(&ctx, BT_SCOPE_EVENT_COMMON_CONTEXT,
		&(*out_sc)->event_common_context_fc,
		bt_stream_class_borrow_event_common_context_field_class_const(
			ir_sc));
	if (ret) {
		goto error;
	}

	goto end;

error:
	fs_sink_ctf_stream_class_destroy(*out_sc);
	*out_sc = NULL;

end:
	ctx_fini(&ctx);
	return ret;
}

BT_HIDDEN
int try_translate_stream_class_trace_ir_to_ctf_ir(
		struct fs_sink_ctf_trace_class *tc,
		const bt_stream_class *ir_sc,
		struct fs_sink_ctf_stream_class **out_sc)
{
	int ret = 0;
	uint64_t i;

	BT_ASSERT(tc);
	BT_ASSERT(ir_sc);

	for (i = 0; i < tc->stream_classes->len; i++) {
		*out_sc = tc->stream_classes->pdata[i];

		if ((*out_sc)->ir_sc == ir_sc) {
			goto end;
		}
	}

	ret = translate_stream_class(tc, ir_sc, out_sc);

end:
	return ret;
}

BT_HIDDEN
struct fs_sink_ctf_trace_class *translate_trace_class_trace_ir_to_ctf_ir(
		const bt_trace_class *ir_tc)
{
	uint64_t count;
	uint64_t i;
	struct fs_sink_ctf_trace_class *tc = NULL;

	/* Check that trace class's environment is TSDL-compatible */
	count = bt_trace_class_get_environment_entry_count(ir_tc);
	for (i = 0; i < count; i++) {
		const char *name;
		const bt_value *val;

		bt_trace_class_borrow_environment_entry_by_index_const(
			ir_tc, i, &name, &val);

		if (!fs_sink_ctf_ist_valid_identifier(name)) {
			BT_LOGE("Unsupported trace class's environment entry name: "
				"name=\"%s\"", name);
			goto end;
		}

		switch (bt_value_get_type(val)) {
		case BT_VALUE_TYPE_INTEGER:
		case BT_VALUE_TYPE_STRING:
			break;
		default:
			BT_LOGE("Unsupported trace class's environment entry value type: "
				"type=%s",
				bt_common_value_type_string(
					bt_value_get_type(val)));
			goto end;
		}
	}

	tc = fs_sink_ctf_trace_class_create(ir_tc);
	BT_ASSERT(tc);

end:
	return tc;
}
