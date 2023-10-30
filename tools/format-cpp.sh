#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2020-2023 Philippe Proulx <pproulx@efficios.com>

expected_formatter_major_version=15

# Runs the formatter, returning 1 if it's not the expected version.
#
# Argument 1:
#     Starting directory.
#
# Remaining arguments:
#     Formatter command.
format_cpp() {
	local root_dir=$1

	shift

	local formatter=("$@")
	local version

	if ! version=$("${formatter[@]}" --version); then
		echo "Cannot execute \`${formatter[*]} --version\`." >&2
		return 1
	fi

	if [[ "$version" != *"clang-format version $expected_formatter_major_version"* ]]; then
		{
			echo "Expecting clang-format $expected_formatter_major_version."
			echo
			echo Got:
			echo
			echo "$version"
		} >& 2

		return 1
	fi

	# Using xargs(1) to fail as soon as the formatter fails (`-exec`
	# won't stop if its subprocess fails).
	#
	# We want an absolute starting directory because find(1) excludes
	# files in specific subdirectories.
	find "$(realpath "$root_dir")" \( -name '*.cpp' -o -name '*.hpp' \) \
		! -path '*/src/cpp-common/optional.hpp' \
		! -path '*/src/cpp-common/string_view.hpp' \
		! -path '*/src/cpp-common/nlohmann/json.hpp' \
		! -path '*/src/plugins/ctf/common/metadata/parser.*' \
		! -path '*/src/plugins/ctf/common/metadata/lexer.*' \
		-print0 | xargs -P"$(nproc)" -n1 -t -0 "${formatter[@]}"
}

# Choose formatter
if [[ -n "$FORMATTER" ]]; then
	# Try using environment-provided formatter
	read -ra formatter <<< "$FORMATTER"
elif command -v clang-format-$expected_formatter_major_version &> /dev/null; then
	# Try using the expected version of clang-format
	formatter=("clang-format-$expected_formatter_major_version" -i)
else
	# Try using `clang-format` as is
	formatter=(clang-format -i)
fi

# Choose root directory
if (($# == 1)); then
	root_dir=$1

	if [[ ! -d "$root_dir" ]]; then
		echo "\`$root_dir\`: expecting an existing directory." >& 2
		exit 1
	fi
else
	# Default: root of the project, processing all C++ files
	root_dir="$(dirname "${BASH_SOURCE[0]}")/.."
fi

# Try to format files
format_cpp "$root_dir" "${formatter[@]}"
