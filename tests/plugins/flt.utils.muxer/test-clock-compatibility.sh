#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2024 EfficiOS Inc.
#

if [[ -n "${BT_TESTS_SRCDIR:-}" ]]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../../utils/utils.sh"
fi

# shellcheck source=../../utils/utils.sh
source "$UTILSSH"

bt_run_in_py_env "${BT_TESTS_BUILDDIR}/plugins/flt.utils.muxer/test-clock-compatibility"
