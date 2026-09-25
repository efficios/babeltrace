# SPDX-FileCopyrightText: 2019 Simon Marchi <simon.marchi@efficios.com>
# SPDX-FileCopyrightText: 2025 EfficiOS Inc.
# SPDX-License-Identifier: GPL-2.0-only

import bt_tests_utils as btu
import bt_tests_cli_utils as btu_cli


def test_valid_trace_output(ctf_traces_dir, build_root_dir):
    result = btu_cli.run_cli(
        build_root_dir,
        ["-o", "ctf-metadata", str(ctf_traces_dir / "1/succeed/wk-heartbeat-u")],
        check=True,
    )

    expected = (
        btu.this_src_dir(__file__) / "test-output-ctf-metadata.expect"
    ).read_text(encoding="utf-8")

    assert result.stdout == expected


def test_invalid_trace_exit_status(ctf_traces_dir, build_root_dir):
    # Using the root `ctf-traces` directory (not a valid trace
    # directory) should fail.
    assert (
        btu_cli.run_cli(
            build_root_dir, ["-o", "ctf-metadata", str(ctf_traces_dir)]
        ).returncode
        != 0
    )
