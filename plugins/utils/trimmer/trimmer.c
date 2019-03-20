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

#define BT_LOG_TAG "PLUGIN-UTILS-TRIMMER-FLT"
#include "logging.h"

#include <babeltrace/compat/utc-internal.h>
#include <babeltrace/compat/time-internal.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/common-internal.h>
#include <plugins-common.h>
#include <babeltrace/assert-internal.h>
#include <stdint.h>
#include <inttypes.h>
#include <glib.h>

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
	bt_self_component_port_input_message_iterator *upstream_iter;
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
	/*
	 * True if both stream beginning and initial stream activity
	 * beginning messages were pushed for this stream.
	 */
	bool inited;

	/*
	 * True if the last pushed message for this stream was a stream
	 * activity end message.
	 */
	bool last_msg_is_stream_activity_end;

	/*
	 * Time to use for a generated stream end activity message when
	 * ending the stream.
	 */
	int64_t stream_act_end_ns_from_origin;

	/* Weak */
	const bt_stream *stream;

	/* Owned by this (`NULL` initially and between packets) */
	const bt_packet *cur_packet;

	/* Owned by this */
	const bt_message *stream_beginning_msg;
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
int set_bound_from_str(const char *str, struct trimmer_bound *bound,
		bool is_gmt)
{
	int ret = 0;
	int s_ret;
	unsigned int year, month, day, hour, minute, second, ns;
	char dummy;

	/* Try `YYYY-MM-DD hh:mm:ss.ns` format */
	s_ret = sscanf(str, "%u-%u-%u %u:%u:%u.%u%c", &year, &month, &day,
		&hour, &minute, &second, &ns, &dummy);
	if (s_ret == 7) {
		ret = set_bound_ns_from_origin(bound, year, month, day,
			hour, minute, second, ns, is_gmt);
		goto end;
	}

	/* Try `YYYY-MM-DD hh:mm:ss` format */
	s_ret = sscanf(str, "%u-%u-%u %u:%u:%u%c", &year, &month, &day,
		&hour, &minute, &second, &dummy);
	if (s_ret == 6) {
		ret = set_bound_ns_from_origin(bound, year, month, day,
			hour, minute, second, 0, is_gmt);
		goto end;
	}

	/* Try `YYYY-MM-DD hh:mm` format */
	s_ret = sscanf(str, "%u-%u-%u %u:%u%c", &year, &month, &day,
		&hour, &minute, &dummy);
	if (s_ret == 5) {
		ret = set_bound_ns_from_origin(bound, year, month, day,
			hour, minute, 0, 0, is_gmt);
		goto end;
	}

	/* Try `YYYY-MM-DD` format */
	s_ret = sscanf(str, "%u-%u-%u%c", &year, &month, &day, &dummy);
	if (s_ret == 3) {
		ret = set_bound_ns_from_origin(bound, year, month, day,
			0, 0, 0, 0, is_gmt);
		goto end;
	}

	/* Try `hh:mm:ss.ns` format */
	s_ret = sscanf(str, "%u:%u:%u.%u%c", &hour, &minute, &second, &ns,
		&dummy);
	if (s_ret == 4) {
		bound->time.hour = hour;
		bound->time.minute = minute;
		bound->time.second = second;
		bound->time.ns = ns;
		goto end;
	}

	/* Try `hh:mm:ss` format */
	s_ret = sscanf(str, "%u:%u:%u%c", &hour, &minute, &second, &dummy);
	if (s_ret == 3) {
		bound->time.hour = hour;
		bound->time.minute = minute;
		bound->time.second = second;
		bound->time.ns = 0;
		goto end;
	}

	/* Try `-s.ns` format */
	s_ret = sscanf(str, "-%u.%u%c", &second, &ns, &dummy);
	if (s_ret == 2) {
		bound->ns_from_origin = -((int64_t) second) * NS_PER_S;
		bound->ns_from_origin -= (int64_t) ns;
		bound->is_set = true;
		goto end;
	}

	/* Try `s.ns` format */
	s_ret = sscanf(str, "%u.%u%c", &second, &ns, &dummy);
	if (s_ret == 2) {
		bound->ns_from_origin = ((int64_t) second) * NS_PER_S;
		bound->ns_from_origin += (int64_t) ns;
		bound->is_set = true;
		goto end;
	}

	/* Try `-s` format */
	s_ret = sscanf(str, "-%u%c", &second, &dummy);
	if (s_ret == 1) {
		bound->ns_from_origin = -((int64_t) second) * NS_PER_S;
		bound->is_set = true;
		goto end;
	}

	/* Try `s` format */
	s_ret = sscanf(str, "%u%c", &second, &dummy);
	if (s_ret == 1) {
		bound->ns_from_origin = (int64_t) second * NS_PER_S;
		bound->is_set = true;
		goto end;
	}

	BT_LOGE("Invalid date/time format: param=\"%s\"", str);
	ret = -1;

end:
	return ret;
}

/*
 * Sets a trimmer bound's properties from a parameter string/integer
 * value.
 *
 * Returns a negative value if anything goes wrong.
 */
static
int set_bound_from_param(const char *param_name, const bt_value *param,
		struct trimmer_bound *bound, bool is_gmt)
{
	int ret;
	const char *arg;
	char tmp_arg[64];

	if (bt_value_is_integer(param)) {
		int64_t value = bt_value_integer_get(param);

		/*
		 * Just convert it to a temporary string to handle
		 * everything the same way.
		 */
		sprintf(tmp_arg, "%" PRId64, value);
		arg = tmp_arg;
	} else if (bt_value_is_string(param)) {
		arg = bt_value_string_get(param);
	} else {
		BT_LOGE("`%s` parameter must be an integer or a string value.",
			param_name);
		ret = -1;
		goto end;
	}

	ret = set_bound_from_str(arg, bound, is_gmt);

end:
	return ret;
}

static
int validate_trimmer_bounds(struct trimmer_bound *begin,
		struct trimmer_bound *end)
{
	int ret = 0;

	BT_ASSERT(begin->is_set);
	BT_ASSERT(end->is_set);

	if (!begin->is_infinite && !end->is_infinite &&
			begin->ns_from_origin > end->ns_from_origin) {
		BT_LOGE("Trimming time range's beginning time is greater than end time: "
			"begin-ns-from-origin=%" PRId64 ", "
			"end-ns-from-origin=%" PRId64,
			begin->ns_from_origin,
			end->ns_from_origin);
		ret = -1;
		goto end;
	}

	if (!begin->is_infinite && begin->ns_from_origin == INT64_MIN) {
		BT_LOGE("Invalid trimming time range's beginning time: "
			"ns-from-origin=%" PRId64,
			begin->ns_from_origin);
		ret = -1;
		goto end;
	}

	if (!end->is_infinite && end->ns_from_origin == INT64_MIN) {
		BT_LOGE("Invalid trimming time range's end time: "
			"ns-from-origin=%" PRId64,
			end->ns_from_origin);
		ret = -1;
		goto end;
	}

end:
	return ret;
}

static
int init_trimmer_comp_from_params(struct trimmer_comp *trimmer_comp,
		const bt_value *params)
{
	const bt_value *value;
	int ret = 0;

	BT_ASSERT(params);
        value = bt_value_map_borrow_entry_value_const(params, "gmt");
	if (value) {
		trimmer_comp->is_gmt = (bool) bt_value_bool_get(value);
	}

        value = bt_value_map_borrow_entry_value_const(params, "begin");
	if (value) {
		if (set_bound_from_param("begin", value,
				&trimmer_comp->begin, trimmer_comp->is_gmt)) {
			/* set_bound_from_param() logs errors */
			ret = BT_SELF_COMPONENT_STATUS_ERROR;
			goto end;
		}
	} else {
		trimmer_comp->begin.is_infinite = true;
		trimmer_comp->begin.is_set = true;
	}

        value = bt_value_map_borrow_entry_value_const(params, "end");
	if (value) {
		if (set_bound_from_param("end", value,
				&trimmer_comp->end, trimmer_comp->is_gmt)) {
			/* set_bound_from_param() logs errors */
			ret = BT_SELF_COMPONENT_STATUS_ERROR;
			goto end;
		}
	} else {
		trimmer_comp->end.is_infinite = true;
		trimmer_comp->end.is_set = true;
	}

end:
	if (trimmer_comp->begin.is_set && trimmer_comp->end.is_set) {
		/* validate_trimmer_bounds() logs errors */
		ret = validate_trimmer_bounds(&trimmer_comp->begin,
			&trimmer_comp->end);
	}

	return ret;
}

bt_self_component_status trimmer_init(bt_self_component_filter *self_comp,
		const bt_value *params, void *init_data)
{
	int ret;
	bt_self_component_status status;
	struct trimmer_comp *trimmer_comp = create_trimmer_comp();

	if (!trimmer_comp) {
		status = BT_SELF_COMPONENT_STATUS_NOMEM;
		goto error;
	}

	status = bt_self_component_filter_add_input_port(
		self_comp, in_port_name, NULL, NULL);
	if (status != BT_SELF_COMPONENT_STATUS_OK) {
		goto error;
	}

	status = bt_self_component_filter_add_output_port(
		self_comp, "out", NULL, NULL);
	if (status != BT_SELF_COMPONENT_STATUS_OK) {
		goto error;
	}

	ret = init_trimmer_comp_from_params(trimmer_comp, params);
	if (ret) {
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto error;
	}

	bt_self_component_set_data(
		bt_self_component_filter_as_self_component(self_comp),
		trimmer_comp);
	goto end;

error:
	if (status == BT_SELF_COMPONENT_STATUS_OK) {
		status = BT_SELF_COMPONENT_STATUS_ERROR;
	}

	if (trimmer_comp) {
		destroy_trimmer_comp(trimmer_comp);
	}

end:
	return status;
}

static
void destroy_trimmer_iterator(struct trimmer_iterator *trimmer_it)
{
	BT_ASSERT(trimmer_it);
	bt_self_component_port_input_message_iterator_put_ref(
		trimmer_it->upstream_iter);

	if (trimmer_it->output_messages) {
		g_queue_free(trimmer_it->output_messages);
	}

	if (trimmer_it->stream_states) {
		g_hash_table_destroy(trimmer_it->stream_states);
	}

	g_free(trimmer_it);
}

static
void destroy_trimmer_iterator_stream_state(
		struct trimmer_iterator_stream_state *sstate)
{
	BT_ASSERT(sstate);
	BT_PACKET_PUT_REF_AND_RESET(sstate->cur_packet);
	BT_MESSAGE_PUT_REF_AND_RESET(sstate->stream_beginning_msg);
	g_free(sstate);
}

BT_HIDDEN
bt_self_message_iterator_status trimmer_msg_iter_init(
		bt_self_message_iterator *self_msg_iter,
		bt_self_component_filter *self_comp,
		bt_self_component_port_output *port)
{
	bt_self_message_iterator_status status =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
	struct trimmer_iterator *trimmer_it;

	trimmer_it = g_new0(struct trimmer_iterator, 1);
	if (!trimmer_it) {
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
		goto end;
	}

	trimmer_it->trimmer_comp = bt_self_component_get_data(
		bt_self_component_filter_as_self_component(self_comp));
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
	trimmer_it->upstream_iter =
		bt_self_component_port_input_message_iterator_create(
			bt_self_component_filter_borrow_input_port_by_name(
				self_comp, in_port_name));
	if (!trimmer_it->upstream_iter) {
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
		goto end;
	}

	trimmer_it->output_messages = g_queue_new();
	if (!trimmer_it->output_messages) {
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
		goto end;
	}

	trimmer_it->stream_states = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL,
		(GDestroyNotify) destroy_trimmer_iterator_stream_state);
	if (!trimmer_it->stream_states) {
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
		goto end;
	}

	trimmer_it->self_msg_iter = self_msg_iter;
	bt_self_message_iterator_set_data(self_msg_iter, trimmer_it);

end:
	if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK && trimmer_it) {
		destroy_trimmer_iterator(trimmer_it);
	}

	return status;
}

static inline
int get_msg_ns_from_origin(const bt_message *msg, int64_t *ns_from_origin,
		bool *skip)
{
	const bt_clock_class *clock_class = NULL;
	const bt_clock_snapshot *clock_snapshot = NULL;
	bt_clock_snapshot_state cs_state = BT_CLOCK_SNAPSHOT_STATE_KNOWN;
	bt_message_stream_activity_clock_snapshot_state sa_cs_state;
	int ret = 0;

	BT_ASSERT(msg);
	BT_ASSERT(ns_from_origin);
	BT_ASSERT(skip);

	switch (bt_message_get_type(msg)) {
	case BT_MESSAGE_TYPE_EVENT:
		clock_class =
			bt_message_event_borrow_stream_class_default_clock_class_const(
				msg);
		if (unlikely(!clock_class)) {
			goto error;
		}

		cs_state = bt_message_event_borrow_default_clock_snapshot_const(
			msg, &clock_snapshot);
		break;
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		clock_class =
			bt_message_packet_beginning_borrow_stream_class_default_clock_class_const(
				msg);
		if (unlikely(!clock_class)) {
			goto error;
		}

		cs_state = bt_message_packet_beginning_borrow_default_clock_snapshot_const(
			msg, &clock_snapshot);
		break;
	case BT_MESSAGE_TYPE_PACKET_END:
		clock_class =
			bt_message_packet_end_borrow_stream_class_default_clock_class_const(
				msg);
		if (unlikely(!clock_class)) {
			goto error;
		}

		cs_state = bt_message_packet_end_borrow_default_clock_snapshot_const(
			msg, &clock_snapshot);
		break;
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
		clock_class =
			bt_message_discarded_events_borrow_stream_class_default_clock_class_const(
				msg);
		if (unlikely(!clock_class)) {
			goto error;
		}

		cs_state = bt_message_discarded_events_borrow_default_beginning_clock_snapshot_const(
			msg, &clock_snapshot);
		break;
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
		clock_class =
			bt_message_discarded_packets_borrow_stream_class_default_clock_class_const(
				msg);
		if (unlikely(!clock_class)) {
			goto error;
		}

		cs_state = bt_message_discarded_packets_borrow_default_beginning_clock_snapshot_const(
			msg, &clock_snapshot);
		break;
	case BT_MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING:
		clock_class =
			bt_message_stream_activity_beginning_borrow_stream_class_default_clock_class_const(
				msg);
		if (unlikely(!clock_class)) {
			goto error;
		}

		sa_cs_state = bt_message_stream_activity_beginning_borrow_default_clock_snapshot_const(
			msg, &clock_snapshot);
		if (sa_cs_state == BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_UNKNOWN ||
				sa_cs_state == BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_INFINITE) {
			/* Lowest possible time to always include them */
			*ns_from_origin = INT64_MIN;
			goto no_clock_snapshot;
		}

		break;
	case BT_MESSAGE_TYPE_STREAM_ACTIVITY_END:
		clock_class =
			bt_message_stream_activity_end_borrow_stream_class_default_clock_class_const(
				msg);
		if (unlikely(!clock_class)) {
			goto error;
		}

		sa_cs_state = bt_message_stream_activity_end_borrow_default_clock_snapshot_const(
			msg, &clock_snapshot);
		if (sa_cs_state == BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_UNKNOWN) {
			/* Lowest time to always include it */
			*ns_from_origin = INT64_MIN;
			goto no_clock_snapshot;
		} else if (sa_cs_state == BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_INFINITE) {
			/* Greatest time to always exclude it */
			*ns_from_origin = INT64_MAX;
			goto no_clock_snapshot;
		}

		break;
	case BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY:
		cs_state =
			bt_message_message_iterator_inactivity_borrow_default_clock_snapshot_const(
				msg, &clock_snapshot);
		break;
	default:
		goto no_clock_snapshot;
	}

	if (unlikely(cs_state != BT_CLOCK_SNAPSHOT_STATE_KNOWN)) {
		BT_LOGE_STR("Unsupported unknown clock snapshot.");
		ret = -1;
		goto end;
	}

	ret = bt_clock_snapshot_get_ns_from_origin(clock_snapshot,
		ns_from_origin);
	if (unlikely(ret)) {
		goto error;
	}

	goto end;

no_clock_snapshot:
	*skip = true;
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
int set_trimmer_iterator_bound(struct trimmer_bound *bound,
		int64_t ns_from_origin, bool is_gmt)
{
	struct tm tm;
	time_t time_seconds = (time_t) (ns_from_origin / NS_PER_S);
	int ret = 0;

	BT_ASSERT(!bound->is_set);
	errno = 0;

	/* We only need to extract the date from this time */
	if (is_gmt) {
		bt_gmtime_r(&time_seconds, &tm);
	} else {
		bt_localtime_r(&time_seconds, &tm);
	}

	if (errno) {
		BT_LOGE_ERRNO("Cannot convert timestamp to date and time",
			"ts=%" PRId64, (int64_t) time_seconds);
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
bt_self_message_iterator_status state_set_trimmer_iterator_bounds(
		struct trimmer_iterator *trimmer_it)
{
	bt_message_iterator_status upstream_iter_status =
		BT_MESSAGE_ITERATOR_STATUS_OK;
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
			bt_self_component_port_input_message_iterator_next(
				trimmer_it->upstream_iter, &msgs, &count);
		if (upstream_iter_status != BT_MESSAGE_ITERATOR_STATUS_OK) {
			goto end;
		}

		for (i = 0; i < count; i++) {
			const bt_message *msg = msgs[i];
			bool skip = false;
			int ret;

			ret = get_msg_ns_from_origin(msg, &ns_from_origin,
				&skip);
			if (ret) {
				goto error;
			}

			if (skip) {
				continue;
			}

			BT_ASSERT(ns_from_origin != INT64_MIN &&
				ns_from_origin != INT64_MAX);
			put_messages(msgs, count);
			goto found;
		}

		put_messages(msgs, count);
	}

found:
	if (!trimmer_it->begin.is_set) {
		BT_ASSERT(!trimmer_it->begin.is_infinite);
		ret = set_trimmer_iterator_bound(&trimmer_it->begin,
			ns_from_origin, trimmer_comp->is_gmt);
		if (ret) {
			goto error;
		}
	}

	if (!trimmer_it->end.is_set) {
		BT_ASSERT(!trimmer_it->end.is_infinite);
		ret = set_trimmer_iterator_bound(&trimmer_it->end,
			ns_from_origin, trimmer_comp->is_gmt);
		if (ret) {
			goto error;
		}
	}

	ret = validate_trimmer_bounds(&trimmer_it->begin,
		&trimmer_it->end);
	if (ret) {
		goto error;
	}

	goto end;

error:
	put_messages(msgs, count);
	upstream_iter_status = BT_MESSAGE_ITERATOR_STATUS_ERROR;

end:
	return (int) upstream_iter_status;
}

static
bt_self_message_iterator_status state_seek_initially(
		struct trimmer_iterator *trimmer_it)
{
	bt_self_message_iterator_status status =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;

	BT_ASSERT(trimmer_it->begin.is_set);

	if (trimmer_it->begin.is_infinite) {
		if (!bt_self_component_port_input_message_iterator_can_seek_beginning(
				trimmer_it->upstream_iter)) {
			BT_LOGE_STR("Cannot make upstream message iterator initially seek its beginning.");
			status = BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
			goto end;
		}

		status = (int) bt_self_component_port_input_message_iterator_seek_beginning(
			trimmer_it->upstream_iter);
	} else {
		if (!bt_self_component_port_input_message_iterator_can_seek_ns_from_origin(
				trimmer_it->upstream_iter,
				trimmer_it->begin.ns_from_origin)) {
			BT_LOGE("Cannot make upstream message iterator initially seek: "
				"seek-ns-from-origin=%" PRId64,
				trimmer_it->begin.ns_from_origin);
			status = BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
			goto end;
		}

		status = (int) bt_self_component_port_input_message_iterator_seek_ns_from_origin(
			trimmer_it->upstream_iter, trimmer_it->begin.ns_from_origin);
	}

	if (status == BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
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
bt_self_message_iterator_status end_stream(struct trimmer_iterator *trimmer_it,
		struct trimmer_iterator_stream_state *sstate)
{
	bt_self_message_iterator_status status =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
	uint64_t raw_value;
	const bt_clock_class *clock_class;
	int ret;
	bt_message *msg = NULL;

	BT_ASSERT(!trimmer_it->end.is_infinite);

	if (!sstate->stream) {
		goto end;
	}

	if (sstate->cur_packet) {
		/*
		 * The last message could not have been a stream
		 * activity end message if we have a current packet.
		 */
		BT_ASSERT(!sstate->last_msg_is_stream_activity_end);

		/*
		 * Create and push a packet end message, making its time
		 * the trimming range's end time.
		 */
		clock_class = bt_stream_class_borrow_default_clock_class_const(
			bt_stream_borrow_class_const(sstate->stream));
		BT_ASSERT(clock_class);
		ret = clock_raw_value_from_ns_from_origin(clock_class,
			trimmer_it->end.ns_from_origin, &raw_value);
		if (ret) {
			status = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
			goto end;
		}

		msg = bt_message_packet_end_create_with_default_clock_snapshot(
			trimmer_it->self_msg_iter, sstate->cur_packet,
			raw_value);
		if (!msg) {
			status = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
			goto end;
		}

		push_message(trimmer_it, msg);
		msg = NULL;
		BT_PACKET_PUT_REF_AND_RESET(sstate->cur_packet);

		/*
		 * Because we generated a packet end message, set the
		 * stream activity end message's time to use to the
		 * trimming range's end time (this packet end message's
		 * time).
		 */
		sstate->stream_act_end_ns_from_origin =
			trimmer_it->end.ns_from_origin;
	}

	if (!sstate->last_msg_is_stream_activity_end) {
		/* Create and push a stream activity end message */
		msg = bt_message_stream_activity_end_create(
			trimmer_it->self_msg_iter, sstate->stream);
		if (!msg) {
			status = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
			goto end;
		}

		clock_class = bt_stream_class_borrow_default_clock_class_const(
			bt_stream_borrow_class_const(sstate->stream));
		BT_ASSERT(clock_class);
		BT_ASSERT(sstate->stream_act_end_ns_from_origin != INT64_MIN);
		ret = clock_raw_value_from_ns_from_origin(clock_class,
			sstate->stream_act_end_ns_from_origin, &raw_value);
		if (ret) {
			status = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
			goto end;
		}

		bt_message_stream_activity_end_set_default_clock_snapshot(
			msg, raw_value);
		push_message(trimmer_it, msg);
		msg = NULL;
	}

	/* Create and push a stream end message */
	msg = bt_message_stream_end_create(trimmer_it->self_msg_iter,
		sstate->stream);
	if (!msg) {
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
		goto end;
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
bt_self_message_iterator_status end_iterator_streams(
		struct trimmer_iterator *trimmer_it)
{
	bt_self_message_iterator_status status =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
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

static inline
bt_self_message_iterator_status create_stream_beginning_activity_message(
		struct trimmer_iterator *trimmer_it,
		const bt_stream *stream,
		const bt_clock_class *clock_class, bt_message **msg)
{
	bt_self_message_iterator_status status =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;

	BT_ASSERT(msg);
	BT_ASSERT(!trimmer_it->begin.is_infinite);

	*msg = bt_message_stream_activity_beginning_create(
		trimmer_it->self_msg_iter, stream);
	if (!*msg) {
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
		goto end;
	}

	if (clock_class) {
		int ret;
		uint64_t raw_value;

		ret = clock_raw_value_from_ns_from_origin(clock_class,
			trimmer_it->begin.ns_from_origin, &raw_value);
		if (ret) {
			status = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
			bt_message_put_ref(*msg);
			goto end;
		}

		bt_message_stream_activity_beginning_set_default_clock_snapshot(
			*msg, raw_value);
	}

end:
	return status;
}

/*
 * Makes sure to initialize a stream state, pushing the appropriate
 * initial messages.
 *
 * `stream_act_beginning_msg` is an initial stream activity beginning
 * message to potentially use, depending on its clock snapshot state.
 * This function consumes `stream_act_beginning_msg` unconditionally.
 */
static inline
bt_self_message_iterator_status ensure_stream_state_is_inited(
		struct trimmer_iterator *trimmer_it,
		struct trimmer_iterator_stream_state *sstate,
		const bt_message *stream_act_beginning_msg)
{
	bt_self_message_iterator_status status =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
	bt_message *new_msg = NULL;
	const bt_clock_class *clock_class =
		bt_stream_class_borrow_default_clock_class_const(
			bt_stream_borrow_class_const(sstate->stream));

	BT_ASSERT(!sstate->inited);

	if (!sstate->stream_beginning_msg) {
		/* No initial stream beginning message: create one */
		sstate->stream_beginning_msg =
			bt_message_stream_beginning_create(
				trimmer_it->self_msg_iter, sstate->stream);
		if (!sstate->stream_beginning_msg) {
			status = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
			goto end;
		}
	}

	/* Push initial stream beginning message */
	BT_ASSERT(sstate->stream_beginning_msg);
	push_message(trimmer_it, sstate->stream_beginning_msg);
	sstate->stream_beginning_msg = NULL;

	if (stream_act_beginning_msg) {
		/*
		 * Initial stream activity beginning message exists: if
		 * its time is -inf, then create and push a new one
		 * having the trimming range's beginning time. Otherwise
		 * push it as is (known and unknown).
		 */
		const bt_clock_snapshot *cs;
		bt_message_stream_activity_clock_snapshot_state sa_cs_state;

		sa_cs_state = bt_message_stream_activity_beginning_borrow_default_clock_snapshot_const(
			stream_act_beginning_msg, &cs);
		if (sa_cs_state == BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_INFINITE &&
				!trimmer_it->begin.is_infinite) {
			/*
			 * -inf time: use trimming range's beginning
			 * time (which is not -inf).
			 */
			status = create_stream_beginning_activity_message(
				trimmer_it, sstate->stream, clock_class,
				&new_msg);
			if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
				goto end;
			}

			push_message(trimmer_it, new_msg);
			new_msg = NULL;
		} else {
			/* Known/unknown: push as is */
			push_message(trimmer_it, stream_act_beginning_msg);
			stream_act_beginning_msg = NULL;
		}
	} else {
		BT_ASSERT(!trimmer_it->begin.is_infinite);

		/*
		 * No stream beginning activity message: create and push
		 * a new message.
	 	 */
		status = create_stream_beginning_activity_message(
			trimmer_it, sstate->stream, clock_class, &new_msg);
		if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
			goto end;
		}

		push_message(trimmer_it, new_msg);
		new_msg = NULL;
	}

	sstate->inited = true;

end:
	bt_message_put_ref(new_msg);
	bt_message_put_ref(stream_act_beginning_msg);
	return status;
}

static inline
bt_self_message_iterator_status ensure_cur_packet_exists(
	struct trimmer_iterator *trimmer_it,
	struct trimmer_iterator_stream_state *sstate,
	const bt_packet *packet)
{
	bt_self_message_iterator_status status =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
	int ret;
	const bt_clock_class *clock_class =
		bt_stream_class_borrow_default_clock_class_const(
			bt_stream_borrow_class_const(sstate->stream));
	bt_message *msg = NULL;
	uint64_t raw_value;

	BT_ASSERT(!trimmer_it->begin.is_infinite);
	BT_ASSERT(!sstate->cur_packet);

	/*
	 * Create and push an initial packet beginning message,
	 * making its time the trimming range's beginning time.
	 */
	ret = clock_raw_value_from_ns_from_origin(clock_class,
		trimmer_it->begin.ns_from_origin, &raw_value);
	if (ret) {
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
		goto end;
	}

	msg = bt_message_packet_beginning_create_with_default_clock_snapshot(
		trimmer_it->self_msg_iter, packet, raw_value);
	if (!msg) {
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
		goto end;
	}

	push_message(trimmer_it, msg);
	msg = NULL;

	/* Set packet as this stream's current packet */
	sstate->cur_packet = packet;
	bt_packet_get_ref(sstate->cur_packet);

end:
	bt_message_put_ref(msg);
	return status;
}

/*
 * Handles a message which is associated to a given stream state. This
 * _could_ make the iterator's output message queue grow; this could
 * also consume the message without pushing anything to this queue, only
 * modifying the stream state.
 *
 * This function consumes the `msg` reference, _whatever the outcome_.
 *
 * `ns_from_origin` is the message's time, as given by
 * get_msg_ns_from_origin().
 *
 * This function sets `reached_end` if handling this message made the
 * iterator reach the end of the trimming range. Note that the output
 * message queue could contain messages even if this function sets
 * `reached_end`.
 */
static inline
bt_self_message_iterator_status handle_message_with_stream_state(
		struct trimmer_iterator *trimmer_it, const bt_message *msg,
		struct trimmer_iterator_stream_state *sstate,
		int64_t ns_from_origin, bool *reached_end)
{
	bt_self_message_iterator_status status =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
	bt_message_type msg_type = bt_message_get_type(msg);
	int ret;

	switch (msg_type) {
	case BT_MESSAGE_TYPE_EVENT:
		if (unlikely(!trimmer_it->end.is_infinite &&
				ns_from_origin > trimmer_it->end.ns_from_origin)) {
			status = end_iterator_streams(trimmer_it);
			*reached_end = true;
			break;
		}

		if (unlikely(!sstate->inited)) {
			status = ensure_stream_state_is_inited(trimmer_it,
				sstate, NULL);
			if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
				goto end;
			}
		}

		if (unlikely(!sstate->cur_packet)) {
			const bt_event *event =
				bt_message_event_borrow_event_const(msg);
			const bt_packet *packet = bt_event_borrow_packet_const(
				event);

			status = ensure_cur_packet_exists(trimmer_it, sstate,
				packet);
			if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
				goto end;
			}
		}

		BT_ASSERT(sstate->cur_packet);
		push_message(trimmer_it, msg);
		msg = NULL;
		break;
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		if (unlikely(!trimmer_it->end.is_infinite &&
				ns_from_origin > trimmer_it->end.ns_from_origin)) {
			status = end_iterator_streams(trimmer_it);
			*reached_end = true;
			break;
		}

		if (unlikely(!sstate->inited)) {
			status = ensure_stream_state_is_inited(trimmer_it,
				sstate, NULL);
			if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
				goto end;
			}
		}

		BT_ASSERT(!sstate->cur_packet);
		sstate->cur_packet =
			bt_message_packet_beginning_borrow_packet_const(msg);
		bt_packet_get_ref(sstate->cur_packet);
		push_message(trimmer_it, msg);
		msg = NULL;
		break;
	case BT_MESSAGE_TYPE_PACKET_END:
		sstate->stream_act_end_ns_from_origin = ns_from_origin;

		if (unlikely(!trimmer_it->end.is_infinite &&
				ns_from_origin > trimmer_it->end.ns_from_origin)) {
			status = end_iterator_streams(trimmer_it);
			*reached_end = true;
			break;
		}

		if (unlikely(!sstate->inited)) {
			status = ensure_stream_state_is_inited(trimmer_it,
				sstate, NULL);
			if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
				goto end;
			}
		}

		if (unlikely(!sstate->cur_packet)) {
			const bt_packet *packet =
				bt_message_packet_end_borrow_packet_const(msg);

			status = ensure_cur_packet_exists(trimmer_it, sstate,
				packet);
			if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
				goto end;
			}
		}

		BT_ASSERT(sstate->cur_packet);
		BT_PACKET_PUT_REF_AND_RESET(sstate->cur_packet);
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

		if (bt_message_get_type(msg) ==
				BT_MESSAGE_TYPE_DISCARDED_EVENTS) {
			/*
			 * Safe to ignore the return value because we
			 * know there's a default clock and it's always
			 * known.
			 */
			(void) bt_message_discarded_events_borrow_default_end_clock_snapshot_const(
				msg, &end_cs);
		} else {
			/*
			 * Safe to ignore the return value because we
			 * know there's a default clock and it's always
			 * known.
			 */
			(void) bt_message_discarded_packets_borrow_default_end_clock_snapshot_const(
				msg, &end_cs);
		}

		if (bt_clock_snapshot_get_ns_from_origin(end_cs,
				&end_ns_from_origin)) {
			status = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
			goto end;
		}

		sstate->stream_act_end_ns_from_origin = end_ns_from_origin;

		if (!trimmer_it->end.is_infinite &&
				ns_from_origin > trimmer_it->end.ns_from_origin) {
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
				status = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
				goto end;
			}

			if (msg_type == BT_MESSAGE_TYPE_DISCARDED_EVENTS) {
				(void) bt_message_discarded_events_borrow_default_beginning_clock_snapshot_const(
					msg, &begin_cs);
				new_msg = bt_message_discarded_events_create_with_default_clock_snapshots(
					trimmer_it->self_msg_iter,
					sstate->stream,
					bt_clock_snapshot_get_value(begin_cs),
					end_raw_value);
			} else {
				(void) bt_message_discarded_packets_borrow_default_beginning_clock_snapshot_const(
					msg, &begin_cs);
				new_msg = bt_message_discarded_packets_create_with_default_clock_snapshots(
					trimmer_it->self_msg_iter,
					sstate->stream,
					bt_clock_snapshot_get_value(begin_cs),
					end_raw_value);
			}

			if (!new_msg) {
				status = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
				goto end;
			}

			/* Replace the original message */
			BT_MESSAGE_MOVE_REF(msg, new_msg);
		}

		if (unlikely(!sstate->inited)) {
			status = ensure_stream_state_is_inited(trimmer_it,
				sstate, NULL);
			if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
				goto end;
			}
		}

		push_message(trimmer_it, msg);
		msg = NULL;
		break;
	}
	case BT_MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING:
		if (!trimmer_it->end.is_infinite &&
				ns_from_origin > trimmer_it->end.ns_from_origin) {
			/*
			 * This only happens when the message's time is
			 * known and is greater than the trimming
			 * range's end time. Unknown and -inf times are
			 * always less than
			 * `trimmer_it->end.ns_from_origin`.
			 */
			status = end_iterator_streams(trimmer_it);
			*reached_end = true;
			break;
		}

		if (!sstate->inited) {
			status = ensure_stream_state_is_inited(trimmer_it,
				sstate, msg);
			msg = NULL;
			if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
				goto end;
			}
		} else {
			push_message(trimmer_it, msg);
			msg = NULL;
		}

		break;
	case BT_MESSAGE_TYPE_STREAM_ACTIVITY_END:
		if (trimmer_it->end.is_infinite) {
			push_message(trimmer_it, msg);
			msg = NULL;
			break;
		}

		if (ns_from_origin == INT64_MIN) {
			/* Unknown: push as is if stream state is inited */
			if (sstate->inited) {
				push_message(trimmer_it, msg);
				msg = NULL;
				sstate->last_msg_is_stream_activity_end = true;
			}
		} else if (ns_from_origin == INT64_MAX) {
			/* Infinite: use trimming range's end time */
			sstate->stream_act_end_ns_from_origin =
				trimmer_it->end.ns_from_origin;
		} else {
			/* Known: check if outside of trimming range */
			if (ns_from_origin > trimmer_it->end.ns_from_origin) {
				sstate->stream_act_end_ns_from_origin =
					trimmer_it->end.ns_from_origin;
				status = end_iterator_streams(trimmer_it);
				*reached_end = true;
				break;
			}

			if (!sstate->inited) {
				/*
				 * First message for this stream is a
				 * stream activity end: we can't deduce
				 * anything about the stream activity
				 * beginning's time, and using this
				 * message's time would make a useless
				 * pair of stream activity beginning/end
				 * with the same time. Just skip this
				 * message and wait for something
				 * useful.
				 */
				break;
			}

			push_message(trimmer_it, msg);
			msg = NULL;
			sstate->last_msg_is_stream_activity_end = true;
			sstate->stream_act_end_ns_from_origin = ns_from_origin;
		}

		break;
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
		/*
		 * We don't know what follows at this point, so just
		 * keep this message until we know what to do with it
		 * (it will be used in ensure_stream_state_is_inited()).
		 */
		BT_ASSERT(!sstate->inited);
		BT_MESSAGE_MOVE_REF(sstate->stream_beginning_msg, msg);
		break;
	case BT_MESSAGE_TYPE_STREAM_END:
		if (sstate->inited) {
			/*
			 * This is the end of an inited stream: end this
			 * stream if its stream activity end message
			 * time is not the trimming range's end time
			 * (which means the final stream activity end
			 * message had an infinite time). end_stream()
			 * will generate its own stream end message.
			 */
			if (trimmer_it->end.is_infinite) {
				push_message(trimmer_it, msg);
				msg = NULL;
				g_hash_table_remove(trimmer_it->stream_states,
					sstate->stream);
			} else if (sstate->stream_act_end_ns_from_origin <
					trimmer_it->end.ns_from_origin) {
				status = end_stream(trimmer_it, sstate);
				if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
					goto end;
				}

				/* We won't need this stream state again */
				g_hash_table_remove(trimmer_it->stream_states,
					sstate->stream);
			}
		} else {
			/* We dont't need this stream state anymore */
			g_hash_table_remove(trimmer_it->stream_states, sstate->stream);
		}

		break;
	default:
		break;
	}

end:
	/* We release the message's reference whatever the outcome */
	bt_message_put_ref(msg);
	return BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
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
bt_self_message_iterator_status handle_message(
		struct trimmer_iterator *trimmer_it, const bt_message *msg,
		bool *reached_end)
{
	bt_self_message_iterator_status status;
	const bt_stream *stream = NULL;
	int64_t ns_from_origin = INT64_MIN;
	bool skip;
	int ret;
	struct trimmer_iterator_stream_state *sstate = NULL;

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
	case BT_MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING:
		stream = bt_message_stream_activity_beginning_borrow_stream_const(msg);
		break;
	case BT_MESSAGE_TYPE_STREAM_ACTIVITY_END:
		stream = bt_message_stream_activity_end_borrow_stream_const(msg);
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

	if (likely(stream)) {
		/* Find stream state */
		sstate = g_hash_table_lookup(trimmer_it->stream_states,
			stream);
		if (unlikely(!sstate)) {
			/* No stream state yet: create one now */
			const bt_stream_class *sc;

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
				BT_LOGE("Unsupported stream: stream class does "
					"not have a default clock class: "
					"stream-addr=%p, "
					"stream-id=%" PRIu64 ", "
					"stream-name=\"%s\"",
					stream, bt_stream_get_id(stream),
					bt_stream_get_name(stream));
				status = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
				goto end;
			}

			if (!bt_stream_class_default_clock_is_always_known(sc)) {
				BT_LOGE("Unsupported stream: clock does not "
					"always have a known value: "
					"stream-addr=%p, "
					"stream-id=%" PRIu64 ", "
					"stream-name=\"%s\"",
					stream, bt_stream_get_id(stream),
					bt_stream_get_name(stream));
				status = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
				goto end;
			}

			sstate = g_new0(struct trimmer_iterator_stream_state,
				1);
			if (!sstate) {
				status = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
				goto end;
			}

			sstate->stream = stream;
			sstate->stream_act_end_ns_from_origin = INT64_MIN;
			g_hash_table_insert(trimmer_it->stream_states,
				(void *) stream, sstate);
		}
	}

	/* Retrieve the message's time */
	ret = get_msg_ns_from_origin(msg, &ns_from_origin, &skip);
	if (unlikely(ret)) {
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
		goto end;
	}

	if (likely(sstate)) {
		/* Message associated to a stream */
		status = handle_message_with_stream_state(trimmer_it, msg,
			sstate, ns_from_origin, reached_end);

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
		if (unlikely(ns_from_origin > trimmer_it->end.ns_from_origin)) {
			BT_MESSAGE_PUT_REF_AND_RESET(msg);
			status = end_iterator_streams(trimmer_it);
			*reached_end = true;
		} else {
			push_message(trimmer_it, msg);
			status = BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
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

	BT_ASSERT(*count > 0);
}

static inline
bt_self_message_iterator_status state_ending(
		struct trimmer_iterator *trimmer_it,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	bt_self_message_iterator_status status =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;

	if (g_queue_is_empty(trimmer_it->output_messages)) {
		trimmer_it->state = TRIMMER_ITERATOR_STATE_ENDED;
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_END;
		goto end;
	}

	fill_message_array_from_output_messages(trimmer_it, msgs,
		capacity, count);

end:
	return status;
}

static inline
bt_self_message_iterator_status state_trim(struct trimmer_iterator *trimmer_it,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	bt_self_message_iterator_status status =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
	bt_message_array_const my_msgs;
	uint64_t my_count;
	uint64_t i;
	bool reached_end = false;

	while (g_queue_is_empty(trimmer_it->output_messages)) {
		status = (int) bt_self_component_port_input_message_iterator_next(
			trimmer_it->upstream_iter, &my_msgs, &my_count);
		if (unlikely(status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK)) {
			if (status == BT_SELF_MESSAGE_ITERATOR_STATUS_END) {
				status = end_iterator_streams(trimmer_it);
				if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
					goto end;
				}

				trimmer_it->state =
					TRIMMER_ITERATOR_STATE_ENDING;
				status = state_ending(trimmer_it, msgs,
					capacity, count);
			}

			goto end;
		}

		BT_ASSERT(my_count > 0);

		for (i = 0; i < my_count; i++) {
			status = handle_message(trimmer_it, my_msgs[i],
				&reached_end);

			/*
			 * handle_message() unconditionally consumes the
			 * message reference.
			 */
			my_msgs[i] = NULL;

			if (unlikely(status !=
					BT_SELF_MESSAGE_ITERATOR_STATUS_OK)) {
				put_messages(my_msgs, my_count);
				goto end;
			}

			if (unlikely(reached_end)) {
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
	BT_ASSERT(!g_queue_is_empty(trimmer_it->output_messages));
	fill_message_array_from_output_messages(trimmer_it, msgs,
		capacity, count);

end:
	return status;
}

BT_HIDDEN
bt_self_message_iterator_status trimmer_msg_iter_next(
		bt_self_message_iterator *self_msg_iter,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	struct trimmer_iterator *trimmer_it =
		bt_self_message_iterator_get_data(self_msg_iter);
	bt_self_message_iterator_status status =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;

	BT_ASSERT(trimmer_it);

	if (likely(trimmer_it->state == TRIMMER_ITERATOR_STATE_TRIM)) {
		status = state_trim(trimmer_it, msgs, capacity, count);
		if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
			goto end;
		}
	} else {
		switch (trimmer_it->state) {
		case TRIMMER_ITERATOR_STATE_SET_BOUNDS_NS_FROM_ORIGIN:
			status = state_set_trimmer_iterator_bounds(trimmer_it);
			if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
				goto end;
			}

			status = state_seek_initially(trimmer_it);
			if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
				goto end;
			}

			status = state_trim(trimmer_it, msgs, capacity, count);
			if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
				goto end;
			}

			break;
		case TRIMMER_ITERATOR_STATE_SEEK_INITIALLY:
			status = state_seek_initially(trimmer_it);
			if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
				goto end;
			}

			status = state_trim(trimmer_it, msgs, capacity, count);
			if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
				goto end;
			}

			break;
		case TRIMMER_ITERATOR_STATE_ENDING:
			status = state_ending(trimmer_it, msgs, capacity,
				count);
			if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
				goto end;
			}

			break;
		case TRIMMER_ITERATOR_STATE_ENDED:
			status = BT_SELF_MESSAGE_ITERATOR_STATUS_END;
			break;
		default:
			abort();
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
