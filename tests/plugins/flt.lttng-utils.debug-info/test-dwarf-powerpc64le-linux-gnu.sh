#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2015 Antoine Busque <abusque@efficios.com>
# Copyright (C) 2019 Michael Jeanson <mjeanson@efficios.com>
#

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../../utils/utils.sh"
fi

# shellcheck source=../../utils/utils.sh
source "$UTILSSH"

this_dir_relative="plugins/flt.lttng-utils.debug-info"
this_dir_build="$BT_TESTS_BUILDDIR/$this_dir_relative"
debug_info_data="$BT_TESTS_DATADIR/$this_dir_relative/powerpc64le-linux-gnu"

"$this_dir_build/test-dwarf" \
	"$debug_info_data"
