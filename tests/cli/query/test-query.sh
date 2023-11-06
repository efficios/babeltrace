#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 Simon Marchi <simon.marchi@efficios.com>
#

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../../utils/utils.sh"
fi

# shellcheck source=../../utils/utils.sh
SH_TAP=1 source "$UTILSSH"

NUM_TESTS=15

plan_tests $NUM_TESTS

data_dir="${BT_TESTS_DATADIR}/cli/query"
plugin_dir="${data_dir}"

stdout_expected_file=$(mktemp -t test-cli-query-stdout-expected.XXXXXX)
stdout_file=$(mktemp -t test-cli-query-stdout.XXXXXX)
stderr_file=$(mktemp -t test-cli-query-stderr.XXXXXX)

expect_success() {
	local expected_str="$1"
	shift 1
	local args=("$@")

	echo "$expected_str" > "$stdout_expected_file"

	bt_diff_cli "$stdout_expected_file" /dev/null \
		--plugin-path "$plugin_dir" \
		query "src.query.SourceWithQueryThatPrintsParams" \
		"${args[@]}"
	ok "$?" "${args[*]}"
}

expect_failure() {
	local expected_str="$1"
	shift 1
	local args=("$@")
	local test_name="${args[*]}"

	echo -n > "$stdout_expected_file"

	bt_cli "$stdout_file" "$stderr_file" \
		--plugin-path "$plugin_dir" \
		query \
		"${args[@]}"
	isnt "$?" 0 "${test_name}: exit code is not 0"

	bt_diff /dev/null "$stdout_file"
	ok "$?" "${test_name}: nothing output on stout"

	# Ensure that a CLI error stack is printed (and that babeltrace doesn't
	# abort before that).
	bt_grep_ok \
		"^ERROR: " \
		"${stderr_file}" \
		"${test_name}: babeltrace produces an error stack"

	bt_grep_ok \
		"${expected_str}" \
		"${stderr_file}" \
		"${test_name}: expect \`${expected_str}\` error message on stderr"
}

expect_success 'the-object:{}' \
	'the-object'
expect_success "the-object:{a=2}" \
	'the-object' -p 'a=2'

# Check that -p parameters are processed in order.
expect_success "the-object:{a=3, ben=kin, voyons=donc}" \
	'the-object' -p 'a=2,ben=kin' -p 'voyons=donc,a=3'

# Failure inside the component class' query method.
expect_failure "ValueError: catastrophic failure" \
	'src.query.SourceWithQueryThatPrintsParams' 'please-fail' '-p' 'a=2'

# Non-existent component class.
expect_failure 'Cannot find component class: plugin-name="query", comp-cls-name="NonExistentSource", comp-cls-type=SOURCE' \
	'src.query.NonExistentSource' 'the-object' '-p' 'a=2'

# Wrong parameter syntax.
expect_failure "Invalid format for --params option's argument:" \
	'src.query.SourceWithQueryThatPrintsParams' 'please-fail' '-p' 'a=3,'

rm -f "$stdout_expected_file"
rm -f "$stdout_file"
rm -f "$stderr_file"
