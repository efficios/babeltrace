#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 Simon Marchi <simon.marchi@efficios.com>
# Copyright (C) 2019 Francis Deslauriers <francis.deslauriers@efficios.com>
#

SH_TAP=1

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../../../utils/utils.sh"
fi

# shellcheck source=../../../utils/utils.sh
source "$UTILSSH"

this_dir_relative="plugins/src.ctf.fs/query"
succeed_trace_dir="$BT_CTF_TRACES_PATH/succeed"
expect_dir="$BT_TESTS_DATADIR/$this_dir_relative"

test_query_metadata_info() {
	local name="$1"
	local ret=0
	local trace_path="$succeed_trace_dir/$name"
	local expected_stdout="$expect_dir/metadata-info-$name.expect"
	local temp_stdout_output_file
	local temp_stderr_output_file
	local query=("query" "src.ctf.fs" "metadata-info" "--params" "path=\"$trace_path\"")

	temp_stdout_output_file="$(mktemp -t actual-stdout.XXXXXX)"
	temp_stderr_output_file="$(mktemp -t actual-stderr.XXXXXX)"

	bt_cli "$temp_stdout_output_file" "$temp_stderr_output_file" \
		"${query[@]}"

	bt_diff "$expected_stdout" "$temp_stdout_output_file"
	ret_stdout=$?

	bt_diff /dev/null "$temp_stderr_output_file"
	ret_stderr=$?

	if ((ret_stdout != 0 || ret_stderr != 0)); then
		ret=1
	fi

	ok $ret "Trace '$name' \`metadata-info\` query gives the expected output"
	rm -f "$temp_stdout_output_file" "$temp_stderr_output_file"
}

test_non_existent_trace_dir() {
	local empty_dir
	local stdout_file
	local stderr_file
	local query

	empty_dir=$(mktemp -d)
	stdout_file="$(mktemp -t actual-stdout.XXXXXX)"
	stderr_file="$(mktemp -t actual-stderr.XXXXXX)"
	query=("query" "src.ctf.fs" "metadata-info" "--params" "path=\"$empty_dir\"")

	bt_cli "$stdout_file" "$stderr_file" \
		"${query[@]}"
	isnt $? 0 "non existent trace dir: babeltrace exits with an error"

	bt_diff "/dev/null" "${stdout_file}"
	ok $? "non existent trace dir: babeltrace produces the expected stdout"

	bt_grep_ok \
		"^CAUSED BY " \
		"${stderr_file}" \
		"non existent trace dir: babeltrace produces an error stack"

	bt_grep_ok \
		"Failed to open metadata file: No such file or directory: path=\".*metadata\"" \
		"$stderr_file" \
		"non existent trace dir: babeltrace produces the expected error message"

	rm -f "${stdout_file}" "${stderr_file}"
	rmdir "${empty_dir}"
}

plan_tests 7
test_query_metadata_info succeed1
test_non_existent_trace_dir
test_query_metadata_info lf-metadata
test_query_metadata_info crlf-metadata
