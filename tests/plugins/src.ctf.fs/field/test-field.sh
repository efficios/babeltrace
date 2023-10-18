#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2023 Efficios, Inc.

SH_TAP=1

if [[ -n ${BT_TESTS_SRCDIR:-} ]]; then
        UTILSSH=$BT_TESTS_SRCDIR/utils/utils.sh
else
        UTILSSH=$(dirname "$0")/../../../utils/utils.sh
fi

# shellcheck source=../../../utils/utils.sh
source "$UTILSSH"

# Directory containing the plugin
data_dir=$BT_TESTS_DATADIR/plugins/src.ctf.fs/field

test_pass() {
    local -r mp_path=$1
    local -r output_dir=$(mktemp -d)

    run_python "$BT_TESTS_PYTHON_BIN" "$data_dir/data_from_mp.py" "$mp_path" "$output_dir"

    local -r res_path=$(mktemp)

    bt_cli "$res_path" /dev/null --plugin-path="$data_dir" \
        -c sink.test-text.single "$output_dir/trace"
    bt_diff "$res_path" "$output_dir/expect"
    ok $? "$mp_path"
    rm -rf "$output_dir" "$res_path"
}

plan_tests 6

for mp_path in "$data_dir"/ctf-1/pass-*.mp; do
    test_pass "$mp_path"
done
