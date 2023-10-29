#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2017 Philippe Proulx <pproulx@efficios.com>
# Copyright (C) 2019 Simon Marchi <simon.marchi@efficios.com>
#

# Execute a shell command in the appropriate environment to have access to the
# bt2 Python bindings. For example, one could use it to run a specific Python
# binding test case with:
#
#   $ tests/utils/run_python_bt2 python3 ./tests/utils/python/testrunner.py \
#     -t test_value.MapValueTestCase.test_deepcopy \
#     ./tests/bindings/python/bt2

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../utils/utils.sh"
fi

# shellcheck source=../utils/utils.sh
source "$UTILSSH"

usage() {
	echo "Usage: run_python_bt2 [PYTHON_BIN] ..."
	echo ""
	echo "Run a binary with the python environment set to use the 'bt2' module"
	echo "from the build system prior to installation."
	echo ""
	echo "When building out of tree export the BT_TESTS_BUILDDIR variable with"
	echo "the path to the built 'tests' directory."
}

if [ -z "$*" ]; then
	usage
	exit 1
fi

# Sanity check that the BT_TESTS_BUILDDIR value makes sense.
if [ ! -f "$BT_TESTS_BUILDDIR/Makefile" ]; then
	fold -w 80 -s <<- END
	$0: BT_TESTS_BUILDDIR does not point to a valid directory (\`$BT_TESTS_BUILDDIR/Makefile\` does not exist).

	If building out-of-tree, set BT_TESTS_BUILDDIR to point to the \`tests\` directory in the build tree.
	END
	exit 1
fi

run_python_bt2 "${@}"
