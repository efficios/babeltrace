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

#define BT_COMP_LOG_SELF_COMP (ctx->self_comp)
#define BT_LOG_OUTPUT_LEVEL (ctx->log_level)
#define BT_LOG_TAG "PLUGIN/SINK.CTF.FS/TRANSLATE-TRACE-IR-TO-CTF-IR"
#include "logging/comp-logging.h"

#include "translate-trace-ir-to-ctf-ir.h"

#include <babeltrace2/babeltrace.h>
#include "common/macros.h"
#include "common/common.h"
#include "common/assert.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <glib.h>

#include "fs-sink.h"
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
	bt_logging_level log_level;
	bt_self_component *self_comp;

	/* Weak */
	struct fs_sink_ctf_stream_class *cur_sc;

	/* Weak */
	struct fs_sink_ctf_event_class *cur_ec;

	bt_field_path_scope cur_scope;

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

static const char *reserved_tsdl_keywords[] = {
	"align",
	"callsite",
	"const",
	"char",
	"clock",
	"double",
	"enum",
	"env",
	"event",
	"floating_point",
	"float",
	"integer",
	"int",
	"long",
	"short",
	"signed",
	"stream",
	"string",
	"struct",
	"trace",
	"typealias",
	"typedef",
	"unsigned",
	"variant",
	"void",
	"_Bool",
	"_Complex",
	"_Imaginary",
};

static inline
bool ist_valid_identifier(const char *name)
{
	const char *at;
	uint64_t i;
	bool ist_valid = true;

	/* Make sure the name is not a reserved keyword */
	for (i = 0; i < sizeof(reserved_tsdl_keywords) / sizeof(*reserved_tsdl_keywords);
			i++) {
		if (strcmp(name, reserved_tsdl_keywords[i]) == 0) {
			ist_valid = false;
			goto end;
		}
	}

	/* Make sure the name is not an empty string */
	if (strlen(name) == 0) {
		ist_valid = false;
		goto end;
	}

	/* Make sure the name starts with a letter or `_` */
	if (!isalpha((unsigned char) name[0]) && name[0] != '_') {
		ist_valid = false;
		goto end;
	}

	/* Make sure the name only contains letters, digits, and `_` */
	for (at = name; *at != '\0'; at++) {
		if (!isalnum((unsigned char) *at) && *at != '_') {
			ist_valid = false;
			goto end;
		}
	}

end:
	return ist_valid;
}

static inline
bool must_protect_identifier(const char *name)
{
	uint64_t i;
	bool must_protect = false;

	/* Protect a reserved keyword */
	for (i = 0; i < sizeof(reserved_tsdl_keywords) / sizeof(*reserved_tsdl_keywords);
			i++) {
		if (strcmp(name, reserved_tsdl_keywords[i]) == 0) {
			must_protect = true;
			goto end;
		}
	}

	/* Protect an identifier which already starts with `_` */
	if (name[0] == '_') {
		must_protect = true;
		goto end;
	}

end:
	return must_protect;
}

static inline
int cur_path_stack_push(struct ctx *ctx,
		uint64_t index_in_parent, const char *name,
		bool force_protect_name, const bt_field_class *ir_fc,
		struct fs_sink_ctf_field_class *parent_fc)
{
	int ret = 0;
	struct field_path_elem *field_path_elem;

	g_array_set_size(ctx->cur_path, ctx->cur_path->len + 1);
	field_path_elem = cur_path_stack_top(ctx);
	field_path_elem->index_in_parent = index_in_parent;
	field_path_elem->name = g_string_new(NULL);

	if (name) {
		if (force_protect_name) {
			g_string_assign(field_path_elem->name, "_");
		}

		g_string_append(field_path_elem->name, name);

		if (ctx->cur_scope == BT_FIELD_PATH_SCOPE_PACKET_CONTEXT) {
			if (is_reserved_member_name(name, "packet_size") ||
					is_reserved_member_name(name, "content_size") ||
					is_reserved_member_name(name, "timestamp_begin") ||
					is_reserved_member_name(name, "timestamp_end") ||
					is_reserved_member_name(name, "events_discarded") ||
					is_reserved_member_name(name, "packet_seq_num")) {
				BT_COMP_LOGE("Unsupported reserved TSDL structure field class member "
					"or variant field class option name: name=\"%s\"",
					name);
				ret = -1;
				goto end;
			}
		}

		if (!ist_valid_identifier(field_path_elem->name->str)) {
			ret = -1;
			BT_COMP_LOGE("Unsupported non-TSDL structure field class member "
				"or variant field class option name: name=\"%s\"",
				field_path_elem->name->str);
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
		const bt_field_path *tgt_ir_field_path, GString *tgt_field_ref,
		struct fs_sink_ctf_field_class **user_tgt_fc)
{
	int ret = 0;
	struct fs_sink_ctf_field_class *tgt_fc = NULL;
	uint64_t i;
	int64_t si;
	const char *tgt_fc_name = NULL;
	struct field_path_elem *field_path_elem;

	/* Get target field class's name */
	switch (bt_field_path_get_root_scope(tgt_ir_field_path)) {
	case BT_FIELD_PATH_SCOPE_PACKET_CONTEXT:
		BT_ASSERT(ctx->cur_sc);
		tgt_fc = ctx->cur_sc->packet_context_fc;
		break;
	case BT_FIELD_PATH_SCOPE_EVENT_COMMON_CONTEXT:
		BT_ASSERT(ctx->cur_sc);
		tgt_fc = ctx->cur_sc->event_common_context_fc;
		break;
	case BT_FIELD_PATH_SCOPE_EVENT_SPECIFIC_CONTEXT:
		BT_ASSERT(ctx->cur_ec);
		tgt_fc = ctx->cur_ec->spec_context_fc;
		break;
	case BT_FIELD_PATH_SCOPE_EVENT_PAYLOAD:
		BT_ASSERT(ctx->cur_ec);
		tgt_fc = ctx->cur_ec->payload_fc;
		break;
	default:
		bt_common_abort();
	}

	i = 0;

	while (i < bt_field_path_get_item_count(tgt_ir_field_path)) {
		const bt_field_path_item *fp_item =
			bt_field_path_borrow_item_by_index_const(
				tgt_ir_field_path, i);
		struct fs_sink_ctf_named_field_class *named_fc = NULL;

		BT_ASSERT(tgt_fc);
		BT_ASSERT(fp_item);

		if (bt_field_path_item_get_type(fp_item) ==
				BT_FIELD_PATH_ITEM_TYPE_CURRENT_OPTION_CONTENT) {
			/* Not supported by CTF 1.8 */
			ret = -1;
			goto end;
		}

		switch (tgt_fc->type) {
		case FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT:
			BT_ASSERT(bt_field_path_item_get_type(fp_item) ==
				BT_FIELD_PATH_ITEM_TYPE_INDEX);
			named_fc = fs_sink_ctf_field_class_struct_borrow_member_by_index(
				(void *) tgt_fc,
				bt_field_path_item_index_get_index(fp_item));
			break;
		case FS_SINK_CTF_FIELD_CLASS_TYPE_VARIANT:
			BT_ASSERT(bt_field_path_item_get_type(fp_item) ==
				BT_FIELD_PATH_ITEM_TYPE_INDEX);
			named_fc = fs_sink_ctf_field_class_variant_borrow_option_by_index(
				(void *) tgt_fc,
				bt_field_path_item_index_get_index(fp_item));
			break;
		case FS_SINK_CTF_FIELD_CLASS_TYPE_ARRAY:
		case FS_SINK_CTF_FIELD_CLASS_TYPE_SEQUENCE:
		{
			struct fs_sink_ctf_field_class_array_base *array_base_fc =
				(void *) tgt_fc;

			BT_ASSERT(bt_field_path_item_get_type(fp_item) ==
				BT_FIELD_PATH_ITEM_TYPE_CURRENT_ARRAY_ELEMENT);
			tgt_fc = array_base_fc->elem_fc;
			break;
		}
		default:
			bt_common_abort();
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
		struct fs_sink_ctf_field_class_struct *struct_fc = NULL;
		struct fs_sink_ctf_field_class_variant *var_fc = NULL;
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

					if (user_tgt_fc) {
						*user_tgt_fc = tgt_fc;
					}
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
		const bt_field_path *tgt_ir_field_path, GString *tgt_field_ref,
		struct fs_sink_ctf_field_class **user_tgt_fc)
{
	int ret = 0;
	struct fs_sink_ctf_field_class *fc = NULL;
	uint64_t i;

	switch (bt_field_path_get_root_scope(tgt_ir_field_path)) {
	case BT_FIELD_PATH_SCOPE_PACKET_CONTEXT:
		BT_ASSERT(ctx->cur_sc);
		fc = ctx->cur_sc->packet_context_fc;
		g_string_assign(tgt_field_ref, "stream.packet.context");
		break;
	case BT_FIELD_PATH_SCOPE_EVENT_COMMON_CONTEXT:
		BT_ASSERT(ctx->cur_sc);
		fc = ctx->cur_sc->event_common_context_fc;
		g_string_assign(tgt_field_ref, "stream.event.context");
		break;
	case BT_FIELD_PATH_SCOPE_EVENT_SPECIFIC_CONTEXT:
		BT_ASSERT(ctx->cur_ec);
		fc = ctx->cur_ec->spec_context_fc;
		g_string_assign(tgt_field_ref, "event.context");
		break;
	case BT_FIELD_PATH_SCOPE_EVENT_PAYLOAD:
		BT_ASSERT(ctx->cur_ec);
		fc = ctx->cur_ec->payload_fc;
		g_string_assign(tgt_field_ref, "event.fields");
		break;
	default:
		bt_common_abort();
	}

	BT_ASSERT(fc);

	for (i = 0; i < bt_field_path_get_item_count(tgt_ir_field_path); i++) {
		const bt_field_path_item *fp_item =
			bt_field_path_borrow_item_by_index_const(
				tgt_ir_field_path, i);
		struct fs_sink_ctf_named_field_class *named_fc = NULL;

		if (bt_field_path_item_get_type(fp_item) !=
				BT_FIELD_PATH_ITEM_TYPE_INDEX) {
			/* Not supported by TSDL 1.8 */
			ret = -1;
			goto end;
		}

		switch (fc->type) {
		case FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT:
			BT_ASSERT(bt_field_path_item_get_type(fp_item) ==
				BT_FIELD_PATH_ITEM_TYPE_INDEX);
			named_fc = fs_sink_ctf_field_class_struct_borrow_member_by_index(
				(void *) fc,
				bt_field_path_item_index_get_index(fp_item));
			break;
		case FS_SINK_CTF_FIELD_CLASS_TYPE_VARIANT:
			BT_ASSERT(bt_field_path_item_get_type(fp_item) ==
				BT_FIELD_PATH_ITEM_TYPE_INDEX);
			named_fc = fs_sink_ctf_field_class_variant_borrow_option_by_index(
				(void *) fc,
				bt_field_path_item_index_get_index(fp_item));
			break;
		default:
			bt_common_abort();
		}

		BT_ASSERT(named_fc);
		g_string_append_c(tgt_field_ref, '.');
		g_string_append(tgt_field_ref, named_fc->name->str);
		fc = named_fc->fc;
	}

	if (user_tgt_fc) {
		*user_tgt_fc = fc;
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
		GString *tgt_field_ref, bool *create_before,
		struct fs_sink_ctf_field_class **user_tgt_fc)
{
	int ret;
	bt_field_path_scope tgt_scope;

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
			tgt_field_ref, user_tgt_fc);
		if (ret) {
			ret = create_absolute_field_ref(ctx, tgt_ir_field_path,
				tgt_field_ref, user_tgt_fc);
			if (ret) {
				*create_before = true;
				goto end;
			}
		}
	} else {
		ret = create_absolute_field_ref(ctx, tgt_ir_field_path,
			tgt_field_ref, user_tgt_fc);

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
	case FS_SINK_CTF_FIELD_CLASS_TYPE_OPTION:
	{
		struct fs_sink_ctf_field_class_option *opt_fc =
			(void *) parent_fc;

		BT_ASSERT(!opt_fc->content_fc);
		opt_fc->content_fc = fc;
		opt_fc->base.alignment = fc->alignment;
		break;
	}
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
		bt_common_abort();
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
		ret = cur_path_stack_push(ctx, i, name, true, memb_ir_fc,
			(void *) struct_fc);
		if (ret) {
			BT_COMP_LOGE("Cannot translate structure field class member: "
				"name=\"%s\"", name);
			goto end;
		}

		ret = translate_field_class(ctx);
		if (ret) {
			BT_COMP_LOGE("Cannot translate structure field class member: "
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

/*
 * This function protects a given variant FC option name (with the `_`
 * prefix) if required. On success, `name_buf->str` contains the variant
 * FC option name to use (original option name or protected if
 * required).
 *
 * One of the goals of `sink.ctf.fs` is to write a CTF trace which is as
 * close as possible to an original CTF trace as decoded by
 * `src.ctf.fs`.
 *
 * This scenario is valid in CTF 1.8:
 *
 *     enum {
 *         HELLO,
 *         MEOW
 *     } tag;
 *
 *     variant <tag> {
 *         int HELLO;
 *         string MEOW;
 *     };
 *
 * Once in trace IR, the enumeration FC mapping names and variant FC
 * option names are kept as is. For this reason, we don't want to
 * protect the variant FC option names here (by prepending `_`): this
 * would make the variant FC option name and the enumeration FC mapping
 * name not match.
 *
 * This scenario is also valid in CTF 1.8:
 *
 *     enum {
 *         _HELLO,
 *         MEOW
 *     } tag;
 *
 *     variant <tag> {
 *         int _HELLO;
 *         string MEOW;
 *     };
 *
 * Once in trace IR, the enumeration FC mapping names are kept as is,
 * but the `_HELLO` variant FC option name becomes `HELLO` (unprotected
 * for presentation, as recommended by CTF 1.8). When going back to
 * TSDL, we need to protect `HELLO` so that it becomes `_HELLO` to match
 * the corresponding enumeration FC mapping name.
 *
 * This scenario is also valid in CTF 1.8:
 *
 *     enum {
 *         __HELLO,
 *         MEOW
 *     } tag;
 *
 *     variant <tag> {
 *         int __HELLO;
 *         string MEOW;
 *     };
 *
 * Once in trace IR, the enumeration FC mapping names are kept as is,
 * but the `__HELLO` variant FC option name becomes `_HELLO`
 * (unprotected). When going back to TSDL, we need to protect `_HELLO`
 * so that it becomes `__HELLO` to match the corresponding enumeration
 * FC mapping name.
 *
 * `src.ctf.fs` always uses the _same_ integer range sets for a selector
 * FC mapping and a corresponding variant FC option. We can use that
 * fact to find the original variant FC option names by matching variant
 * FC options and enumeration FC mappings by range set.
 */
static
int maybe_protect_variant_option_name(const bt_field_class *ir_var_fc,
		const bt_field_class *ir_tag_fc, uint64_t opt_i,
		GString *name_buf)
{
	int ret = 0;
	uint64_t i;
	bt_field_class_type ir_var_fc_type;
	const void *opt_ranges = NULL;
	const char *mapping_label = NULL;
	const char *ir_opt_name;
	const bt_field_class_variant_option *base_var_opt;
	bool force_protect = false;

	ir_var_fc_type = bt_field_class_get_type(ir_var_fc);
	base_var_opt = bt_field_class_variant_borrow_option_by_index_const(
		ir_var_fc, opt_i);
	BT_ASSERT(base_var_opt);
	ir_opt_name = bt_field_class_variant_option_get_name(base_var_opt);
	BT_ASSERT(ir_opt_name);

	/*
	 * Check if the variant FC option name is required to be
	 * protected (reserved TSDL keyword or starts with `_`). In that
	 * case, the name of the selector FC mapping we find must match
	 * exactly the protected name.
	 */
	force_protect = must_protect_identifier(ir_opt_name);
	if (force_protect) {
		g_string_assign(name_buf, "_");
		g_string_append(name_buf, ir_opt_name);
	} else {
		g_string_assign(name_buf, ir_opt_name);
	}

	/* Borrow option's ranges */
	if (ir_var_fc_type == BT_FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD) {
		/* No ranges: we're done */
		goto end;
	} if (ir_var_fc_type == BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD) {
		const bt_field_class_variant_with_selector_field_integer_unsigned_option *var_opt =
			bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_index_const(
				ir_var_fc, opt_i);
		opt_ranges =
			bt_field_class_variant_with_selector_field_integer_unsigned_option_borrow_ranges_const(
				var_opt);
	} else {
		const bt_field_class_variant_with_selector_field_integer_signed_option *var_opt =
			bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_index_const(
				ir_var_fc, opt_i);
		opt_ranges =
			bt_field_class_variant_with_selector_field_integer_signed_option_borrow_ranges_const(
				var_opt);
	}

	/* Find corresponding mapping by range set in selector FC */
	for (i = 0; i < bt_field_class_enumeration_get_mapping_count(ir_tag_fc);
			i++) {
		if (ir_var_fc_type == BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD) {
			const bt_field_class_enumeration_mapping *mapping_base;
			const bt_field_class_enumeration_unsigned_mapping *mapping;
			const bt_integer_range_set_unsigned *mapping_ranges;

			mapping = bt_field_class_enumeration_unsigned_borrow_mapping_by_index_const(
				ir_tag_fc, i);
			mapping_ranges = bt_field_class_enumeration_unsigned_mapping_borrow_ranges_const(
				mapping);

			if (bt_integer_range_set_unsigned_is_equal(opt_ranges,
					mapping_ranges)) {
				/* We have a winner */
				mapping_base =
					bt_field_class_enumeration_unsigned_mapping_as_mapping_const(
						mapping);
				mapping_label =
					bt_field_class_enumeration_mapping_get_label(
						mapping_base);
				break;
			}
		} else {
			const bt_field_class_enumeration_mapping *mapping_base;
			const bt_field_class_enumeration_signed_mapping *mapping;
			const bt_integer_range_set_signed *mapping_ranges;

			mapping = bt_field_class_enumeration_signed_borrow_mapping_by_index_const(
				ir_tag_fc, i);
			mapping_ranges = bt_field_class_enumeration_signed_mapping_borrow_ranges_const(
				mapping);

			if (bt_integer_range_set_signed_is_equal(opt_ranges,
					mapping_ranges)) {
				/* We have a winner */
				mapping_base =
					bt_field_class_enumeration_signed_mapping_as_mapping_const(
						mapping);
				mapping_label =
					bt_field_class_enumeration_mapping_get_label(
						mapping_base);
				break;
			}
		}
	}

	if (!mapping_label) {
		/* Range set not found: invalid selector for CTF 1.8 */
		ret = -1;
		goto end;
	}

	/*
	 * If the enumeration FC mapping name is not the same as the
	 * variant FC option name and we didn't protect already, try
	 * protecting the option name and check again.
	 */
	if (strcmp(mapping_label, name_buf->str) != 0) {
		if (force_protect) {
			ret = -1;
			goto end;
		}

		if (mapping_label[0] == '\0') {
			ret = -1;
			goto end;
		}

		g_string_assign(name_buf, "_");
		g_string_append(name_buf, ir_opt_name);

		if (strcmp(mapping_label, name_buf->str) != 0) {
			ret = -1;
			goto end;
		}
	}

end:
	return ret;
}

static inline
int translate_option_field_class(struct ctx *ctx)
{
	struct fs_sink_ctf_field_class_option *fc =
		fs_sink_ctf_field_class_option_create_empty(
			cur_path_stack_top(ctx)->ir_fc,
			cur_path_stack_top(ctx)->index_in_parent);
	const bt_field_class *content_ir_fc =
		bt_field_class_option_borrow_field_class_const(fc->base.ir_fc);
	int ret;

	BT_ASSERT(fc);

	/*
	 * CTF 1.8 does not support the option field class type. To
	 * write something anyway, this component translates this type
	 * to a variant field class where the options are:
	 *
	 * * An empty structure field class.
	 * * The optional field class itself.
	 *
	 * The "tag" is always generated/before in that case (an 8-bit
	 * unsigned enumeration field class).
	 */
	append_to_parent_field_class(ctx, (void *) fc);
	ret = cur_path_stack_push(ctx, UINT64_C(-1), NULL, false, content_ir_fc,
		(void *) fc);
	if (ret) {
		BT_COMP_LOGE_STR("Cannot translate option field class content.");
		goto end;
	}

	ret = translate_field_class(ctx);
	if (ret) {
		BT_COMP_LOGE_STR("Cannot translate option field class content.");
		goto end;
	}

	cur_path_stack_pop(ctx);
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
	bt_field_class_type ir_fc_type;
	const bt_field_path *ir_selector_field_path = NULL;
	struct fs_sink_ctf_field_class *tgt_fc = NULL;
	GString *name_buf = g_string_new(NULL);
	bt_value *prot_opt_names = bt_value_array_create();
	uint64_t opt_count;

	BT_ASSERT(fc);
	BT_ASSERT(name_buf);
	BT_ASSERT(prot_opt_names);
	ir_fc_type = bt_field_class_get_type(fc->base.ir_fc);
	opt_count = bt_field_class_variant_get_option_count(fc->base.ir_fc);

	if (bt_field_class_type_is(ir_fc_type,
			BT_FIELD_CLASS_TYPE_VARIANT_WITH_SELECTOR_FIELD)) {
		ir_selector_field_path = bt_field_class_variant_with_selector_field_borrow_selector_field_path_const(
			fc->base.ir_fc);
		BT_ASSERT(ir_selector_field_path);
	}

	/* Resolve tag field class before appending to parent */
	resolve_field_class(ctx, ir_selector_field_path, fc->tag_ref,
		&fc->tag_is_before, &tgt_fc);

	if (ir_selector_field_path && tgt_fc) {
		uint64_t mapping_count;
		uint64_t option_count;

		/* CTF 1.8: selector FC must be an enumeration FC */
		bt_field_class_type type = bt_field_class_get_type(
			tgt_fc->ir_fc);

		if (!bt_field_class_type_is(type,
				BT_FIELD_CLASS_TYPE_ENUMERATION)) {
			fc->tag_is_before = true;
			goto validate_opts;
		}

		/*
		 * Call maybe_protect_variant_option_name() for each
		 * option below. In that case we also want selector FC
		 * to contain as many mappings as the variant FC has
		 * options.
		 */
		mapping_count = bt_field_class_enumeration_get_mapping_count(
			tgt_fc->ir_fc);
		option_count = bt_field_class_variant_get_option_count(
			fc->base.ir_fc);

		if (mapping_count != option_count) {
			fc->tag_is_before = true;
			goto validate_opts;
		}
	} else {
		/*
		 * No compatible selector field class for CTF 1.8:
		 * create the appropriate selector field class.
		 */
		fc->tag_is_before = true;
		goto validate_opts;
	}

validate_opts:
	/*
	 * First pass: detect any option name clash with option name
	 * protection. In that case, we don't fail: just create the
	 * selector field class before the variant field class.
	 *
	 * After this, `prot_opt_names` contains the final option names,
	 * potentially protected if needed. They can still be invalid
	 * TSDL identifiers however; this will be checked by
	 * cur_path_stack_push().
	 */
	for (i = 0; i < opt_count; i++) {
		if (!fc->tag_is_before) {
			BT_ASSERT(tgt_fc->ir_fc);
			ret = maybe_protect_variant_option_name(fc->base.ir_fc,
				tgt_fc->ir_fc, i, name_buf);
			if (ret) {
				fc->tag_is_before = true;
			}
		}

		ret = bt_value_array_append_string_element(prot_opt_names,
			name_buf->str);
		if (ret) {
			goto end;
		}
	}

	for (i = 0; i < opt_count; i++) {
		uint64_t j;
		const bt_value *opt_name_a =
			bt_value_array_borrow_element_by_index_const(
				prot_opt_names, i);

		for (j = 0; j < opt_count; j++) {
			const bt_value *opt_name_b;

			if (i == j) {
				continue;
			}

			opt_name_b =
				bt_value_array_borrow_element_by_index_const(
					prot_opt_names, j);
			if (bt_value_is_equal(opt_name_a, opt_name_b)) {
				/*
				 * Variant FC option names are not
				 * unique when protected.
				 */
				fc->tag_is_before = true;
				goto append_to_parent;
			}
		}
	}

append_to_parent:
	append_to_parent_field_class(ctx, (void *) fc);

	for (i = 0; i < opt_count; i++) {
		const bt_field_class_variant_option *opt;
		const bt_field_class *opt_ir_fc;
		const bt_value *prot_opt_name_val =
			bt_value_array_borrow_element_by_index_const(
				prot_opt_names, i);
		const char *prot_opt_name = bt_value_string_get(
			prot_opt_name_val);

		BT_ASSERT(prot_opt_name);
		opt = bt_field_class_variant_borrow_option_by_index_const(
			fc->base.ir_fc, i);
		opt_ir_fc = bt_field_class_variant_option_borrow_field_class_const(
			opt);

		/*
		 * We don't ask cur_path_stack_push() to protect the
		 * option name because it's already protected at this
		 * point.
		 */
		ret = cur_path_stack_push(ctx, i, prot_opt_name, false,
			opt_ir_fc, (void *) fc);
		if (ret) {
			BT_COMP_LOGE("Cannot translate variant field class option: "
				"name=\"%s\"", prot_opt_name);
			goto end;
		}

		ret = translate_field_class(ctx);
		if (ret) {
			BT_COMP_LOGE("Cannot translate variant field class option: "
				"name=\"%s\"", prot_opt_name);
			goto end;
		}

		cur_path_stack_pop(ctx);
	}

end:
	if (name_buf) {
		g_string_free(name_buf, TRUE);
	}

	bt_value_put_ref(prot_opt_names);
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
	ret = cur_path_stack_push(ctx, UINT64_C(-1), NULL, false, elem_ir_fc,
		(void *) fc);
	if (ret) {
		BT_COMP_LOGE_STR("Cannot translate static array field class element.");
		goto end;
	}

	ret = translate_field_class(ctx);
	if (ret) {
		BT_COMP_LOGE_STR("Cannot translate static array field class element.");
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
	if (bt_field_class_get_type(cur_path_stack_top(ctx)->ir_fc) ==
			BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD) {
		resolve_field_class(ctx,
			bt_field_class_array_dynamic_with_length_field_borrow_length_field_path_const(
				fc->base.base.ir_fc),
			fc->length_ref, &fc->length_is_before, NULL);
	}

	append_to_parent_field_class(ctx, (void *) fc);
	ret = cur_path_stack_push(ctx, UINT64_C(-1), NULL, false, elem_ir_fc,
		(void *) fc);
	if (ret) {
		BT_COMP_LOGE_STR("Cannot translate dynamic array field class element.");
		goto end;
	}

	ret = translate_field_class(ctx);
	if (ret) {
		BT_COMP_LOGE_STR("Cannot translate dynamic array field class element.");
		goto end;
	}

	cur_path_stack_pop(ctx);
	update_parent_field_class_alignment(ctx, fc->base.base.alignment);

end:
	return ret;
}

static inline
int translate_bool_field_class(struct ctx *ctx)
{
	struct fs_sink_ctf_field_class_bool *fc =
		fs_sink_ctf_field_class_bool_create(
			cur_path_stack_top(ctx)->ir_fc,
			cur_path_stack_top(ctx)->index_in_parent);

	BT_ASSERT(fc);
	append_to_parent_field_class(ctx, (void *) fc);
	return 0;
}

static inline
int translate_bit_array_field_class(struct ctx *ctx)
{
	struct fs_sink_ctf_field_class_bit_array *fc =
		fs_sink_ctf_field_class_bit_array_create(
			cur_path_stack_top(ctx)->ir_fc,
			cur_path_stack_top(ctx)->index_in_parent);

	BT_ASSERT(fc);
	append_to_parent_field_class(ctx, (void *) fc);
	return 0;
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
	bt_field_class_type ir_fc_type =
		bt_field_class_get_type(cur_path_stack_top(ctx)->ir_fc);

	if (ir_fc_type == BT_FIELD_CLASS_TYPE_BOOL) {
		ret = translate_bool_field_class(ctx);
	} else if (ir_fc_type == BT_FIELD_CLASS_TYPE_BIT_ARRAY) {
		ret = translate_bit_array_field_class(ctx);
	} else if (bt_field_class_type_is(ir_fc_type,
			BT_FIELD_CLASS_TYPE_INTEGER)) {
		ret = translate_integer_field_class(ctx);
	} else if (bt_field_class_type_is(ir_fc_type,
			BT_FIELD_CLASS_TYPE_REAL)) {
		ret = translate_real_field_class(ctx);
	} else if (ir_fc_type == BT_FIELD_CLASS_TYPE_STRING) {
		ret = translate_string_field_class(ctx);
	} else if (ir_fc_type == BT_FIELD_CLASS_TYPE_STRUCTURE) {
		ret = translate_structure_field_class(ctx);
	} else if (ir_fc_type == BT_FIELD_CLASS_TYPE_STATIC_ARRAY) {
		ret = translate_static_array_field_class(ctx);
	} else if (bt_field_class_type_is(ir_fc_type,
			BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY)) {
		ret = translate_dynamic_array_field_class(ctx);
	} else if (bt_field_class_type_is(ir_fc_type,
			BT_FIELD_CLASS_TYPE_OPTION)) {
		ret = translate_option_field_class(ctx);
	} else if (bt_field_class_type_is(ir_fc_type,
			BT_FIELD_CLASS_TYPE_VARIANT)) {
		ret = translate_variant_field_class(ctx);
	} else {
		bt_common_abort();
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
	case FS_SINK_CTF_FIELD_CLASS_TYPE_OPTION:
	{
		/*
		 * CTF 1.8 does not support the option field class type.
		 * To write something anyway, this component translates
		 * this type to a variant field class where the options
		 * are:
		 *
		 * * An empty structure field class.
		 * * The optional field class itself.
		 *
		 * Because the option field class becomes a CTF variant
		 * field class, we use the term "tag" too here.
		 *
		 * The "tag" is always generated/before in that case (an
		 * 8-bit unsigned enumeration field class).
		 */
		struct fs_sink_ctf_field_class_option *opt_fc = (void *) fc;

		field_ref = opt_fc->tag_ref;
		is_before = true;
		tgt_type = "tag";
		break;
	}
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
		bt_common_abort();
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
int set_field_refs(struct fs_sink_ctf_field_class * const fc,
		const char *fc_name, struct fs_sink_ctf_field_class *parent_fc)
{
	int ret = 0;
	enum fs_sink_ctf_field_class_type fc_type;
	BT_ASSERT(fc);

	fc_type = fc->type;

	switch (fc_type) {
	case FS_SINK_CTF_FIELD_CLASS_TYPE_OPTION:
	{
		struct fs_sink_ctf_field_class_option *opt_fc = (void *) fc;

		ret = set_field_ref(fc, fc_name, parent_fc);
		if (ret) {
			goto end;
		}

		ret = set_field_refs(opt_fc->content_fc, NULL, fc);
		if (ret) {
			goto end;
		}

		break;
	}
	case FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT:
	case FS_SINK_CTF_FIELD_CLASS_TYPE_VARIANT:
	{
		uint64_t i;
		uint64_t len;
		struct fs_sink_ctf_field_class_struct *struct_fc = NULL;
		struct fs_sink_ctf_field_class_variant *var_fc = NULL;
		struct fs_sink_ctf_named_field_class *named_fc;

		if (fc_type == FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT) {
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
			if (fc_type == FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT) {
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

		if (fc_type == FS_SINK_CTF_FIELD_CLASS_TYPE_SEQUENCE) {
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
int translate_scope_field_class(struct ctx *ctx, bt_field_path_scope scope,
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
	ret = cur_path_stack_push(ctx, UINT64_C(-1), NULL, false, ir_fc, NULL);
	if (ret) {
		BT_COMP_LOGE("Cannot translate scope structure field class: "
			"scope=%d", scope);
		goto end;
	}

	ret = translate_structure_field_class_members(ctx, (void *) *fc, ir_fc);
	if (ret) {
		BT_COMP_LOGE("Cannot translate scope structure field class: "
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
void ctx_init(struct ctx *ctx, struct fs_sink_comp *fs_sink)
{
	memset(ctx, 0, sizeof(struct ctx));
	ctx->cur_path = g_array_new(FALSE, TRUE,
		sizeof(struct field_path_elem));
	BT_ASSERT(ctx->cur_path);
	ctx->log_level = fs_sink->log_level;
	ctx->self_comp = fs_sink->self_comp;
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
int translate_event_class(struct fs_sink_comp *fs_sink,
		struct fs_sink_ctf_stream_class *sc,
		const bt_event_class *ir_ec,
		struct fs_sink_ctf_event_class **out_ec)
{
	int ret = 0;
	struct ctx ctx;
	struct fs_sink_ctf_event_class *ec;

	BT_ASSERT(sc);
	BT_ASSERT(ir_ec);

	ctx_init(&ctx, fs_sink);
	ec = fs_sink_ctf_event_class_create(sc, ir_ec);
	BT_ASSERT(ec);
	ctx.cur_sc = sc;
	ctx.cur_ec = ec;
	ret = translate_scope_field_class(&ctx, BT_FIELD_PATH_SCOPE_EVENT_SPECIFIC_CONTEXT,
		&ec->spec_context_fc,
		bt_event_class_borrow_specific_context_field_class_const(
			ir_ec));
	if (ret) {
		goto end;
	}

	ret = translate_scope_field_class(&ctx, BT_FIELD_PATH_SCOPE_EVENT_PAYLOAD,
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
		struct fs_sink_comp *fs_sink,
		struct fs_sink_ctf_stream_class *sc,
		const bt_event_class *ir_ec,
		struct fs_sink_ctf_event_class **out_ec)
{
	int ret = 0;

	BT_ASSERT(sc);
	BT_ASSERT(ir_ec);

	/* Check in hash table first */
	*out_ec = g_hash_table_lookup(sc->event_classes_from_ir, ir_ec);
	if (G_LIKELY(*out_ec)) {
		goto end;
	}

	ret = translate_event_class(fs_sink, sc, ir_ec, out_ec);

end:
	return ret;
}

static
bool default_clock_class_name_exists(struct fs_sink_ctf_trace *trace,
		const char *name)
{
	bool exists = false;
	uint64_t i;

	for (i = 0; i < trace->stream_classes->len; i++) {
		struct fs_sink_ctf_stream_class *sc =
			trace->stream_classes->pdata[i];

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

	while (default_clock_class_name_exists(sc->trace, buf)) {
		sprintf(buf, "default%u", suffix);
		suffix++;
	}

	g_string_assign(sc->default_clock_class_name, buf);
}

static
int translate_stream_class(struct fs_sink_comp *fs_sink,
		struct fs_sink_ctf_trace *trace,
		const bt_stream_class *ir_sc,
		struct fs_sink_ctf_stream_class **out_sc)
{
	int ret = 0;
	struct ctx ctx;

	BT_ASSERT(trace);
	BT_ASSERT(ir_sc);
	ctx_init(&ctx, fs_sink);
	*out_sc = fs_sink_ctf_stream_class_create(trace, ir_sc);
	BT_ASSERT(*out_sc);

	/* Set default clock class's protected name, if any */
	if ((*out_sc)->default_clock_class) {
		const char *name = bt_clock_class_get_name(
			(*out_sc)->default_clock_class);

		if (name) {
			/* Try original name, protected */
			g_string_assign((*out_sc)->default_clock_class_name,
				"");

			if (must_protect_identifier(name)) {
				g_string_assign(
					(*out_sc)->default_clock_class_name,
					"_");
			}

			g_string_assign((*out_sc)->default_clock_class_name,
				name);
			if (!ist_valid_identifier(
					(*out_sc)->default_clock_class_name->str)) {
				/* Invalid: create a new name */
				make_unique_default_clock_class_name(*out_sc);
			}
		} else {
			/* No name: create a name */
			make_unique_default_clock_class_name(*out_sc);
		}
	}

	ctx.cur_sc = *out_sc;
	ret = translate_scope_field_class(&ctx, BT_FIELD_PATH_SCOPE_PACKET_CONTEXT,
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

	ret = translate_scope_field_class(&ctx, BT_FIELD_PATH_SCOPE_EVENT_COMMON_CONTEXT,
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
		struct fs_sink_comp *fs_sink,
		struct fs_sink_ctf_trace *trace,
		const bt_stream_class *ir_sc,
		struct fs_sink_ctf_stream_class **out_sc)
{
	int ret = 0;
	uint64_t i;

	BT_ASSERT(trace);
	BT_ASSERT(ir_sc);

	for (i = 0; i < trace->stream_classes->len; i++) {
		*out_sc = trace->stream_classes->pdata[i];

		if ((*out_sc)->ir_sc == ir_sc) {
			goto end;
		}
	}

	ret = translate_stream_class(fs_sink, trace, ir_sc, out_sc);

end:
	return ret;
}

BT_HIDDEN
struct fs_sink_ctf_trace *translate_trace_trace_ir_to_ctf_ir(
		struct fs_sink_comp *fs_sink, const bt_trace *ir_trace)
{
	uint64_t count;
	uint64_t i;
	struct fs_sink_ctf_trace *trace = NULL;

	/* Check that trace's environment is TSDL-compatible */
	count = bt_trace_get_environment_entry_count(ir_trace);
	for (i = 0; i < count; i++) {
		const char *name;
		const bt_value *val;

		bt_trace_borrow_environment_entry_by_index_const(
			ir_trace, i, &name, &val);

		if (!ist_valid_identifier(name)) {
			BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, fs_sink->log_level,
				fs_sink->self_comp,
				"Unsupported trace class's environment entry name: "
				"name=\"%s\"", name);
			goto end;
		}

		switch (bt_value_get_type(val)) {
		case BT_VALUE_TYPE_SIGNED_INTEGER:
		case BT_VALUE_TYPE_STRING:
			break;
		default:
			BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, fs_sink->log_level,
				fs_sink->self_comp,
				"Unsupported trace class's environment entry value type: "
				"type=%s",
				bt_common_value_type_string(
					bt_value_get_type(val)));
			goto end;
		}
	}

	trace = fs_sink_ctf_trace_create(ir_trace);
	BT_ASSERT(trace);

end:
	return trace;
}
