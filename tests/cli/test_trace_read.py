# SPDX-FileCopyrightText: 2013 Christian Babeux <christian.babeux@efficios.com>
# SPDX-FileCopyrightText: 2025 EfficiOS Inc.
# SPDX-License-Identifier: GPL-2.0-only

import bt_tests_cli_utils as btu_cli


def pytest_generate_tests(metafunc):
    if "fail_trace" in metafunc.fixturenames:
        traces = sorted((metafunc.config.ctf_traces_dir / "1/fail").iterdir())
        metafunc.parametrize("fail_trace", traces, ids=[t.name for t in traces])


def test_fail_trace(build_root_dir, fail_trace):
    assert btu_cli.run_cli(build_root_dir, [str(fail_trace)]).returncode != 0
