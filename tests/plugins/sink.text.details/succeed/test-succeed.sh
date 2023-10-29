#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 Philippe Proulx <pproulx@efficios.com>
#

SH_TAP=1

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../../../utils/utils.sh"
fi

# shellcheck source=../../../utils/utils.sh
source "$UTILSSH"

this_dir_relative="plugins/sink.text.details/succeed"
expect_dir="$BT_TESTS_DATADIR/$this_dir_relative"

test_details() {
	local test_name="$1"
	local trace_name="$2"
	shift 2
	local details_args=("$@")
	local trace_dir="$BT_CTF_TRACES_PATH/succeed/$trace_name"
	local expect_path="$expect_dir/$test_name.expect"

	bt_diff_cli "$expect_path" /dev/null \
		"$trace_dir" -p trace-name=the-trace \
		-c sink.text.details "${details_args[@]+${details_args[@]}}"
	ok $? "'$test_name' test has the expected output"
}

# This is used for the moment because the source is `src.ctf.fs` and
# such a component can make its stream names contain absolute paths.
test_details_no_stream_name() {
	local test_name="$1"
	local trace_name="$2"
	shift 2
	local details_args=("$@")

	test_details "$test_name" "$trace_name" \
		"${details_args[@]+${details_args[@]}}" -p with-stream-name=no
}

plan_tests 12

test_details_no_stream_name default wk-heartbeat-u
test_details_no_stream_name default-compact wk-heartbeat-u -p compact=yes
test_details_no_stream_name default-compact-without-metadata wk-heartbeat-u -p compact=yes,with-metadata=no
test_details_no_stream_name default-compact-without-time wk-heartbeat-u -p compact=yes,with-time=no
test_details_no_stream_name default-without-data wk-heartbeat-u -p with-data=no
test_details_no_stream_name default-without-data-without-metadata wk-heartbeat-u -p with-data=no,with-metadata=no
test_details_no_stream_name default-without-metadata wk-heartbeat-u -p with-metadata=no
test_details_no_stream_name default-without-names wk-heartbeat-u -p with-stream-name=no,with-trace-name=no,with-stream-class-name=no
test_details_no_stream_name default-without-time wk-heartbeat-u -p with-time=no
test_details_no_stream_name default-without-trace-name wk-heartbeat-u -p with-trace-name=no
test_details_no_stream_name default-without-uuid wk-heartbeat-u -p with-uuid=no
test_details_no_stream_name no-packet-context no-packet-context
