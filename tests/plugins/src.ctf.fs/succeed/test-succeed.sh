#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 Philippe Proulx <pproulx@efficios.com>
#

# This test validates that a `src.ctf.fs` component successfully reads
# specific CTF traces and creates the expected messages.
#
# Such CTF traces to open either exist (in `tests/ctf-traces/succeed`)
# or are generated by this test using local trace generators.

SH_TAP=1

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../../../utils/utils.sh"
fi

# shellcheck source=../../../utils/utils.sh
source "$UTILSSH"

this_dir_relative="plugins/src.ctf.fs/succeed"
this_dir_build="$BT_TESTS_BUILDDIR/$this_dir_relative"
succeed_trace_dir="$BT_CTF_TRACES_PATH/succeed"
expect_dir="$BT_TESTS_DATADIR/$this_dir_relative"

test_ctf_common_details_args=("-p" "with-trace-name=no,with-stream-name=no")

test_ctf_gen_single() {
	name="$1"

	diag "Generating trace '$name'"
	bt_diff_details_ctf_gen_single "$this_dir_build/gen-trace-$name" \
		"$expect_dir/trace-$name.expect" \
		"${test_ctf_common_details_args[@]}" "-p" "with-uuid=no"
	ok $? "Generated trace '$name' gives the expected output"
}

test_ctf_single() {
	name="$1"

	bt_diff_details_ctf_single "$expect_dir/trace-$name.expect" \
		"$succeed_trace_dir/$name" "${test_ctf_common_details_args[@]}"
	ok $? "Trace '$name' gives the expected output"
}

test_packet_end() {
	local name="$1"
	local expected_stdout="$expect_dir/trace-$name.expect"
	local ret=0
	local ret_stdout
	local ret_stderr
	local details_comp=("-c" "sink.text.details")
	local details_args=("-p" "with-trace-name=no,with-stream-name=no,with-metadata=no,compact=yes")
	local temp_stdout_output_file
	local temp_greped_stdout_output_file
	local temp_stderr_output_file

	temp_stdout_output_file="$(mktemp -t actual-stdout.XXXXXX)"
	temp_greped_stdout_output_file="$(mktemp -t greped-stdout.XXXXXX)"
	temp_stderr_output_file="$(mktemp -t actual-stderr.XXXXXX)"

	bt_cli "$temp_stdout_output_file" "$temp_stderr_output_file" \
		"$succeed_trace_dir/$name" "${details_comp[@]}" \
		"${details_args[@]}"

	bt_grep "Packet end" "$temp_stdout_output_file" > "$temp_greped_stdout_output_file"

	bt_diff "$expected_stdout" "$temp_greped_stdout_output_file"
	ret_stdout=$?

	bt_diff /dev/null "$temp_stderr_output_file"
	ret_stderr=$?

	if ((ret_stdout != 0 || ret_stderr != 0)); then
		ret=1
	fi

	ok $ret "Trace '$name' gives the expected output"
	rm -f "$temp_stdout_output_file" "$temp_stderr_output_file" "$temp_greped_stdout_output_file"
}

test_force_origin_unix_epoch() {
	local name1="$1"
	local name2="$2"
	local expected_stdout="$expect_dir/trace-$name1-$name2.expect"
	local ret=0
	local ret_stdout
	local ret_stderr
	local src_ctf_fs_args=("-p" "force-clock-class-origin-unix-epoch=true")
	local details_comp=("-c" "sink.text.details")
	local details_args=("-p" "with-trace-name=no,with-stream-name=no,with-metadata=yes,compact=yes")
	local temp_stdout_output_file
	local temp_stderr_output_file

	temp_stdout_output_file="$(mktemp -t actual-stdout.XXXXXX)"
	temp_stderr_output_file="$(mktemp -t actual-stderr.XXXXXX)"

	bt_cli "$temp_stdout_output_file" "$temp_stderr_output_file" \
		"$succeed_trace_dir/$name1" "${src_ctf_fs_args[@]}" \
		"$succeed_trace_dir/$name2" "${src_ctf_fs_args[@]}" \
		"${details_comp[@]}" "${details_args[@]}"

	bt_diff "$expected_stdout" "$temp_stdout_output_file"
	ret_stdout=$?

	if ((ret_stdout != 0)); then
		ret=1
	fi

	ok $ret "Trace '$name1' and '$name2' give the expected stdout"

	bt_diff /dev/null "$temp_stderr_output_file"
	ret_stderr=$?

	if ((ret_stderr != 0)); then
		ret=1
	fi

	ok $ret "Trace '$name1' and '$name2' give the expected stderr"

	rm -f "$temp_stdout_output_file" "$temp_stderr_output_file"
}

plan_tests 13

test_force_origin_unix_epoch 2packets barectf-event-before-packet
test_ctf_gen_single simple
test_ctf_single smalltrace
test_ctf_single 2packets
test_ctf_single barectf-event-before-packet
test_ctf_single session-rotation
test_ctf_single lttng-tracefile-rotation
test_ctf_single array-align-elem
test_ctf_single struct-array-align-elem
test_ctf_single meta-ctx-sequence
test_packet_end lttng-event-after-packet
test_packet_end lttng-crash
