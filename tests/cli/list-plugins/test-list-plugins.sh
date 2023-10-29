#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 EfficiOS Inc.
#

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../../utils/utils.sh"
fi

# shellcheck source=../../utils/utils.sh
SH_TAP=1 source "$UTILSSH"

plan_tests 3

data_dir="${BT_TESTS_DATADIR}/cli/list-plugins"
plugin_dir="${data_dir}"

stdout_file=$(mktemp -t test-cli-list-plugins-stdout.XXXXXX)
stderr_file=$(mktemp -t test-cli-list-plugins-stderr.XXXXXX)
grep_stdout_file=$(mktemp -t test-cli-list-plugins-grep-stdout.XXXXXX)
py_plugin_expected_stdout_file=$(mktemp -t test-cli-list-plugins-expected-py-plugin-stdout.XXXXXX)

# Run list-plugins.
bt_cli "$stdout_file" "$stderr_file" \
	--plugin-path "$plugin_dir" \
	list-plugins
ok "$?" "exit code is 0"

# Extract the section about our custom this-is-a-plugin Python plugin.
grep --after-context=11 '^this-is-a-plugin:$' "${stdout_file}" > "${grep_stdout_file}"
ok "$?" "entry for this-is-a-plugin is present"

if [ "$BT_TESTS_OS_TYPE" = "mingw" ]; then
       platform_plugin_dir=$(cygpath -m "${plugin_dir}")
else
       platform_plugin_dir="${plugin_dir}"
fi

# Generate the expected output file for that plugin.
cat <<- EOF > "${py_plugin_expected_stdout_file}"
	this-is-a-plugin:
	  Path: ${platform_plugin_dir}/bt_plugin_list_plugins.py
	  Version: 1.2.3bob
	  Description: A plugin
	  Author: Jorge Mario Bergoglio
	  License: The license
	  Source component classes:
	    'source.this-is-a-plugin.ThisIsASource'
	  Filter component classes:
	    'filter.this-is-a-plugin.ThisIsAFilter'
	  Sink component classes:
	    'sink.this-is-a-plugin.ThisIsASink'
EOF

# Compare the entry for this-is-a-plugin with the expected version.
bt_diff "${py_plugin_expected_stdout_file}" "${grep_stdout_file}"
ok "$?" "entry for this-is-a-plugin is as expected"

rm -f "${stdout_file}"
rm -f "${stderr_file}"
rm -f "${grep_stdout_file}"
rm -f "${py_plugin_expected_stdout_file}"
