/*
 * print.c
 *
 * Babeltrace CTF Text Output Plugin Event Printing
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2016 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/bitfield.h>
#include <babeltrace/common-internal.h>
#include <inttypes.h>
#include "text.h"

#define NSEC_PER_SEC 1000000000LL

#define COLOR_NAME			BT_COMMON_COLOR_BOLD
#define COLOR_FIELD_NAME		BT_COMMON_COLOR_FG_CYAN
#define COLOR_RST			BT_COMMON_COLOR_RESET
#define COLOR_STRING_VALUE		BT_COMMON_COLOR_BOLD
#define COLOR_NUMBER_VALUE		BT_COMMON_COLOR_BOLD
#define COLOR_ENUM_MAPPING_NAME		BT_COMMON_COLOR_BOLD
#define COLOR_UNKNOWN			BT_COMMON_COLOR_BOLD BT_COMMON_COLOR_FG_RED
#define COLOR_EVENT_NAME		BT_COMMON_COLOR_BOLD BT_COMMON_COLOR_FG_MAGENTA
#define COLOR_TIMESTAMP			BT_COMMON_COLOR_BOLD BT_COMMON_COLOR_FG_YELLOW

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
		struct bt_ctf_field *field, bool print_names,
		GQuark *filters_fields, int filter_array_len);

static
void print_name_equal(struct text_component *text, const char *name)
{
	if (text->use_colors) {
		fprintf(text->out, "%s%s%s = ", COLOR_NAME, name, COLOR_RST);
	} else {
		fprintf(text->out, "%s = ", name);
	}
}

static
void print_field_name_equal(struct text_component *text, const char *name)
{
	if (text->use_colors) {
		fprintf(text->out, "%s%s%s = ", COLOR_FIELD_NAME, name,
			COLOR_RST);
	} else {
		fprintf(text->out, "%s = ", name);
	}
}

static
void print_timestamp_cycles(struct text_component *text,
		struct bt_ctf_clock_class *clock_class,
		struct bt_ctf_event *event)
{
	int ret;
	struct bt_ctf_clock_value *clock_value;
	uint64_t cycles;

	clock_value = bt_ctf_event_get_clock_value(event, clock_class);
	if (!clock_value) {
	        fputs("????????????????????", text->out);
		return;
	}

	ret = bt_ctf_clock_value_get_value(clock_value, &cycles);
	bt_put(clock_value);
	if (ret) {
	        fprintf(text->out, "Error");
		return;
	}
	fprintf(text->out, "%020" PRIu64, cycles);

	if (text->last_cycles_timestamp != -1ULL) {
		text->delta_cycles = cycles - text->last_cycles_timestamp;
	}
	text->last_cycles_timestamp = cycles;
}

static
void print_timestamp_wall(struct text_component *text,
		struct bt_ctf_clock_class *clock_class,
		struct bt_ctf_event *event)
{
	int ret;
	struct bt_ctf_clock_value *clock_value;
	int64_t ts_nsec = 0;	/* add configurable offset */
	int64_t ts_sec = 0;	/* add configurable offset */
	uint64_t ts_sec_abs, ts_nsec_abs;
	bool is_negative;

	clock_value = bt_ctf_event_get_clock_value(event, clock_class);
	if (!clock_value) {
		fputs("??:??:??.?????????", text->out);
		return;
	}

	ret = bt_ctf_clock_value_get_value_ns_from_epoch(clock_value, &ts_nsec);
	bt_put(clock_value);
	if (ret) {
	        fprintf(text->out, "Error");
		return;
	}

	if (text->last_real_timestamp != -1ULL) {
		text->delta_real_timestamp = ts_nsec - text->last_real_timestamp;
	}
	text->last_real_timestamp = ts_nsec;

	ts_sec += ts_nsec / NSEC_PER_SEC;
	ts_nsec = ts_nsec % NSEC_PER_SEC;
	if (ts_sec >= 0 && ts_nsec >= 0) {
		is_negative = false;
		ts_sec_abs = ts_sec;
		ts_nsec_abs = ts_nsec;
	} else if (ts_sec > 0 && ts_nsec < 0) {
		is_negative = false;
		ts_sec_abs = ts_sec - 1;
		ts_nsec_abs = NSEC_PER_SEC + ts_nsec;
	} else if (ts_sec == 0 && ts_nsec < 0) {
		is_negative = true;
		ts_sec_abs = ts_sec;
		ts_nsec_abs = -ts_nsec;
	} else if (ts_sec < 0 && ts_nsec > 0) {
		is_negative = true;
		ts_sec_abs = -(ts_sec + 1);
		ts_nsec_abs = NSEC_PER_SEC - ts_nsec;
	} else if (ts_sec < 0 && ts_nsec == 0) {
		is_negative = true;
		ts_sec_abs = -ts_sec;
		ts_nsec_abs = ts_nsec;
	} else {	/* (ts_sec < 0 && ts_nsec < 0) */
		is_negative = true;
		ts_sec_abs = -ts_sec;
		ts_nsec_abs = -ts_nsec;
	}

	if (!text->options.clock_seconds) {
		struct tm tm;
		time_t time_s = (time_t) ts_sec_abs;

		if (is_negative) {
			fprintf(stderr, "[warning] Fallback to [sec.ns] to print negative time value. Use --clock-seconds.\n");
			goto seconds;
		}

		if (!text->options.clock_gmt) {
			struct tm *res;

			res = localtime_r(&time_s, &tm);
			if (!res) {
				fprintf(stderr, "[warning] Unable to get localtime.\n");
				goto seconds;
			}
		} else {
			struct tm *res;

			res = gmtime_r(&time_s, &tm);
			if (!res) {
				fprintf(stderr, "[warning] Unable to get gmtime.\n");
				goto seconds;
			}
		}
		if (text->options.clock_date) {
			char timestr[26];
			size_t res;

			/* Print date and time */
			res = strftime(timestr, sizeof(timestr),
					"%F ", &tm);
			if (!res) {
				fprintf(stderr, "[warning] Unable to print ascii time.\n");
				goto seconds;
			}
			fprintf(text->out, "%s", timestr);
		}
		/* Print time in HH:MM:SS.ns */
		fprintf(text->out, "%02d:%02d:%02d.%09" PRIu64,
				tm.tm_hour, tm.tm_min, tm.tm_sec, ts_nsec_abs);
		goto end;
	}
seconds:
	fprintf(text->out, "%s%" PRId64 ".%09" PRIu64,
			is_negative ? "-" : "", ts_sec_abs, ts_nsec_abs);
end:
	return;
}

static
enum bt_component_status print_event_timestamp(struct text_component *text,
		struct bt_ctf_event *event, bool *start_line)
{
	bool print_names = text->options.print_header_field_names;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_stream *stream = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_trace *trace = NULL;
	struct bt_ctf_clock_class *clock_class = NULL;
	FILE *out = text->out;

	stream = bt_ctf_event_get_stream(event);
	if (!stream) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	stream_class = bt_ctf_stream_get_class(stream);
	if (!stream_class) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	trace = bt_ctf_stream_class_get_trace(stream_class);
	if (!trace) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	clock_class = bt_ctf_trace_get_clock_class(trace, 0);
	if (!clock_class) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	if (print_names) {
		print_name_equal(text, "timestamp");
	} else {
		fputs("[", out);
	}
	if (text->use_colors) {
		fputs(COLOR_TIMESTAMP, text->out);
	}
	if (text->options.print_timestamp_cycles) {
		print_timestamp_cycles(text, clock_class, event);
	} else {
		print_timestamp_wall(text, clock_class, event);
	}
	if (text->use_colors) {
		fputs(COLOR_RST, text->out);
	}

	if (!print_names)
		fputs("] ", out);

	if (text->options.print_delta_field) {
		if (print_names) {
			fputs(", ", text->out);
			print_name_equal(text, "delta");
		} else {
			fputs("(", text->out);
		}
		if (text->options.print_timestamp_cycles) {
			if (text->delta_cycles == -1ULL) {
				fputs("+??????????\?\?) ", text->out); /* Not a trigraph. */
			} else {
				fprintf(text->out, "+%012" PRIu64, text->delta_cycles);
			}
		} else {
			if (text->delta_real_timestamp != -1ULL) {
				uint64_t delta_sec, delta_nsec, delta;

				delta = text->delta_real_timestamp;
				delta_sec = delta / NSEC_PER_SEC;
				delta_nsec = delta % NSEC_PER_SEC;
				fprintf(text->out, "+%" PRIu64 ".%09" PRIu64,
					delta_sec, delta_nsec);
			} else {
				fputs("+?.?????????", text->out);
			}
		}
		if (!print_names) {
			fputs(") ", text->out);
		}
	}
	*start_line = !print_names;

end:
	bt_put(stream);
	bt_put(clock_class);
	bt_put(stream_class);
	bt_put(trace);
	return ret;
}

static
enum bt_component_status print_event_header(struct text_component *text,
		struct bt_ctf_event *event)
{
	bool print_names = text->options.print_header_field_names;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_event_class *event_class = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_trace *trace_class = NULL;

	event_class = bt_ctf_event_get_class(event);
	if (!event_class) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	stream_class = bt_ctf_event_class_get_stream_class(event_class);
	if (!stream_class) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	trace_class = bt_ctf_stream_class_get_trace(stream_class);
	if (!trace_class) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	if (!text->start_line) {
		fputs(", ", text->out);
	}
	text->start_line = false;
	ret = print_event_timestamp(text, event, &text->start_line);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (text->options.print_trace_field) {
		const char *name;

		name = bt_ctf_trace_get_name(trace_class);
		if (name) {
			if (!text->start_line) {
				fputs(", ", text->out);
			}
			text->start_line = false;
			if (print_names) {
				print_name_equal(text, "trace");
			}
			fprintf(text->out, "%s", name);
		}
	}
	if (text->options.print_trace_hostname_field) {
		struct bt_value *hostname_str;

		hostname_str = bt_ctf_trace_get_environment_field_value_by_name(trace_class,
				"hostname");
		if (hostname_str) {
			const char *str;

			if (!text->start_line) {
				fputs(", ", text->out);
			}
			text->start_line = false;
			if (print_names) {
				print_name_equal(text, "trace:hostname");
			}
			if (bt_value_string_get(hostname_str, &str)
					== BT_VALUE_STATUS_OK) {
				fprintf(text->out, "%s", str);
			}
			bt_put(hostname_str);
		}
	}
	if (text->options.print_trace_domain_field) {
		struct bt_value *domain_str;

		domain_str = bt_ctf_trace_get_environment_field_value_by_name(trace_class,
				"domain");
		if (domain_str) {
			const char *str;

			if (!text->start_line) {
				fputs(", ", text->out);
			}
			text->start_line = false;
			if (print_names) {
				print_name_equal(text, "trace:domain");
			}
			if (bt_value_string_get(domain_str, &str)
					== BT_VALUE_STATUS_OK) {
				fprintf(text->out, "%s", str);
			}
			bt_put(domain_str);
		}
	}
	if (text->options.print_trace_procname_field) {
		struct bt_value *procname_str;

		procname_str = bt_ctf_trace_get_environment_field_value_by_name(trace_class,
				"procname");
		if (procname_str) {
			const char *str;

			if (!text->start_line) {
				fputs(", ", text->out);
			}
			text->start_line = false;
			if (print_names) {
				print_name_equal(text, "trace:procname");
			}
			if (bt_value_string_get(procname_str, &str)
					== BT_VALUE_STATUS_OK) {
				fprintf(text->out, "%s", str);
			}
			bt_put(procname_str);
		}
	}
	if (text->options.print_trace_vpid_field) {
		struct bt_value *vpid_value;

		vpid_value = bt_ctf_trace_get_environment_field_value_by_name(trace_class,
				"vpid");
		if (vpid_value) {
			int64_t value;

			if (!text->start_line) {
				fputs(", ", text->out);
			}
			text->start_line = false;
			if (print_names) {
				print_name_equal(text, "trace:vpid");
			}
			if (bt_value_integer_get(vpid_value, &value)
					== BT_VALUE_STATUS_OK) {
				fprintf(text->out, "(%" PRId64 ")", value);
			}
			bt_put(vpid_value);
		}
	}
	if (text->options.print_loglevel_field) {
		struct bt_value *loglevel_str, *loglevel_value;

		loglevel_str = bt_ctf_event_class_get_attribute_value_by_name(event_class,
				"loglevel_string");
		loglevel_value = bt_ctf_event_class_get_attribute_value_by_name(event_class,
				"loglevel");
		if (loglevel_str || loglevel_value) {
			bool has_str = false;

			if (!text->start_line) {
				fputs(", ", text->out);
			}
			text->start_line = false;
			if (print_names) {
				print_name_equal(text, "loglevel");
			}
			if (loglevel_str) {
				const char *str;

				if (bt_value_string_get(loglevel_str, &str)
						== BT_VALUE_STATUS_OK) {
					fprintf(text->out, "%s", str);
					has_str = true;
				}
			}
			if (loglevel_value) {
				int64_t value;

				if (bt_value_integer_get(loglevel_value, &value)
						== BT_VALUE_STATUS_OK) {
					fprintf(text->out, "%s(%" PRId64 ")",
						has_str ? " " : "", value);
				}
			}
			bt_put(loglevel_str);
			bt_put(loglevel_value);
		}
	}
	if (text->options.print_emf_field) {
		struct bt_value *uri_str;

		uri_str = bt_ctf_event_class_get_attribute_value_by_name(event_class,
				"model.emf.uri");
		if (uri_str) {
			if (!text->start_line) {
				fputs(", ", text->out);
			}
			text->start_line = false;
			if (print_names) {
				print_name_equal(text, "model.emf.uri");
			}
			if (uri_str) {
				const char *str;

				if (bt_value_string_get(uri_str, &str)
						== BT_VALUE_STATUS_OK) {
					fprintf(text->out, "%s", str);
				}
			}
			bt_put(uri_str);
		}
	}
	if (!text->start_line) {
		fputs(", ", text->out);
	}
	text->start_line = false;
	if (print_names) {
		print_name_equal(text, "name");
	}
	if (text->use_colors) {
		fputs(COLOR_EVENT_NAME, text->out);
	}
	fputs(bt_ctf_event_class_get_name(event_class), text->out);
	if (text->use_colors) {
		fputs(COLOR_RST, text->out);
	}
end:
	bt_put(trace_class);
	bt_put(stream_class);
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
	bool rst_color = false;

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

	if (text->use_colors) {
		fputs(COLOR_NUMBER_VALUE, text->out);
		rst_color = true;
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
	if (rst_color) {
		fputs(COLOR_RST, text->out);
	}
	bt_put(field_type);
	return ret;
}

static
enum bt_component_status print_enum(struct text_component *text,
		struct bt_ctf_field *field)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field *container_field = NULL;
	struct bt_ctf_field_type *enumeration_field_type = NULL;
	struct bt_ctf_field_type *container_field_type = NULL;
	struct bt_ctf_field_type_enumeration_mapping_iterator *iter = NULL;
	int nr_mappings = 0;
	int is_signed;

	enumeration_field_type = bt_ctf_field_get_type(field);
	if (!enumeration_field_type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	container_field = bt_ctf_field_enumeration_get_container(field);
	if (!container_field) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	container_field_type = bt_ctf_field_get_type(container_field);
	if (!container_field_type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	is_signed = bt_ctf_field_type_integer_get_signed(container_field_type);
	if (is_signed < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	if (is_signed) {
		int64_t value;

		if (bt_ctf_field_signed_integer_get_value(container_field,
				&value)) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		iter = bt_ctf_field_type_enumeration_find_mappings_by_signed_value(
				enumeration_field_type, value);
	} else {
		uint64_t value;

		if (bt_ctf_field_unsigned_integer_get_value(container_field,
				&value)) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		iter = bt_ctf_field_type_enumeration_find_mappings_by_unsigned_value(
				enumeration_field_type, value);
	}
	if (!iter) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	fprintf(text->out, "( ");
	for (;;) {
		const char *mapping_name;

		if (bt_ctf_field_type_enumeration_mapping_iterator_get_signed(
				iter, &mapping_name, NULL, NULL) < 0) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		if (nr_mappings++)
			fprintf(text->out, ", ");
		if (text->use_colors) {
			fputs(COLOR_ENUM_MAPPING_NAME, text->out);
		}
		// TODO: escape string
		fprintf(text->out, "\"%s\"", mapping_name);
		if (text->use_colors) {
			fputs(COLOR_RST, text->out);
		}
		if (bt_ctf_field_type_enumeration_mapping_iterator_next(iter) < 0) {
			break;
		}
	}
	if (!nr_mappings) {
		if (text->use_colors) {
			fputs(COLOR_UNKNOWN, text->out);
		}
		fprintf(text->out, "<unknown>");
		if (text->use_colors) {
			fputs(COLOR_RST, text->out);
		}
	}
	fprintf(text->out, " : container = ");
	ret = print_integer(text, container_field);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	fprintf(text->out, " )");
end:
	bt_put(iter);
	bt_put(container_field_type);
	bt_put(container_field);
	bt_put(enumeration_field_type);
	return ret;
}

static
int filter_field_name(struct text_component *text, const char *field_name,
		GQuark *filter_fields, int filter_array_len)
{
	int i;
	GQuark field_quark = g_quark_try_string(field_name);

	if (!field_quark || text->options.verbose) {
		return 1;
	}

	for (i = 0; i < filter_array_len; i++) {
		if (field_quark == filter_fields[i]) {
			return 0;
		}
	}
	return 1;
}

static
enum bt_component_status print_struct_field(struct text_component *text,
		struct bt_ctf_field *_struct,
		struct bt_ctf_field_type *struct_type,
		int i, bool print_names, int *nr_printed_fields,
		GQuark *filter_fields, int filter_array_len)
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

	if (filter_fields && !filter_field_name(text, field_name,
				filter_fields, filter_array_len)) {
		ret = BT_COMPONENT_STATUS_OK;
		goto end;
	}

	if (*nr_printed_fields > 0) {
		fprintf(text->out, ", ");
	} else {
		fprintf(text->out, " ");
	}
	if (print_names) {
		print_field_name_equal(text, rem_(field_name));
	}
	ret = print_field(text, field, print_names, NULL, 0);
	*nr_printed_fields += 1;
end:
	bt_put(field_type);
	bt_put(field);
	return ret;
}

static
enum bt_component_status print_struct(struct text_component *text,
		struct bt_ctf_field *_struct, bool print_names,
		GQuark *filter_fields, int filter_array_len)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field_type *struct_type = NULL;
	int nr_fields, i, nr_printed_fields;

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
	nr_printed_fields = 0;
	for (i = 0; i < nr_fields; i++) {
		ret = print_struct_field(text, _struct, struct_type, i,
				print_names, &nr_printed_fields, filter_fields,
				filter_array_len);
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
	ret = print_field(text, field, print_names, NULL, 0);
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
		if (text->use_colors) {
			fputs(COLOR_STRING_VALUE, text->out);
		}
		// TODO: escape string
		fprintf(text->out, "\"%s\"", text->string->str);
		if (text->use_colors) {
			fputs(COLOR_RST, text->out);
		}
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
	ret = print_field(text, field, print_names, NULL, 0);
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
		if (text->use_colors) {
			fputs(COLOR_STRING_VALUE, text->out);
		}
		// TODO: escape string
		fprintf(text->out, "\"%s\"", text->string->str);
		if (text->use_colors) {
			fputs(COLOR_RST, text->out);
		}
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
		int iter_ret;
		struct bt_ctf_field *tag_field = NULL;
		const char *tag_choice;
		struct bt_ctf_field_type_enumeration_mapping_iterator *iter;

		tag_field = bt_ctf_field_variant_get_tag(variant);
		if (!tag_field) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}

		iter = bt_ctf_field_enumeration_get_mappings(tag_field);
		if (!iter) {
			bt_put(tag_field);
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}

		iter_ret =
			bt_ctf_field_type_enumeration_mapping_iterator_get_signed(
				iter, &tag_choice, NULL, NULL);
		if (iter_ret) {
			bt_put(iter);
			bt_put(tag_field);
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		print_field_name_equal(text, rem_(tag_choice));
		bt_put(tag_field);
		bt_put(iter);
	}
	ret = print_field(text, field, print_names, NULL, 0);
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
		struct bt_ctf_field *field, bool print_names,
		GQuark *filter_fields, int filter_array_len)
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
		if (text->use_colors) {
			fputs(COLOR_NUMBER_VALUE, text->out);
		}
		fprintf(text->out, "%g", v);
		if (text->use_colors) {
			fputs(COLOR_RST, text->out);
		}
		return BT_COMPONENT_STATUS_OK;
	}
	case CTF_TYPE_ENUM:
		return print_enum(text, field);
	case CTF_TYPE_STRING:
		if (text->use_colors) {
			fputs(COLOR_STRING_VALUE, text->out);
		}
		// TODO: escape the string value
		fprintf(text->out, "\"%s\"",
			bt_ctf_field_string_get_value(field));
		if (text->use_colors) {
			fputs(COLOR_RST, text->out);
		}
		return BT_COMPONENT_STATUS_OK;
	case CTF_TYPE_STRUCT:
		return print_struct(text, field, print_names, filter_fields,
				filter_array_len);
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
		goto end;
	}
	if (!text->start_line) {
		fputs(", ", text->out);
	}
	text->start_line = false;
	if (text->options.print_scope_field_names) {
		print_name_equal(text, "stream.packet.context");
	}
	ret = print_field(text, main_field,
			text->options.print_context_field_names,
			stream_packet_context_quarks,
			STREAM_PACKET_CONTEXT_QUARKS_LEN);
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
		goto end;
	}
	if (!text->start_line) {
		fputs(", ", text->out);
	}
	text->start_line = false;
	if (text->options.print_scope_field_names) {
		print_name_equal(text, "stream.event.header");
	}
	ret = print_field(text, main_field,
			text->options.print_header_field_names, NULL, 0);
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
		goto end;
	}
	if (!text->start_line) {
		fputs(", ", text->out);
	}
	text->start_line = false;
	if (text->options.print_scope_field_names) {
		print_name_equal(text, "stream.event.context");
	}
	ret = print_field(text, main_field,
			text->options.print_context_field_names, NULL, 0);
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
		goto end;
	}
	if (!text->start_line) {
		fputs(", ", text->out);
	}
	text->start_line = false;
	if (text->options.print_scope_field_names) {
		print_name_equal(text, "event.context");
	}
	ret = print_field(text, main_field,
			text->options.print_context_field_names, NULL, 0);
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
		goto end;
	}
	if (!text->start_line) {
		fputs(", ", text->out);
	}
	text->start_line = false;
	if (text->options.print_scope_field_names) {
		print_name_equal(text, "event.fields");
	}
	ret = print_field(text, main_field,
			text->options.print_payload_field_names, NULL, 0);
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
