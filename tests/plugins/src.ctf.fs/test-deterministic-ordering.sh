#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 Efficios, Inc.
#

# Test the deterministic behavior of the src.ctf.fs component versus the
# ordering of the given input paths.
#
# In presence of multiple copies of the same packet, we want it to pick the
# copy of the packet to read in a deterministic fashion.
#
# This test is written assuming the specific implementation of the src.ctf.fs
# component class, which sorts its input paths lexicographically.
#
# There are three traces (a-corrupted, b-not-corrupted and c-corrupted) with the
# same UUID and the same packet, except that this packet is corrupted in
# a-corrupted and c-corrupted. In these cases, there is an event with an
# invalid id.  When reading these corrupted packets, we expect babeltrace to
# emit an error.
#
# When reading a-corrupted and b-not-corrupted together, the copy of the packet
# from a-corrupted is read, and babeltrace exits with an error.
#
# When reading b-not-corrupted and c-corrupted together, the copy of the packet
# from b-not-corrupted is read, and babeltrace executes successfully.

SH_TAP=1

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../../utils/utils.sh"
fi

# shellcheck source=../../utils/utils.sh
source "$UTILSSH"

traces_dir="${BT_CTF_TRACES_PATH}/deterministic-ordering"
trace_a_corrupted="${traces_dir}/a-corrupted"
trace_b_not_corrupted="${traces_dir}/b-not-corrupted"
trace_c_corrupted="${traces_dir}/c-corrupted"

if [ "$BT_TESTS_OS_TYPE" = "mingw" ]; then
	# The MSYS2 shell makes a mess trying to convert the Unix-like paths
	# to Windows-like paths, so just disable the automatic conversion and
	# do it by hand.
	export MSYS2_ARG_CONV_EXCL="*"
	trace_a_corrupted=$(cygpath -m "${trace_a_corrupted}")
	trace_b_not_corrupted=$(cygpath -m "${trace_b_not_corrupted}")
	trace_c_corrupted=$(cygpath -m "${trace_c_corrupted}")
fi

stdout_file=$(mktemp -t test-deterministic-ordering-stdout.XXXXXX)
stderr_file=$(mktemp -t test-deterministic-ordering-stderr.XXXXXX)

expect_failure() {
	local test_name
	local inputs

	test_name="$1"
	inputs="$2"

	bt_cli "${stdout_file}" "${stderr_file}" \
		-c src.ctf.fs -p "inputs=[${inputs}]"
	isnt 0 "$?" "${test_name}: exit status is not 0"

	bt_grep_ok \
		"^ERROR: " \
		"${stderr_file}" \
		"${test_name}: error stack is produced"

	bt_grep_ok \
		"No event class with ID of event class ID to use in stream class" \
		"$stderr_file" \
		"$test_name: expected error message is present"
}

expect_success() {
	local test_name
	local inputs

	test_name="$1"
	inputs="$2"

	bt_cli "${stdout_file}" "${stderr_file}" \
		-c src.ctf.fs -p "inputs=[${inputs}]" \
		-c sink.text.details -p 'with-trace-name=no,with-stream-name=no,with-metadata=no,compact=yes'
	ok "$?" "${test_name}: exit status is 0"

	bt_diff "${traces_dir}/b-c.expect" "${stdout_file}"
	ok "$?" "${test_name}: expected output is produced"
}

plan_tests 10

# Trace with corrupted packet comes first lexicographically, expect a failure.

expect_failure "ab" "\"${trace_a_corrupted}\",\"${trace_b_not_corrupted}\""
expect_failure "ba" "\"${trace_b_not_corrupted}\",\"${trace_a_corrupted}\""

# Trace with non-corrupted packet comes first lexicographically, expect a success.

expect_success "bc" "\"${trace_b_not_corrupted}\",\"${trace_c_corrupted}\""
expect_success "cb" "\"${trace_c_corrupted}\",\"${trace_b_not_corrupted}\""

rm -f "${stdout_file}" "${stderr_file}"
