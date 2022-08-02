#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 Simon Marchi <simon.marchi@efficios.com>
#

# Test how parameters are applied to sources auto-discovered by the convert
# command.

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../../utils/utils.sh"
fi

# shellcheck source=../../utils/utils.sh
SH_TAP=1 source "$UTILSSH"

NUM_TESTS=4

plan_tests $NUM_TESTS

data_dir="${BT_TESTS_DATADIR}/auto-source-discovery/params-log-level"
plugin_dir="${data_dir}"
dir_a="${data_dir}/dir-a"
dir_b="${data_dir}/dir-b"
dir_ab="${data_dir}/dir-ab"

expected_file=$(mktemp -t expected.XXXXXX)

print_test_params=("--params" 'what="test-params"')
details_sink=("-c" "sink.text.details" "--params=with-metadata=false")

# Apply params to two components from one non-option argument.
cat > "$expected_file" <<END
{Trace 0, Stream class ID 0, Stream ID 0}
Stream beginning:
  Name: TestSourceA: ('test-allo', 'madame')
  Trace:
    Stream (ID 0, Class ID 0)

{Trace 1, Stream class ID 0, Stream ID 0}
Stream beginning:
  Name: TestSourceB: ('test-allo', 'madame')
  Trace:
    Stream (ID 0, Class ID 0)

{Trace 0, Stream class ID 0, Stream ID 0}
Stream end

{Trace 1, Stream class ID 0, Stream ID 0}
Stream end
END

bt_diff_cli "$expected_file" "/dev/null" \
	--plugin-path "${plugin_dir}" convert \
	"${dir_ab}" --params 'test-allo="madame"' "${print_test_params[@]}" \
	"${details_sink[@]}"
ok "$?" "apply params to two components from one non-option argument"

# Apply params to two components from two distinct non-option arguments.
cat > "$expected_file" <<END
{Trace 0, Stream class ID 0, Stream ID 0}
Stream beginning:
  Name: TestSourceA: ('test-allo', 'madame')
  Trace:
    Stream (ID 0, Class ID 0)

{Trace 1, Stream class ID 0, Stream ID 0}
Stream beginning:
  Name: TestSourceB: ('test-bonjour', 'monsieur')
  Trace:
    Stream (ID 0, Class ID 0)

{Trace 0, Stream class ID 0, Stream ID 0}
Stream end

{Trace 1, Stream class ID 0, Stream ID 0}
Stream end
END

bt_diff_cli "$expected_file" "/dev/null" \
	--plugin-path "${plugin_dir}" convert \
	"${dir_a}" --params 'test-allo="madame"' "${print_test_params[@]}" "${dir_b}" --params 'test-bonjour="monsieur"' "${print_test_params[@]}" \
	"${details_sink[@]}"
ok "$?" "apply params to two non-option arguments"

# Apply params to one component coming from one non-option argument and one component coming from two non-option arguments (1).
cat > "$expected_file" <<END
{Trace 0, Stream class ID 0, Stream ID 0}
Stream beginning:
  Name: TestSourceA: ('test-allo', 'madame'), ('test-bonjour', 'monsieur')
  Trace:
    Stream (ID 0, Class ID 0)

{Trace 1, Stream class ID 0, Stream ID 0}
Stream beginning:
  Name: TestSourceB: ('test-bonjour', 'monsieur')
  Trace:
    Stream (ID 0, Class ID 0)

{Trace 0, Stream class ID 0, Stream ID 0}
Stream end

{Trace 1, Stream class ID 0, Stream ID 0}
Stream end
END

bt_diff_cli "$expected_file" "/dev/null" \
	--plugin-path "${plugin_dir}" convert \
	"${dir_a}" --params 'test-allo="madame"' "${print_test_params[@]}" "${dir_ab}" --params 'test-bonjour="monsieur"' "${print_test_params[@]}" \
	"${details_sink[@]}"
ok "$?" "apply params to one component coming from one non-option argument and one component coming from two non-option arguments (1)"

# Apply params to one component coming from one non-option argument and one component coming from two non-option arguments (2).
cat > "$expected_file" <<END
{Trace 0, Stream class ID 0, Stream ID 0}
Stream beginning:
  Name: TestSourceA: ('test-bonjour', 'monsieur'), ('test-salut', 'les amis')
  Trace:
    Stream (ID 0, Class ID 0)

{Trace 1, Stream class ID 0, Stream ID 0}
Stream beginning:
  Name: TestSourceB: ('test-bonjour', 'madame'), ('test-salut', 'les amis')
  Trace:
    Stream (ID 0, Class ID 0)

{Trace 0, Stream class ID 0, Stream ID 0}
Stream end

{Trace 1, Stream class ID 0, Stream ID 0}
Stream end
END

bt_diff_cli "$expected_file" "/dev/null" \
	--plugin-path "${plugin_dir}" convert \
	"${dir_ab}" --params 'test-bonjour="madame",test-salut="les amis"' "${print_test_params[@]}" "${dir_a}" --params 'test-bonjour="monsieur"' "${print_test_params[@]}" \
	"${details_sink[@]}"
ok "$?" "apply params to one component coming from one non-option argument and one component coming from two non-option arguments (2)"

rm -f "$expected_file"
