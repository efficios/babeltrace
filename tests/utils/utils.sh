#!/bin/bash

# Copyright (c) 2019 Michael Jeanson <mjeanson@efficios.com>
# Copyright (C) 2019 Philippe Proulx <pproulx@efficios.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; under version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

# This file is meant to be sourced at the start of shell script-based tests.


# Error out when encountering an undefined variable
set -u

# If "readlink -f" is available, get a resolved absolute path to the
# tests source dir, otherwise make do with a relative path.
scriptdir="$(dirname "${BASH_SOURCE[0]}")"
if readlink -f "." >/dev/null 2>&1; then
	testsdir=$(readlink -f "$scriptdir/..")
else
	testsdir="$scriptdir/.."
fi

# The OS on which we are running. See [1] for possible values of 'uname -s'.
# We do a bit of translation to ease our life down the road for comparison.
# Export it so that called executables can use it.
# [1] https://en.wikipedia.org/wiki/Uname#Examples
if [ "x${BT_OS_TYPE:-}" = "x" ]; then
	BT_OS_TYPE="$(uname -s)"
	case "$BT_OS_TYPE" in
	MINGW*)
		BT_OS_TYPE="mingw"
		;;
	Darwin)
		BT_OS_TYPE="darwin"
		;;
	Linux)
		BT_OS_TYPE="linux"
		;;
	CYGWIN*)
		BT_OS_TYPE="cygwin"
		;;
	*)
		BT_OS_TYPE="unsupported"
		;;
	esac
fi
export BT_OS_TYPE

# Allow overriding the source and build directories
if [ "x${BT_TESTS_SRCDIR:-}" = "x" ]; then
	BT_TESTS_SRCDIR="$testsdir"
fi
export BT_TESTS_SRCDIR

if [ "x${BT_TESTS_BUILDDIR:-}" = "x" ]; then
	BT_TESTS_BUILDDIR="$testsdir"
fi
export BT_TESTS_BUILDDIR

# By default, it will not source tap.sh.  If you to tap output directly from
# the test script, define the 'SH_TAP' variable to '1' before sourcing this
# script.
if [ "x${SH_TAP:-}" = x1 ]; then
	# shellcheck source=./tap/tap.sh
	. "${BT_TESTS_SRCDIR}/utils/tap/tap.sh"
fi

# Allow overriding the babeltrace2 executables
if [ "x${BT_TESTS_BT2_BIN:-}" = "x" ]; then
	BT_TESTS_BT2_BIN="$BT_TESTS_BUILDDIR/../src/cli/babeltrace2"
	if [ "$BT_OS_TYPE" = "mingw" ]; then
		BT_TESTS_BT2_BIN="${BT_TESTS_BT2_BIN}.exe"
	fi
fi
export BT_TESTS_BT2_BIN

# TODO: Remove when bindings/python/bt2/test_plugin.py is fixed
BT_PLUGINS_PATH="${BT_TESTS_BUILDDIR}/../src/plugins"

# Allow overriding the babeltrace2 plugin path
if [ "x${BT_TESTS_BABELTRACE_PLUGIN_PATH:-}" = "x" ]; then
	BT_TESTS_BABELTRACE_PLUGIN_PATH="${BT_PLUGINS_PATH}/ctf:${BT_PLUGINS_PATH}/utils:${BT_PLUGINS_PATH}/text:${BT_PLUGINS_PATH}/lttng-utils"
fi

if [ "x${BT_TESTS_PROVIDER_DIR:-}" = "x" ]; then
	BT_TESTS_PROVIDER_DIR="${BT_TESTS_BUILDDIR}/../src/python-plugin-provider/.libs"
fi

# Allow overriding the babeltrace2 executables
if [ "x${BT_TESTS_PYTHONPATH:-}" = "x" ]; then
	BT_TESTS_PYTHONPATH="${BT_TESTS_BUILDDIR}/../src/bindings/python/bt2/build/build_lib"
fi


### External Tools ###
if [ "x${BT_TESTS_AWK_BIN:-}" = "x" ]; then
	BT_TESTS_AWK_BIN="awk"
fi
export BT_TESTS_AWK_BIN

if [ "x${BT_TESTS_GREP_BIN:-}" = "x" ]; then
	BT_TESTS_GREP_BIN="grep"
fi
export BT_TESTS_GREP_BIN

if [ "x${BT_TESTS_PYTHON_BIN:-}" = "x" ]; then
	BT_TESTS_PYTHON_BIN="python3"
fi
export BT_TESTS_PYTHON_BIN

if [ "x${BT_TESTS_PYTHON_CONFIG_BIN:-}" = "x" ]; then
	BT_TESTS_PYTHON_CONFIG_BIN="python3-config"
fi
export BT_TESTS_PYTHON_CONFIG_BIN

if [ "x${BT_TESTS_SED_BIN:-}" = "x" ]; then
	BT_TESTS_SED_BIN="sed"
fi
export BT_TESTS_SED_BIN


# Data files path
BT_TESTS_DATADIR="${BT_TESTS_SRCDIR}/data"
BT_CTF_TRACES_PATH="${BT_TESTS_DATADIR}/ctf-traces"

# Remove CR characters in file "$1".

bt_remove_cr() {
	"$BT_TESTS_SED_BIN" -i 's/\r//g' "$1"
}

# Run the Babeltrace CLI, redirecting stdout and stderr to specified files.
#
#   $1: file to redirect stdout to
#   $2: file to redirect stderr to
#   remaining args: arguments to pass to the CLI
#
# Return the exit code of the CLI.

bt_cli() {
	local stdout_file="$1"
	local stderr_file="$2"
	shift 2
	local args=("$@")

	echo "Running: $BT_TESTS_BT2_BIN ${args[*]}" >&2
	run_python_bt2 "$BT_TESTS_BT2_BIN" "${args[@]}" 1>"$stdout_file" 2>"$stderr_file"
}

### Diff Functions ###

# Check the differences between two files (typically some expected output vs
# some actual output).  If there are differences, print the diff to stderr.
#
#   $1: file 1 (expected)
#   $2: file 2 (actual)
#
# Return 0 if there's no difference, and non-zero if there are.
#
# Note that this function modifies the actual output file ($2) _in-place_ to
# remove any \r character.

bt_diff() {
	local expected_file="$1"
	local actual_file="$2"
	local ret=0

	# Strip any \r present due to Windows (\n -> \r\n).
	# "diff --string-trailing-cr" is not used since it is not present on
	# Solaris.
	bt_remove_cr "$actual_file"

	diff -u "$expected_file" "$actual_file" 1>&2

	return $?
}

# Checks the difference between:
#
#   1. What the CLI outputs on its standard output when given the arguments
#   "$@" (excluding the first two arguments).
#   2. The file with path "$1".
#
# And the difference between:
#
#   1. What the CLI outputs on its standard error when given the arguments
#   "$@" (excluding the first two arguments).
#   2. The file with path "$2".
#
# Returns 0 if there's no difference, and 1 if there is, also printing
# said difference to the standard error.
bt_diff_cli() {
	local expected_stdout_file="$1"
	local expected_stderr_file="$2"
	shift 2
	local args=("$@")

	local temp_stdout_output_file
	local temp_stderr_output_file
	local ret=0
	local ret_stdout
	local ret_stderr

	temp_stdout_output_file="$(mktemp -t actual_stdout.XXXXXX)"
	temp_stderr_output_file="$(mktemp -t actual_stderr.XXXXXX)"

	# Run the CLI to get a detailed file.
	bt_cli "$temp_stdout_output_file" "$temp_stderr_output_file" "${args[@]}"

	bt_diff "$expected_stdout_file" "$temp_stdout_output_file" "${args[@]}"
	ret_stdout=$?
	bt_diff "$expected_stderr_file" "$temp_stderr_output_file" "${args[@]}"
	ret_stderr=$?

	if ((ret_stdout != 0 || ret_stderr != 0)); then
		ret=1
	fi

	rm -f "$temp_stdout_output_file" "$temp_stderr_output_file"

	return $ret
}

# Checks the difference between the content of the file with path "$1"
# and the output of the CLI when called on the directory path "$2" with
# the arguments '-c sink.text.details' and the rest of the arguments to
# this function.
#
# Returns 0 if there's no difference, and 1 if there is, also printing
# said difference to the standard error.
bt_diff_details_ctf_single() {
	local expected_stdout_file="$1"
	local trace_dir="$2"
	shift 2
	local extra_details_args=("$@")
	expected_stderr_file="/dev/null"

	# Compare using the CLI with `sink.text.details`
	bt_diff_cli "$expected_stdout_file" "$expected_stderr_file" "$trace_dir" \
		"-c" "sink.text.details" "${extra_details_args[@]+${extra_details_args[@]}}"
}

# Calls bt_diff_details_ctf_single(), except that "$1" is the path to a
# program which generates the CTF trace to compare to. The program "$1"
# receives the path to a temporary, empty directory where to write the
# CTF trace as its first argument.
bt_diff_details_ctf_gen_single() {
	local ctf_gen_prog_path="$1"
	local expected_stdout_file="$2"
	shift 2
	local extra_details_args=("$@")

	local temp_trace_dir
	local ret

	temp_trace_dir="$(mktemp -d)"

	# Run the CTF trace generator program to get a CTF trace
	if ! "$ctf_gen_prog_path" "$temp_trace_dir" 2>/dev/null; then
		echo "ERROR: \"$ctf_gen_prog_path\" \"$temp_trace_dir\" failed" >&2
		rm -rf "$temp_trace_dir"
		return 1
	fi

	# Compare using the CLI with `sink.text.details`
	bt_diff_details_ctf_single "$expected_stdout_file" "$temp_trace_dir" \
		"${extra_details_args[@]+${extra_details_args[@]}}"
	ret=$?
	rm -rf "$temp_trace_dir"
	return $ret
}


### Functions ###

check_coverage() {
	coverage run "$@"
}

# Execute a shell command in the appropriate environment to have access to the
# bt2 Python bindings.
run_python_bt2() {
	local env_args

	env_args=(
		"BABELTRACE_PYTHON_BT2_NO_TRACEBACK=1" \
		"BABELTRACE_PLUGIN_PATH=${BT_TESTS_BABELTRACE_PLUGIN_PATH}" \
		"LIBBABELTRACE2_PLUGIN_PROVIDER_DIR=${BT_TESTS_PROVIDER_DIR}" \
		"BT_TESTS_DATADIR=${BT_TESTS_DATADIR}" \
		"BT_CTF_TRACES_PATH=${BT_CTF_TRACES_PATH}" \
		"BT_PLUGINS_PATH=${BT_PLUGINS_PATH}" \
		"PYTHONPATH=${BT_TESTS_PYTHONPATH}:${BT_TESTS_SRCDIR}/utils/python"
		)

	local main_lib_path="${BT_TESTS_BUILDDIR}/../src/lib/.libs"

	# Set the library search path so the python interpreter can load libbabeltrace2
	if [ "$BT_OS_TYPE" = "mingw" ] || [ "$BT_OS_TYPE" = "cygwin" ]; then
		env_args+=("PATH=${main_lib_path}:${PATH:-}")
	elif [ "$BT_OS_TYPE" = "darwin" ]; then
		env_args+=("DYLD_LIBRARY_PATH=${main_lib_path}:${DYLD_LIBRARY_PATH:-}")
	else
		env_args+=("LD_LIBRARY_PATH=${main_lib_path}:${LD_LIBRARY_PATH:-}")
	fi

	# On Windows, an embedded Python interpreter needs a way to locate the path
	# to it's internal modules, set the prefix from python-config to the
	# PYTHONHOME variable.
	if [ "$BT_OS_TYPE" = "mingw" ]; then
		env_args+=("PYTHONHOME=$($BT_TESTS_PYTHON_CONFIG_BIN --prefix)")
	fi

	env "${env_args[@]}" "$@"
}

# Set the environment and run python tests in the directory.
#
# $1 : The directory containing the python test scripts
# $2 : The pattern to match python test script names (optional)
run_python_bt2_test() {
	local test_dir="$1"
	local test_pattern="${2:-'*'}" # optional, if none default to "*"

	local ret
	local test_runner_args=()

	test_runner_args+=("$test_dir")
	if [ "x${test_pattern}" != "x" ]; then
		test_runner_args+=("${test_pattern}")
	fi

	if test "x${BT_TESTS_COVERAGE:-}" = "x1"; then
		python_exec="check_coverage"
	else
		python_exec="${BT_TESTS_PYTHON_BIN}"
	fi

	run_python_bt2 \
		"${python_exec}" \
		"${BT_TESTS_SRCDIR}/utils/python/testrunner.py" \
		--pattern "$test_pattern" \
		"$test_dir" \

	ret=$?

	if test "x${BT_TESTS_COVERAGE_REPORT:-}" = "x1"; then
		coverage report -m
	fi

	if test "x${BT_TESTS_COVERAGE_HTML:-}" = "x1"; then
		coverage html
	fi

	return $ret
}
