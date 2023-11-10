#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2017-2023 Philippe Proulx <pproulx@efficios.com>
# Copyright (C) 2019 Simon Marchi <simon.marchi@efficios.com>

if [[ -n "${BT_TESTS_SRCDIR:-}" ]]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../utils/utils.sh"
fi

# shellcheck source=../utils/utils.sh
source "$UTILSSH"

usage() {
	echo "Usage: run-in-py-utils-bt2-env.sh COMMAND [ARGS]..."
	echo ""
	echo "Runs the command \`COMMAND\` with the arguments \`ARGS\` within an environment"
	echo "which can import the testing Python modules (in \`tests/utils/python\`) and the"
	echo "built \`bt2\` Python package."
	echo ""
	echo "NOTE: If you build out of tree, export and set the \`BT_TESTS_BUILDDIR\`"
	echo "environment variable to the built \`tests\` directory."
}

if [[ -z "$*" ]]; then
	usage
	exit 1
fi

# Sanity check that the BT_TESTS_BUILDDIR value makes sense.
if [[ ! -f "$BT_TESTS_BUILDDIR/Makefile" ]]; then
	{
		echo "ERROR: Invalid \`BT_TESTS_BUILDDIR\` variable (\`\$BT_TESTS_BUILDDIR/Makefile\`"
		echo "doesn't exist)."
		echo ""
		echo "If you build out of tree, export and set the \`BT_TESTS_BUILDDIR\` environment"
		echo "variable to the built \`tests\` directory."
	} >&2

	exit 1
fi

bt_run_in_py_env "${@}"
