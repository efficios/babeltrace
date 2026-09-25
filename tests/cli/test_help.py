# SPDX-FileCopyrightText: 2019 EfficiOS Inc.
# SPDX-FileCopyrightText: 2025 EfficiOS Inc.
# SPDX-License-Identifier: GPL-2.0-only

import pytest
import bt_tests_cli_utils as btu_cli


@pytest.mark.parametrize(
    ["args", "success", "expected_stdout", "expected_stderr"],
    [
        pytest.param(
            ["ctf"],
            True,
            "Description: CTF input and output",
            None,
            id="plugin",
        ),
        pytest.param(
            ["src.ctf.fs"],
            True,
            "Description: Read CTF traces from the file system.",
            None,
            id="comp-cls",
        ),
        pytest.param(
            [],
            False,
            None,
            "Missing plugin name or component class descriptor.",
            id="no-param",
        ),
        pytest.param(
            ["ctf", "fs"],
            False,
            None,
            "Extraneous command-line argument specified to `help` command:",
            id="too-many-params",
        ),
        pytest.param(
            ["zigotos"],
            False,
            None,
            'Cannot find plugin: plugin-name="zigotos"',
            id="unknown-plugin",
        ),
        pytest.param(
            ["src.ctf.bob"],
            False,
            "Description: CTF input and output",
            'Cannot find component class: plugin-name="ctf", comp-cls-name="bob", comp-cls-type=SOURCE',
            id="unknown-comp-cls",
        ),
        pytest.param(
            ["src.bob.fs"],
            False,
            None,
            'Cannot find plugin: plugin-name="bob"',
            id="unknown-comp-cls-plugin",
        ),
    ],
)
def test_help(build_root_dir, args, success, expected_stdout, expected_stderr):
    result = btu_cli.run_cli(build_root_dir, ["help", *args])

    assert (result.returncode == 0) == success

    for output, expected in (
        (result.stdout, expected_stdout),
        (result.stderr, expected_stderr),
    ):
        if expected is None:
            assert output == ""
        else:
            assert expected in output
