/*
 * print.c
 *
 * Babeltrace CTF Text Output Plugin Event Printing
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/ctf-ir/event.h>
#include <babeltrace/ctf-ir/event-class.h>
#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/clock.h>
#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/bitfield.h>
#include <inttypes.h>
#include "text.h"

static inline
const char *rem_(const char *str)
{
	if (str[0] == '_')
		return &str[1];
	else
		return str;
}

struct timestamp {
	int64_t real_timestamp;	/* Relative to UNIX epoch. */
	uint64_t clock_value;	/* In cycles. */
};

static
enum bt_component_status print_field(struct text_component *text,
		struct bt_ctf_field *field, bool print_names);

static
void print_timestamp_cycles(struct text_component *text,
		struct bt_ctf_clock *clock,
		struct bt_ctf_event *event)
{
	fputs("00000000000000000000", text->out);
}

static
void print_timestamp_wall(struct text_component *text,
		struct bt_ctf_clock *clock,
		struct bt_ctf_event *event)
{
	fputs("??:??:??.?????????", text->out);
}

static
enum bt_component_status get_event_timestamp(struct bt_ctf_event *event)
{
/*	int ret;
	uint64_t value, frequency;
	int64_t offset_s, offset;
*/
	return BT_COMPONENT_STATUS_OK;
}

static
enum bt_component_status print_event_timestamp(struct text_component *text,
		struct bt_ctf_event *event)
{
	bool print_names = text->options.print_header_field_names;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_stream *stream = NULL;
	struct bt_ctf_clock *clock = NULL;
	FILE *out = text->out;
	FILE *err = text->err;
	uint64_t real_timestamp;

	stream = bt_ctf_event_get_stream(event);
	if (!stream) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	clock = bt_ctf_event_get_clock(event);
	if (!clock) {
		/* Stream has no timestamp. */
		//puts("no_timestamp!");
		//goto end;
	}

	fputs(print_names ? "timestamp = " : "[", out);
	if (text->options.print_timestamp_cycles) {
		print_timestamp_cycles(text, clock, event);
	} else {
		print_timestamp_wall(text, clock, event);
	}

	fputs(print_names ? ", " : "] ", out);
	if (!text->options.print_delta_field) {
		goto end;
	}
end:
	bt_put(stream);
	bt_put(clock);
	return ret;
}

static
enum bt_component_status print_event_header(struct text_component *text,
		struct bt_ctf_event *event)
{
	bool print_names = text->options.print_header_field_names;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_event_class *event_class = NULL;

	event_class = bt_ctf_event_get_class(event);
	if (!event_class) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	if (!text->start_line) {
		fputs(", ", text->out);
	}
	text->start_line = false;
	ret = print_event_timestamp(text, event);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (print_names) {
		fputs("name = ", text->out);
	}
	fputs(bt_ctf_event_class_get_name(event_class), text->out);
	bt_put(event_class);
end:
	bt_put(event_class);
	return ret;
}

static
enum bt_component_status print_integer(struct text_component *text,
		struct bt_ctf_field *field)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field_type *field_type = NULL;
	enum bt_ctf_integer_base base;
	enum bt_ctf_string_encoding encoding;
	int signedness;
	union {
		uint64_t u;
		int64_t s;
	} v;

	field_type = bt_ctf_field_get_type(field);
	if (!field_type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	signedness = bt_ctf_field_type_integer_get_signed(field_type);
	if (signedness < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	if (!signedness) {
		if (bt_ctf_field_unsigned_integer_get_value(field, &v.u) < 0) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
	} else {
		if (bt_ctf_field_signed_integer_get_value(field, &v.s) < 0) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
	}

	encoding = bt_ctf_field_type_integer_get_encoding(field_type);
	switch (encoding) {
	case BT_CTF_STRING_ENCODING_UTF8:
	case BT_CTF_STRING_ENCODING_ASCII:
		g_string_append_c(text->string, (int) v.u);
		goto end;
	case BT_CTF_STRING_ENCODING_NONE:
	case BT_CTF_STRING_ENCODING_UNKNOWN:
		break;
	default:
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	base = bt_ctf_field_type_integer_get_base(field_type);
	switch (base) {
	case BT_CTF_INTEGER_BASE_BINARY:
	{
		int bitnr, len;

		len = bt_ctf_field_type_integer_get_size(field_type);
		if (len < 0) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		fprintf(text->out, "0b");
		v.u = _bt_piecewise_lshift(v.u, 64 - len);
		for (bitnr = 0; bitnr < len; bitnr++) {
			fprintf(text->out, "%u", (v.u & (1ULL << 63)) ? 1 : 0);
			v.u = _bt_piecewise_lshift(v.u, 1);
		}
		break;
	}
	case BT_CTF_INTEGER_BASE_OCTAL:
	{
		if (signedness) {
			int len;

			len = bt_ctf_field_type_integer_get_size(field_type);
			if (len < 0) {
				ret = BT_COMPONENT_STATUS_ERROR;
				goto end;
			}
			if (len < 64) {
			        size_t rounded_len;

				assert(len != 0);
				/* Round length to the nearest 3-bit */
				rounded_len = (((len - 1) / 3) + 1) * 3;
				v.u &= ((uint64_t) 1 << rounded_len) - 1;
			}
		}

		fprintf(text->out, "0%" PRIo64, v.u);
		break;
	}
	case BT_CTF_INTEGER_BASE_DECIMAL:
		if (!signedness) {
			fprintf(text->out, "%" PRIu64, v.u);
		} else {
			fprintf(text->out, "%" PRId64, v.s);
		}
		break;
	case BT_CTF_INTEGER_BASE_HEXADECIMAL:
	{
		int len;

		len = bt_ctf_field_type_integer_get_size(field_type);
		if (len < 0) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		if (len < 64) {
			/* Round length to the nearest nibble */
			uint8_t rounded_len = ((len + 3) & ~0x3);

			v.u &= ((uint64_t) 1 << rounded_len) - 1;
		}

		fprintf(text->out, "0x%" PRIX64, v.u);
		break;
	}
	default:
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
end:
	bt_put(field_type);
	return ret;
}

static
enum bt_component_status print_enum(struct text_component *text,
		struct bt_ctf_field *field)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field *container_field = NULL;
	const char *mapping_name;

	container_field = bt_ctf_field_enumeration_get_container(field);
	if (!container_field) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	mapping_name = bt_ctf_field_enumeration_get_mapping_name(field);
	if (mapping_name) {
		fprintf(text->out, "( \"%s\"", mapping_name);
	} else {
		fprintf(text->out, "( <unknown>");
	}
	fprintf(text->out, " : container = ");
	ret = print_integer(text, container_field);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	fprintf(text->out, " )");
end:
	bt_put(container_field);
	return ret;
}

static
enum bt_component_status print_struct_field(struct text_component *text,
		struct bt_ctf_field *_struct,
		struct bt_ctf_field_type *struct_type,
		int i, bool print_names)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	const char *field_name;
	struct bt_ctf_field *field = NULL;
	struct bt_ctf_field_type *field_type = NULL;;

	field = bt_ctf_field_structure_get_field_by_index(_struct, i);
	if (!field) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	if (bt_ctf_field_type_structure_get_field(struct_type,
			&field_name, &field_type, i) < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	if (i != 0) {
		fprintf(text->out, ", ");
	} else {
		fprintf(text->out, " ");
	}
	if (print_names) {
		fprintf(text->out, "%s = ", rem_(field_name));
	}
	ret = print_field(text, field, print_names);
end:
	bt_put(field_type);
	bt_put(field);
	return ret;
}

static
enum bt_component_status print_struct(struct text_component *text,
		struct bt_ctf_field *_struct, bool print_names)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field_type *struct_type = NULL;
	int nr_fields, i;

	struct_type = bt_ctf_field_get_type(_struct);
	if (!struct_type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	nr_fields = bt_ctf_field_type_structure_get_field_count(struct_type);
	if (nr_fields < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	fprintf(text->out, "{");
	text->depth++;
	for (i = 0; i < nr_fields; i++) {
		ret = print_struct_field(text, _struct, struct_type, i,
				print_names);
		if (ret != BT_COMPONENT_STATUS_OK) {
			goto end;
		}
	}
	text->depth--;
	fprintf(text->out, " }");
end:
	bt_put(struct_type);
	return ret;
}

static
enum bt_component_status print_array_field(struct text_component *text,
		struct bt_ctf_field *array, uint64_t i,
		bool is_string, bool print_names)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field *field = NULL;

	if (!is_string) {
		if (i != 0) {
			fprintf(text->out, ", ");
		} else {
			fprintf(text->out, " ");
		}
	}
	field = bt_ctf_field_array_get_field(array, i);
	if (!field) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	ret = print_field(text, field, print_names);
end:
	bt_put(field);
	return ret;
}

static
enum bt_component_status print_array(struct text_component *text,
		struct bt_ctf_field *array, bool print_names)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field_type *array_type = NULL, *field_type = NULL;
	enum bt_ctf_type_id type_id;
	int64_t len;
	uint64_t i;
	bool is_string = false;

	array_type = bt_ctf_field_get_type(array);
	if (!array_type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	field_type = bt_ctf_field_type_array_get_element_type(array_type);
	if (!field_type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	len = bt_ctf_field_type_array_get_length(array_type);
	if (len < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	type_id = bt_ctf_field_type_get_type_id(field_type);
	if (type_id == BT_CTF_TYPE_ID_INTEGER) {
		enum bt_ctf_string_encoding encoding;

		encoding = bt_ctf_field_type_integer_get_encoding(field_type);
		if (encoding == BT_CTF_STRING_ENCODING_UTF8
				|| encoding == BT_CTF_STRING_ENCODING_ASCII) {
			int integer_len, integer_alignment;

			integer_len = bt_ctf_field_type_integer_get_size(field_type);
			if (integer_len < 0) {
				return BT_COMPONENT_STATUS_ERROR;
			}
			integer_alignment = bt_ctf_field_type_get_alignment(field_type);
			if (integer_alignment < 0) {
				return BT_COMPONENT_STATUS_ERROR;
			}
			if (integer_len == CHAR_BIT
					&& integer_alignment == CHAR_BIT) {
				is_string = true;
			}
		}
	}

	if (is_string) {
		g_string_assign(text->string, "");
	} else {
		fprintf(text->out, "[");
	}

	text->depth++;
	for (i = 0; i < len; i++) {
		ret = print_array_field(text, array, i, is_string, print_names);
		if (ret != BT_COMPONENT_STATUS_OK) {
			goto end;
		}
	}
	text->depth--;

	if (is_string) {
		fprintf(text->out, "\"%s\"", text->string->str);
	} else {
		fprintf(text->out, " ]");
	}
end:
	bt_put(field_type);
	bt_put(array_type);
	return ret;
}

static
enum bt_component_status print_sequence_field(struct text_component *text,
		struct bt_ctf_field *seq, uint64_t i,
		bool is_string, bool print_names)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field *field = NULL;

	if (!is_string) {
		if (i != 0) {
			fprintf(text->out, ", ");
		} else {
			fprintf(text->out, " ");
		}
	}
	field = bt_ctf_field_sequence_get_field(seq, i);
	if (!field) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	ret = print_field(text, field, print_names);
end:
	bt_put(field);
	return ret;
}

static
enum bt_component_status print_sequence(struct text_component *text,
		struct bt_ctf_field *seq, bool print_names)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field_type *seq_type = NULL, *field_type = NULL;
	struct bt_ctf_field *length_field = NULL;
	enum bt_ctf_type_id type_id;
	uint64_t len;
	uint64_t i;
	bool is_string = false;

	seq_type = bt_ctf_field_get_type(seq);
	if (!seq_type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	length_field = bt_ctf_field_sequence_get_length(seq);
	if (!length_field) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	if (bt_ctf_field_unsigned_integer_get_value(length_field, &len) < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	field_type = bt_ctf_field_type_sequence_get_element_type(seq_type);
	if (!field_type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	type_id = bt_ctf_field_type_get_type_id(field_type);
	if (type_id == BT_CTF_TYPE_ID_INTEGER) {
		enum bt_ctf_string_encoding encoding;

		encoding = bt_ctf_field_type_integer_get_encoding(field_type);
		if (encoding == BT_CTF_STRING_ENCODING_UTF8
				|| encoding == BT_CTF_STRING_ENCODING_ASCII) {
			int integer_len, integer_alignment;

			integer_len = bt_ctf_field_type_integer_get_size(field_type);
			if (integer_len < 0) {
				ret = BT_COMPONENT_STATUS_ERROR;
				goto end;
			}
			integer_alignment = bt_ctf_field_type_get_alignment(field_type);
			if (integer_alignment < 0) {
				ret = BT_COMPONENT_STATUS_ERROR;
				goto end;
			}
			if (integer_len == CHAR_BIT
					&& integer_alignment == CHAR_BIT) {
				is_string = true;
			}
		}
	}

	if (is_string) {
		g_string_assign(text->string, "");
	} else {
		fprintf(text->out, "[");
	}

	text->depth++;
	for (i = 0; i < len; i++) {
		ret = print_sequence_field(text, seq, i,
			is_string, print_names);
		if (ret != BT_COMPONENT_STATUS_OK) {
			goto end;
		}
	}
	text->depth--;

	if (is_string) {
		fprintf(text->out, "\"%s\"", text->string->str);
	} else {
		fprintf(text->out, " ]");
	}
end:
	bt_put(length_field);
	bt_put(field_type);
	bt_put(seq_type);
	return ret;
}

static
enum bt_component_status print_variant(struct text_component *text,
		struct bt_ctf_field *variant, bool print_names)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field *field = NULL;

	field = bt_ctf_field_variant_get_current_field(variant);
	if (!field) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	fprintf(text->out, "{ ");
	text->depth++;
	if (print_names) {
		struct bt_ctf_field *tag_field = NULL;
		const char *tag_choice;

		tag_field = bt_ctf_field_variant_get_tag(variant);
		if (!tag_field) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		tag_choice = bt_ctf_field_enumeration_get_mapping_name(tag_field);
		if (!tag_choice) {
			bt_put(tag_field);
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		fprintf(text->out, "%s = ", rem_(tag_choice));
		bt_put(tag_field);
	}
	ret = print_field(text, field, print_names);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->depth--;
	fprintf(text->out, " }");
end:
	bt_put(field);
	return ret;
}

static
enum bt_component_status print_field(struct text_component *text,
		struct bt_ctf_field *field, bool print_names)
{
	enum bt_ctf_type_id type_id;

	type_id = bt_ctf_field_get_type_id(field);
	switch (type_id) {
	case CTF_TYPE_INTEGER:
		return print_integer(text, field);
	case CTF_TYPE_FLOAT:
	{
		double v;

		if (bt_ctf_field_floating_point_get_value(field, &v)) {
			return BT_COMPONENT_STATUS_ERROR;
		}
		fprintf(text->out, "%g", v);
		return BT_COMPONENT_STATUS_OK;
	}
	case CTF_TYPE_ENUM:
		return print_enum(text, field);
	case CTF_TYPE_STRING:
		fprintf(text->out, "\"%s\"", bt_ctf_field_string_get_value(field));
		return BT_COMPONENT_STATUS_OK;
	case CTF_TYPE_STRUCT:
		return print_struct(text, field, print_names);
	case CTF_TYPE_UNTAGGED_VARIANT:
	case CTF_TYPE_VARIANT:
		return print_variant(text, field, print_names);
	case CTF_TYPE_ARRAY:
		return print_array(text, field, print_names);
	case CTF_TYPE_SEQUENCE:
		return print_sequence(text, field, print_names);
	default:
		fprintf(text->err, "[error] Unknown type id: %d\n", (int) type_id);
		return BT_COMPONENT_STATUS_ERROR;
	}
}

static
enum bt_component_status print_stream_packet_context(struct text_component *text,
		struct bt_ctf_event *event)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_packet *packet = NULL;
	struct bt_ctf_field *main_field = NULL;

	packet = bt_ctf_event_get_packet(event);
	if (!packet) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	main_field = bt_ctf_packet_get_context(packet);
	if (!main_field) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	if (!text->start_line) {
		fputs(", ", text->out);
	}
	text->start_line = false;
	if (text->options.print_scope_field_names) {
		fputs("stream.packet.context = ", text->out);
	}
	ret = print_field(text, main_field,
			text->options.print_context_field_names);
end:
	bt_put(main_field);
	bt_put(packet);
	return ret;
}

static
enum bt_component_status print_event_header_raw(struct text_component *text,
		struct bt_ctf_event *event)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field *main_field = NULL;

	main_field = bt_ctf_event_get_header(event);
	if (!main_field) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	if (!text->start_line) {
		fputs(", ", text->out);
	}
	text->start_line = false;
	if (text->options.print_scope_field_names) {
		fputs("stream.event.header = ", text->out);
	}
	ret = print_field(text, main_field,
			text->options.print_header_field_names);
end:
	bt_put(main_field);
	return ret;
}

static
enum bt_component_status print_stream_event_context(struct text_component *text,
		struct bt_ctf_event *event)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field *main_field = NULL;

	main_field = bt_ctf_event_get_stream_event_context(event);
	if (!main_field) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	if (!text->start_line) {
		fputs(", ", text->out);
	}
	text->start_line = false;
	if (text->options.print_scope_field_names) {
		fputs("stream.event.context = ", text->out);
	}
	ret = print_field(text, main_field,
			text->options.print_context_field_names);
end:
	bt_put(main_field);
	return ret;
}

static
enum bt_component_status print_event_context(struct text_component *text,
		struct bt_ctf_event *event)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field *main_field = NULL;

	main_field = bt_ctf_event_get_event_context(event);
	if (!main_field) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	if (!text->start_line) {
		fputs(", ", text->out);
	}
	text->start_line = false;
	if (text->options.print_scope_field_names) {
		fputs("event.context = ", text->out);
	}
	ret = print_field(text, main_field,
			text->options.print_context_field_names);
end:
	bt_put(main_field);
	return ret;
}

static
enum bt_component_status print_event_payload(struct text_component *text,
		struct bt_ctf_event *event)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field *main_field = NULL;

	main_field = bt_ctf_event_get_payload_field(event);
	if (!main_field) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	if (!text->start_line) {
		fputs(", ", text->out);
	}
	text->start_line = false;
	if (text->options.print_scope_field_names) {
		fputs("event.fields = ", text->out);
	}
	ret = print_field(text, main_field,
			text->options.print_payload_field_names);
end:
	bt_put(main_field);
	return ret;
}

BT_HIDDEN
enum bt_component_status text_print_event(struct text_component *text,
		struct bt_ctf_event *event)
{
	enum bt_component_status ret;

	text->start_line = true;
	ret = print_event_header(text, event);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	ret = print_stream_packet_context(text, event);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	ret = print_event_header_raw(text, event);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	ret = print_stream_event_context(text, event);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	ret = print_event_context(text, event);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	ret = print_event_payload(text, event);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	fputc('\n', text->out);
end:
	return ret;
}
