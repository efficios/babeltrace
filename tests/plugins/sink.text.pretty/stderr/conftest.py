# SPDX-FileCopyrightText: 2026 EfficiOS Inc.
# SPDX-License-Identifier: GPL-2.0-only

# pyright: strict, reportMissingTypeStubs=false, reportPrivateUsage=false

import re
import pathlib
from typing import Any, Dict, ClassVar, FrozenSet

import pytest
import moultipart
import bt_tests_utils as btu
import bt_tests_cli_utils as btu_cli
import bt_tests_mctf_utils as btu_mctf


class _StderrMctfItem(btu_mctf.MCtfTestItem):
    _allowed_non_ctf_parts: ClassVar[FrozenSet[str]] = frozenset(
        {"@params", "@stderr-re"}
    )
    _required_non_ctf_parts: ClassVar[FrozenSet[str]] = frozenset({"@stderr-re"})

    def _check(
        self,
        trace_dir: pathlib.Path,
        non_ctf_parts: Dict[str, moultipart.Part],
    ) -> None:
        pretty_params: Dict[str, Any] = {"color": "never"}

        if "@params" in non_ctf_parts:
            pretty_params.update(btu.params_from_part(non_ctf_parts["@params"].content))

        actual_stderr = btu_cli.run_cli(
            btu.config_build_root_dir(self.config),
            [
                "-c",
                "src.ctf.fs",
                btu_cli.CliParams(
                    {
                        "inputs": [str(trace_dir)],
                        "trace-name": "the-trace",
                    }
                ),
                "-c",
                "sink.text.pretty",
                "--log-level=W",
                btu_cli.CliParams(pretty_params),
            ],
            check=True,
        ).stderr.strip()

        expected_pattern = non_ctf_parts["@stderr-re"].content.strip()

        if not re.fullmatch(expected_pattern, actual_stderr):
            raise btu_mctf.MCtfMismatchError(
                self.mctf_path,
                "Expected `sink.text.pretty` standard error regex",
                expected_pattern,
                "Actual `sink.text.pretty` standard error",
                actual_stderr,
            )


# pytest hook.
def pytest_collect_file(file_path: pathlib.Path, parent: pytest.Collector):
    return btu_mctf.collect_mctf_file(
        file_path, parent, _StderrMctfItem, log_label="stderr"
    )
