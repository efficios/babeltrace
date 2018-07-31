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

#include <babeltrace/babeltrace.h>
#include <babeltrace/bitfield-internal.h>
#include <babeltrace/common-internal.h>
#include <babeltrace/compat/time-internal.h>
#include <babeltrace/assert-internal.h>
#include <inttypes.h>
#include <ctype.h>
#include "pretty.h"

#define NSEC_PER_SEC 1000000000LL

#define COLOR_NAME		BT_COMMON_COLOR_BOLD
#define COLOR_FIELD_NAME	BT_COMMON_COLOR_FG_CYAN
#define COLOR_RST		BT_COMMON_COLOR_RESET
#define COLOR_STRING_VALUE	BT_COMMON_COLOR_BOLD
#define COLOR_NUMBER_VALUE	BT_COMMON_COLOR_BOLD
#define COLOR_ENUM_MAPPING_NAME	BT_COMMON_COLOR_BOLD
#define COLOR_UNKNOWN		BT_COMMON_COLOR_BOLD BT_COMMON_COLOR_FG_RED
#define COLOR_EVENT_NAME	BT_COMMON_COLOR_BOLD BT_COMMON_COLOR_FG_MAGENTA
#define COLOR_TIMESTAMP		BT_COMMON_COLOR_BOLD BT_COMMON_COLOR_FG_YELLOW

struct timestamp {
	int64_t real_timestamp;	/* Relative to UNIX epoch. */
	uint64_t clock_value;	/* In cycles. */
};

static
enum bt_component_status print_field(struct pretty_component *pretty,
		struct bt_field *field, bool print_names,
		GQuark *filters_fields, int filter_array_len);

static
void print_name_equal(struct pretty_component *pretty, const char *name)
{
	if (pretty->use_colors) {
		g_string_append_printf(pretty->string, "%s%s%s = ", COLOR_NAME,
			name, COLOR_RST);
	} else {
		g_string_append_printf(pretty->string, "%s = ", name);
	}
}

static
void print_field_name_equal(struct pretty_component *pretty, const char *name)
{
	if (pretty->use_colors) {
		g_string_append_printf(pretty->string, "%s%s%s = ",
			COLOR_FIELD_NAME, name, COLOR_RST);
	} else {
		g_string_append_printf(pretty->string, "%s = ", name);
	}
}

static
void print_timestamp_cycles(struct pretty_component *pretty,
		struct bt_event *event)
{
	int ret;
	struct bt_clock_value *clock_value;
	uint64_t cycles;

	clock_value = bt_event_borrow_default_clock_value(event);
	if (!clock_value) {
		g_string_append(pretty->string, "????????????????????");
		return;
	}

	ret = bt_clock_value_get_value(clock_value, &cycles);
	if (ret) {
		// TODO: log, this is unexpected
		g_string_append(pretty->string, "Error");
		return;
	}

	g_string_append_printf(pretty->string, "%020" PRIu64, cycles);

	if (pretty->last_cycles_timestamp != -1ULL) {
		pretty->delta_cycles = cycles - pretty->last_cycles_timestamp;
	}
	pretty->last_cycles_timestamp = cycles;
}

static
void print_timestamp_wall(struct pretty_component *pretty,
		struct bt_clock_value *clock_value)
{
	int ret;
	int64_t ts_nsec = 0;	/* add configurable offset */
	int64_t ts_sec = 0;	/* add configurable offset */
	uint64_t ts_sec_abs, ts_nsec_abs;
	bool is_negative;

	if (!clock_value) {
		g_string_append(pretty->string, "??:??:??.?????????");
		return;
	}

	ret = bt_clock_value_get_value_ns_from_epoch(clock_value, &ts_nsec);
	if (ret) {
		// TODO: log, this is unexpected
		g_string_append(pretty->string, "Error");
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

		if (is_negative && !pretty->negative_timestamp_warning_done) {
			// TODO: log instead
			fprintf(stderr, "[warning] Fallback to [sec.ns] to print negative time value. Use --clock-seconds.\n");
			pretty->negative_timestamp_warning_done = true;
			goto seconds;
		}

		if (!pretty->options.clock_gmt) {
			struct tm *res;

			res = bt_localtime_r(&time_s, &tm);
			if (!res) {
				// TODO: log instead
				fprintf(stderr, "[warning] Unable to get localtime.\n");
				goto seconds;
			}
		} else {
			struct tm *res;

			res = bt_gmtime_r(&time_s, &tm);
			if (!res) {
				// TODO: log instead
				fprintf(stderr, "[warning] Unable to get gmtime.\n");
				goto seconds;
			}
		}
		if (pretty->options.clock_date) {
			char timestr[26];
			size_t res;

			/* Print date and time */
			res = strftime(timestr, sizeof(timestr),
					"%Y-%m-%d ", &tm);
			if (!res) {
				// TODO: log instead
				fprintf(stderr, "[warning] Unable to print ascii time.\n");
				goto seconds;
			}

			g_string_append(pretty->string, timestr);
		}

		/* Print time in HH:MM:SS.ns */
		g_string_append_printf(pretty->string,
			"%02d:%02d:%02d.%09" PRIu64, tm.tm_hour, tm.tm_min,
			tm.tm_sec, ts_nsec_abs);
		goto end;
	}
seconds:
	g_string_append_printf(pretty->string, "%s%" PRId64 ".%09" PRIu64,
		is_negative ? "-" : "", ts_sec_abs, ts_nsec_abs);
end:
	return;
}

static
enum bt_component_status print_event_timestamp(struct pretty_component *pretty,
		struct bt_event *event, bool *start_line)
{
	bool print_names = pretty->options.print_header_field_names;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_stream *stream = NULL;
	struct bt_stream_class *stream_class = NULL;
	struct bt_trace *trace = NULL;
	struct bt_clock_value *clock_value = NULL;

	stream = bt_event_borrow_stream(event);
	if (!stream) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	stream_class = bt_stream_borrow_class(stream);
	if (!stream_class) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	trace = bt_stream_class_borrow_trace(stream_class);
	if (!trace) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	clock_value = bt_event_borrow_default_clock_value(event);
	if (!clock_value) {
		/* No default clock value: skip the timestamp without an error */
		goto end;
	}

	if (print_names) {
		print_name_equal(pretty, "timestamp");
	} else {
		g_string_append(pretty->string, "[");
	}
	if (pretty->use_colors) {
		g_string_append(pretty->string, COLOR_TIMESTAMP);
	}
	if (pretty->options.print_timestamp_cycles) {
		print_timestamp_cycles(pretty, event);
	} else {
		struct bt_clock_value *clock_value =
			bt_event_borrow_default_clock_value(event);

		print_timestamp_wall(pretty, clock_value);
	}
	if (pretty->use_colors) {
		g_string_append(pretty->string, COLOR_RST);
	}

	if (!print_names)
		g_string_append(pretty->string, "] ");

	if (pretty->options.print_delta_field) {
		if (print_names) {
			g_string_append(pretty->string, ", ");
			print_name_equal(pretty, "delta");
		} else {
			g_string_append(pretty->string, "(");
		}
		if (pretty->options.print_timestamp_cycles) {
			if (pretty->delta_cycles == -1ULL) {
				g_string_append(pretty->string,
					"+??????????\?\?) "); /* Not a trigraph. */
			} else {
				g_string_append_printf(pretty->string,
					"+%012" PRIu64, pretty->delta_cycles);
			}
		} else {
			if (pretty->delta_real_timestamp != -1ULL) {
				uint64_t delta_sec, delta_nsec, delta;

				delta = pretty->delta_real_timestamp;
				delta_sec = delta / NSEC_PER_SEC;
				delta_nsec = delta % NSEC_PER_SEC;
				g_string_append_printf(pretty->string,
					"+%" PRIu64 ".%09" PRIu64,
					delta_sec, delta_nsec);
			} else {
				g_string_append(pretty->string, "+?.?????????");
			}
		}
		if (!print_names) {
			g_string_append(pretty->string, ") ");
		}
	}
	*start_line = !print_names;

end:
	return ret;
}

static
enum bt_component_status print_event_header(struct pretty_component *pretty,
		struct bt_event *event)
{
	bool print_names = pretty->options.print_header_field_names;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_event_class *event_class = NULL;
	struct bt_stream_class *stream_class = NULL;
	struct bt_trace *trace_class = NULL;
	int dom_print = 0;

	event_class = bt_event_borrow_class(event);
	if (!event_class) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	stream_class = bt_event_class_borrow_stream_class(event_class);
	if (!stream_class) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	trace_class = bt_stream_class_borrow_trace(stream_class);
	if (!trace_class) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	ret = print_event_timestamp(pretty, event, &pretty->start_line);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (pretty->options.print_trace_field) {
		const char *name;

		name = bt_trace_get_name(trace_class);
		if (name) {
			if (!pretty->start_line) {
				g_string_append(pretty->string, ", ");
			}
			if (print_names) {
				print_name_equal(pretty, "trace");
			}

			g_string_append(pretty->string, name);

			if (!print_names) {
				g_string_append(pretty->string, " ");
			}
		}
	}
	if (pretty->options.print_trace_hostname_field) {
		struct bt_value *hostname_str;

		hostname_str = bt_trace_borrow_environment_field_value_by_name(
			trace_class, "hostname");
		if (hostname_str) {
			const char *str;

			if (!pretty->start_line) {
				g_string_append(pretty->string, ", ");
			}
			if (print_names) {
				print_name_equal(pretty, "trace:hostname");
			}
			if (bt_value_string_get(hostname_str, &str)
					== BT_VALUE_STATUS_OK) {
				g_string_append(pretty->string, str);
			}
			dom_print = 1;
		}
	}
	if (pretty->options.print_trace_domain_field) {
		struct bt_value *domain_str;

		domain_str = bt_trace_borrow_environment_field_value_by_name(
			trace_class, "domain");
		if (domain_str) {
			const char *str;

			if (!pretty->start_line) {
				g_string_append(pretty->string, ", ");
			}
			if (print_names) {
				print_name_equal(pretty, "trace:domain");
			} else if (dom_print) {
				g_string_append(pretty->string, ":");
			}
			if (bt_value_string_get(domain_str, &str)
					== BT_VALUE_STATUS_OK) {
				g_string_append(pretty->string, str);
			}
			dom_print = 1;
		}
	}
	if (pretty->options.print_trace_procname_field) {
		struct bt_value *procname_str;

		procname_str = bt_trace_borrow_environment_field_value_by_name(
			trace_class, "procname");
		if (procname_str) {
			const char *str;

			if (!pretty->start_line) {
				g_string_append(pretty->string, ", ");
			}
			if (print_names) {
				print_name_equal(pretty, "trace:procname");
			} else if (dom_print) {
				g_string_append(pretty->string, ":");
			}
			if (bt_value_string_get(procname_str, &str)
					== BT_VALUE_STATUS_OK) {
				g_string_append(pretty->string, str);
			}

			dom_print = 1;
		}
	}
	if (pretty->options.print_trace_vpid_field) {
		struct bt_value *vpid_value;

		vpid_value = bt_trace_borrow_environment_field_value_by_name(
			trace_class, "vpid");
		if (vpid_value) {
			int64_t value;

			if (!pretty->start_line) {
				g_string_append(pretty->string, ", ");
			}
			if (print_names) {
				print_name_equal(pretty, "trace:vpid");
			} else if (dom_print) {
				g_string_append(pretty->string, ":");
			}
			if (bt_value_integer_get(vpid_value, &value)
					== BT_VALUE_STATUS_OK) {
				g_string_append_printf(pretty->string, "(%" PRId64 ")", value);
			}

			dom_print = 1;
		}
	}
	if (pretty->options.print_loglevel_field) {
		static const char *log_level_names[] = {
			[ BT_EVENT_CLASS_LOG_LEVEL_EMERGENCY ] = "TRACE_EMERG",
			[ BT_EVENT_CLASS_LOG_LEVEL_ALERT ] = "TRACE_ALERT",
			[ BT_EVENT_CLASS_LOG_LEVEL_CRITICAL ] = "TRACE_CRIT",
			[ BT_EVENT_CLASS_LOG_LEVEL_ERROR ] = "TRACE_ERR",
			[ BT_EVENT_CLASS_LOG_LEVEL_WARNING ] = "TRACE_WARNING",
			[ BT_EVENT_CLASS_LOG_LEVEL_NOTICE ] = "TRACE_NOTICE",
			[ BT_EVENT_CLASS_LOG_LEVEL_INFO ] = "TRACE_INFO",
			[ BT_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM ] = "TRACE_DEBUG_SYSTEM",
			[ BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM ] = "TRACE_DEBUG_PROGRAM",
			[ BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS ] = "TRACE_DEBUG_PROCESS",
			[ BT_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE ] = "TRACE_DEBUG_MODULE",
			[ BT_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT ] = "TRACE_DEBUG_UNIT",
			[ BT_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION ] = "TRACE_DEBUG_FUNCTION",
			[ BT_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE ] = "TRACE_DEBUG_LINE",
			[ BT_EVENT_CLASS_LOG_LEVEL_DEBUG ] = "TRACE_DEBUG",
		};
		enum bt_event_class_log_level log_level;
		const char *log_level_str = NULL;

		log_level = bt_event_class_get_log_level(event_class);
		BT_ASSERT(log_level != BT_EVENT_CLASS_LOG_LEVEL_UNKNOWN);
		if (log_level != BT_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED) {
			log_level_str = log_level_names[log_level];
		}

		if (log_level_str) {
			if (!pretty->start_line) {
				g_string_append(pretty->string, ", ");
			}
			if (print_names) {
				print_name_equal(pretty, "loglevel");
			} else if (dom_print) {
				g_string_append(pretty->string, ":");
			}

			g_string_append(pretty->string, log_level_str);
			g_string_append_printf(
				pretty->string, " (%d)", (int) log_level);
			dom_print = 1;
		}
	}
	if (pretty->options.print_emf_field) {
		const char *uri_str;

		uri_str = bt_event_class_get_emf_uri(event_class);
		if (uri_str) {
			if (!pretty->start_line) {
				g_string_append(pretty->string, ", ");
			}
			if (print_names) {
				print_name_equal(pretty, "model.emf.uri");
			} else if (dom_print) {
				g_string_append(pretty->string, ":");
			}

			g_string_append(pretty->string, uri_str);
			dom_print = 1;
		}
	}
	if (dom_print && !print_names) {
		g_string_append(pretty->string, " ");
	}
	if (!pretty->start_line) {
		g_string_append(pretty->string, ", ");
	}
	pretty->start_line = true;
	if (print_names) {
		print_name_equal(pretty, "name");
	}
	if (pretty->use_colors) {
		g_string_append(pretty->string, COLOR_EVENT_NAME);
	}
	g_string_append(pretty->string, bt_event_class_get_name(event_class));
	if (pretty->use_colors) {
		g_string_append(pretty->string, COLOR_RST);
	}
	if (!print_names) {
		g_string_append(pretty->string, ": ");
	} else {
		g_string_append(pretty->string, ", ");
	}

end:
	return ret;
}

static
enum bt_component_status print_integer(struct pretty_component *pretty,
		struct bt_field *field)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_field_type *field_type = NULL;
	enum bt_integer_base base;
	enum bt_string_encoding encoding;
	int signedness;
	struct bt_field_type *int_ft;
	union {
		uint64_t u;
		int64_t s;
	} v;
	bool rst_color = false;
	enum bt_field_type_id ft_id;

	field_type = bt_field_borrow_type(field);
	if (!field_type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	ft_id = bt_field_get_type_id(field);

	switch (ft_id) {
	case BT_FIELD_TYPE_ID_INTEGER:
		int_ft = field_type;
		break;
	case BT_FIELD_TYPE_ID_ENUM:
		int_ft = bt_field_type_enumeration_borrow_container_field_type(
			field_type);
		break;
	default:
		abort();
	}

	signedness = bt_field_type_integer_is_signed(int_ft);
	if (signedness < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	if (!signedness) {
		ret = bt_field_integer_unsigned_get_value(field, &v.u);
	} else {
		ret = bt_field_integer_signed_get_value(field, &v.s);
	}

	if (ret < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	encoding = bt_field_type_integer_get_encoding(int_ft);
	switch (encoding) {
	case BT_STRING_ENCODING_UTF8:
	case BT_STRING_ENCODING_ASCII:
		g_string_append_c(pretty->tmp_string, (int) v.u);
		goto end;
	case BT_STRING_ENCODING_NONE:
	case BT_STRING_ENCODING_UNKNOWN:
		break;
	default:
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	if (pretty->use_colors) {
		g_string_append(pretty->string, COLOR_NUMBER_VALUE);
		rst_color = true;
	}

	base = bt_field_type_integer_get_base(int_ft);
	switch (base) {
	case BT_INTEGER_BASE_BINARY:
	{
		int bitnr, len;

		len = bt_field_type_integer_get_size(int_ft);
		if (len < 0) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		g_string_append(pretty->string, "0b");
		v.u = _bt_piecewise_lshift(v.u, 64 - len);
		for (bitnr = 0; bitnr < len; bitnr++) {
			g_string_append_printf(pretty->string, "%u", (v.u & (1ULL << 63)) ? 1 : 0);
			v.u = _bt_piecewise_lshift(v.u, 1);
		}
		break;
	}
	case BT_INTEGER_BASE_OCTAL:
	{
		if (signedness) {
			int len;

			len = bt_field_type_integer_get_size(int_ft);
			if (len < 0) {
				ret = BT_COMPONENT_STATUS_ERROR;
				goto end;
			}
			if (len < 64) {
			        size_t rounded_len;

				BT_ASSERT(len != 0);
				/* Round length to the nearest 3-bit */
				rounded_len = (((len - 1) / 3) + 1) * 3;
				v.u &= ((uint64_t) 1 << rounded_len) - 1;
			}
		}

		g_string_append_printf(pretty->string, "0%" PRIo64, v.u);
		break;
	}
	case BT_INTEGER_BASE_DECIMAL:
	case BT_INTEGER_BASE_UNSPECIFIED:
		if (!signedness) {
			g_string_append_printf(pretty->string, "%" PRIu64, v.u);
		} else {
			g_string_append_printf(pretty->string, "%" PRId64, v.s);
		}
		break;
	case BT_INTEGER_BASE_HEXADECIMAL:
	{
		int len;

		len = bt_field_type_integer_get_size(int_ft);
		if (len < 0) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		if (len < 64) {
			/* Round length to the nearest nibble */
			uint8_t rounded_len = ((len + 3) & ~0x3);

			v.u &= ((uint64_t) 1 << rounded_len) - 1;
		}

		g_string_append_printf(pretty->string, "0x%" PRIX64, v.u);
		break;
	}
	default:
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
end:
	if (rst_color) {
		g_string_append(pretty->string, COLOR_RST);
	}
	return ret;
}

static
void print_escape_string(struct pretty_component *pretty, const char *str)
{
	int i;

	g_string_append_c(pretty->string, '"');

	for (i = 0; i < strlen(str); i++) {
		/* Escape sequences not recognized by iscntrl(). */
		switch (str[i]) {
		case '\\':
			g_string_append(pretty->string, "\\\\");
			continue;
		case '\'':
			g_string_append(pretty->string, "\\\'");
			continue;
		case '\"':
			g_string_append(pretty->string, "\\\"");
			continue;
		case '\?':
			g_string_append(pretty->string, "\\\?");
			continue;
		}

		/* Standard characters. */
		if (!iscntrl(str[i])) {
			g_string_append_c(pretty->string, str[i]);
			continue;
		}

		switch (str[i]) {
		case '\0':
			g_string_append(pretty->string, "\\0");
			break;
		case '\a':
			g_string_append(pretty->string, "\\a");
			break;
		case '\b':
			g_string_append(pretty->string, "\\b");
			break;
		case '\e':
			g_string_append(pretty->string, "\\e");
			break;
		case '\f':
			g_string_append(pretty->string, "\\f");
			break;
		case '\n':
			g_string_append(pretty->string, "\\n");
			break;
		case '\r':
			g_string_append(pretty->string, "\\r");
			break;
		case '\t':
			g_string_append(pretty->string, "\\t");
			break;
		case '\v':
			g_string_append(pretty->string, "\\v");
			break;
		default:
			/* Unhandled control-sequence, print as hex. */
			g_string_append_printf(pretty->string, "\\x%02x", str[i]);
			break;
		}
	}

	g_string_append_c(pretty->string, '"');
}

static
enum bt_component_status print_enum(struct pretty_component *pretty,
		struct bt_field *field)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_field_type *enumeration_field_type = NULL;
	struct bt_field_type *container_field_type = NULL;
	struct bt_field_type_enumeration_mapping_iterator *iter = NULL;
	int nr_mappings = 0;

	enumeration_field_type = bt_field_borrow_type(field);
	if (!enumeration_field_type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	container_field_type =
		bt_field_type_enumeration_borrow_container_field_type(
			enumeration_field_type);
	if (!container_field_type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	iter = bt_field_enumeration_get_mappings(field);
	if (!iter) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	g_string_append(pretty->string, "( ");
	ret = bt_field_type_enumeration_mapping_iterator_next(iter);
	if (ret) {
		if (pretty->use_colors) {
			g_string_append(pretty->string, COLOR_UNKNOWN);
		}
		g_string_append(pretty->string, "<unknown>");
		if (pretty->use_colors) {
			g_string_append(pretty->string, COLOR_RST);
		}
		goto skip_loop;
	}
	for (;;) {
		const char *mapping_name;

		if (bt_field_type_enumeration_mapping_iterator_signed_get(
				iter, &mapping_name, NULL, NULL) < 0) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		if (nr_mappings++)
			g_string_append(pretty->string, ", ");
		if (pretty->use_colors) {
			g_string_append(pretty->string, COLOR_ENUM_MAPPING_NAME);
		}
		print_escape_string(pretty, mapping_name);
		if (pretty->use_colors) {
			g_string_append(pretty->string, COLOR_RST);
		}
		if (bt_field_type_enumeration_mapping_iterator_next(iter) < 0) {
			break;
		}
	}
skip_loop:
	g_string_append(pretty->string, " : container = ");
	ret = print_integer(pretty, field);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	g_string_append(pretty->string, " )");
end:
	bt_put(iter);
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
		struct bt_field *_struct,
		struct bt_field_type *struct_type,
		int i, bool print_names, int *nr_printed_fields,
		GQuark *filter_fields, int filter_array_len)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	const char *field_name;
	struct bt_field *field = NULL;
	struct bt_field_type *field_type = NULL;;

	field = bt_field_structure_borrow_field_by_index(_struct, i);
	if (!field) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	if (bt_field_type_structure_borrow_field_by_index(struct_type,
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
		g_string_append(pretty->string, ", ");
	} else {
		g_string_append(pretty->string, " ");
	}
	if (print_names) {
		print_field_name_equal(pretty, field_name);
	}
	ret = print_field(pretty, field, print_names, NULL, 0);
	*nr_printed_fields += 1;

end:
	return ret;
}

static
enum bt_component_status print_struct(struct pretty_component *pretty,
		struct bt_field *_struct, bool print_names,
		GQuark *filter_fields, int filter_array_len)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_field_type *struct_type = NULL;
	int nr_fields, i, nr_printed_fields;

	struct_type = bt_field_borrow_type(_struct);
	if (!struct_type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	nr_fields = bt_field_type_structure_get_field_count(struct_type);
	if (nr_fields < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	g_string_append(pretty->string, "{");
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
	g_string_append(pretty->string, " }");

end:
	return ret;
}

static
enum bt_component_status print_array_field(struct pretty_component *pretty,
		struct bt_field *array, uint64_t i,
		bool is_string, bool print_names)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_field *field = NULL;

	if (!is_string) {
		if (i != 0) {
			g_string_append(pretty->string, ", ");
		} else {
			g_string_append(pretty->string, " ");
		}
		if (print_names) {
			g_string_append_printf(pretty->string, "[%" PRIu64 "] = ", i);
		}
	}
	field = bt_field_array_borrow_field(array, i);
	if (!field) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	ret = print_field(pretty, field, print_names, NULL, 0);

end:
	return ret;
}

static
enum bt_component_status print_array(struct pretty_component *pretty,
		struct bt_field *array, bool print_names)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_field_type *array_type = NULL, *field_type = NULL;
	enum bt_field_type_id type_id;
	int64_t len;
	uint64_t i;
	bool is_string = false;

	array_type = bt_field_borrow_type(array);
	if (!array_type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	field_type = bt_field_type_array_borrow_element_field_type(array_type);
	if (!field_type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	len = bt_field_type_array_get_length(array_type);
	if (len < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	type_id = bt_field_type_get_type_id(field_type);
	if (type_id == BT_FIELD_TYPE_ID_INTEGER) {
		enum bt_string_encoding encoding;

		encoding = bt_field_type_integer_get_encoding(field_type);
		if (encoding == BT_STRING_ENCODING_UTF8
				|| encoding == BT_STRING_ENCODING_ASCII) {
			int integer_len, integer_alignment;

			integer_len = bt_field_type_integer_get_size(field_type);
			if (integer_len < 0) {
				return BT_COMPONENT_STATUS_ERROR;
			}
			integer_alignment = bt_field_type_get_alignment(field_type);
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
		g_string_assign(pretty->tmp_string, "");
	} else {
		g_string_append(pretty->string, "[");
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
			g_string_append(pretty->string, COLOR_STRING_VALUE);
		}
		print_escape_string(pretty, pretty->tmp_string->str);
		if (pretty->use_colors) {
			g_string_append(pretty->string, COLOR_RST);
		}
	} else {
		g_string_append(pretty->string, " ]");
	}

end:
	return ret;
}

static
enum bt_component_status print_sequence_field(struct pretty_component *pretty,
		struct bt_field *seq, uint64_t i,
		bool is_string, bool print_names)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_field *field = NULL;

	if (!is_string) {
		if (i != 0) {
			g_string_append(pretty->string, ", ");
		} else {
			g_string_append(pretty->string, " ");
		}
		if (print_names) {
			g_string_append_printf(pretty->string, "[%" PRIu64 "] = ", i);
		}
	}
	field = bt_field_sequence_borrow_field(seq, i);
	if (!field) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	ret = print_field(pretty, field, print_names, NULL, 0);

end:
	return ret;
}

static
enum bt_component_status print_sequence(struct pretty_component *pretty,
		struct bt_field *seq, bool print_names)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_field_type *seq_type = NULL, *field_type = NULL;
	enum bt_field_type_id type_id;
	int64_t len;
	uint64_t i;
	bool is_string = false;

	seq_type = bt_field_borrow_type(seq);
	if (!seq_type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	len = bt_field_sequence_get_length(seq);
	if (len < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	field_type = bt_field_type_sequence_borrow_element_field_type(seq_type);
	if (!field_type) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	type_id = bt_field_type_get_type_id(field_type);
	if (type_id == BT_FIELD_TYPE_ID_INTEGER) {
		enum bt_string_encoding encoding;

		encoding = bt_field_type_integer_get_encoding(field_type);
		if (encoding == BT_STRING_ENCODING_UTF8
				|| encoding == BT_STRING_ENCODING_ASCII) {
			int integer_len, integer_alignment;

			integer_len = bt_field_type_integer_get_size(field_type);
			if (integer_len < 0) {
				ret = BT_COMPONENT_STATUS_ERROR;
				goto end;
			}
			integer_alignment = bt_field_type_get_alignment(field_type);
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
		g_string_assign(pretty->tmp_string, "");
	} else {
		g_string_append(pretty->string, "[");
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
			g_string_append(pretty->string, COLOR_STRING_VALUE);
		}
		print_escape_string(pretty, pretty->tmp_string->str);
		if (pretty->use_colors) {
			g_string_append(pretty->string, COLOR_RST);
		}
	} else {
		g_string_append(pretty->string, " ]");
	}

end:
	return ret;
}

static
enum bt_component_status print_variant(struct pretty_component *pretty,
		struct bt_field *variant, bool print_names)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_field *field = NULL;

	field = bt_field_variant_borrow_current_field(variant);
	if (!field) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	g_string_append(pretty->string, "{ ");
	pretty->depth++;
	if (print_names) {
		int iret;
		struct bt_field_type *var_ft;
		struct bt_field_type *tag_ft;
		struct bt_field_type *container_ft;
		const char *tag_choice;
		bt_bool is_signed;
		struct bt_field_type_enumeration_mapping_iterator *iter;

		var_ft = bt_field_borrow_type(variant);
		tag_ft = bt_field_type_variant_borrow_tag_field_type(
			var_ft);
		container_ft =
			bt_field_type_enumeration_borrow_container_field_type(
				tag_ft);
		is_signed = bt_field_type_integer_is_signed(container_ft);

		if (is_signed) {
			int64_t tag;

			iret = bt_field_variant_get_tag_signed(variant, &tag);
			if (iret) {
				ret = BT_COMPONENT_STATUS_ERROR;
				goto end;
			}

			iter = bt_field_type_enumeration_signed_find_mappings_by_value(
				tag_ft, tag);
		} else {
			uint64_t tag;

			iret = bt_field_variant_get_tag_unsigned(variant, &tag);
			if (iret) {
				ret = BT_COMPONENT_STATUS_ERROR;
				goto end;
			}

			iter = bt_field_type_enumeration_unsigned_find_mappings_by_value(
				tag_ft, tag);
		}

		if (!iter) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}

		iret = bt_field_type_enumeration_mapping_iterator_next(
			iter);
		if (!iter || ret) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}

		iret =
			bt_field_type_enumeration_mapping_iterator_signed_get(
				iter, &tag_choice, NULL, NULL);
		if (iret) {
			bt_put(iter);
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		print_field_name_equal(pretty, tag_choice);
		bt_put(iter);
	}
	ret = print_field(pretty, field, print_names, NULL, 0);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	pretty->depth--;
	g_string_append(pretty->string, " }");

end:
	return ret;
}

static
enum bt_component_status print_field(struct pretty_component *pretty,
		struct bt_field *field, bool print_names,
		GQuark *filter_fields, int filter_array_len)
{
	enum bt_field_type_id type_id;

	type_id = bt_field_get_type_id(field);
	switch (type_id) {
	case BT_CTF_FIELD_TYPE_ID_INTEGER:
		return print_integer(pretty, field);
	case BT_CTF_FIELD_TYPE_ID_FLOAT:
	{
		double v;

		if (bt_field_floating_point_get_value(field, &v)) {
			return BT_COMPONENT_STATUS_ERROR;
		}
		if (pretty->use_colors) {
			g_string_append(pretty->string, COLOR_NUMBER_VALUE);
		}
		g_string_append_printf(pretty->string, "%g", v);
		if (pretty->use_colors) {
			g_string_append(pretty->string, COLOR_RST);
		}
		return BT_COMPONENT_STATUS_OK;
	}
	case BT_CTF_FIELD_TYPE_ID_ENUM:
		return print_enum(pretty, field);
	case BT_CTF_FIELD_TYPE_ID_STRING:
	{
		const char *str;

		str = bt_field_string_get_value(field);
		if (!str) {
			return BT_COMPONENT_STATUS_ERROR;
		}

		if (pretty->use_colors) {
			g_string_append(pretty->string, COLOR_STRING_VALUE);
		}
		print_escape_string(pretty, str);
		if (pretty->use_colors) {
			g_string_append(pretty->string, COLOR_RST);
		}
		return BT_COMPONENT_STATUS_OK;
	}
	case BT_CTF_FIELD_TYPE_ID_STRUCT:
		return print_struct(pretty, field, print_names, filter_fields,
				filter_array_len);
	case BT_CTF_FIELD_TYPE_ID_VARIANT:
		return print_variant(pretty, field, print_names);
	case BT_CTF_FIELD_TYPE_ID_ARRAY:
		return print_array(pretty, field, print_names);
	case BT_CTF_FIELD_TYPE_ID_SEQUENCE:
		return print_sequence(pretty, field, print_names);
	default:
		// TODO: log instead
		fprintf(pretty->err, "[error] Unknown type id: %d\n", (int) type_id);
		return BT_COMPONENT_STATUS_ERROR;
	}
}

static
enum bt_component_status print_stream_packet_context(struct pretty_component *pretty,
		struct bt_event *event)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_packet *packet = NULL;
	struct bt_field *main_field = NULL;

	packet = bt_event_borrow_packet(event);
	if (!packet) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	main_field = bt_packet_borrow_context(packet);
	if (!main_field) {
		goto end;
	}
	if (!pretty->start_line) {
		g_string_append(pretty->string, ", ");
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
	return ret;
}

static
enum bt_component_status print_event_header_raw(struct pretty_component *pretty,
		struct bt_event *event)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_field *main_field = NULL;

	main_field = bt_event_borrow_header(event);
	if (!main_field) {
		goto end;
	}
	if (!pretty->start_line) {
		g_string_append(pretty->string, ", ");
	}
	pretty->start_line = false;
	if (pretty->options.print_scope_field_names) {
		print_name_equal(pretty, "stream.event.header");
	}
	ret = print_field(pretty, main_field,
			pretty->options.print_header_field_names, NULL, 0);

end:
	return ret;
}

static
enum bt_component_status print_stream_event_context(struct pretty_component *pretty,
		struct bt_event *event)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_field *main_field = NULL;

	main_field = bt_event_borrow_stream_event_context(event);
	if (!main_field) {
		goto end;
	}
	if (!pretty->start_line) {
		g_string_append(pretty->string, ", ");
	}
	pretty->start_line = false;
	if (pretty->options.print_scope_field_names) {
		print_name_equal(pretty, "stream.event.context");
	}
	ret = print_field(pretty, main_field,
			pretty->options.print_context_field_names, NULL, 0);

end:
	return ret;
}

static
enum bt_component_status print_event_context(struct pretty_component *pretty,
		struct bt_event *event)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_field *main_field = NULL;

	main_field = bt_event_borrow_context(event);
	if (!main_field) {
		goto end;
	}
	if (!pretty->start_line) {
		g_string_append(pretty->string, ", ");
	}
	pretty->start_line = false;
	if (pretty->options.print_scope_field_names) {
		print_name_equal(pretty, "event.context");
	}
	ret = print_field(pretty, main_field,
			pretty->options.print_context_field_names, NULL, 0);

end:
	return ret;
}

static
enum bt_component_status print_event_payload(struct pretty_component *pretty,
		struct bt_event *event)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_field *main_field = NULL;

	main_field = bt_event_borrow_payload(event);
	if (!main_field) {
		goto end;
	}
	if (!pretty->start_line) {
		g_string_append(pretty->string, ", ");
	}
	pretty->start_line = false;
	if (pretty->options.print_scope_field_names) {
		print_name_equal(pretty, "event.fields");
	}
	ret = print_field(pretty, main_field,
			pretty->options.print_payload_field_names, NULL, 0);

end:
	return ret;
}

static
int flush_buf(struct pretty_component *pretty)
{
	int ret = 0;

	if (pretty->string->len == 0) {
		goto end;
	}

	if (fwrite(pretty->string->str, pretty->string->len, 1, pretty->out) != 1) {
		ret = -1;
	}

end:
	return ret;
}

BT_HIDDEN
enum bt_component_status pretty_print_event(struct pretty_component *pretty,
		struct bt_notification *event_notif)
{
	enum bt_component_status ret;
	struct bt_event *event =
		bt_notification_event_borrow_event(event_notif);

	BT_ASSERT(event);
	pretty->start_line = true;
	g_string_assign(pretty->string, "");
	ret = print_event_header(pretty, event);
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

	g_string_append_c(pretty->string, '\n');
	if (flush_buf(pretty)) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

end:
	return ret;
}
