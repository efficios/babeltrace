#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2017 Philippe Proulx <pproulx@efficios.com>
#

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../utils/utils.sh"
fi

# shellcheck source=../utils/utils.sh
source "$UTILSSH"

"${BT_TESTS_BUILDDIR}/lib/plugin" "${BT_TESTS_BUILDDIR}/lib/test-plugin-plugins/.libs"
