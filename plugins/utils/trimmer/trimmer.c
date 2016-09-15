/*
 * trimmer.c
 *
 * Babeltrace Trace Trimmer
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

#include <babeltrace/compat/utc-internal.h>
#include <babeltrace/plugin/plugin-dev.h>
#include <babeltrace/graph/component.h>
#include <babeltrace/graph/private-component.h>
#include <babeltrace/graph/component-filter.h>
#include <babeltrace/graph/notification.h>
#include <babeltrace/graph/notification-iterator.h>
#include <babeltrace/graph/private-notification-iterator.h>
#include <babeltrace/graph/private-component-filter.h>
#include <babeltrace/graph/notification-event.h>
#include <plugins-common.h>
#include "trimmer.h"
#include "iterator.h"
#include <assert.h>

static
void destroy_trimmer_data(struct trimmer *trimmer)
{
	g_free(trimmer);
}

static
struct trimmer *create_trimmer_data(void)
{
	struct trimmer *trimmer;

	trimmer = g_new0(struct trimmer, 1);
	if (!trimmer) {
		goto end;
	}
end:
	return trimmer;
}

void finalize_trimmer(struct bt_private_component *component)
{
	void *data = bt_private_component_get_user_data(component);

	destroy_trimmer_data(data);
}

/*
 * Parses a timestamp, figuring out its format.
 *
 * Returns a negative value if anything goes wrong.
 *
 * Expected formats:
 *
 *   YYYY-MM-DD hh:mm:ss.ns
 *   hh:mm:ss.ns
 *   -ss.ns
 *   ss.ns
 *   YYYY-MM-DD hh:mm:ss
 *   hh:mm:ss
 *   -ss
 *   ss
 */
static
int timestamp_from_arg(const char *arg, struct trimmer *trimmer,
		struct trimmer_bound *result_bound, bt_bool gmt)
{
	int ret;
	int64_t value;
	unsigned int year, month, day, hh, mm, ss, ns;

	/* YYYY-MM-DD hh:mm:ss.ns */
	ret = sscanf(arg, "%u-%u-%u %u:%u:%u.%u",
		&year, &month, &day, &hh, &mm, &ss, &ns);
	if (ret == 7) {
		struct tm tm = {
			.tm_sec = ss,
			.tm_min = mm,
			.tm_hour = hh,
			.tm_mday = day,
			.tm_mon = month - 1,
			.tm_year = year - 1900,
			.tm_isdst = -1,
		};
		time_t result;

		if (gmt) {
			result = bt_timegm(&tm);
			if (result < 0) {
				return -1;
			}
		} else {
			result = mktime(&tm);
			if (result < 0) {
				return -1;
			}
		}
		value = (int64_t) result;
		value *= NSEC_PER_SEC;
		value += ns;
		if (!trimmer->date) {
			trimmer->year = year;
			trimmer->month = month;
			trimmer->day = day;
			trimmer->date = true;
		}
		goto set;
	}
	/* hh:mm:ss.ns */
	ret = sscanf(arg, "%u:%u:%u.%u",
		&hh, &mm, &ss, &ns);
	if (ret == 4) {
		if (!trimmer->date) {
			/* We don't know which day until we get an event. */
			result_bound->lazy_values.hh = hh;
			result_bound->lazy_values.mm = mm;
			result_bound->lazy_values.ss = ss;
			result_bound->lazy_values.ns = ns;
			result_bound->lazy_values.gmt = gmt;
			goto lazy;
		} else {
			struct tm tm = {
				.tm_sec = ss,
				.tm_min = mm,
				.tm_hour = hh,
				.tm_mday = trimmer->day,
				.tm_mon = trimmer->month - 1,
				.tm_year = trimmer->year - 1900,
				.tm_isdst = -1,
			};
			time_t result;

			if (gmt) {
				result = bt_timegm(&tm);
				if (result < 0) {
					return -1;
				}
			} else {
				result = mktime(&tm);
				if (result < 0) {
					return -1;
				}
			}
			value = (int64_t) result;
			value *= NSEC_PER_SEC;
			value += ns;
			goto set;
		}
	}
	/* -ss.ns */
	ret = sscanf(arg, "-%u.%u",
		&ss, &ns);
	if (ret == 2) {
		value = -ss * NSEC_PER_SEC;
		value -= ns;
		goto set;
	}
	/* ss.ns */
	ret = sscanf(arg, "%u.%u",
		&ss, &ns);
	if (ret == 2) {
		value = ss * NSEC_PER_SEC;
		value += ns;
		goto set;
	}

	/* YYYY-MM-DD hh:mm:ss */
	ret = sscanf(arg, "%u-%u-%u %u:%u:%u",
		&year, &month, &day, &hh, &mm, &ss);
	if (ret == 6) {
		struct tm tm = {
			.tm_sec = ss,
			.tm_min = mm,
			.tm_hour = hh,
			.tm_mday = day,
			.tm_mon = month - 1,
			.tm_year = year - 1900,
			.tm_isdst = -1,
		};

		if (gmt) {
			value = bt_timegm(&tm);
			if (value < 0) {
				return -1;
			}
		} else {
			value = mktime(&tm);
			if (value < 0) {
				return -1;
			}
		}
		value *= NSEC_PER_SEC;
		if (!trimmer->date) {
			trimmer->year = year;
			trimmer->month = month;
			trimmer->day = day;
			trimmer->date = true;
		}
		goto set;
	}
	/* hh:mm:ss */
	ret = sscanf(arg, "%u:%u:%u",
		&hh, &mm, &ss);
	if (ret == 3) {
		if (!trimmer->date) {
			/* We don't know which day until we get an event. */
			result_bound->lazy_values.hh = hh;
			result_bound->lazy_values.mm = mm;
			result_bound->lazy_values.ss = ss;
			result_bound->lazy_values.ns = 0;
			result_bound->lazy_values.gmt = gmt;
			goto lazy;
		} else {
			struct tm tm = {
				.tm_sec = ss,
				.tm_min = mm,
				.tm_hour = hh,
				.tm_mday = trimmer->day,
				.tm_mon = trimmer->month - 1,
				.tm_year = trimmer->year - 1900,
				.tm_isdst = -1,
			};
			time_t result;

			if (gmt) {
				result = bt_timegm(&tm);
				if (result < 0) {
					return -1;
				}
			} else {
				result = mktime(&tm);
				if (result < 0) {
					return -1;
				}
			}
			value = (int64_t) result;
			value *= NSEC_PER_SEC;
			goto set;
		}
	}
	/* -ss */
	ret = sscanf(arg, "-%u",
		&ss);
	if (ret == 1) {
		value = -ss * NSEC_PER_SEC;
		goto set;
	}
	/* ss */
	ret = sscanf(arg, "%u",
		&ss);
	if (ret == 1) {
		value = ss * NSEC_PER_SEC;
		goto set;
	}

	/* Not found. */
	return -1;

set:
	result_bound->value = value;
	result_bound->set = true;
	return 0;

lazy:
	result_bound->lazy = true;
	return 0;
}

static
enum bt_component_status init_from_params(struct trimmer *trimmer,
		struct bt_value *params)
{
	struct bt_value *value = NULL;
	bt_bool gmt = BT_FALSE;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	assert(params);

        value = bt_value_map_get(params, "clock-gmt");
	if (value) {
		enum bt_value_status value_ret;

		value_ret = bt_value_bool_get(value, &gmt);
		if (value_ret) {
			ret = BT_COMPONENT_STATUS_INVALID;
			printf_error("Failed to retrieve clock-gmt value. Expecting a boolean");
		}
	}
	bt_put(value);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

        value = bt_value_map_get(params, "begin");
	if (value) {
		enum bt_value_status value_ret;
		const char *str;

		value_ret = bt_value_string_get(value, &str);
		if (value_ret || timestamp_from_arg(str,
				trimmer, &trimmer->begin, gmt)) {
			ret = BT_COMPONENT_STATUS_INVALID;
			printf_error("Failed to retrieve begin value. Expecting a timestamp string");
		}
	}
	bt_put(value);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

        value = bt_value_map_get(params, "end");
	if (value) {
		enum bt_value_status value_ret;
		const char *str;

		value_ret = bt_value_string_get(value, &str);
		if (value_ret || timestamp_from_arg(str,
				trimmer, &trimmer->end, gmt)) {
			ret = BT_COMPONENT_STATUS_INVALID;
			printf_error("Failed to retrieve end value. Expecting a timestamp string");
		}
	}
	bt_put(value);
end:
	if (trimmer->begin.set && trimmer->end.set) {
		if (trimmer->begin.value > trimmer->end.value) {
			printf_error("Unexpected: time range begin value is above end value");
			ret = BT_COMPONENT_STATUS_INVALID;
		}
	}
	return ret;
}

enum bt_component_status trimmer_component_init(
	struct bt_private_component *component, struct bt_value *params,
	UNUSED_VAR void *init_method_data)
{
	enum bt_component_status ret;
	struct trimmer *trimmer = create_trimmer_data();
	struct bt_private_port *priv_port = NULL;

	if (!trimmer) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	/* Create input and output ports */
	priv_port = bt_private_component_filter_add_input_private_port(
		component, "in", NULL);
	if (!priv_port) {
		goto error;
	}

	bt_put(priv_port);
	priv_port = bt_private_component_filter_add_output_private_port(
		component, "out", NULL);
	if (!priv_port) {
		goto error;
	}

	ret = bt_private_component_set_user_data(component, trimmer);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	ret = init_from_params(trimmer, params);
end:
	bt_put(priv_port);
	return ret;
error:
	destroy_trimmer_data(trimmer);
	ret = BT_COMPONENT_STATUS_ERROR;
	return ret;
}
