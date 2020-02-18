/*
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#define BT_LOG_OUTPUT_LEVEL (trimmer_comp->log_level)
#define BT_LOG_TAG "PLUGIN/FLT.UTILS.TRIMMER"
#include "logging/comp-logging.h"

#include "compat/utc.h"
#include "compat/time.h"
#include <babeltrace2/babeltrace.h>
#include "common/common.h"
#include "common/assert.h"
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <glib.h>
#include "compat/glib.h"
#include "plugins/common/param-validation/param-validation.h"

#include "trimmer.h"

#define NS_PER_S	INT64_C(1000000000)

static const char * const in_port_name = "in";

struct trimmer_time {
	unsigned int hour, minute, second, ns;
};

struct trimmer_bound {
	/*
	 * Nanoseconds from origin, valid if `is_set` is set and
	 * `is_infinite` is false.
	 */
	int64_t ns_from_origin;

	/* True if this bound's full time (`ns_from_origin`) is set */
	bool is_set;

	/*
	 * True if this bound represents the infinity (negative or
	 * positive depending on which bound it is). If this is true,
	 * then we don't care about `ns_from_origin` above.
	 */
	bool is_infinite;

	/*
	 * This bound's time without the date; this time is used to set
	 * `ns_from_origin` once we know the date.
	 */
	struct trimmer_time time;
};

struct trimmer_comp {
	struct trimmer_bound begin, end;
	bool is_gmt;
	bt_logging_level log_level;
	bt_self_component *self_comp;
	bt_self_component_filter *self_comp_filter;
};

enum trimmer_iterator_state {
	/*
	 * Find the first message's date and set the bounds's times
	 * accordingly.
	 */
	TRIMMER_ITERATOR_STATE_SET_BOUNDS_NS_FROM_ORIGIN,

	/*
	 * Initially seek to the trimming range's beginning time.
	 */
	TRIMMER_ITERATOR_STATE_SEEK_INITIALLY,

	/*
	 * Fill the output message queue for as long as received input
	 * messages are within the trimming time range.
	 */
	TRIMMER_ITERATOR_STATE_TRIM,

	/* Flush the remaining messages in the output message queue */
	TRIMMER_ITERATOR_STATE_ENDING,

	/* Trimming operation and message iterator is ended */
	TRIMMER_ITERATOR_STATE_ENDED,
};

struct trimmer_iterator {
	/* Weak */
	struct trimmer_comp *trimmer_comp;

	/* Weak */
	bt_self_message_iterator *self_msg_iter;

	enum trimmer_iterator_state state;

	/* Owned by this */
	bt_message_iterator *upstream_iter;
	struct trimmer_bound begin, end;

	/*
	 * Queue of `const bt_message *` (owned by the queue).
	 *
	 * This is where the trimming operation pushes the messages to
	 * output by this message iterator.
	 */
	GQueue *output_messages;

	/*
	 * Hash table of `bt_stream *` (weak) to
	 * `struct trimmer_iterator_stream_state *` (owned by the HT).
	 */
	GHashTable *stream_states;
};

struct trimmer_iterator_stream_state {
	/* Weak */
	const bt_stream *stream;

	/* Have we seen a message with clock_snapshot going through this stream? */
	bool seen_clock_snapshot;

	/* Owned by this (`NULL` initially and between packets) */
	const bt_packet *cur_packet;
};

static
void destroy_trimmer_comp(struct trimmer_comp *trimmer_comp)
{
	BT_ASSERT(trimmer_comp);
	g_free(trimmer_comp);
}

static
struct trimmer_comp *create_trimmer_comp(void)
{
	return g_new0(struct trimmer_comp, 1);
}

BT_HIDDEN
void trimmer_finalize(bt_self_component_filter *self_comp)
{
	struct trimmer_comp *trimmer_comp =
		bt_self_component_get_data(
			bt_self_component_filter_as_self_component(self_comp));

	if (trimmer_comp) {
		destroy_trimmer_comp(trimmer_comp);
	}
}

/*
 * Compile regex in `pattern`, and try to match `string`.  If there's a match,
 * return true and set `*match_info` to the list of matches.  The list of
 * matches must be freed by the caller. If there's no match, return false and
 * set `*match_info` to NULL;
 */
static
bool compile_and_match(const char *pattern, const char *string, GMatchInfo **match_info) {
	bool matches = false;
	GError *regex_error = NULL;
	GRegex *regex;

	regex = g_regex_new(pattern, 0, 0, &regex_error);
	if (!regex) {
		goto end;
	}

	matches = g_regex_match(regex, string, 0, match_info);
	if (!matches) {
		/*
		 * g_regex_match allocates `*match_info` even if it returns
		 * FALSE.  If there's no match, we have no use for it, so free
		 * it immediatly and don't return it to the caller.
		 */
		g_match_info_free(*match_info);
		*match_info = NULL;
	}

	g_regex_unref(regex);

end:

	if (regex_error) {
		g_error_free(regex_error);
	}

	return matches;
}

/*
 * Convert the captured text in match number `match_num` in `match_info`
 * to an unsigned integer.
 */
static
guint64 match_to_uint(const GMatchInfo *match_info, gint match_num) {
	gchar *text, *endptr;
	guint64 result;

	text = g_match_info_fetch(match_info, match_num);
	BT_ASSERT(text);

	/*
	 * Because the input is carefully sanitized with regexes by the caller,
	 * we assume that g_ascii_strtoull cannot fail.
	 */
	errno = 0;
	result = g_ascii_strtoull(text, &endptr, 10);
	BT_ASSERT(endptr > text);
	BT_ASSERT(errno == 0);

	g_free(text);

	return result;
}

/*
 * When parsing the nanoseconds part, .512 means .512000000, not .000000512.
 * This function is like match_to_uint, but multiplies the parsed number to get
 * the expected result.
 */
static
guint64 match_to_uint_ns(const GMatchInfo *match_info, gint match_num) {
	guint64 nanoseconds;
	gboolean ret;
	gint start_pos, end_pos, power;
	static int pow10[] = {
		1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000,
	};

	nanoseconds = match_to_uint(match_info, match_num);

	/* Multiply by 10 as many times as there are omitted digits. */
	ret = g_match_info_fetch_pos(match_info, match_num, &start_pos, &end_pos);
	BT_ASSERT(ret);

	power = 9 - (end_pos - start_pos);
	BT_ASSERT(power >= 0 && power <= 8);

	nanoseconds *= pow10[power];

	return nanoseconds;
}

/*
 * Sets the time (in ns from origin) of a trimmer bound from date and
 * time components.
 *
 * Returns a negative value if anything goes wrong.
 */
static
int set_bound_ns_from_origin(struct trimmer_bound *bound,
		unsigned int year, unsigned int month, unsigned int day,
		unsigned int hour, unsigned int minute, unsigned int second,
		unsigned int ns, bool is_gmt)
{
	int ret = 0;
	time_t result;
	struct tm tm = {
		.tm_sec = second,
		.tm_min = minute,
		.tm_hour = hour,
		.tm_mday = day,
		.tm_mon = month - 1,
		.tm_year = year - 1900,
		.tm_isdst = -1,
	};

	if (is_gmt) {
		result = bt_timegm(&tm);
	} else {
		result = mktime(&tm);
	}

	if (result < 0) {
		ret = -1;
		goto end;
	}

	BT_ASSERT(bound);
	bound->ns_from_origin = (int64_t) result;
	bound->ns_from_origin *= NS_PER_S;
	bound->ns_from_origin += ns;
	bound->is_set = true;

end:
	return ret;
}

/*
 * Parses a timestamp, figuring out its format.
 *
 * Returns a negative value if anything goes wrong.
 *
 * Expected formats:
 *
 *     YYYY-MM-DD hh:mm[:ss[.ns]]
 *     [hh:mm:]ss[.ns]
 *     [-]s[.ns]
 *
 * TODO: Check overflows.
 */
static
int set_bound_from_str(struct trimmer_comp *trimmer_comp,
		const char *str, struct trimmer_bound *bound, bool is_gmt)
{
/* Matches YYYY-MM-DD */
#define DATE_RE "([0-9]{4})-([0-9]{2})-([0-9]{2})"

/* Matches HH:MM[:SS[.NS]] */
#define TIME_RE "([0-9]{2}):([0-9]{2})(?::([0-9]{2})(?:\\.([0-9]{1,9}))?)?"

/* Matches [-]SS[.NS] */
#define S_NS_RE "^(-?)([0-9]+)(?:\\.([0-9]{1,9}))?$"

	GMatchInfo *match_info;
	int ret = 0;

	/* Try `YYYY-MM-DD hh:mm[:ss[.ns]]` format */
	if (compile_and_match("^" DATE_RE " " TIME_RE "$", str, &match_info)) {
		unsigned int year = 0, month = 0, day = 0, hours = 0, minutes = 0, seconds = 0, nanoseconds = 0;
		gint match_count = g_match_info_get_match_count(match_info);

		BT_ASSERT(match_count >= 6 && match_count <= 8);

		year = match_to_uint(match_info, 1);
		month = match_to_uint(match_info, 2);
		day = match_to_uint(match_info, 3);
		hours = match_to_uint(match_info, 4);
		minutes = match_to_uint(match_info, 5);

		if (match_count >= 7) {
			seconds = match_to_uint(match_info, 6);
		}

		if (match_count >= 8) {
			nanoseconds = match_to_uint_ns(match_info, 7);
		}

		set_bound_ns_from_origin(bound, year, month, day, hours, minutes, seconds, nanoseconds, is_gmt);

		goto end;
	}

	if (compile_and_match("^" DATE_RE "$", str, &match_info)) {
		unsigned int year = 0, month = 0, day = 0;

		BT_ASSERT(g_match_info_get_match_count(match_info) == 4);

		year = match_to_uint(match_info, 1);
		month = match_to_uint(match_info, 2);
		day = match_to_uint(match_info, 3);

		set_bound_ns_from_origin(bound, year, month, day, 0, 0, 0, 0, is_gmt);

		goto end;
	}

	/* Try `hh:mm[:ss[.ns]]` format */
	if (compile_and_match("^" TIME_RE "$", str, &match_info)) {
		gint match_count = g_match_info_get_match_count(match_info);
		BT_ASSERT(match_count >= 3 && match_count <= 5);
		bound->time.hour = match_to_uint(match_info, 1);
		bound->time.minute = match_to_uint(match_info, 2);

		if (match_count >= 4) {
			bound->time.second = match_to_uint(match_info, 3);
		}

		if (match_count >= 5) {
			bound->time.ns = match_to_uint_ns(match_info, 4);
		}

		goto end;
	}

	/* Try `[-]s[.ns]` format */
	if (compile_and_match("^" S_NS_RE "$", str, &match_info)) {
		gboolean is_neg, fetch_pos_ret;
		gint start_pos, end_pos, match_count;
		guint64 seconds, nanoseconds = 0;

		match_count = g_match_info_get_match_count(match_info);
		BT_ASSERT(match_count >= 3 && match_count <= 4);

		/* Check for presence of negation sign. */
		fetch_pos_ret = g_match_info_fetch_pos(match_info, 1, &start_pos, &end_pos);
		BT_ASSERT(fetch_pos_ret);
		is_neg = (end_pos - start_pos) > 0;

		seconds = match_to_uint(match_info, 2);

		if (match_count >= 4) {
			nanoseconds = match_to_uint_ns(match_info, 3);
		}

		bound->ns_from_origin = seconds * NS_PER_S + nanoseconds;

		if (is_neg) {
			bound->ns_from_origin = -bound->ns_from_origin;
		}

		bound->is_set = true;

		goto end;
	}

	BT_COMP_LOGE_APPEND_CAUSE(trimmer_comp->self_comp,
		"Invalid date/time format: param=\"%s\"", str);
	ret = -1;

end:
	g_match_info_free(match_info);

	return ret;
}

/*
 * Sets a trimmer bound's properties from a parameter string/integer
 * value.
 *
 * Returns a negative value if anything goes wrong.
 */
static
int set_bound_from_param(struct trimmer_comp *trimmer_comp,
		const char *param_name, const bt_value *param,
		struct trimmer_bound *bound, bool is_gmt)
{
	int ret;
	const char *arg;
	char tmp_arg[64];

	if (bt_value_is_signed_integer(param)) {
		int64_t value = bt_value_integer_signed_get(param);

		/*
		 * Just convert it to a temporary string to handle
		 * everything the same way.
		 */
		sprintf(tmp_arg, "%" PRId64, value);
		arg = tmp_arg;
	} else {
		BT_ASSERT(bt_value_is_string(param));
		arg = bt_value_string_get(param);
	}

	ret = set_bound_from_str(trimmer_comp, arg, bound, is_gmt);

	return ret;
}

static
int validate_trimmer_bounds(struct trimmer_comp *trimmer_comp,
		struct trimmer_bound *begin, struct trimmer_bound *end)
{
	int ret = 0;

	BT_ASSERT(begin->is_set);
	BT_ASSERT(end->is_set);

	if (!begin->is_infinite && !end->is_infinite &&
			begin->ns_from_origin > end->ns_from_origin) {
		BT_COMP_LOGE_APPEND_CAUSE(trimmer_comp->self_comp,
			"Trimming time range's beginning time is greater than end time: "
			"begin-ns-from-origin=%" PRId64 ", "
			"end-ns-from-origin=%" PRId64,
			begin->ns_from_origin,
			end->ns_from_origin);
		ret = -1;
		goto end;
	}

	if (!begin->is_infinite && begin->ns_from_origin == INT64_MIN) {
		BT_COMP_LOGE_APPEND_CAUSE(trimmer_comp->self_comp,
			"Invalid trimming time range's beginning time: "
			"ns-from-origin=%" PRId64,
			begin->ns_from_origin);
		ret = -1;
		goto end;
	}

	if (!end->is_infinite && end->ns_from_origin == INT64_MIN) {
		BT_COMP_LOGE_APPEND_CAUSE(trimmer_comp->self_comp,
			"Invalid trimming time range's end time: "
			"ns-from-origin=%" PRId64,
			end->ns_from_origin);
		ret = -1;
		goto end;
	}

end:
	return ret;
}

static
enum bt_param_validation_status validate_bound_type(
		const bt_value *value,
		struct bt_param_validation_context *context)
{
	enum bt_param_validation_status status = BT_PARAM_VALIDATION_STATUS_OK;

	if (!bt_value_is_signed_integer(value) &&
			!bt_value_is_string(value)) {
		status = bt_param_validation_error(context,
			"unexpected type: expected-types=[%s, %s], actual-type=%s",
			bt_common_value_type_string(BT_VALUE_TYPE_SIGNED_INTEGER),
			bt_common_value_type_string(BT_VALUE_TYPE_STRING),
			bt_common_value_type_string(bt_value_get_type(value)));
	}

	return status;
}

static
struct bt_param_validation_map_value_entry_descr trimmer_params[] = {
	{ "gmt", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	{ "begin", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .validation_func = validate_bound_type } },
	{ "end", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .validation_func = validate_bound_type } },
	BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
};

static
bt_component_class_initialize_method_status init_trimmer_comp_from_params(
		struct trimmer_comp *trimmer_comp,
		const bt_value *params)
{
	const bt_value *value;
	bt_component_class_initialize_method_status status;
	enum bt_param_validation_status validation_status;
	gchar *validate_error = NULL;

	validation_status = bt_param_validation_validate(params,
		trimmer_params, &validate_error);
	if (validation_status == BT_PARAM_VALIDATION_STATUS_MEMORY_ERROR) {
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		goto end;
	} else if (validation_status == BT_PARAM_VALIDATION_STATUS_VALIDATION_ERROR) {
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
		BT_COMP_LOGE_APPEND_CAUSE(trimmer_comp->self_comp, "%s",
			validate_error);
		goto end;
	}

	BT_ASSERT(params);
        value = bt_value_map_borrow_entry_value_const(params, "gmt");
	if (value) {
		trimmer_comp->is_gmt = (bool) bt_value_bool_get(value);
	}

        value = bt_value_map_borrow_entry_value_const(params, "begin");
	if (value) {
		if (set_bound_from_param(trimmer_comp, "begin", value,
				&trimmer_comp->begin, trimmer_comp->is_gmt)) {
			/* set_bound_from_param() logs errors */
			status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
			goto end;
		}
	} else {
		trimmer_comp->begin.is_infinite = true;
		trimmer_comp->begin.is_set = true;
	}

        value = bt_value_map_borrow_entry_value_const(params, "end");
	if (value) {
		if (set_bound_from_param(trimmer_comp, "end", value,
				&trimmer_comp->end, trimmer_comp->is_gmt)) {
			/* set_bound_from_param() logs errors */
			status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
			goto end;
		}
	} else {
		trimmer_comp->end.is_infinite = true;
		trimmer_comp->end.is_set = true;
	}

	if (trimmer_comp->begin.is_set && trimmer_comp->end.is_set) {
		/* validate_trimmer_bounds() logs errors */
		if (validate_trimmer_bounds(trimmer_comp,
			&trimmer_comp->begin, &trimmer_comp->end)) {
			status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
			goto end;
		}
	}

	status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;

end:
	g_free(validate_error);

	return status;
}

bt_component_class_initialize_method_status trimmer_init(
		bt_self_component_filter *self_comp_flt,
		bt_self_component_filter_configuration *config,
		const bt_value *params, void *init_data)
{
	bt_component_class_initialize_method_status status;
	bt_self_component_add_port_status add_port_status;
	struct trimmer_comp *trimmer_comp = create_trimmer_comp();
	bt_self_component *self_comp =
		bt_self_component_filter_as_self_component(self_comp_flt);

	if (!trimmer_comp) {
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		goto error;
	}

	trimmer_comp->log_level = bt_component_get_logging_level(
		bt_self_component_as_component(self_comp));
	trimmer_comp->self_comp = self_comp;
	trimmer_comp->self_comp_filter = self_comp_flt;

	add_port_status = bt_self_component_filter_add_input_port(
		self_comp_flt, in_port_name, NULL, NULL);
	if (add_port_status != BT_SELF_COMPONENT_ADD_PORT_STATUS_OK) {
		status = (int) add_port_status;
		goto error;
	}

	add_port_status = bt_self_component_filter_add_output_port(
		self_comp_flt, "out", NULL, NULL);
	if (add_port_status != BT_SELF_COMPONENT_ADD_PORT_STATUS_OK) {
		status = (int) add_port_status;
		goto error;
	}

	status = init_trimmer_comp_from_params(trimmer_comp, params);
	if (status != BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK) {
		goto error;
	}

	bt_self_component_set_data(self_comp, trimmer_comp);

	status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;
	goto end;

error:
	if (trimmer_comp) {
		destroy_trimmer_comp(trimmer_comp);
	}

end:
	return status;
}

static
void destroy_trimmer_iterator(struct trimmer_iterator *trimmer_it)
{
	if (!trimmer_it) {
		goto end;
	}

	bt_message_iterator_put_ref(
		trimmer_it->upstream_iter);

	if (trimmer_it->output_messages) {
		g_queue_free(trimmer_it->output_messages);
	}

	if (trimmer_it->stream_states) {
		g_hash_table_destroy(trimmer_it->stream_states);
	}

	g_free(trimmer_it);
end:
	return;
}

static
void destroy_trimmer_iterator_stream_state(
		struct trimmer_iterator_stream_state *sstate)
{
	BT_ASSERT(sstate);
	BT_PACKET_PUT_REF_AND_RESET(sstate->cur_packet);
	g_free(sstate);
}

BT_HIDDEN
bt_message_iterator_class_initialize_method_status trimmer_msg_iter_init(
		bt_self_message_iterator *self_msg_iter,
		bt_self_message_iterator_configuration *config,
		bt_self_component_port_output *port)
{
	bt_message_iterator_class_initialize_method_status status;
	bt_message_iterator_create_from_message_iterator_status
		msg_iter_status;
	struct trimmer_iterator *trimmer_it;
	bt_self_component *self_comp =
		bt_self_message_iterator_borrow_component(self_msg_iter);

	trimmer_it = g_new0(struct trimmer_iterator, 1);
	if (!trimmer_it) {
		status = BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		goto error;
	}

	trimmer_it->trimmer_comp = bt_self_component_get_data(self_comp);
	BT_ASSERT(trimmer_it->trimmer_comp);

	if (trimmer_it->trimmer_comp->begin.is_set &&
			trimmer_it->trimmer_comp->end.is_set) {
		/*
		 * Both trimming time range's bounds are set, so skip
		 * the
		 * `TRIMMER_ITERATOR_STATE_SET_BOUNDS_NS_FROM_ORIGIN`
		 * phase.
		 */
		trimmer_it->state = TRIMMER_ITERATOR_STATE_SEEK_INITIALLY;
	}

	trimmer_it->begin = trimmer_it->trimmer_comp->begin;
	trimmer_it->end = trimmer_it->trimmer_comp->end;
	msg_iter_status =
		bt_message_iterator_create_from_message_iterator(
			self_msg_iter,
			bt_self_component_filter_borrow_input_port_by_name(
				trimmer_it->trimmer_comp->self_comp_filter, in_port_name),
			&trimmer_it->upstream_iter);
	if (msg_iter_status != BT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_OK) {
		status = (int) msg_iter_status;
		goto error;
	}

	trimmer_it->output_messages = g_queue_new();
	if (!trimmer_it->output_messages) {
		status = BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		goto error;
	}

	trimmer_it->stream_states = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL,
		(GDestroyNotify) destroy_trimmer_iterator_stream_state);
	if (!trimmer_it->stream_states) {
		status = BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		goto error;
	}

	/*
	 * The trimmer requires upstream messages to have times, so it can
	 * always seek forward.
	 */
	bt_self_message_iterator_configuration_set_can_seek_forward(
		config, BT_TRUE);

	trimmer_it->self_msg_iter = self_msg_iter;
	bt_self_message_iterator_set_data(self_msg_iter, trimmer_it);

	status = BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_OK;
	goto end;

error:
	destroy_trimmer_iterator(trimmer_it);

end:
	return status;
}

static inline
int get_msg_ns_from_origin(const bt_message *msg, int64_t *ns_from_origin,
		bool *has_clock_snapshot)
{
	const bt_clock_class *clock_class = NULL;
	const bt_clock_snapshot *clock_snapshot = NULL;
	int ret = 0;

	BT_ASSERT_DBG(msg);
	BT_ASSERT_DBG(ns_from_origin);
	BT_ASSERT_DBG(has_clock_snapshot);

	switch (bt_message_get_type(msg)) {
	case BT_MESSAGE_TYPE_EVENT:
		clock_class =
			bt_message_event_borrow_stream_class_default_clock_class_const(
				msg);
		if (G_UNLIKELY(!clock_class)) {
			goto error;
		}

		clock_snapshot = bt_message_event_borrow_default_clock_snapshot_const(
			msg);
		break;
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		clock_class =
			bt_message_packet_beginning_borrow_stream_class_default_clock_class_const(
				msg);
		if (G_UNLIKELY(!clock_class)) {
			goto error;
		}

		clock_snapshot = bt_message_packet_beginning_borrow_default_clock_snapshot_const(
			msg);
		break;
	case BT_MESSAGE_TYPE_PACKET_END:
		clock_class =
			bt_message_packet_end_borrow_stream_class_default_clock_class_const(
				msg);
		if (G_UNLIKELY(!clock_class)) {
			goto error;
		}

		clock_snapshot = bt_message_packet_end_borrow_default_clock_snapshot_const(
			msg);
		break;
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
	{
		enum bt_message_stream_clock_snapshot_state cs_state;

		clock_class =
			bt_message_stream_beginning_borrow_stream_class_default_clock_class_const(msg);
		if (G_UNLIKELY(!clock_class)) {
			goto error;
		}

		cs_state = bt_message_stream_beginning_borrow_default_clock_snapshot_const(msg, &clock_snapshot);
		if (cs_state != BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN) {
			goto no_clock_snapshot;
		}

		break;
	}
	case BT_MESSAGE_TYPE_STREAM_END:
	{
		enum bt_message_stream_clock_snapshot_state cs_state;

		clock_class =
			bt_message_stream_end_borrow_stream_class_default_clock_class_const(msg);
		if (G_UNLIKELY(!clock_class)) {
			goto error;
		}

		cs_state = bt_message_stream_end_borrow_default_clock_snapshot_const(msg, &clock_snapshot);
		if (cs_state != BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN) {
			goto no_clock_snapshot;
		}

		break;
	}
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
		clock_class =
			bt_message_discarded_events_borrow_stream_class_default_clock_class_const(
				msg);
		if (G_UNLIKELY(!clock_class)) {
			goto error;
		}

		clock_snapshot = bt_message_discarded_events_borrow_beginning_default_clock_snapshot_const(
			msg);
		break;
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
		clock_class =
			bt_message_discarded_packets_borrow_stream_class_default_clock_class_const(
				msg);
		if (G_UNLIKELY(!clock_class)) {
			goto error;
		}

		clock_snapshot = bt_message_discarded_packets_borrow_beginning_default_clock_snapshot_const(
			msg);
		break;
	case BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY:
		clock_snapshot =
			bt_message_message_iterator_inactivity_borrow_clock_snapshot_const(
				msg);
		break;
	default:
		goto no_clock_snapshot;
	}

	ret = bt_clock_snapshot_get_ns_from_origin(clock_snapshot,
		ns_from_origin);
	if (G_UNLIKELY(ret)) {
		goto error;
	}

	*has_clock_snapshot = true;
	goto end;

no_clock_snapshot:
	*has_clock_snapshot = false;
	goto end;

error:
	ret = -1;

end:
	return ret;
}

static inline
void put_messages(bt_message_array_const msgs, uint64_t count)
{
	uint64_t i;

	for (i = 0; i < count; i++) {
		BT_MESSAGE_PUT_REF_AND_RESET(msgs[i]);
	}
}

static inline
int set_trimmer_iterator_bound(struct trimmer_iterator *trimmer_it,
		struct trimmer_bound *bound, int64_t ns_from_origin,
		bool is_gmt)
{
	struct trimmer_comp *trimmer_comp = trimmer_it->trimmer_comp;
	struct tm tm;
	struct tm *res;
	time_t time_seconds = (time_t) (ns_from_origin / NS_PER_S);
	int ret = 0;

	BT_ASSERT(!bound->is_set);
	errno = 0;

	/* We only need to extract the date from this time */
	if (is_gmt) {
		res = bt_gmtime_r(&time_seconds, &tm);
	} else {
		res = bt_localtime_r(&time_seconds, &tm);
	}

	if (!res) {
		BT_COMP_LOGE_APPEND_CAUSE_ERRNO(trimmer_comp->self_comp,
			"Cannot convert timestamp to date and time",
			": ts=%" PRId64, (int64_t) time_seconds);
		ret = -1;
		goto end;
	}

	ret = set_bound_ns_from_origin(bound, tm.tm_year + 1900, tm.tm_mon + 1,
		tm.tm_mday, bound->time.hour, bound->time.minute,
		bound->time.second, bound->time.ns, is_gmt);

end:
	return ret;
}

static
bt_message_iterator_class_next_method_status
state_set_trimmer_iterator_bounds(
		struct trimmer_iterator *trimmer_it)
{
	bt_message_iterator_next_status upstream_iter_status =
		BT_MESSAGE_ITERATOR_NEXT_STATUS_OK;
	struct trimmer_comp *trimmer_comp = trimmer_it->trimmer_comp;
	bt_message_array_const msgs;
	uint64_t count = 0;
	int64_t ns_from_origin = INT64_MIN;
	uint64_t i;
	int ret;

	BT_ASSERT(!trimmer_it->begin.is_set ||
		!trimmer_it->end.is_set);

	while (true) {
		upstream_iter_status =
			bt_message_iterator_next(
				trimmer_it->upstream_iter, &msgs, &count);
		if (upstream_iter_status != BT_MESSAGE_ITERATOR_NEXT_STATUS_OK) {
			goto end;
		}

		for (i = 0; i < count; i++) {
			const bt_message *msg = msgs[i];
			bool has_ns_from_origin;
			ret = get_msg_ns_from_origin(msg, &ns_from_origin,
				&has_ns_from_origin);
			if (ret) {
				goto error;
			}

			if (!has_ns_from_origin) {
				continue;
			}

			BT_ASSERT_DBG(ns_from_origin != INT64_MIN &&
				ns_from_origin != INT64_MAX);
			put_messages(msgs, count);
			goto found;
		}

		put_messages(msgs, count);
	}

found:
	if (!trimmer_it->begin.is_set) {
		BT_ASSERT(!trimmer_it->begin.is_infinite);
		ret = set_trimmer_iterator_bound(trimmer_it, &trimmer_it->begin,
			ns_from_origin, trimmer_comp->is_gmt);
		if (ret) {
			goto error;
		}
	}

	if (!trimmer_it->end.is_set) {
		BT_ASSERT(!trimmer_it->end.is_infinite);
		ret = set_trimmer_iterator_bound(trimmer_it, &trimmer_it->end,
			ns_from_origin, trimmer_comp->is_gmt);
		if (ret) {
			goto error;
		}
	}

	ret = validate_trimmer_bounds(trimmer_it->trimmer_comp,
		&trimmer_it->begin, &trimmer_it->end);
	if (ret) {
		goto error;
	}

	goto end;

error:
	put_messages(msgs, count);
	upstream_iter_status = BT_MESSAGE_ITERATOR_NEXT_STATUS_ERROR;

end:
	return (int) upstream_iter_status;
}

static
bt_message_iterator_class_next_method_status state_seek_initially(
		struct trimmer_iterator *trimmer_it)
{
	struct trimmer_comp *trimmer_comp = trimmer_it->trimmer_comp;
	bt_message_iterator_class_next_method_status status;

	BT_ASSERT(trimmer_it->begin.is_set);

	if (trimmer_it->begin.is_infinite) {
		bt_bool can_seek;

		status = (int) bt_message_iterator_can_seek_beginning(
			trimmer_it->upstream_iter, &can_seek);
		if (status != BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK) {
			if (status < 0) {
				BT_COMP_LOGE_APPEND_CAUSE(trimmer_comp->self_comp,
					"Cannot make upstream message iterator initially seek its beginning.");
			}

			goto end;
		}

		if (!can_seek) {
			BT_COMP_LOGE_APPEND_CAUSE(trimmer_comp->self_comp,
				"Cannot make upstream message iterator initially seek its beginning.");
			status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
			goto end;
		}

		status = (int) bt_message_iterator_seek_beginning(
			trimmer_it->upstream_iter);
	} else {
		bt_bool can_seek;

		status = (int) bt_message_iterator_can_seek_ns_from_origin(
			trimmer_it->upstream_iter, trimmer_it->begin.ns_from_origin,
			&can_seek);

		if (status != BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK) {
			if (status < 0) {
				BT_COMP_LOGE_APPEND_CAUSE(trimmer_comp->self_comp,
					"Cannot make upstream message iterator initially seek: seek-ns-from-origin=%" PRId64,
					trimmer_it->begin.ns_from_origin);
			}

			goto end;
		}

		if (!can_seek) {
			BT_COMP_LOGE_APPEND_CAUSE(trimmer_comp->self_comp,
				"Cannot make upstream message iterator initially seek: seek-ns-from-origin=%" PRId64,
				trimmer_it->begin.ns_from_origin);
			status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
			goto end;
		}

		status = (int) bt_message_iterator_seek_ns_from_origin(
			trimmer_it->upstream_iter, trimmer_it->begin.ns_from_origin);
	}

	if (status == BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK) {
		trimmer_it->state = TRIMMER_ITERATOR_STATE_TRIM;
	}

end:
	return status;
}

static inline
void push_message(struct trimmer_iterator *trimmer_it, const bt_message *msg)
{
	g_queue_push_head(trimmer_it->output_messages, (void *) msg);
}

static inline
const bt_message *pop_message(struct trimmer_iterator *trimmer_it)
{
	return g_queue_pop_tail(trimmer_it->output_messages);
}

static inline
int clock_raw_value_from_ns_from_origin(const bt_clock_class *clock_class,
		int64_t ns_from_origin, uint64_t *raw_value)
{

	int64_t cc_offset_s;
	uint64_t cc_offset_cycles;
	uint64_t cc_freq;

	bt_clock_class_get_offset(clock_class, &cc_offset_s, &cc_offset_cycles);
	cc_freq = bt_clock_class_get_frequency(clock_class);
	return bt_common_clock_value_from_ns_from_origin(cc_offset_s,
		cc_offset_cycles, cc_freq, ns_from_origin, raw_value);
}

static inline
bt_message_iterator_class_next_method_status
end_stream(struct trimmer_iterator *trimmer_it,
		struct trimmer_iterator_stream_state *sstate)
{
	bt_message_iterator_class_next_method_status status =
		BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
	/* Initialize to silence maybe-uninitialized warning. */
	uint64_t raw_value = 0;
	bt_message *msg = NULL;

	BT_ASSERT(!trimmer_it->end.is_infinite);
	BT_ASSERT(sstate->stream);

	/*
	 * If we haven't seen a message with a clock snapshot, we don't know if the trimmer's end bound is within
	 * the clock's range, so it wouldn't be safe to try to convert ns_from_origin to a clock value.
	 *
	 * Also, it would be a bit of a lie to generate a stream end message with the end bound as its
	 * clock snapshot, because we don't really know if the stream existed at that time.  If we have
	 * seen a message with a clock snapshot and the stream is cut short by another message with a
	 * clock snapshot, then we are sure that the the end bound time is not below the clock range,
	 * and we know the stream was active at that time (and that we cut it short).
	 */
	if (sstate->seen_clock_snapshot) {
		const bt_clock_class *clock_class;
		int ret;

		clock_class = bt_stream_class_borrow_default_clock_class_const(
			bt_stream_borrow_class_const(sstate->stream));
		BT_ASSERT(clock_class);
		ret = clock_raw_value_from_ns_from_origin(clock_class,
			trimmer_it->end.ns_from_origin, &raw_value);
		if (ret) {
			status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
			goto end;
		}
	}

	if (sstate->cur_packet) {
		/*
		 * Create and push a packet end message, making its time
		 * the trimming range's end time.
		 *
		 * We know that we must have seen a clock snapshot, the one in
		 * the packet beginning message, since trimmer currently
		 * requires packet messages to have clock snapshots (see comment
		 * in create_stream_state_entry).
		 */
		BT_ASSERT(sstate->seen_clock_snapshot);

		msg = bt_message_packet_end_create_with_default_clock_snapshot(
			trimmer_it->self_msg_iter, sstate->cur_packet,
			raw_value);
		if (!msg) {
			status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_MEMORY_ERROR;
			goto end;
		}

		push_message(trimmer_it, msg);
		msg = NULL;
		BT_PACKET_PUT_REF_AND_RESET(sstate->cur_packet);
	}

	/* Create and push a stream end message. */
	msg = bt_message_stream_end_create(trimmer_it->self_msg_iter,
		sstate->stream);
	if (!msg) {
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_MEMORY_ERROR;
		goto end;
	}

	if (sstate->seen_clock_snapshot) {
		bt_message_stream_end_set_default_clock_snapshot(msg, raw_value);
	}

	push_message(trimmer_it, msg);
	msg = NULL;

	/*
	 * Just to make sure that we don't use this stream state again
	 * in the future without an obvious error.
	 */
	sstate->stream = NULL;

end:
	bt_message_put_ref(msg);
	return status;
}

static inline
bt_message_iterator_class_next_method_status end_iterator_streams(
		struct trimmer_iterator *trimmer_it)
{
	bt_message_iterator_class_next_method_status status =
		BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
	GHashTableIter iter;
	gpointer key, sstate;

	if (trimmer_it->end.is_infinite) {
		/*
		 * An infinite trimming range's end time guarantees that
		 * we received (and pushed) all the appropriate end
		 * messages.
		 */
		goto remove_all;
	}

	/*
	 * End each stream and then remove them from the hash table of
	 * stream states to release unneeded references.
	 */
	g_hash_table_iter_init(&iter, trimmer_it->stream_states);

	while (g_hash_table_iter_next(&iter, &key, &sstate)) {
		status = end_stream(trimmer_it, sstate);
		if (status) {
			goto end;
		}
	}

remove_all:
	g_hash_table_remove_all(trimmer_it->stream_states);

end:
	return status;
}

static
bt_message_iterator_class_next_method_status
create_stream_state_entry(
		struct trimmer_iterator *trimmer_it,
		const struct bt_stream *stream,
		struct trimmer_iterator_stream_state **stream_state)
{
	struct trimmer_comp *trimmer_comp = trimmer_it->trimmer_comp;
	bt_message_iterator_class_next_method_status status;
	struct trimmer_iterator_stream_state *sstate;
	const bt_stream_class *sc;

	BT_ASSERT(!bt_g_hash_table_contains(trimmer_it->stream_states, stream));

	/*
	 * Validate right now that the stream's class
	 * has a registered default clock class so that
	 * an existing stream state guarantees existing
	 * default clock snapshots for its associated
	 * messages.
	 *
	 * Also check that clock snapshots are always
	 * known.
	 */
	sc = bt_stream_borrow_class_const(stream);
	if (!bt_stream_class_borrow_default_clock_class_const(sc)) {
		BT_COMP_LOGE_APPEND_CAUSE(trimmer_comp->self_comp,
			"Unsupported stream: stream class does "
			"not have a default clock class: "
			"stream-addr=%p, "
			"stream-id=%" PRIu64 ", "
			"stream-name=\"%s\"",
			stream, bt_stream_get_id(stream),
			bt_stream_get_name(stream));
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
		goto end;
	}

	/*
	 * Temporary: make sure packet beginning, packet
	 * end, discarded events, and discarded packets
	 * messages have default clock snapshots until
	 * the support for not having them is
	 * implemented.
	 */
	if (bt_stream_class_supports_packets(sc)) {
		if (!bt_stream_class_packets_have_beginning_default_clock_snapshot(
				sc)) {
			BT_COMP_LOGE_APPEND_CAUSE(trimmer_comp->self_comp,
				"Unsupported stream: packets have no beginning clock snapshot: "
				"stream-addr=%p, "
				"stream-id=%" PRIu64 ", "
				"stream-name=\"%s\"",
				stream, bt_stream_get_id(stream),
				bt_stream_get_name(stream));
			status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
			goto end;
		}

		if (!bt_stream_class_packets_have_end_default_clock_snapshot(
				sc)) {
			BT_COMP_LOGE_APPEND_CAUSE(trimmer_comp->self_comp,
				"Unsupported stream: packets have no end clock snapshot: "
				"stream-addr=%p, "
				"stream-id=%" PRIu64 ", "
				"stream-name=\"%s\"",
				stream, bt_stream_get_id(stream),
				bt_stream_get_name(stream));
			status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
			goto end;
		}

		if (bt_stream_class_supports_discarded_packets(sc) &&
				!bt_stream_class_discarded_packets_have_default_clock_snapshots(sc)) {
			BT_COMP_LOGE_APPEND_CAUSE(trimmer_comp->self_comp,
				"Unsupported stream: discarded packets "
				"have no clock snapshots: "
				"stream-addr=%p, "
				"stream-id=%" PRIu64 ", "
				"stream-name=\"%s\"",
				stream, bt_stream_get_id(stream),
				bt_stream_get_name(stream));
			status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
			goto end;
		}
	}

	if (bt_stream_class_supports_discarded_events(sc) &&
			!bt_stream_class_discarded_events_have_default_clock_snapshots(sc)) {
		BT_COMP_LOGE_APPEND_CAUSE(trimmer_comp->self_comp,
			"Unsupported stream: discarded events have no clock snapshots: "
			"stream-addr=%p, "
			"stream-id=%" PRIu64 ", "
			"stream-name=\"%s\"",
			stream, bt_stream_get_id(stream),
			bt_stream_get_name(stream));
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
		goto end;
	}

	sstate = g_new0(struct trimmer_iterator_stream_state, 1);
	if (!sstate) {
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_MEMORY_ERROR;
		goto end;
	}

	sstate->stream = stream;

	g_hash_table_insert(trimmer_it->stream_states, (void *) stream, sstate);

	*stream_state = sstate;

	status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;

end:
	return status;
}

static
struct trimmer_iterator_stream_state *get_stream_state_entry(
		struct trimmer_iterator *trimmer_it,
		const struct bt_stream *stream)
{
	struct trimmer_iterator_stream_state *sstate;

	BT_ASSERT_DBG(stream);
	sstate = g_hash_table_lookup(trimmer_it->stream_states, stream);
	BT_ASSERT_DBG(sstate);

	return sstate;
}

/*
 * Handles a message which is associated to a given stream state. This
 * _could_ make the iterator's output message queue grow; this could
 * also consume the message without pushing anything to this queue, only
 * modifying the stream state.
 *
 * This function consumes the `msg` reference, _whatever the outcome_.
 *
 * If non-NULL, `ns_from_origin` is the message's time, as given by
 * get_msg_ns_from_origin().  If NULL, the message doesn't have a time.
 *
 * This function sets `reached_end` if handling this message made the
 * iterator reach the end of the trimming range. Note that the output
 * message queue could contain messages even if this function sets
 * `reached_end`.
 */
static
bt_message_iterator_class_next_method_status
handle_message_with_stream(
		struct trimmer_iterator *trimmer_it, const bt_message *msg,
		const struct bt_stream *stream, const int64_t *ns_from_origin,
		bool *reached_end)
{
	bt_message_iterator_class_next_method_status status =
		BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
	bt_message_type msg_type = bt_message_get_type(msg);
	int ret;
	struct trimmer_iterator_stream_state *sstate = NULL;

	/*
	 * Retrieve the stream's state - except if the message is stream
	 * beginning, in which case we don't know about about this stream yet.
	 */
	if (msg_type != BT_MESSAGE_TYPE_STREAM_BEGINNING) {
		sstate = get_stream_state_entry(trimmer_it, stream);
	}

	switch (msg_type) {
	case BT_MESSAGE_TYPE_EVENT:
		/*
		 * Event messages always have a clock snapshot if the stream
		 * class has a clock class. And we know it has, otherwise we
		 * couldn't be using the trimmer component.
		 */
		BT_ASSERT_DBG(ns_from_origin);

		if (G_UNLIKELY(!trimmer_it->end.is_infinite &&
				*ns_from_origin > trimmer_it->end.ns_from_origin)) {
			status = end_iterator_streams(trimmer_it);
			*reached_end = true;
			break;
		}

		sstate->seen_clock_snapshot = true;

		push_message(trimmer_it, msg);
		msg = NULL;
		break;

	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		/*
		 * Packet beginning messages won't have a clock snapshot if
		 * stream_class->packets_have_beginning_default_clock_snapshot
		 * is false.  But for now, assume they always do.
		 */
		BT_ASSERT(ns_from_origin);
		BT_ASSERT(!sstate->cur_packet);

		if (G_UNLIKELY(!trimmer_it->end.is_infinite &&
				*ns_from_origin > trimmer_it->end.ns_from_origin)) {
			status = end_iterator_streams(trimmer_it);
			*reached_end = true;
			break;
		}

		sstate->cur_packet =
			bt_message_packet_beginning_borrow_packet_const(msg);
		bt_packet_get_ref(sstate->cur_packet);

		sstate->seen_clock_snapshot = true;

		push_message(trimmer_it, msg);
		msg = NULL;
		break;

	case BT_MESSAGE_TYPE_PACKET_END:
		/*
		 * Packet end messages won't have a clock snapshot if
		 * stream_class->packets_have_end_default_clock_snapshot
		 * is false.  But for now, assume they always do.
		 */
		BT_ASSERT(ns_from_origin);
		BT_ASSERT(sstate->cur_packet);

		if (G_UNLIKELY(!trimmer_it->end.is_infinite &&
				*ns_from_origin > trimmer_it->end.ns_from_origin)) {
			status = end_iterator_streams(trimmer_it);
			*reached_end = true;
			break;
		}

		BT_PACKET_PUT_REF_AND_RESET(sstate->cur_packet);

		sstate->seen_clock_snapshot = true;

		push_message(trimmer_it, msg);
		msg = NULL;
		break;

	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
	{
		/*
		 * `ns_from_origin` is the message's time range's
		 * beginning time here.
		 */
		int64_t end_ns_from_origin;
		const bt_clock_snapshot *end_cs;

		BT_ASSERT(ns_from_origin);

		sstate->seen_clock_snapshot = true;

		if (bt_message_get_type(msg) ==
				BT_MESSAGE_TYPE_DISCARDED_EVENTS) {
			/*
			 * Safe to ignore the return value because we
			 * know there's a default clock and it's always
			 * known.
			 */
			end_cs = bt_message_discarded_events_borrow_end_default_clock_snapshot_const(
				msg);
		} else {
			/*
			 * Safe to ignore the return value because we
			 * know there's a default clock and it's always
			 * known.
			 */
			end_cs = bt_message_discarded_packets_borrow_end_default_clock_snapshot_const(
				msg);
		}

		if (bt_clock_snapshot_get_ns_from_origin(end_cs,
				&end_ns_from_origin)) {
			status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
			goto end;
		}

		if (!trimmer_it->end.is_infinite &&
				*ns_from_origin > trimmer_it->end.ns_from_origin) {
			status = end_iterator_streams(trimmer_it);
			*reached_end = true;
			break;
		}

		if (!trimmer_it->end.is_infinite &&
				end_ns_from_origin > trimmer_it->end.ns_from_origin) {
			/*
			 * This message's end time is outside the
			 * trimming time range: replace it with a new
			 * message having an end time equal to the
			 * trimming time range's end and without a
			 * count.
			 */
			const bt_clock_class *clock_class =
				bt_clock_snapshot_borrow_clock_class_const(
					end_cs);
			const bt_clock_snapshot *begin_cs;
			bt_message *new_msg;
			uint64_t end_raw_value;

			ret = clock_raw_value_from_ns_from_origin(clock_class,
				trimmer_it->end.ns_from_origin, &end_raw_value);
			if (ret) {
				status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
				goto end;
			}

			if (msg_type == BT_MESSAGE_TYPE_DISCARDED_EVENTS) {
				begin_cs = bt_message_discarded_events_borrow_beginning_default_clock_snapshot_const(
					msg);
				new_msg = bt_message_discarded_events_create_with_default_clock_snapshots(
					trimmer_it->self_msg_iter,
					sstate->stream,
					bt_clock_snapshot_get_value(begin_cs),
					end_raw_value);
			} else {
				begin_cs = bt_message_discarded_packets_borrow_beginning_default_clock_snapshot_const(
					msg);
				new_msg = bt_message_discarded_packets_create_with_default_clock_snapshots(
					trimmer_it->self_msg_iter,
					sstate->stream,
					bt_clock_snapshot_get_value(begin_cs),
					end_raw_value);
			}

			if (!new_msg) {
				status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_MEMORY_ERROR;
				goto end;
			}

			/* Replace the original message */
			BT_MESSAGE_MOVE_REF(msg, new_msg);
		}

		push_message(trimmer_it, msg);
		msg = NULL;
		break;
	}

	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
		/*
		 * If this message has a time and this time is greater than the
		 * trimmer's end bound, it triggers the end of the trim window.
		 */
		if (G_UNLIKELY(ns_from_origin && !trimmer_it->end.is_infinite &&
				*ns_from_origin > trimmer_it->end.ns_from_origin)) {
			status = end_iterator_streams(trimmer_it);
			*reached_end = true;
			break;
		}

		/* Learn about this stream. */
		status = create_stream_state_entry(trimmer_it, stream, &sstate);
		if (status != BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK) {
			goto end;
		}

		if (ns_from_origin) {
			sstate->seen_clock_snapshot = true;
		}

		push_message(trimmer_it,  msg);
		msg = NULL;
		break;
	case BT_MESSAGE_TYPE_STREAM_END:
	{
		gboolean removed;

		/*
		 * If this message has a time and this time is greater than the
		 * trimmer's end bound, it triggers the end of the trim window.
		 */
		if (G_UNLIKELY(ns_from_origin && !trimmer_it->end.is_infinite &&
				*ns_from_origin > trimmer_it->end.ns_from_origin)) {
			status = end_iterator_streams(trimmer_it);
			*reached_end = true;
			break;
		}

		/*
		 * Either the stream end message's time is within the trimmer's
		 * bounds, or it doesn't have a time.  In both cases, pass
		 * the message unmodified.
		 */
		push_message(trimmer_it, msg);
		msg = NULL;

		/* Forget about this stream. */
		removed = g_hash_table_remove(trimmer_it->stream_states, sstate->stream);
		BT_ASSERT(removed);
		break;
	}
	default:
		break;
	}

end:
	/* We release the message's reference whatever the outcome */
	bt_message_put_ref(msg);
	return status;
}

/*
 * Handles an input message. This _could_ make the iterator's output
 * message queue grow; this could also consume the message without
 * pushing anything to this queue, only modifying the stream state.
 *
 * This function consumes the `msg` reference, _whatever the outcome_.
 *
 * This function sets `reached_end` if handling this message made the
 * iterator reach the end of the trimming range. Note that the output
 * message queue could contain messages even if this function sets
 * `reached_end`.
 */
static inline
bt_message_iterator_class_next_method_status handle_message(
		struct trimmer_iterator *trimmer_it, const bt_message *msg,
		bool *reached_end)
{
	bt_message_iterator_class_next_method_status status;
	const bt_stream *stream = NULL;
	int64_t ns_from_origin = INT64_MIN;
	bool has_ns_from_origin = false;
	int ret;

	/* Find message's associated stream */
	switch (bt_message_get_type(msg)) {
	case BT_MESSAGE_TYPE_EVENT:
		stream = bt_event_borrow_stream_const(
			bt_message_event_borrow_event_const(msg));
		break;
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		stream = bt_packet_borrow_stream_const(
			bt_message_packet_beginning_borrow_packet_const(msg));
		break;
	case BT_MESSAGE_TYPE_PACKET_END:
		stream = bt_packet_borrow_stream_const(
			bt_message_packet_end_borrow_packet_const(msg));
		break;
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
		stream = bt_message_discarded_events_borrow_stream_const(msg);
		break;
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
		stream = bt_message_discarded_packets_borrow_stream_const(msg);
		break;
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
		stream = bt_message_stream_beginning_borrow_stream_const(msg);
		break;
	case BT_MESSAGE_TYPE_STREAM_END:
		stream = bt_message_stream_end_borrow_stream_const(msg);
		break;
	default:
		break;
	}

	/* Retrieve the message's time */
	ret = get_msg_ns_from_origin(msg, &ns_from_origin, &has_ns_from_origin);
	if (G_UNLIKELY(ret)) {
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
		goto end;
	}

	if (G_LIKELY(stream)) {
		/* Message associated to a stream */
		status = handle_message_with_stream(trimmer_it, msg,
			stream, has_ns_from_origin ? &ns_from_origin : NULL, reached_end);

		/*
		 * handle_message_with_stream_state() unconditionally
		 * consumes `msg`.
		 */
		msg = NULL;
	} else {
		/*
		 * Message not associated to a stream (message iterator
		 * inactivity).
		 */
		if (G_UNLIKELY(ns_from_origin > trimmer_it->end.ns_from_origin)) {
			BT_MESSAGE_PUT_REF_AND_RESET(msg);
			status = end_iterator_streams(trimmer_it);
			*reached_end = true;
		} else {
			push_message(trimmer_it, msg);
			status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
			msg = NULL;
		}
	}

end:
	/* We release the message's reference whatever the outcome */
	bt_message_put_ref(msg);
	return status;
}

static inline
void fill_message_array_from_output_messages(
		struct trimmer_iterator *trimmer_it,
		bt_message_array_const msgs, uint64_t capacity, uint64_t *count)
{
	*count = 0;

	/*
	 * Move auto-seek messages to the output array (which is this
	 * iterator's base message array).
	 */
	while (capacity > 0 && !g_queue_is_empty(trimmer_it->output_messages)) {
		msgs[*count] = pop_message(trimmer_it);
		capacity--;
		(*count)++;
	}

	BT_ASSERT_DBG(*count > 0);
}

static inline
bt_message_iterator_class_next_method_status state_ending(
		struct trimmer_iterator *trimmer_it,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	bt_message_iterator_class_next_method_status status =
		BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;

	if (g_queue_is_empty(trimmer_it->output_messages)) {
		trimmer_it->state = TRIMMER_ITERATOR_STATE_ENDED;
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END;
		goto end;
	}

	fill_message_array_from_output_messages(trimmer_it, msgs,
		capacity, count);

end:
	return status;
}

static inline
bt_message_iterator_class_next_method_status
state_trim(struct trimmer_iterator *trimmer_it,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	bt_message_iterator_class_next_method_status status =
		BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
	bt_message_array_const my_msgs;
	uint64_t my_count;
	uint64_t i;
	bool reached_end = false;

	while (g_queue_is_empty(trimmer_it->output_messages)) {
		status = (int) bt_message_iterator_next(
			trimmer_it->upstream_iter, &my_msgs, &my_count);
		if (G_UNLIKELY(status != BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK)) {
			if (status == BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END) {
				status = end_iterator_streams(trimmer_it);
				if (status != BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK) {
					goto end;
				}

				trimmer_it->state =
					TRIMMER_ITERATOR_STATE_ENDING;
				status = state_ending(trimmer_it, msgs,
					capacity, count);
			}

			goto end;
		}

		BT_ASSERT_DBG(my_count > 0);

		for (i = 0; i < my_count; i++) {
			status = handle_message(trimmer_it, my_msgs[i],
				&reached_end);

			/*
			 * handle_message() unconditionally consumes the
			 * message reference.
			 */
			my_msgs[i] = NULL;

			if (G_UNLIKELY(status !=
					BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK)) {
				put_messages(my_msgs, my_count);
				goto end;
			}

			if (G_UNLIKELY(reached_end)) {
				/*
				 * This message's time was passed the
				 * trimming time range's end time: we
				 * are done. Their might still be
				 * messages in the output message queue,
				 * so move to the "ending" state and
				 * apply it immediately since
				 * state_trim() is called within the
				 * "next" method.
				 */
				put_messages(my_msgs, my_count);
				trimmer_it->state =
					TRIMMER_ITERATOR_STATE_ENDING;
				status = state_ending(trimmer_it, msgs,
					capacity, count);
				goto end;
			}
		}
	}

	/*
	 * There's at least one message in the output message queue:
	 * move the messages to the output message array.
	 */
	BT_ASSERT_DBG(!g_queue_is_empty(trimmer_it->output_messages));
	fill_message_array_from_output_messages(trimmer_it, msgs,
		capacity, count);

end:
	return status;
}

BT_HIDDEN
bt_message_iterator_class_next_method_status trimmer_msg_iter_next(
		bt_self_message_iterator *self_msg_iter,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	struct trimmer_iterator *trimmer_it =
		bt_self_message_iterator_get_data(self_msg_iter);
	bt_message_iterator_class_next_method_status status =
		BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;

	BT_ASSERT_DBG(trimmer_it);

	if (G_LIKELY(trimmer_it->state == TRIMMER_ITERATOR_STATE_TRIM)) {
		status = state_trim(trimmer_it, msgs, capacity, count);
		if (status != BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK) {
			goto end;
		}
	} else {
		switch (trimmer_it->state) {
		case TRIMMER_ITERATOR_STATE_SET_BOUNDS_NS_FROM_ORIGIN:
			status = state_set_trimmer_iterator_bounds(trimmer_it);
			if (status != BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK) {
				goto end;
			}

			status = state_seek_initially(trimmer_it);
			if (status != BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK) {
				goto end;
			}

			status = state_trim(trimmer_it, msgs, capacity, count);
			if (status != BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK) {
				goto end;
			}

			break;
		case TRIMMER_ITERATOR_STATE_SEEK_INITIALLY:
			status = state_seek_initially(trimmer_it);
			if (status != BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK) {
				goto end;
			}

			status = state_trim(trimmer_it, msgs, capacity, count);
			if (status != BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK) {
				goto end;
			}

			break;
		case TRIMMER_ITERATOR_STATE_ENDING:
			status = state_ending(trimmer_it, msgs, capacity,
				count);
			if (status != BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK) {
				goto end;
			}

			break;
		case TRIMMER_ITERATOR_STATE_ENDED:
			status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END;
			break;
		default:
			bt_common_abort();
		}
	}

end:
	return status;
}

BT_HIDDEN
void trimmer_msg_iter_finalize(bt_self_message_iterator *self_msg_iter)
{
	struct trimmer_iterator *trimmer_it =
		bt_self_message_iterator_get_data(self_msg_iter);

	BT_ASSERT(trimmer_it);
	destroy_trimmer_iterator(trimmer_it);
}
