#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2024 EfficiOS, Inc.
#

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../utils/utils.sh"
fi

# shellcheck source=../utils/utils.sh
source "$UTILSSH"

ret=0

"${BT_TESTS_BUILDDIR}/lib/test-plugin-init-fail" "${BT_TESTS_BUILDDIR}/lib/test-plugin-init-fail-plugin/.libs" yes || ret=1
"${BT_TESTS_BUILDDIR}/lib/test-plugin-init-fail" "${BT_TESTS_BUILDDIR}/lib/test-plugin-init-fail-plugin/.libs" no || ret=1

exit $ret
