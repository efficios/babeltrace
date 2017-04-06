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
#include <babeltrace/graph/notification-event.h>
#include <babeltrace/graph/clock-class-priority-map.h>
#include <babeltrace/bitfield-internal.h>
#include <babeltrace/common-internal.h>
#include <inttypes.h>
#include <ctype.h>
#include "pretty.h"

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
enum bt_component_status print_field(struct pretty_component *pretty,
		struct bt_ctf_field *field, bool print_names,
		GQuark *filters_fields, int filter_array_len);

static
void print_name_equal(struct pretty_component *pretty, const char *name)
{
	if (pretty->use_colors) {
		fprintf(pretty->out, "%s%s%s = ", COLOR_NAME, name, COLOR_RST);
	} else {
		fprintf(pretty->out, "%s = ", name);
	}
}

static
void print_field_name_equal(struct pretty_component *pretty, const char *name)
{
	if (pretty->use_colors) {
		fprintf(pretty->out, "%s%s%s = ", COLOR_FIELD_NAME, name,
			COLOR_RST);
	} else {
		fprintf(pretty->out, "%s = ", name);
	}
}

static
void print_timestamp_cycles(struct pretty_component *pretty,
		struct bt_ctf_clock_class *clock_class,
		struct bt_ctf_event *event)
{
	int ret;
	struct bt_ctf_clock_value *clock_value;
	uint64_t cycles;

	clock_value = bt_ctf_event_get_clock_value(event, clock_class);
	if (!clock_value) {
	        fputs("????????????????????", pretty->out);
		return;
	}

	ret = bt_ctf_clock_value_get_value(clock_value, &cycles);
	bt_put(clock_value);
	if (ret) {
	        fprintf(pretty->out, "Error");
		return;
	}
	fprintf(pretty->out, "%020" PRIu64, cycles);

	if (pretty->last_cycles_timestamp != -1ULL) {
		pretty->delta_cycles = cycles - pretty->last_cycles_timestamp;
	}
	pretty->last_cycles_timestamp = cycles;
}

static
void print_timestamp_wall(struct pretty_component *pretty,
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
		fputs("??:??:??.?????????", pretty->out);
		return;
	}

	ret = bt_ctf_clock_value_get_value_ns_from_epoch(clock_value, &ts_nsec);
	bt_put(clock_value);
	if (ret) {
	        fprintf(pretty->out, "Error");
		return;
	}

	if (pretty->last_real_timestamp != -1ULL) {
		pretty->delta_real_timestamp = ts_nsec - pretty->last_real_timestamp;
	}
	pretty->last_real_timestamp = ts_nsec;

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

	if (!pretty->options.clock_seconds) {
		struct tm tm;
		time_t time_s = (time_t) ts_sec_abs;

		if (is_negative) {
			fprintf(stderr, "[warning] Fallback to [sec.ns] to print negative time value. Use --clock-seconds.\n");
			goto seconds;
		}

		if (!pretty->options.clock_gmt) {
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
		if (pretty->options.clock_date) {
			char timestr[26];
			size_t res;

			/* Print date and time */
			res = strftime(timestr, sizeof(timestr),
					"%F ", &tm);
			if (!res) {
				fprintf(stderr, "[warning] Unable to print ascii time.\n");
				goto seconds;
			}
			fprintf(pretty->out, "%s", timestr);
		}
		/* Print time in HH:MM:SS.ns */
		fprintf(pretty->out, "%02d:%02d:%02d.%09" PRIu64,
				tm.tm_hour, tm.tm_min, tm.tm_sec, ts_nsec_abs);
		goto end;
	}
seconds:
	fprintf(pretty->out, "%s%" PRId64 ".%09" PRIu64,
			is_negative ? "-" : "", ts_sec_abs, ts_nsec_abs);
end:
	return;
}

static
enum bt_component_status print_event_timestamp(struct pretty_component *pretty,
		struct bt_ctf_event *event,
		struct bt_clock_class_priority_map *cc_prio_map,
		bool *start_line)
{
	bool print_names = pretty->options.print_header_field_names;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_stream *stream = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_trace *trace = NULL;
	struct bt_ctf_clock_class *clock_class = NULL;
	FILE *out = pretty->out;

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

	if (bt_clock_class_priority_map_get_clock_class_count(cc_prio_map) == 0) {
		/* No clock class: skip the timestamp without an error */
		goto end;
	}

	clock_class =
		bt_clock_class_priority_map_get_highest_priority_clock_class(
			cc_prio_map);
	if (!clock_class) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	if (print_names) {
		print_name_equal(pretty, "timestamp");
	} else {
		fputs("[", out);
	}
	if (pretty->use_colors) {
		fputs(COLOR_TIMESTAMP, pretty->out);
	}
	if (pretty->options.print_timestamp_cycles) {
		print_timestamp_cycles(pretty, clock_class, event);
	} else {
		print_timestamp_wall(pretty, clock_class, event);
	}
	if (pretty->use_colors) {
		fputs(COLOR_RST, pretty->out);
	}

	if (!print_names)
		fputs("] ", out);

	if (pretty->options.print_delta_field) {
		if (print_names) {
			fputs(", ", pretty->out);
			print_name_equal(pretty, "delta");
		} else {
			fputs("(", pretty->out);
		}
		if (pretty->options.print_timestamp_cycles) {
			if (pretty->delta_cycles == -1ULL) {
				fputs("+??????????\?\?) ", pretty->out); /* Not a trigraph. */
			} else {
				fprintf(pretty->out, "+%012" PRIu64, pretty->delta_cycles);
			}
		} else {
			if (pretty->delta_real_timestamp != -1ULL) {
				uint64_t delta_sec, delta_nsec, delta;

				delta = pretty->delta_real_timestamp;
				delta_sec = delta / NSEC_PER_SEC;
				delta_nsec = delta % NSEC_PER_SEC;
				fprintf(pretty->out, "+%" PRIu64 ".%09" PRIu64,
					delta_sec, delta_nsec);
			} else {
				fputs("+?.?????????", pretty->out);
			}
		}
		if (!print_names) {
			fputs(") ", pretty->out);
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
enum bt_component_status print_event_header(struct pretty_component *pretty,
		struct bt_ctf_event *event,
		struct bt_clock_class_priority_map *cc_prio_map)
{
	bool print_names = pretty->options.print_header_field_names;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_event_class *event_class = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_trace *trace_class = NULL;
	int dom_print = 0;

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
	ret = print_event_timestamp(pretty, event, cc_prio_map,
		&pretty->start_line);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (pretty->options.print_trace_field) {
		const char *name;

		name = bt_ctf_trace_get_name(trace_class);
		if (name) {
			if (!pretty->start_line) {
				fputs(", ", pretty->out);
			}
			if (print_names) {
				print_name_equal(pretty, "trace");
			}
			fprintf(pretty->out, "%s", name);
			if (!print_names) {
				fprintf(pretty->out, " ");
			}
		}
	}
	if (pretty->options.print_trace_hostname_field) {
		struct bt_value *hostname_str;

		hostname_str = bt_ctf_trace_get_environment_field_value_by_name(trace_class,
				"hostname");
		if (hostname_str) {
			const char *str;

			if (!pretty->start_line) {
				fputs(", ", pretty->out);
			}
			if (print_names) {
				print_name_equal(pretty, "trace:hostname");
			}
			if (bt_value_string_get(hostname_str, &str)
					== BT_VALUE_STATUS_OK) {
				fprintf(pretty->out, "%s", str);
			}
			bt_put(hostname_str);
			dom_print = 1;
		}
	}
	if (pretty->options.print_trace_domain_field) {
		struct bt_value *domain_str;

		domain_str = bt_ctf_trace_get_environment_field_value_by_name(trace_class,
				"domain");
		if (domain_str) {
			const char *str;

			if (!pretty->start_line) {
				fputs(", ", pretty->out);
			}
			if (print_names) {
				print_name_equal(pretty, "trace:domain");
			} else if (dom_print) {
				fputs(":", pretty->out);
			}
			if (bt_value_string_get(domain_str, &str)
					== BT_VALUE_STATUS_OK) {
				fprintf(pretty->out, "%s", str);
			}
			bt_put(domain_str);
			dom_print = 1;
		}
	}
	if (pretty->options.print_trace_procname_field) {
		struct bt_value *procname_str;

		procname_str = bt_ctf_trace_get_environment_field_value_by_name(trace_class,
				"procname");
		if (procname_str) {
			const char *str;

			if (!pretty->start_line) {
				fputs(", ", pretty->out);
			}
			if (print_names) {
				print_name_equal(pretty, "trace:procname");
			} else if (dom_print) {
				fputs(":", pretty->out);
			}
			if (bt_value_string_get(procname_str, &str)
					== BT_VALUE_STATUS_OK) {
				fprintf(pretty->out, "%s", str);
			}
			bt_put(procname_str);
			dom_print = 1;
		}
	}
	if (pretty->options.print_trace_vpid_field) {
		struct bt_value *vpid_value;

		vpid_value = bt_ctf_trace_get_environment_field_value_by_name(trace_class,
				"vpid");
		if (vpid_value) {
			int64_t value;

			if (!pretty->start_line) {
				fputs(", ", pretty->out);
			}
			if (print_names) {
				print_name_equal(pretty, "trace:vpid");
			} else if (dom_print) {
				fputs(":", pretty->out);
			}
			if (bt_value_integer_get(vpid_value, &value)
					== BT_VALUE_STATUS_OK) {
				fprintf(pretty->out, "(%" PRId64 ")", value);
			}
			bt_put(vpid_value);
			dom_print = 1;
		}
	}
	if (pretty->options.print_loglevel_field) {
		struct bt_value *loglevel_str, *loglevel_value;

		loglevel_str = bt_ctf_event_class_get_attribute_value_by_name(event_class,
				"loglevel_string");
		loglevel_value = bt_ctf_event_class_get_attribute_value_by_name(event_class,
				"loglevel");
		if (loglevel_str || loglevel_value) {
			bool has_str = false;

			if (!pretty->start_line) {
				fputs(", ", pretty->out);
			}
			if (print_names) {
				print_name_equal(pretty, "loglevel");
			} else if (dom_print) {
				fputs(":", pretty->out);
			}
			if (loglevel_str) {
				const char *str;

				if (bt_value_string_get(loglevel_str, &str)
						== BT_VALUE_STATUS_OK) {
					fprintf(pretty->out, "%s", str);
					has_str = true;
				}
			}
			if (loglevel_value) {
				int64_t value;

				if (bt_value_integer_get(loglevel_value, &value)
						== BT_VALUE_STATUS_OK) {
					fprintf(pretty->out, "%s(%" PRId64 ")",
						has_str ? " " : "", value);
				}
			}
			bt_put(loglevel_str);
			bt_put(loglevel_value);
			dom_print = 1;
		}
	}
	if (pretty->options.print_emf_field) {
		struct bt_value *uri_str;

		uri_str = bt_ctf_event_class_get_attribute_value_by_name(event_class,
				"model.emf.uri");
		if (uri_str) {
			if (!pretty->start_line) {
				fputs(", ", pretty->out);
			}
			if (print_names) {
				print_name_equal(pretty, "model.emf.uri");
			} else if (dom_print) {
				fputs(":", pretty->out);
			}
			if (uri_str) {
				const char *str;

				if (bt_value_string_get(uri_str, &str)
						== BT_VALUE_STATUS_OK) {
					fprintf(pretty->out, "%s", str);
				}
			}
			bt_put(uri_str);
			dom_print = 1;
		}
	}
	if (dom_print && !print_names) {
		fputs(" ", pretty->out);
	}
	if (!pretty->start_line) {
		fputs(", ", pretty->out);
	}
	pretty->start_line = true;
	if (print_names) {
		print_name_equal(pretty, "name");
	}
	if (pretty->use_colors) {
		fputs(COLOR_EVENT_NAME, pretty->out);
	}
	fputs(bt_ctf_event_class_get_name(event_class), pretty->out);
	if (pretty->use_colors) {
		fputs(COLOR_RST, pretty->out);
	}
	if (!print_names) {
		fputs(": ", pretty->out);
	} else {
		fputs(", ", pretty->out);
	}
end:
	bt_put(trace_class);
	bt_put(stream_class);
	bt_put(event_class);
	return ret;
}

static
enum bt_component_status print_integer(struct pretty_component *pretty,
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
		g_string_append_c(pretty->string, (int) v.u);
		goto end;
	case BT_CTF_STRING_ENCODING_NONE:
	case BT_CTF_STRING_ENCODING_UNKNOWN:
		break;
	default:
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	if (pretty->use_colors) {
		fputs(COLOR_NUMBER_VALUE, pretty->out);
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
		fprintf(pretty->out, "0b");
		v.u = _bt_piecewise_lshift(v.u, 64 - len);
		for (bitnr = 0; bitnr < len; bitnr++) {
			fprintf(pretty->out, "%u", (v.u & (1ULL << 63)) ? 1 : 0);
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

		fprintf(pretty->out, "0%" PRIo64, v.u);
		break;
	}
	case BT_CTF_INTEGER_BASE_DECIMAL:
		if (!signedness) {
			fprintf(pretty->out, "%" PRIu64, v.u);
		} else {
			fprintf(pretty->out, "%" PRId64, v.s);
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

		fprintf(pretty->out, "0x%" PRIX64, v.u);
		break;
	}
	default:
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
end:
	if (rst_color) {
		fputs(COLOR_RST, pretty->out);
	}
	bt_put(field_type);
	return ret;
}

static
void print_escape_string(struct pretty_component *pretty, const char *str)
{
	int i;

	fputc('"', pretty->out);
	for (i = 0; i < strlen(str); i++) {
		/* Escape sequences not recognized by iscntrl(). */
		switch (str[i]) {
		case '\\':
			fputs("\\\\", pretty->out);
			continue;
		case '\'':
			fputs("\\\'", pretty->out);
			continue;
		case '\"':
			fputs("\\\"", pretty->out);
			continue;
		case '\?':
			fputs("\\\?", pretty->out);
			continue;
		}

		/* Standard characters. */
		if (!iscntrl(str[i])) {
			fputc(str[i], pretty->out);
			continue;
		}

		switch (str[i]) {
		case '\0':
			fputs("\\0", pretty->out);
			break;
		case '\a':
			fputs("\\a", pretty->out);
			break;
		case '\b':
			fputs("\\b", pretty->out);
			break;
		case '\e':
			fputs("\\e", pretty->out);
			break;
		case '\f':
			fputs("\\f", pretty->out);
			break;
		case '\n':
			fputs("\\n", pretty->out);
			break;
		case '\r':
			fputs("\\r", pretty->out);
			break;
		case '\t':
			fputs("\\t", pretty->out);
			break;
		case '\v':
			fputs("\\v", pretty->out);
			break;
		default:
			/* Unhandled control-sequence, print as hex. */
			fprintf(pretty->out, "\\x%02x", str[i]);
			break;
		}
	}
	fputc('"', pretty->out);
}

static
enum bt_component_status print_enum(struct pretty_component *pretty,
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
	fprintf(pretty->out, "( ");
	for (;;) {
		const char *mapping_name;

		if (bt_ctf_field_type_enumeration_mapping_iterator_get_signed(
				iter, &mapping_name, NULL, NULL) < 0) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		if (nr_mappings++)
			fprintf(pretty->out, ", ");
		if (pretty->use_colors) {
			fputs(COLOR_ENUM_MAPPING_NAME, pretty->out);
		}
		print_escape_string(pretty, mapping_name);
		if (pretty->use_colors) {
			fputs(COLOR_RST, pretty->out);
		}
		if (bt_ctf_field_type_enumeration_mapping_iterator_next(iter) < 0) {
			break;
		}
	}
	if (!nr_mappings) {
		if (pretty->use_colors) {
			fputs(COLOR_UNKNOWN, pretty->out);
		}
		fprintf(pretty->out, "<unknown>");
		if (pretty->use_colors) {
			fputs(COLOR_RST, pretty->out);
		}
	}
	fprintf(pretty->out, " : container = ");
	ret = print_integer(pretty, container_field);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	fprintf(pretty->out, " )");
end:
	bt_put(iter);
	bt_put(container_field_type);
	bt_put(container_field);
	bt_put(enumeration_field_type);
	return ret;
}

static
int filter_field_name(struct pretty_component *pretty, const char *field_name,
		GQuark *filter_fields, int filter_array_len)
{
	int i;
	GQuark field_quark = g_quark_try_string(field_name);

	if (!field_quark || pretty->options.verbose) {
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
enum bt_component_status print_struct_field(struct pretty_component *pretty,
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

	if (filter_fields && !filter_field_name(pretty, field_name,
				filter_fields, filter_array_len)) {
		ret = BT_COMPONENT_STATUS_OK;
		goto end;
	}

	if (*nr_printed_fields > 0) {
		fprintf(pretty->out, ", ");
	} else {
		fprintf(pretty->out, " ");
	}
	if (print_names) {
		print_field_name_equal(pretty, rem_(field_name));
	}
	ret = print_field(pretty, field, print_names, NULL, 0);
	*nr_printed_fields += 1;
end:
	bt_put(field_type);
	bt_put(field);
	return ret;
}

static
enum bt_component_status print_struct(struct pretty_component *pretty,
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
	fprintf(pretty->out, "{");
	pretty->depth++;
	nr_printed_fields = 0;
	for (i = 0; i < nr_fields; i++) {
		ret = print_struct_field(pretty, _struct, struct_type, i,
				print_names, &nr_printed_fields, filter_fields,
				filter_array_len);
		if (ret != BT_COMPONENT_STATUS_OK) {
			goto end;
		}
	}
	pretty->depth--;
	fprintf(pretty->out, " }");
end:
	bt_put(struct_type);
	return ret;
}

static
enum bt_component_status print_array_field(struct pretty_component *pretty,
		struct bt_ctf_field *array, uint64_t i,
		bool is_string, bool print_names)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field *field = NULL;

	if (!is_string) {
		if (i != 0) {
			fprintf(pretty->out, ", ");
		} else {
			fprintf(pretty->out, " ");
		}
		if (print_names) {
			fprintf(pretty->out, "[%" PRIu64 "] = ", i);
		}
	}
	field = bt_ctf_field_array_get_field(array, i);
	if (!field) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	ret = print_field(pretty, field, print_names, NULL, 0);
end:
	bt_put(field);
	return ret;
}

static
enum bt_component_status print_array(struct pretty_component *pretty,
		struct bt_ctf_field *array, bool print_names)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field_type *array_type = NULL, *field_type = NULL;
	enum bt_ctf_field_type_id type_id;
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
	if (type_id == BT_CTF_FIELD_TYPE_ID_INTEGER) {
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
		g_string_assign(pretty->string, "");
	} else {
		fprintf(pretty->out, "[");
	}

	pretty->depth++;
	for (i = 0; i < len; i++) {
		ret = print_array_field(pretty, array, i, is_string, print_names);
		if (ret != BT_COMPONENT_STATUS_OK) {
			goto end;
		}
	}
	pretty->depth--;

	if (is_string) {
		if (pretty->use_colors) {
			fputs(COLOR_STRING_VALUE, pretty->out);
		}
		print_escape_string(pretty, pretty->string->str);
		if (pretty->use_colors) {
			fputs(COLOR_RST, pretty->out);
		}
	} else {
		fprintf(pretty->out, " ]");
	}
end:
	bt_put(field_type);
	bt_put(array_type);
	return ret;
}

static
enum bt_component_status print_sequence_field(struct pretty_component *pretty,
		struct bt_ctf_field *seq, uint64_t i,
		bool is_string, bool print_names)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field *field = NULL;

	if (!is_string) {
		if (i != 0) {
			fprintf(pretty->out, ", ");
		} else {
			fprintf(pretty->out, " ");
		}
		if (print_names) {
			fprintf(pretty->out, "[%" PRIu64 "] = ", i);
		}
	}
	field = bt_ctf_field_sequence_get_field(seq, i);
	if (!field) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	ret = print_field(pretty, field, print_names, NULL, 0);
end:
	bt_put(field);
	return ret;
}

static
enum bt_component_status print_sequence(struct pretty_component *pretty,
		struct bt_ctf_field *seq, bool print_names)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field_type *seq_type = NULL, *field_type = NULL;
	struct bt_ctf_field *length_field = NULL;
	enum bt_ctf_field_type_id type_id;
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
	if (type_id == BT_CTF_FIELD_TYPE_ID_INTEGER) {
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
		g_string_assign(pretty->string, "");
	} else {
		fprintf(pretty->out, "[");
	}

	pretty->depth++;
	for (i = 0; i < len; i++) {
		ret = print_sequence_field(pretty, seq, i,
			is_string, print_names);
		if (ret != BT_COMPONENT_STATUS_OK) {
			goto end;
		}
	}
	pretty->depth--;

	if (is_string) {
		if (pretty->use_colors) {
			fputs(COLOR_STRING_VALUE, pretty->out);
		}
		print_escape_string(pretty, pretty->string->str);
		if (pretty->use_colors) {
			fputs(COLOR_RST, pretty->out);
		}
	} else {
		fprintf(pretty->out, " ]");
	}
end:
	bt_put(length_field);
	bt_put(field_type);
	bt_put(seq_type);
	return ret;
}

static
enum bt_component_status print_variant(struct pretty_component *pretty,
		struct bt_ctf_field *variant, bool print_names)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field *field = NULL;

	field = bt_ctf_field_variant_get_current_field(variant);
	if (!field) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	fprintf(pretty->out, "{ ");
	pretty->depth++;
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
		print_field_name_equal(pretty, rem_(tag_choice));
		bt_put(tag_field);
		bt_put(iter);
	}
	ret = print_field(pretty, field, print_names, NULL, 0);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	pretty->depth--;
	fprintf(pretty->out, " }");
end:
	bt_put(field);
	return ret;
}

static
enum bt_component_status print_field(struct pretty_component *pretty,
		struct bt_ctf_field *field, bool print_names,
		GQuark *filter_fields, int filter_array_len)
{
	enum bt_ctf_field_type_id type_id;

	type_id = bt_ctf_field_get_type_id(field);
	switch (type_id) {
	case CTF_TYPE_INTEGER:
		return print_integer(pretty, field);
	case CTF_TYPE_FLOAT:
	{
		double v;

		if (bt_ctf_field_floating_point_get_value(field, &v)) {
			return BT_COMPONENT_STATUS_ERROR;
		}
		if (pretty->use_colors) {
			fputs(COLOR_NUMBER_VALUE, pretty->out);
		}
		fprintf(pretty->out, "%g", v);
		if (pretty->use_colors) {
			fputs(COLOR_RST, pretty->out);
		}
		return BT_COMPONENT_STATUS_OK;
	}
	case CTF_TYPE_ENUM:
		return print_enum(pretty, field);
	case CTF_TYPE_STRING:
		if (pretty->use_colors) {
			fputs(COLOR_STRING_VALUE, pretty->out);
		}
		print_escape_string(pretty, bt_ctf_field_string_get_value(field));
		if (pretty->use_colors) {
			fputs(COLOR_RST, pretty->out);
		}
		return BT_COMPONENT_STATUS_OK;
	case CTF_TYPE_STRUCT:
		return print_struct(pretty, field, print_names, filter_fields,
				filter_array_len);
	case CTF_TYPE_UNTAGGED_VARIANT:
	case CTF_TYPE_VARIANT:
		return print_variant(pretty, field, print_names);
	case CTF_TYPE_ARRAY:
		return print_array(pretty, field, print_names);
	case CTF_TYPE_SEQUENCE:
		return print_sequence(pretty, field, print_names);
	default:
		fprintf(pretty->err, "[error] Unknown type id: %d\n", (int) type_id);
		return BT_COMPONENT_STATUS_ERROR;
	}
}

static
enum bt_component_status print_stream_packet_context(struct pretty_component *pretty,
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
	if (!pretty->start_line) {
		fputs(", ", pretty->out);
	}
	pretty->start_line = false;
	if (pretty->options.print_scope_field_names) {
		print_name_equal(pretty, "stream.packet.context");
	}
	ret = print_field(pretty, main_field,
			pretty->options.print_context_field_names,
			stream_packet_context_quarks,
			STREAM_PACKET_CONTEXT_QUARKS_LEN);
end:
	bt_put(main_field);
	bt_put(packet);
	return ret;
}

static
enum bt_component_status print_event_header_raw(struct pretty_component *pretty,
		struct bt_ctf_event *event)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field *main_field = NULL;

	main_field = bt_ctf_event_get_header(event);
	if (!main_field) {
		goto end;
	}
	if (!pretty->start_line) {
		fputs(", ", pretty->out);
	}
	pretty->start_line = false;
	if (pretty->options.print_scope_field_names) {
		print_name_equal(pretty, "stream.event.header");
	}
	ret = print_field(pretty, main_field,
			pretty->options.print_header_field_names, NULL, 0);
end:
	bt_put(main_field);
	return ret;
}

static
enum bt_component_status print_stream_event_context(struct pretty_component *pretty,
		struct bt_ctf_event *event)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field *main_field = NULL;

	main_field = bt_ctf_event_get_stream_event_context(event);
	if (!main_field) {
		goto end;
	}
	if (!pretty->start_line) {
		fputs(", ", pretty->out);
	}
	pretty->start_line = false;
	if (pretty->options.print_scope_field_names) {
		print_name_equal(pretty, "stream.event.context");
	}
	ret = print_field(pretty, main_field,
			pretty->options.print_context_field_names, NULL, 0);
end:
	bt_put(main_field);
	return ret;
}

static
enum bt_component_status print_event_context(struct pretty_component *pretty,
		struct bt_ctf_event *event)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field *main_field = NULL;

	main_field = bt_ctf_event_get_event_context(event);
	if (!main_field) {
		goto end;
	}
	if (!pretty->start_line) {
		fputs(", ", pretty->out);
	}
	pretty->start_line = false;
	if (pretty->options.print_scope_field_names) {
		print_name_equal(pretty, "event.context");
	}
	ret = print_field(pretty, main_field,
			pretty->options.print_context_field_names, NULL, 0);
end:
	bt_put(main_field);
	return ret;
}

static
enum bt_component_status print_event_payload(struct pretty_component *pretty,
		struct bt_ctf_event *event)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_field *main_field = NULL;

	main_field = bt_ctf_event_get_payload_field(event);
	if (!main_field) {
		goto end;
	}
	if (!pretty->start_line) {
		fputs(", ", pretty->out);
	}
	pretty->start_line = false;
	if (pretty->options.print_scope_field_names) {
		print_name_equal(pretty, "event.fields");
	}
	ret = print_field(pretty, main_field,
			pretty->options.print_payload_field_names, NULL, 0);
end:
	bt_put(main_field);
	return ret;
}

BT_HIDDEN
enum bt_component_status pretty_print_event(struct pretty_component *pretty,
		struct bt_notification *event_notif)
{
	enum bt_component_status ret;
	struct bt_ctf_event *event =
		bt_notification_event_get_event(event_notif);
	struct bt_clock_class_priority_map *cc_prio_map =
		bt_notification_event_get_clock_class_priority_map(event_notif);

	assert(event);
	assert(cc_prio_map);
	pretty->start_line = true;
	ret = print_event_header(pretty, event, cc_prio_map);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	ret = print_stream_packet_context(pretty, event);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	if (pretty->options.verbose) {
		ret = print_event_header_raw(pretty, event);
		if (ret != BT_COMPONENT_STATUS_OK) {
			goto end;
		}
	}

	ret = print_stream_event_context(pretty, event);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	ret = print_event_context(pretty, event);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	ret = print_event_payload(pretty, event);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	fputc('\n', pretty->out);
end:
	bt_put(event);
	bt_put(cc_prio_map);
	return ret;
}
