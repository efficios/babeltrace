/*
 * test-seeks.c
 *
 * Lib BabelTrace - Seeks test program
 *
 * Copyright 2012 - Yannick Brosseau <yannick.brosseau@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#define _GNU_SOURCE
#include <babeltrace/context.h>
#include <babeltrace/iterator.h>
#include <babeltrace/ctf/iterator.h>
#include <babeltrace/ctf/events.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include "common.h"
#include "tap.h"

#define NR_TESTS	23

void run_seek_begin(char *path, uint64_t expected_begin)
{
	struct bt_context *ctx;
	struct bt_ctf_iter *iter;
	struct bt_ctf_event *event;
	struct bt_iter_pos newpos;
	int ret;
	uint64_t timestamp_begin;
	uint64_t timestamp_seek_begin;

	/* Open the trace */
	ctx = create_context_with_path(path);
	if (!ctx) {
		plan_skip_all("Cannot create valid context");
	}

	/* Create iterator with null begin and end */
	iter = bt_ctf_iter_create(ctx, NULL, NULL);
	if (!iter) {
		plan_skip_all("Cannot create valid iterator");
	}

	event = bt_ctf_iter_read_event(iter);

	ok(event, "Event valid");

	/* Validate that the first timestamp is right */
	timestamp_begin = bt_ctf_get_timestamp(event);

	ok1(timestamp_begin == expected_begin);

	/* Validate that we get the same value after a seek begin */
	newpos.type = BT_SEEK_BEGIN;
	ret = bt_iter_set_pos(bt_ctf_get_iter(iter), &newpos);

	ok(ret == 0, "Seek begin retval %d", ret);

	event = bt_ctf_iter_read_event(iter);

	ok(event, "Event valid");

	timestamp_seek_begin = bt_ctf_get_timestamp(event);

	ok1(timestamp_begin == timestamp_seek_begin);

	bt_context_put(ctx);
}


void run_seek_last(char *path, uint64_t expected_last)
{
	struct bt_context *ctx;
	struct bt_ctf_iter *iter;
	struct bt_ctf_event *event;
	struct bt_iter_pos newpos;
	int ret;
	uint64_t timestamp_last;

	/* Open the trace */
	ctx = create_context_with_path(path);
	if (!ctx) {
		plan_skip_all("Cannot create valid context");
	}

	/* Create iterator with null last and end */
	iter = bt_ctf_iter_create(ctx, NULL, NULL);
	if (!iter) {
		plan_skip_all("Cannot create valid iterator");
	}

	event = bt_ctf_iter_read_event(iter);

	ok(event, "Event valid at beginning");

	/* Seek to last */
	newpos.type = BT_SEEK_LAST;
	ret = bt_iter_set_pos(bt_ctf_get_iter(iter), &newpos);

	ok(ret == 0, "Seek last retval %d", ret);

	event = bt_ctf_iter_read_event(iter);

	ok(event, "Event valid at last position");

	timestamp_last = bt_ctf_get_timestamp(event);

	ok1(timestamp_last == expected_last);

	/* Try to read next event */
	ret = bt_iter_next(bt_ctf_get_iter(iter));

	ok(ret == 0, "iter next should return an error");

	event = bt_ctf_iter_read_event(iter);

	ok(event == 0, "Event after last should be invalid");

	bt_context_put(ctx);
}

void run_seek_cycles(char *path,
		uint64_t expected_begin,
		uint64_t expected_last)
{
	struct bt_context *ctx;
	struct bt_ctf_iter *iter;
	struct bt_ctf_event *event;
	struct bt_iter_pos newpos;
	int ret;
	uint64_t timestamp;

	/* Open the trace */
	ctx = create_context_with_path(path);
	if (!ctx) {
		plan_skip_all("Cannot create valid context");
	}

	/* Create iterator with null last and end */
	iter = bt_ctf_iter_create(ctx, NULL, NULL);
	if (!iter) {
		plan_skip_all("Cannot create valid iterator");
	}

	event = bt_ctf_iter_read_event(iter);

	ok(event, "Event valid at beginning");

	/* Seek to last */
	newpos.type = BT_SEEK_LAST;
	ret = bt_iter_set_pos(bt_ctf_get_iter(iter), &newpos);

	ok(ret == 0, "Seek last retval %d", ret);

	event = bt_ctf_iter_read_event(iter);

	ok(event, "Event valid at last position");

	timestamp = bt_ctf_get_timestamp(event);

	ok1(timestamp == expected_last);

	/* Try to read next event */
	ret = bt_iter_next(bt_ctf_get_iter(iter));

	ok(ret == 0, "iter next should return an error");

	event = bt_ctf_iter_read_event(iter);

	ok(event == 0, "Event after last should be invalid");

	/* Seek to BEGIN */
	newpos.type = BT_SEEK_BEGIN;
	ret = bt_iter_set_pos(bt_ctf_get_iter(iter), &newpos);

	ok(ret == 0, "Seek begin retval %d", ret);

	event = bt_ctf_iter_read_event(iter);

	ok(event, "Event valid at first position");

	timestamp = bt_ctf_get_timestamp(event);

	ok1(timestamp == expected_begin);

	/* Seek last again */
	newpos.type = BT_SEEK_LAST;
	ret = bt_iter_set_pos(bt_ctf_get_iter(iter), &newpos);

	ok(ret == 0, "Seek last retval %d", ret);

	event = bt_ctf_iter_read_event(iter);

	ok(event, "Event valid at last position");

	timestamp = bt_ctf_get_timestamp(event);

	ok1(timestamp == expected_last);

	bt_context_put(ctx);
}

int main(int argc, char **argv)
{
	char *path;
	uint64_t expected_begin;
	uint64_t expected_last;

	plan_tests(NR_TESTS);

	if (argc < 4) {
		plan_skip_all("Invalid arguments: need a trace path and the start and last timestamp");

	}

	/* Parse arguments (Trace, begin timestamp) */
	path = argv[1];

	expected_begin = strtoull(argv[2], NULL, 0);
	if (ULLONG_MAX == expected_begin && errno == ERANGE) {
		plan_skip_all("Invalid value for begin timestamp");
	}

	expected_last = strtoull(argv[3], NULL, 0);
	if (ULLONG_MAX == expected_last && errno == ERANGE) {
		plan_skip_all("Invalid value for last timestamp");
	}

	run_seek_begin(path, expected_begin);
	run_seek_last(path, expected_last);
	run_seek_cycles(path, expected_begin, expected_last);

	return exit_status();
}
