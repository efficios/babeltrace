#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2013 Jérémie Galarneau <jeremie.galarneau@efficios.com>
#

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../utils/utils.sh"
fi

# shellcheck source=../utils/utils.sh
source "$UTILSSH"

"${BT_TESTS_BUILDDIR}/ctf-writer/ctf-writer" "$BT_TESTS_BT2_BIN"
