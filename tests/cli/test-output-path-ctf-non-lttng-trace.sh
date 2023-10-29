#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) EfficiOS Inc.
#

# This test verifies that generic (non-LTTng) CTF traces are output with the
# expected directory structure.
#
# Traces found when invoking
#
#   babeltrace2 in -c sink.ctf.fs -p 'path="out"'
#
# are expected to use the same directory structure relative to `out` as the
# original traces had relative to `in`.

SH_TAP=1

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../utils/utils.sh"
fi

# shellcheck source=../utils/utils.sh
source "$UTILSSH"

plan_tests 3

temp_input_dir=$(mktemp -t -d test-output-path-ctf-non-lttng-trace-input.XXXXXX)
temp_output_dir=$(mktemp -t -d test-output-path-ctf-non-lttng-trace-output.XXXXXX)

mkdir -p "${temp_input_dir}/a/b/c"
cp -a "${BT_CTF_TRACES_PATH}/intersection/3eventsintersect" "${temp_input_dir}/a/b/c"

mkdir -p "${temp_input_dir}/a/b/c"
cp -a "${BT_CTF_TRACES_PATH}/intersection/3eventsintersectreverse" "${temp_input_dir}/a/b/c"

mkdir -p "${temp_input_dir}/d/e/f"
cp -a "${BT_CTF_TRACES_PATH}/intersection/nointersect" "${temp_input_dir}/d/e/f"

bt_cli "/dev/null" "/dev/null" "${temp_input_dir}" -c sink.ctf.fs -p "path=\"${temp_output_dir}\""

test -f "${temp_output_dir}/a/b/c/3eventsintersect/metadata"
ok "$?" "3eventsintersect output trace exists"

test -f "${temp_output_dir}/a/b/c/3eventsintersectreverse/metadata"
ok "$?" "3eventsintersectreverse output trace exists"

test -f "${temp_output_dir}/d/e/f/nointersect/metadata"
ok "$?" "nointersect output trace exists"

rm -rf "${temp_input_dir}" "${temp_output_dir}"
