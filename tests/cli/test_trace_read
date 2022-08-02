#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2013 Christian Babeux <christian.babeux@efficios.com>
#

SH_TAP=1

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../utils/utils.sh"
fi

# shellcheck source=../utils/utils.sh
source "$UTILSSH"

SUCCESS_TRACES=("${BT_CTF_TRACES_PATH}/succeed/"*)
FAIL_TRACES=("${BT_CTF_TRACES_PATH}/fail/"*)

NUM_TESTS=$((${#SUCCESS_TRACES[@]} + ${#FAIL_TRACES[@]}))

plan_tests $NUM_TESTS

for path in "${SUCCESS_TRACES[@]}"; do
	trace=$(basename "${path}")
	"${BT_TESTS_BT2_BIN}" "${path}" > /dev/null 2>&1
	ok $? "Run babeltrace2 with trace ${trace}"
done

for path in "${FAIL_TRACES[@]}"; do
	trace=$(basename "${path}")
	if "${BT_TESTS_BT2_BIN}" "${path}" > /dev/null 2>&1; then
		fail "Run babeltrace2 with invalid trace ${trace}"
	else
		pass "Run babeltrace2 with invalid trace ${trace}"
	fi
done
