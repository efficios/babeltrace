#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 Francis Deslauriers <francis.deslauriers@efficios.com>
#

# This file tests what happens when we mux messages.

SH_TAP=1

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../../../utils/utils.sh"
fi

# shellcheck source=../../../utils/utils.sh
source "$UTILSSH"

data_dir="$BT_TESTS_DATADIR/plugins/flt.utils.muxer"

plan_tests 12

function run_test
{
	local test_name="$1"
	local local_args=(
		"--plugin-path" "$data_dir"
		"-c" "src.test-muxer.TheSourceOfConfusion"
		"-p" "test-name=$test_name"
		"-c" "sink.text.details"
		"--params=compact=false,with-metadata=false"
	)

	stdout_expected="$data_dir/succeed/$test_name.expect"
	bt_diff_cli "$stdout_expected" /dev/null "${local_args[@]}"
	ok $? "$test_name"
}


test_cases=(
	basic-timestamp-ordering
	diff-event-class-id
	diff-event-class-name
	diff-inactivity-msg-cs
	diff-stream-class-id
	diff-stream-class-name
	diff-stream-class-no-name
	diff-stream-id
	diff-stream-name
	diff-stream-no-name
	diff-trace-name
	multi-iter-ordering
)

for i in "${test_cases[@]}"
do
	run_test "$i"
done
