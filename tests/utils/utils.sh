#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (c) 2019 Michael Jeanson <mjeanson@efficios.com>
# Copyright (C) 2019-2023 Philippe Proulx <pproulx@efficios.com>

# Source this file at the beginning of a shell script test to access
# useful testing variables and functions:
#
#     SH_TAP=1
#
#     if [[ -n ${BT_TESTS_SRCDIR:-} ]]; then
#         UTILSSH=$BT_TESTS_SRCDIR/utils/utils.sh
#     else
#         UTILSSH=$(dirname "$0")/../utils/utils.sh
#     fi
#
#     # shellcheck source=../utils/utils.sh
#     source "$UTILSSH"
#
# Make sure the relative path to `utils.sh` (this file) above is
# correct (twice).

# An unbound variable is an error
set -u

# Name of the OS on which we're running, if not set.
#
# One of:
#
# `mingw`:          MinGW (Windows)
# `darwin`:         macOS
# `linux`:          Linux
# `cygwin`:         Cygwin (Windows)
# `unsupported`:    Anything else
#
# See <https://en.wikipedia.org/wiki/Uname#Examples> for possible values
# of `uname -s`.
#
# Do some translation to ease our life down the road for comparison.
# Export it so that executed commands can use it.
if [ -z "${BT_TESTS_OS_TYPE:-}" ]; then
	BT_TESTS_OS_TYPE="$(uname -s)"
	case "$BT_TESTS_OS_TYPE" in
	MINGW*)
		BT_TESTS_OS_TYPE="mingw"
		;;
	Darwin)
		BT_TESTS_OS_TYPE="darwin"
		;;
	Linux)
		BT_TESTS_OS_TYPE="linux"
		;;
	CYGWIN*)
		BT_TESTS_OS_TYPE="cygwin"
		;;
	*)
		BT_TESTS_OS_TYPE="unsupported"
		;;
	esac
fi
export BT_TESTS_OS_TYPE

# Sets and exports, if not set:
#
# • `BT_TESTS_SRCDIR` to the base source directory of tests.
# • `BT_TESTS_BUILDDIR` to the base build directory of tests.
_set_vars_srcdir_builddir() {
	# If `readlink -f` is available, then get a resolved absolute path
	# to the tests source directory. Otherwise, make do with a relative
	# path.
	local -r scriptdir="$(dirname "${BASH_SOURCE[0]}")"
	local testsdir

	if readlink -f "." >/dev/null 2>&1; then
		testsdir=$(readlink -f "$scriptdir/..")
	else
		testsdir="$scriptdir/.."
	fi

	# Base source directory of tests
	if [ -z "${BT_TESTS_SRCDIR:-}" ]; then
		BT_TESTS_SRCDIR="$testsdir"
	fi
	export BT_TESTS_SRCDIR

	# Base build directory of tests
	if [ -z "${BT_TESTS_BUILDDIR:-}" ]; then
		BT_TESTS_BUILDDIR="$testsdir"
	fi
	export BT_TESTS_BUILDDIR
}

_set_vars_srcdir_builddir
unset -f _set_vars_srcdir_builddir

# Sources the generated environment file (`env.sh`) if it exists.
_source_env_sh() {
	local -r env_sh_path="$BT_TESTS_BUILDDIR/utils/env.sh"

	if [ -f "${env_sh_path}" ]; then
		# shellcheck disable=SC1090,SC1091
		. "${env_sh_path}"
	fi
}

_source_env_sh
unset -f _source_env_sh

# Path to the `babeltrace2` command, if not set
if [ -z "${BT_TESTS_BT2_BIN:-}" ]; then
	BT_TESTS_BT2_BIN="$BT_TESTS_BUILDDIR/../src/cli/babeltrace2"
	if [ "$BT_TESTS_OS_TYPE" = "mingw" ]; then
		BT_TESTS_BT2_BIN+=".exe"
	fi
fi
export BT_TESTS_BT2_BIN

# This doesn't need to be exported, but it needs to remain set for
# run_python_bt2() to use it.
#
# TODO: Remove when `tests/bindings/python/bt2/test_plugin.py` is fixed.
_bt_tests_plugins_path="${BT_TESTS_BUILDDIR}/../src/plugins"

# Colon-separated list of project plugin paths, if not set
if [ -z "${BT_TESTS_BABELTRACE_PLUGIN_PATH:-}" ]; then
	BT_TESTS_BABELTRACE_PLUGIN_PATH="${_bt_tests_plugins_path}/ctf:${_bt_tests_plugins_path}/utils:${_bt_tests_plugins_path}/text:${_bt_tests_plugins_path}/lttng-utils"
fi
export BT_TESTS_BABELTRACE_PLUGIN_PATH

# Directory containing the Python plugin provider library, if not set
if [ -z "${BT_TESTS_PROVIDER_DIR:-}" ]; then
	BT_TESTS_PROVIDER_DIR="${BT_TESTS_BUILDDIR}/../src/python-plugin-provider/.libs"
fi
export BT_TESTS_PROVIDER_DIR

# Directory containing the built `bt2` Python package, if not set
if [ -z "${BT_TESTS_PYTHONPATH:-}" ]; then
	BT_TESTS_PYTHONPATH="${BT_TESTS_BUILDDIR}/../src/bindings/python/bt2/build/build_lib"
fi
export BT_TESTS_PYTHONPATH

# Name of the `awk` command to use when testing, if not set
if [ -z "${BT_TESTS_AWK_BIN:-}" ]; then
	BT_TESTS_AWK_BIN="awk"
fi
export BT_TESTS_AWK_BIN

# Name of the `grep` command to use when testing, if not set
if [ -z "${BT_TESTS_GREP_BIN:-}" ]; then
	BT_TESTS_GREP_BIN="grep"
fi
export BT_TESTS_GREP_BIN

# Name of the `python3` command to use when testing, if not set
if [ -z "${BT_TESTS_PYTHON_BIN:-}" ]; then
	BT_TESTS_PYTHON_BIN="python3"
fi
export BT_TESTS_PYTHON_BIN

# Major and minor version of the `python3` command to use when testing.
#
# This doesn't need to be exported, but it needs to remain set for
# run_python() to use it.
_bt_tests_py3_version=$("$BT_TESTS_PYTHON_BIN" -c 'import sys; print("{}.{}".format(sys.version_info.major, sys.version_info.minor))')

# Name of the `python3-config` command to use when testing, if not set
if [ -z "${BT_TESTS_PYTHON_CONFIG_BIN:-}" ]; then
	BT_TESTS_PYTHON_CONFIG_BIN="python3-config"
fi
export BT_TESTS_PYTHON_CONFIG_BIN

# Name of the `sed` command to use when testing, if not set
if [ -z "${BT_TESTS_SED_BIN:-}" ]; then
	BT_TESTS_SED_BIN="sed"
fi
export BT_TESTS_SED_BIN

# Name of the `cc` command to use when testing, if not set
if [ -z "${BT_TESTS_CC_BIN:-}" ]; then
	BT_TESTS_CC_BIN="cc"
fi
export BT_TESTS_CC_BIN

# Whether or not to enable AddressSanitizer, `0` (disabled) if not set.
#
# This doesn't need to be exported from the point of view of this file,
# but the sourced `env.sh` above does export it.
if [ -z "${BT_TESTS_ENABLE_ASAN:-}" ]; then
	BT_TESTS_ENABLE_ASAN="0"
fi

# Directory containing test data
BT_TESTS_DATADIR="${BT_TESTS_SRCDIR}/data"

# Directory containing test CTF traces
BT_CTF_TRACES_PATH="${BT_TESTS_DATADIR}/ctf-traces"

# Source the shell TAP utilities if `SH_TAP` is `1`
if [ "${SH_TAP:-}" = 1 ]; then
	# shellcheck source=./tap/tap.sh
	. "${BT_TESTS_SRCDIR}/utils/tap/tap.sh"
fi

# Removes the CR characters from the file having the path `$1`.
#
# This is sometimes needed on Windows with text files.
#
# We can't use the `--string-trailing-cr` option of `diff` because
# Solaris doesn't have it.
bt_remove_cr() {
	"$BT_TESTS_SED_BIN" -i'' -e 's/\r//g' "$1"
}

# Prints `$1` without CR characters.
bt_remove_cr_inline() {
    "$BT_TESTS_SED_BIN" 's/\r//g' "$1"
}

# Runs the `$BT_TESTS_BT2_BIN` command within an environment which can
# import the `bt2` Python package, redirecting the standard output to
# the `$1` file and the standard error to the `$2` file.
#
# The remaining arguments are forwarded to the `$BT_TESTS_BT2_BIN`
# command.
#
# Returns the exit status of the executed `$BT_TESTS_BT2_BIN`.
bt_cli() {
	local -r stdout_file="$1"
	local -r stderr_file="$2"
	shift 2
	local -r args=("$@")

	echo "Running: \`$BT_TESTS_BT2_BIN ${args[*]}\`" >&2
	run_python_bt2 "$BT_TESTS_BT2_BIN" "${args[@]}" 1>"$stdout_file" 2>"$stderr_file"
}

# Checks the differences between:
#
# • The (expected) contents of the file having the path `$1`.
#
# • The contents of another file having the path `$2`.
#
# Both files are passed through bt_remove_cr_inline() to remove CR
# characters.
#
# Returns 0 if there's no difference, or not zero otherwise.
bt_diff() {
	local -r expected_file="$1"
	local -r actual_file="$2"

	diff -u <(bt_remove_cr_inline "$expected_file") <(bt_remove_cr_inline "$actual_file") 1>&2
}

# Checks the difference between:
#
# • What the `$BT_TESTS_BT2_BIN` command prints to its standard output
#   when given the third and following arguments of this function.
#
# • The file having the path `$1`.
#
# as well as the difference between:
#
# • What the `$BT_TESTS_BT2_BIN` command prints to its standard error
#   when given the third and following arguments of this function.
#
# • The file having the path `$2`.
#
# Returns 0 if there's no difference, or 1 otherwise, also printing said
# difference to the standard error.
bt_diff_cli() {
	local -r expected_stdout_file="$1"
	local -r expected_stderr_file="$2"
	shift 2
	local -r args=("$@")

	local -r temp_stdout_output_file="$(mktemp -t actual-stdout.XXXXXX)"
	local -r temp_stderr_output_file="$(mktemp -t actual-stderr.XXXXXX)"

	bt_cli "$temp_stdout_output_file" "$temp_stderr_output_file" "${args[@]}"

	bt_diff "$expected_stdout_file" "$temp_stdout_output_file" "${args[@]}"
	local -r ret_stdout=$?
	bt_diff "$expected_stderr_file" "$temp_stderr_output_file" "${args[@]}"
	local -r ret_stderr=$?

	rm -f "$temp_stdout_output_file" "$temp_stderr_output_file"

	return $((ret_stdout || ret_stderr))
}

# Checks the difference between:
#
# • The content of the file having the path `$1`.
#
# • What the `$BT_TESTS_BT2_BIN` command prints to the standard output
#   when executed with:
#
#   1. The CTF trace directory `$2`.
#   2. The arguments `-c` and `sink.text.details`.
#   3. The third and following arguments of this function.
#
# Returns 0 if there's no difference, or 1 otherwise, also printing said
# difference to the standard error.
bt_diff_details_ctf_single() {
	local -r expected_stdout_file="$1"
	local -r trace_dir="$2"
	shift 2
	local -r extra_details_args=("$@")

	# Compare using the CLI with `sink.text.details`
	bt_diff_cli "$expected_stdout_file" /dev/null "$trace_dir" \
		"-c" "sink.text.details" "${extra_details_args[@]+${extra_details_args[@]}}"
}

# Like bt_diff_details_ctf_single(), except that `$1` is the path to a
# program which generates the CTF trace to compare to.
#
# The program `$1` receives the path to a temporary, empty directory
# where to write the CTF trace as its first argument.
bt_diff_details_ctf_gen_single() {
	local -r ctf_gen_prog_path="$1"
	local -r expected_stdout_file="$2"
	shift 2
	local -r extra_details_args=("$@")

	local -r temp_trace_dir="$(mktemp -d)"

	# Run the CTF trace generator program to get a CTF trace
	if ! "$ctf_gen_prog_path" "$temp_trace_dir" 2>/dev/null; then
		echo "ERROR: \`$ctf_gen_prog_path $temp_trace_dir\` failed" >&2
		rm -rf "$temp_trace_dir"
		return 1
	fi

	# Compare using the CLI with `sink.text.details`
	bt_diff_details_ctf_single "$expected_stdout_file" "$temp_trace_dir" \
		"${extra_details_args[@]+${extra_details_args[@]}}"
	local -r ret=$?
	rm -rf "$temp_trace_dir"
	return $ret
}

# Like `grep`, but using `$BT_TESTS_GREP_BIN`.
bt_grep() {
	"$BT_TESTS_GREP_BIN" "$@"
}

# Only if `tap.sh` is sourced because bt_grep_ok() uses ok()
if [[ ${SH_TAP:-} == 1 ]]; then
	# ok() with the test name `$3` on the result of bt_grep() matching
	# the pattern `$1` within the file `$2`.
	bt_grep_ok() {
		local -r pattern=$1
		local -r file=$2
		local -r test_name=$3

		bt_grep --silent "$pattern" "$file"

		local -r ret=$?

		if ! ok $ret "$test_name"; then
			{
				echo "Pattern \`$pattern\` doesn't match the contents of \`$file\`:"
				echo '--- 8< ---'
				cat "$file"
				echo '--- >8 ---'
			} >&2
		fi

		return $ret
	}
fi

# Forwards the arguments to `coverage run`.
_bt_tests_check_coverage() {
	coverage run "$@"
}

# Executes a command within an environment which can import the testing
# Python modules (in `tests/utils/python`).
run_python() {
	local our_pythonpath="${BT_TESTS_SRCDIR}/utils/python"

	if [[ $_bt_tests_py3_version =~ 3.[45] ]]; then
		# Add a local directory containing a `typing.py` to `PYTHONPATH`
		# for Python 3.4 and Python 3.5 which either don't offer the
		# `typing` module at all, or offer a partial one.
		our_pythonpath="$our_pythonpath:${BT_TESTS_SRCDIR}/utils/python/typing"
	fi

	PYTHONPATH="${our_pythonpath}${PYTHONPATH:+:}${PYTHONPATH:-}" "$@"
}

# Executes a command within an environment which can import the testing
# Python modules (in `tests/utils/python`) and the `bt2` Python package.
run_python_bt2() {
	local -x "BABELTRACE_PLUGIN_PATH=${BT_TESTS_BABELTRACE_PLUGIN_PATH}"
	local -x "LIBBABELTRACE2_PLUGIN_PROVIDER_DIR=${BT_TESTS_PROVIDER_DIR}"
	local -x "BT_TESTS_DATADIR=${BT_TESTS_DATADIR}"
	local -x "BT_CTF_TRACES_PATH=${BT_CTF_TRACES_PATH}"
	local -x "BT_PLUGINS_PATH=${_bt_tests_plugins_path}"
	local -x "PYTHONPATH=${BT_TESTS_PYTHONPATH}${PYTHONPATH:+:}${PYTHONPATH:-}"

	local -r main_lib_path="${BT_TESTS_BUILDDIR}/../src/lib/.libs"

	# Set the library search path so that the Python 3 interpreter can
	# load `libbabeltrace2`.
	if [ "$BT_TESTS_OS_TYPE" = "mingw" ] || [ "$BT_TESTS_OS_TYPE" = "cygwin" ]; then
		local -x PATH="${main_lib_path}${PATH:+:}${PATH:-}"
	elif [ "$BT_TESTS_OS_TYPE" = "darwin" ]; then
		local -x DYLD_LIBRARY_PATH="${main_lib_path}${DYLD_LIBRARY_PATH:+:}${DYLD_LIBRARY_PATH:-}"
	else
		local -x LD_LIBRARY_PATH="${main_lib_path}${LD_LIBRARY_PATH:+:}${LD_LIBRARY_PATH:-}"
	fi

	# On Windows, an embedded Python 3 interpreter needs a way to locate
	# the path to its internal modules: set the `PYTHONHOME` variable to
	# the prefix from `python3-config`.
	if [ "$BT_TESTS_OS_TYPE" = "mingw" ]; then
		local -x PYTHONHOME

		PYTHONHOME=$("$BT_TESTS_PYTHON_CONFIG_BIN" --prefix)
	fi

	# If AddressSanitizer is used, we must preload `libasan.so` so that
	# libasan doesn't complain about not being the first loaded library.
	#
	# Python and sed (executed as part of the Libtool wrapper) produce
	# some leaks, so we must unfortunately disable leak detection.
	#
	# Append it to existing `ASAN_OPTIONS` variable, such that we
	# override the user's value if it contains `detect_leaks=1`.
	if [ "${BT_TESTS_ENABLE_ASAN:-}" = "1" ]; then
		if "${BT_TESTS_CC_BIN}" --version | head -n 1 | bt_grep -q '^gcc'; then
			local -r lib_asan="$("${BT_TESTS_CC_BIN}" -print-file-name=libasan.so)"
			local -x LD_PRELOAD="${lib_asan}${LD_PRELOAD:+:}${LD_PRELOAD:-}"
		fi

		local -x  "ASAN_OPTIONS=${ASAN_OPTIONS:-}${ASAN_OPTIONS:+,}detect_leaks=0"
	fi

	run_python "$@"
}

# Runs the Python tests matching the pattern `$2` (optional, `*` if
# missing) in the directory `$1` using `testrunner.py`.
#
# This function uses run_python_bt2(), therefore such tests can import
# the testing Python modules (in `tests/utils/python`) and the `bt2`
# Python package.
run_python_bt2_test() {
	local -r test_dir="$1"
	local -r test_pattern="${2:-'*'}"

	local python_exec

	if test "${BT_TESTS_COVERAGE:-}" = "1"; then
		python_exec="_bt_tests_check_coverage"
	else
		python_exec="${BT_TESTS_PYTHON_BIN}"
	fi

	run_python_bt2 \
		"${python_exec}" \
		"${BT_TESTS_SRCDIR}/utils/python/testrunner.py" \
		--pattern "$test_pattern" \
		"$test_dir" \

	local -r ret=$?

	if test "${BT_TESTS_COVERAGE_REPORT:-}" = "1"; then
		coverage report -m
	fi

	if test "${BT_TESTS_COVERAGE_HTML:-}" = "1"; then
		coverage html
	fi

	return $ret
}

# Generates a CTF trace into the directory `$2` from the moultipart
# document `$1` using `mctf.py`.
gen_mctf_trace() {
	local -r input_file="$1"
	local -r base_dir="$2"
	local -r cmd=(
		"$BT_TESTS_PYTHON_BIN" "$BT_TESTS_SRCDIR/utils/python/mctf.py"
		--base-dir "$base_dir"
		"$input_file"
	)

	echo "Running: \`${cmd[*]}\`" >&2
	run_python "${cmd[@]}"
}
