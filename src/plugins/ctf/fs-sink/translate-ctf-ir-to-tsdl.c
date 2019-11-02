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

#include "translate-ctf-ir-to-tsdl.h"

#include <babeltrace2/babeltrace.h>
#include "common/macros.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <glib.h>
#include "common/assert.h"
#include "compat/endian.h"

#include "fs-sink-ctf-meta.h"

struct ctx {
	unsigned int indent_level;
	GString *tsdl;
};

static inline
void append_indent(struct ctx *ctx)
{
	unsigned int i;

	for (i = 0; i < ctx->indent_level; i++) {
		g_string_append_c(ctx->tsdl, '\t');
	}
}

static
void append_uuid(struct ctx *ctx, bt_uuid uuid)
{
	g_string_append_printf(ctx->tsdl,
		"\"" BT_UUID_FMT "\"",
		BT_UUID_FMT_VALUES(uuid));
}

static
void append_quoted_string_content(struct ctx *ctx, const char *str)
{
	const char *ch;

	for (ch = str; *ch != '\0'; ch++) {
		unsigned char uch = (unsigned char) *ch;

		if (uch < 32 || uch >= 127) {
			switch (*ch) {
			case '\a':
				g_string_append(ctx->tsdl, "\\a");
				break;
			case '\b':
				g_string_append(ctx->tsdl, "\\b");
				break;
			case '\f':
				g_string_append(ctx->tsdl, "\\f");
				break;
			case '\n':
				g_string_append(ctx->tsdl, "\\n");
				break;
			case '\r':
				g_string_append(ctx->tsdl, "\\r");
				break;
			case '\t':
				g_string_append(ctx->tsdl, "\\t");
				break;
			case '\v':
				g_string_append(ctx->tsdl, "\\v");
				break;
			default:
				g_string_append_printf(ctx->tsdl, "\\x%02x",
					(unsigned int) uch);
				break;
			}
		} else if (*ch == '"' || *ch == '\\') {
			g_string_append_c(ctx->tsdl, '\\');
			g_string_append_c(ctx->tsdl, *ch);
		} else {
			g_string_append_c(ctx->tsdl, *ch);
		}
	}
}

static
void append_quoted_string(struct ctx *ctx, const char *str)
{
	g_string_append_c(ctx->tsdl, '"');
	append_quoted_string_content(ctx, str);
	g_string_append_c(ctx->tsdl, '"');
}

static
void append_integer_field_class_from_props(struct ctx *ctx, unsigned int size,
		unsigned int alignment, bool is_signed,
		bt_field_class_integer_preferred_display_base disp_base,
		const char *mapped_clock_class_name, const char *field_name,
		bool end)
{
	g_string_append_printf(ctx->tsdl,
		"integer { size = %u; align = %u;",
		size, alignment);

	if (is_signed) {
		g_string_append(ctx->tsdl, " signed = true;");
	}

	if (disp_base != BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL) {
		g_string_append(ctx->tsdl, " base = ");

		switch (disp_base) {
		case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_BINARY:
			g_string_append(ctx->tsdl, "b");
			break;
		case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL:
			g_string_append(ctx->tsdl, "o");
			break;
		case BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL:
			g_string_append(ctx->tsdl, "x");
			break;
		default:
			bt_common_abort();
		}

		g_string_append_c(ctx->tsdl, ';');
	}

	if (mapped_clock_class_name) {
		g_string_append_printf(ctx->tsdl, " map = clock.%s.value;",
			mapped_clock_class_name);
	}

	g_string_append(ctx->tsdl, " }");

	if (field_name) {
		g_string_append_printf(ctx->tsdl, " %s", field_name);
	}

	if (end) {
		g_string_append(ctx->tsdl, ";\n");
	}
}

static
void append_end_block(struct ctx *ctx)
{
	ctx->indent_level--;
	append_indent(ctx);
	g_string_append(ctx->tsdl, "}");
}

static
void append_end_block_semi_nl(struct ctx *ctx)
{
	ctx->indent_level--;
	append_indent(ctx);
	g_string_append(ctx->tsdl, "};\n");
}

static
void append_end_block_semi_nl_nl(struct ctx *ctx)
{
	append_end_block_semi_nl(ctx);
	g_string_append_c(ctx->tsdl, '\n');
}

static
void append_bool_field_class(struct ctx *ctx,
		__attribute__((unused)) struct fs_sink_ctf_field_class_bool *fc)
{
	/*
	 * CTF 1.8 has no boolean field class type, so this component
	 * translates it to an 8-bit unsigned integer field class.
	 */
	append_integer_field_class_from_props(ctx, fc->base.size,
		fc->base.base.alignment, false,
		BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL,
		NULL, NULL, false);
}

static
void append_bit_array_field_class(struct ctx *ctx,
		struct fs_sink_ctf_field_class_bit_array *fc)
{
	/*
	 * CTF 1.8 has no bit array field class type, so this component
	 * translates it to an unsigned integer field class with an
	 * hexadecimal base.
	 */
	append_integer_field_class_from_props(ctx, fc->size,
		fc->base.alignment, false,
		BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL,
		NULL, NULL, false);
}

static
void append_integer_field_class(struct ctx *ctx,
		struct fs_sink_ctf_field_class_int *fc)
{
	const bt_field_class *ir_fc = fc->base.base.ir_fc;
	bt_field_class_type type = bt_field_class_get_type(ir_fc);
	bool is_signed = bt_field_class_type_is(type,
		BT_FIELD_CLASS_TYPE_SIGNED_INTEGER);

	if (bt_field_class_type_is(type, BT_FIELD_CLASS_TYPE_ENUMERATION)) {
		g_string_append(ctx->tsdl, "enum : ");
	}

	append_integer_field_class_from_props(ctx, fc->base.size,
		fc->base.base.alignment, is_signed,
		bt_field_class_integer_get_preferred_display_base(ir_fc),
		NULL, NULL, false);

	if (bt_field_class_type_is(type, BT_FIELD_CLASS_TYPE_ENUMERATION)) {
		uint64_t i;

		g_string_append(ctx->tsdl, " {\n");
		ctx->indent_level++;

		for (i = 0; i < bt_field_class_enumeration_get_mapping_count(ir_fc); i++) {
			const char *label;
			const bt_field_class_enumeration_mapping *mapping;
			const bt_field_class_enumeration_unsigned_mapping *u_mapping;
			const bt_field_class_enumeration_signed_mapping *s_mapping;
			const bt_integer_range_set *ranges;
			const bt_integer_range_set_unsigned *u_ranges;
			const bt_integer_range_set_signed *s_ranges;
			uint64_t range_count;
			uint64_t range_i;

			if (is_signed) {
				s_mapping = bt_field_class_enumeration_signed_borrow_mapping_by_index_const(
					ir_fc, i);
				mapping = bt_field_class_enumeration_signed_mapping_as_mapping_const(
					s_mapping);
				s_ranges = bt_field_class_enumeration_signed_mapping_borrow_ranges_const(
					s_mapping);
				ranges = bt_integer_range_set_signed_as_range_set_const(
					s_ranges);
			} else {
				u_mapping = bt_field_class_enumeration_unsigned_borrow_mapping_by_index_const(
					ir_fc, i);
				mapping = bt_field_class_enumeration_unsigned_mapping_as_mapping_const(
					u_mapping);
				u_ranges = bt_field_class_enumeration_unsigned_mapping_borrow_ranges_const(
					u_mapping);
				ranges = bt_integer_range_set_unsigned_as_range_set_const(
					u_ranges);
			}

			label = bt_field_class_enumeration_mapping_get_label(
				mapping);
			range_count = bt_integer_range_set_get_range_count(
				ranges);

			for (range_i = 0; range_i < range_count; range_i++) {
				append_indent(ctx);
				g_string_append(ctx->tsdl, "\"");
				append_quoted_string_content(ctx, label);
				g_string_append(ctx->tsdl, "\" = ");

				if (is_signed) {
					const bt_integer_range_signed *range;
					int64_t lower, upper;

					range = bt_integer_range_set_signed_borrow_range_by_index_const(
						s_ranges, range_i);
					lower = bt_integer_range_signed_get_lower(
						range);
					upper = bt_integer_range_signed_get_upper(
						range);

					if (lower == upper) {
						g_string_append_printf(
							ctx->tsdl, "%" PRId64,
							lower);
					} else {
						g_string_append_printf(
							ctx->tsdl, "%" PRId64 " ... %" PRId64,
							lower, upper);
					}
				} else {
					const bt_integer_range_unsigned *range;
					uint64_t lower, upper;

					range = bt_integer_range_set_unsigned_borrow_range_by_index_const(
						u_ranges, range_i);
					lower = bt_integer_range_unsigned_get_lower(
						range);
					upper = bt_integer_range_unsigned_get_upper(
						range);

					if (lower == upper) {
						g_string_append_printf(
							ctx->tsdl, "%" PRIu64,
							lower);
					} else {
						g_string_append_printf(
							ctx->tsdl, "%" PRIu64 " ... %" PRIu64,
							lower, upper);
					}
				}

				g_string_append(ctx->tsdl, ",\n");
			}
		}

		append_end_block(ctx);
	}
}

static
void append_float_field_class(struct ctx *ctx,
		struct fs_sink_ctf_field_class_float *fc)
{
	unsigned int mant_dig, exp_dig;

	if (bt_field_class_get_type(fc->base.base.ir_fc) ==
			BT_FIELD_CLASS_TYPE_SINGLE_PRECISION_REAL) {
		mant_dig = 24;
		exp_dig = 8;
	} else {
		mant_dig = 53;
		exp_dig = 11;
	}

	g_string_append_printf(ctx->tsdl,
		"floating_point { mant_dig = %u; exp_dig = %u; align = %u; }",
		mant_dig, exp_dig, fc->base.base.alignment);
}

static
void append_string_field_class(struct ctx *ctx,
		struct fs_sink_ctf_field_class_float *fc)
{
	g_string_append(ctx->tsdl, "string { encoding = UTF8; }");
}

static
void append_field_class(struct ctx *ctx, struct fs_sink_ctf_field_class *fc);

static
void append_member(struct ctx *ctx, const char *name,
		struct fs_sink_ctf_field_class *fc)
{
	GString *lengths = NULL;
	const char *lengths_str = "";

	BT_ASSERT(fc);

	while (fc->type == FS_SINK_CTF_FIELD_CLASS_TYPE_ARRAY ||
			fc->type == FS_SINK_CTF_FIELD_CLASS_TYPE_SEQUENCE) {
		if (!lengths) {
			lengths = g_string_new(NULL);
			BT_ASSERT(lengths);
		}

		if (fc->type == FS_SINK_CTF_FIELD_CLASS_TYPE_ARRAY) {
			struct fs_sink_ctf_field_class_array *array_fc =
				(void *) fc;

			g_string_append_printf(lengths, "[%" PRIu64 "]",
				array_fc->length);
			fc = array_fc->base.elem_fc;
		} else {
			struct fs_sink_ctf_field_class_sequence *seq_fc =
				(void *) fc;

			g_string_append_printf(lengths, "[%s]",
				seq_fc->length_ref->str);
			fc = seq_fc->base.elem_fc;
		}
	}

	append_field_class(ctx, fc);

	if (lengths) {
		lengths_str = lengths->str;
	}

	g_string_append_printf(ctx->tsdl, " %s%s;\n", name, lengths_str);

	if (lengths) {
		g_string_free(lengths, TRUE);
	}
}

static
void append_struct_field_class_members(struct ctx *ctx,
		struct fs_sink_ctf_field_class_struct *struct_fc)
{
	uint64_t i;

	for (i = 0; i < struct_fc->members->len; i++) {
		struct fs_sink_ctf_named_field_class *named_fc =
			fs_sink_ctf_field_class_struct_borrow_member_by_index(
				struct_fc, i);
		struct fs_sink_ctf_field_class *fc = named_fc->fc;

		/*
		 * For sequence, option, and variant field classes, if
		 * the length/tag field class is generated before, write
		 * it now before the dependent field class.
		 */
		if (fc->type == FS_SINK_CTF_FIELD_CLASS_TYPE_SEQUENCE) {
			struct fs_sink_ctf_field_class_sequence *seq_fc =
				(void *) fc;

			if (seq_fc->length_is_before) {
				append_indent(ctx);
				append_integer_field_class_from_props(ctx,
					32, 8, false,
					BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL,
					NULL, seq_fc->length_ref->str, true);
			}
		} else if (fc->type == FS_SINK_CTF_FIELD_CLASS_TYPE_OPTION) {
			struct fs_sink_ctf_field_class_option *opt_fc =
				(void *) fc;

			/*
			 * CTF 1.8 does not support the option field
			 * class type. To write something anyway, this
			 * component translates this type to a variant
			 * field class where the options are:
			 *
			 * * An empty structure field class.
			 * * The optional field class itself.
			 *
			 * The "tag" is always generated/before in that
			 * case (an 8-bit unsigned enumeration field
			 * class).
			 */
			append_indent(ctx);
			g_string_append(ctx->tsdl,
				"/* The enumeration and variant field classes "
				"below were a trace IR option field class. */\n");
			append_indent(ctx);
			g_string_append(ctx->tsdl, "enum : ");
			append_integer_field_class_from_props(ctx,
				8, 8, false,
				BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL,
				NULL, NULL, false);
			g_string_append(ctx->tsdl, " {\n");
			ctx->indent_level++;
			append_indent(ctx);
			g_string_append(ctx->tsdl, "none = 0,\n");
			append_indent(ctx);
			g_string_append(ctx->tsdl, "content = 1,\n");
			append_end_block(ctx);
			g_string_append_printf(ctx->tsdl, " %s;\n",
				opt_fc->tag_ref->str);
		} else if (fc->type == FS_SINK_CTF_FIELD_CLASS_TYPE_VARIANT) {
			struct fs_sink_ctf_field_class_variant *var_fc =
				(void *) fc;

			if (var_fc->tag_is_before) {
				append_indent(ctx);
				g_string_append(ctx->tsdl, "enum : ");
				append_integer_field_class_from_props(ctx,
					16, 8, false,
					BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL,
					NULL, NULL, false);
				g_string_append(ctx->tsdl, " {\n");
				ctx->indent_level++;

				for (i = 0; i < var_fc->options->len; i++) {
					struct fs_sink_ctf_named_field_class *option_named_fc =
						fs_sink_ctf_field_class_variant_borrow_option_by_index(
							var_fc, i);

					append_indent(ctx);
					g_string_append_printf(ctx->tsdl,
						"\"%s\" = %" PRIu64 ",\n",
						option_named_fc->name->str, i);
				}

				append_end_block(ctx);
				g_string_append_printf(ctx->tsdl, " %s;\n",
					var_fc->tag_ref->str);
			}
		} else if (fc->type == FS_SINK_CTF_FIELD_CLASS_TYPE_BOOL) {
			append_indent(ctx);
			g_string_append(ctx->tsdl,
				"/* The integer field class below was a trace IR boolean field class. */\n");
		} else if (fc->type == FS_SINK_CTF_FIELD_CLASS_TYPE_BIT_ARRAY) {
			append_indent(ctx);
			g_string_append(ctx->tsdl,
				"/* The integer field class below was a trace IR bit array field class. */\n");
		}

		append_indent(ctx);
		append_member(ctx, named_fc->name->str, fc);
	}
}

static
void append_struct_field_class(struct ctx *ctx,
		struct fs_sink_ctf_field_class_struct *fc)
{
	g_string_append(ctx->tsdl, "struct {\n");
	ctx->indent_level++;
	append_struct_field_class_members(ctx, fc);
	append_end_block(ctx);
	g_string_append_printf(ctx->tsdl, " align(%u)",
		fc->base.alignment);
}

static
void append_option_field_class(struct ctx *ctx,
		struct fs_sink_ctf_field_class_option *opt_fc)
{
	g_string_append_printf(ctx->tsdl, "variant <%s> {\n",
		opt_fc->tag_ref->str);
	ctx->indent_level++;
	append_indent(ctx);
	g_string_append(ctx->tsdl, "struct { } none;\n");
	append_indent(ctx);
	append_member(ctx, "content", opt_fc->content_fc);
	append_end_block(ctx);
}

static
void append_variant_field_class(struct ctx *ctx,
		struct fs_sink_ctf_field_class_variant *var_fc)
{
	uint64_t i;

	g_string_append_printf(ctx->tsdl, "variant <%s> {\n",
		var_fc->tag_ref->str);
	ctx->indent_level++;

	for (i = 0; i < var_fc->options->len; i++) {
		struct fs_sink_ctf_named_field_class *named_fc =
			fs_sink_ctf_field_class_variant_borrow_option_by_index(
				var_fc, i);

		append_indent(ctx);
		append_member(ctx, named_fc->name->str, named_fc->fc);
	}

	append_end_block(ctx);
}

static
void append_field_class(struct ctx *ctx, struct fs_sink_ctf_field_class *fc)
{
	switch (fc->type) {
	case FS_SINK_CTF_FIELD_CLASS_TYPE_BOOL:
		append_bool_field_class(ctx, (void *) fc);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_BIT_ARRAY:
		append_bit_array_field_class(ctx, (void *) fc);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_INT:
		append_integer_field_class(ctx, (void *) fc);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_FLOAT:
		append_float_field_class(ctx, (void *) fc);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_STRING:
		append_string_field_class(ctx, (void *) fc);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT:
		append_struct_field_class(ctx, (void *) fc);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_OPTION:
		append_option_field_class(ctx, (void *) fc);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_VARIANT:
		append_variant_field_class(ctx, (void *) fc);
		break;
	default:
		bt_common_abort();
	}
}

static
void append_event_class(struct ctx *ctx, struct fs_sink_ctf_event_class *ec)
{
	const char *str;
	bt_event_class_log_level log_level;

	/* Event class */
	append_indent(ctx);
	g_string_append(ctx->tsdl, "event {\n");
	ctx->indent_level++;

	/* Event class properties */
	append_indent(ctx);
	g_string_append(ctx->tsdl, "name = ");
	str = bt_event_class_get_name(ec->ir_ec);
	if (!str) {
		str = "unknown";
	}

	append_quoted_string(ctx, str);
	g_string_append(ctx->tsdl, ";\n");
	append_indent(ctx);
	g_string_append_printf(ctx->tsdl, "stream_id = %" PRIu64 ";\n",
		bt_stream_class_get_id(ec->sc->ir_sc));
	append_indent(ctx);
	g_string_append_printf(ctx->tsdl, "id = %" PRIu64 ";\n",
		bt_event_class_get_id(ec->ir_ec));

	str = bt_event_class_get_emf_uri(ec->ir_ec);
	if (str) {
		append_indent(ctx);
		g_string_append(ctx->tsdl, "model.emf.uri = ");
		append_quoted_string(ctx, str);
		g_string_append(ctx->tsdl, ";\n");
	}

	if (bt_event_class_get_log_level(ec->ir_ec, &log_level) ==
			BT_PROPERTY_AVAILABILITY_AVAILABLE) {
		unsigned int level;

		append_indent(ctx);
		g_string_append(ctx->tsdl, "loglevel = ");

		switch (log_level) {
		case BT_EVENT_CLASS_LOG_LEVEL_EMERGENCY:
			level = 0;
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_ALERT:
			level = 1;
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_CRITICAL:
			level = 2;
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_ERROR:
			level = 3;
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_WARNING:
			level = 4;
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_NOTICE:
			level = 5;
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_INFO:
			level = 6;
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM:
			level = 7;
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM:
			level = 8;
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS:
			level = 9;
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE:
			level = 10;
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT:
			level = 11;
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION:
			level = 12;
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE:
			level = 13;
			break;
		case BT_EVENT_CLASS_LOG_LEVEL_DEBUG:
			level = 14;
			break;
		default:
			bt_common_abort();
		}

		g_string_append_printf(ctx->tsdl, "%u;\n", level);
	}

	/* Event specific context field class */
	if (ec->spec_context_fc) {
		append_indent(ctx);
		g_string_append(ctx->tsdl, "context := ");
		append_field_class(ctx, ec->spec_context_fc);
		g_string_append(ctx->tsdl, ";\n");
	}

	/* Event payload field class */
	if (ec->payload_fc) {
		append_indent(ctx);
		g_string_append(ctx->tsdl, "fields := ");
		append_field_class(ctx, ec->payload_fc);
		g_string_append(ctx->tsdl, ";\n");
	}

	append_end_block_semi_nl_nl(ctx);
}

static
void append_stream_class(struct ctx *ctx,
		struct fs_sink_ctf_stream_class *sc)
{
	uint64_t i;

	/* Default clock class */
	if (sc->default_clock_class) {
		const char *descr;
		int64_t offset_seconds;
		uint64_t offset_cycles;
		bt_uuid uuid;

		append_indent(ctx);
		g_string_append(ctx->tsdl, "clock {\n");
		ctx->indent_level++;
		BT_ASSERT(sc->default_clock_class_name->len > 0);
		append_indent(ctx);
		g_string_append_printf(ctx->tsdl, "name = %s;\n",
			sc->default_clock_class_name->str);
		descr = bt_clock_class_get_description(sc->default_clock_class);
		if (descr) {
			append_indent(ctx);
			g_string_append(ctx->tsdl, "description = ");
			append_quoted_string(ctx, descr);
			g_string_append(ctx->tsdl, ";\n");
		}

		append_indent(ctx);
		g_string_append_printf(ctx->tsdl, "freq = %" PRIu64 ";\n",
			bt_clock_class_get_frequency(sc->default_clock_class));
		append_indent(ctx);
		g_string_append_printf(ctx->tsdl, "precision = %" PRIu64 ";\n",
			bt_clock_class_get_precision(sc->default_clock_class));
		bt_clock_class_get_offset(sc->default_clock_class,
			&offset_seconds, &offset_cycles);
		append_indent(ctx);
		g_string_append_printf(ctx->tsdl, "offset_s = %" PRId64 ";\n",
			offset_seconds);
		append_indent(ctx);
		g_string_append_printf(ctx->tsdl, "offset = %" PRIu64 ";\n",
			offset_cycles);
		append_indent(ctx);
		g_string_append(ctx->tsdl, "absolute = ");

		if (bt_clock_class_origin_is_unix_epoch(
				sc->default_clock_class)) {
			g_string_append(ctx->tsdl, "true");
		} else {
			g_string_append(ctx->tsdl, "false");
		}

		g_string_append(ctx->tsdl, ";\n");
		uuid = bt_clock_class_get_uuid(sc->default_clock_class);
		if (uuid) {
			append_indent(ctx);
			g_string_append(ctx->tsdl, "uuid = ");
			append_uuid(ctx, uuid);
			g_string_append(ctx->tsdl, ";\n");
		}

		/* End clock class */
		append_end_block_semi_nl_nl(ctx);
	}

	/* Stream class */
	append_indent(ctx);
	g_string_append(ctx->tsdl, "stream {\n");
	ctx->indent_level++;

	/* Stream class properties */
	append_indent(ctx);
	g_string_append_printf(ctx->tsdl, "id = %" PRIu64 ";\n",
		bt_stream_class_get_id(sc->ir_sc));

	/* Packet context field class */
	append_indent(ctx);
	g_string_append(ctx->tsdl, "packet.context := struct {\n");
	ctx->indent_level++;
	append_indent(ctx);
	append_integer_field_class_from_props(ctx, 64, 8, false,
		BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL,
		NULL, "packet_size", true);
	append_indent(ctx);
	append_integer_field_class_from_props(ctx, 64, 8, false,
		BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL,
		NULL, "content_size", true);

	if (sc->packets_have_ts_begin) {
		append_indent(ctx);
		append_integer_field_class_from_props(ctx, 64, 8, false,
			BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL,
			sc->default_clock_class_name->str,
			"timestamp_begin", true);
	}

	if (sc->packets_have_ts_end) {
		append_indent(ctx);
		append_integer_field_class_from_props(ctx, 64, 8, false,
			BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL,
			sc->default_clock_class_name->str,
			"timestamp_end", true);
	}

	if (sc->has_discarded_events) {
		append_indent(ctx);
		append_integer_field_class_from_props(ctx, 64, 8, false,
			BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL,
			NULL, "events_discarded", true);
	}

	/*
	 * Unconditionnally write the packet sequence number as, even if
	 * there's no possible discarded packets message, it's still
	 * useful information to have.
	 */
	append_indent(ctx);
	append_integer_field_class_from_props(ctx, 64, 8, false,
		BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL,
		NULL, "packet_seq_num", true);

	if (sc->packet_context_fc) {
		append_struct_field_class_members(ctx,
			(void *) sc->packet_context_fc);
		fs_sink_ctf_field_class_struct_align_at_least(
			(void *) sc->packet_context_fc, 8);
	}

	/* End packet context field class */
	append_end_block(ctx);
	g_string_append_printf(ctx->tsdl, " align(%u);\n\n",
		sc->packet_context_fc ? sc->packet_context_fc->alignment : 8);

	/* Event header field class */
	append_indent(ctx);
	g_string_append(ctx->tsdl, "event.header := struct {\n");
	ctx->indent_level++;
	append_indent(ctx);
	append_integer_field_class_from_props(ctx, 64, 8, false,
		BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL,
		NULL, "id", true);

	if (sc->default_clock_class) {
		append_indent(ctx);
		append_integer_field_class_from_props(ctx, 64, 8, false,
			BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL,
			sc->default_clock_class_name->str,
			"timestamp", true);
	}

	/* End event header field class */
	append_end_block(ctx);
	g_string_append(ctx->tsdl, " align(8);\n");

	/* Event common context field class */
	if (sc->event_common_context_fc) {
		append_indent(ctx);
		g_string_append(ctx->tsdl, "event.context := ");
		append_field_class(ctx,
			(void *) sc->event_common_context_fc);
		g_string_append(ctx->tsdl, ";\n");
	}

	/* End stream class */
	append_end_block_semi_nl_nl(ctx);

	/* Event classes */
	for (i = 0; i < sc->event_classes->len; i++) {
		append_event_class(ctx, sc->event_classes->pdata[i]);
	}
}

BT_HIDDEN
void translate_trace_ctf_ir_to_tsdl(struct fs_sink_ctf_trace *trace,
		GString *tsdl)
{
	struct ctx ctx = {
		.indent_level = 0,
		.tsdl = tsdl,
	};
	uint64_t i;
	uint64_t count;

	g_string_assign(tsdl, "/* CTF 1.8 */\n\n");
	g_string_append(tsdl, "/* This was generated by a Babeltrace `sink.ctf.fs` component. */\n\n");

	/* Trace class */
	append_indent(&ctx);
	g_string_append(tsdl, "trace {\n");
	ctx.indent_level++;

	/* Trace class properties */
	append_indent(&ctx);
	g_string_append(tsdl, "major = 1;\n");
	append_indent(&ctx);
	g_string_append(tsdl, "minor = 8;\n");
	append_indent(&ctx);
	g_string_append(tsdl, "uuid = ");
	append_uuid(&ctx, trace->uuid);
	g_string_append(tsdl, ";\n");
	append_indent(&ctx);
	g_string_append(tsdl, "byte_order = ");

	if (BYTE_ORDER == LITTLE_ENDIAN) {
		g_string_append(tsdl, "le");
	} else {
		g_string_append(tsdl, "be");
	}

	g_string_append(tsdl, ";\n");

	/* Packet header field class */
	append_indent(&ctx);
	g_string_append(tsdl, "packet.header := struct {\n");
	ctx.indent_level++;
	append_indent(&ctx);
	append_integer_field_class_from_props(&ctx, 32, 8, false,
		BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL,
		NULL, "magic", true);
	append_indent(&ctx);
	append_integer_field_class_from_props(&ctx, 8, 8, false,
		BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL,
		NULL, "uuid[16]", true);
	append_indent(&ctx);
	append_integer_field_class_from_props(&ctx, 64, 8, false,
		BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL,
		NULL, "stream_id", true);
	append_indent(&ctx);
	append_integer_field_class_from_props(&ctx, 64, 8, false,
		BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL,
		NULL, "stream_instance_id", true);

	/* End packet header field class */
	append_end_block(&ctx);
	g_string_append(ctx.tsdl, " align(8);\n");

	/* End trace class */
	append_end_block_semi_nl_nl(&ctx);

	/* Trace environment */
	count = bt_trace_get_environment_entry_count(trace->ir_trace);
	if (count > 0) {
		append_indent(&ctx);
		g_string_append(tsdl, "env {\n");
		ctx.indent_level++;

		for (i = 0; i < count; i++) {
			const char *name;
			const bt_value *val;

			bt_trace_borrow_environment_entry_by_index_const(
				trace->ir_trace, i, &name, &val);
			append_indent(&ctx);
			g_string_append_printf(tsdl, "%s = ", name);

			switch (bt_value_get_type(val)) {
			case BT_VALUE_TYPE_SIGNED_INTEGER:
				g_string_append_printf(tsdl, "%" PRId64,
					bt_value_integer_signed_get(val));
				break;
			case BT_VALUE_TYPE_STRING:
				append_quoted_string(&ctx, bt_value_string_get(val));
				break;
			default:
				/*
				 * This is checked in
				 * translate_trace_trace_ir_to_ctf_ir().
				 */
				bt_common_abort();
			}

			g_string_append(tsdl, ";\n");
		}

		/* End trace class environment */
		append_end_block_semi_nl_nl(&ctx);
	}

	/* Stream classes and their event classes */
	for (i = 0; i < trace->stream_classes->len; i++) {
		append_stream_class(&ctx, trace->stream_classes->pdata[i]);
	}
}
