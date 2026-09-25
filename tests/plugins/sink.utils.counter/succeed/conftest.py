# SPDX-FileCopyrightText: 2026 EfficiOS Inc.
# SPDX-License-Identifier: GPL-2.0-only

# pyright: strict, reportMissingTypeStubs=false, reportPrivateUsage=false

import typing
import pathlib
from typing import Dict, List, ClassVar, FrozenSet

import pytest
import moultipart
import bt_tests_utils as btu
import bt_tests_cli_utils as btu_cli
import bt_tests_mctf_utils as btu_mctf


class _CounterMctfItem(btu_mctf.MCtfTestItem):
    _allowed_non_ctf_parts: ClassVar[FrozenSet[str]] = frozenset({"@params", "@output"})
    _required_non_ctf_parts: ClassVar[FrozenSet[str]] = frozenset({"@output"})

    def _check(
        self,
        trace_dir: pathlib.Path,
        non_ctf_parts: Dict[str, moultipart.Part],
    ) -> None:
        cli_args: List[typing.Union[str, btu_cli.CliParams]] = [
            "-c",
            "src.ctf.fs",
            btu_cli.CliParams({"inputs": [str(trace_dir)]}),
            "-c",
            "sink.utils.counter",
        ]

        if "@params" in non_ctf_parts:
            counter_params = btu.params_from_part(non_ctf_parts["@params"].content)

            if counter_params:
                cli_args.append(btu_cli.CliParams(counter_params))

        actual_output = btu_cli.run_cli(
            btu.config_build_root_dir(self.config),
            cli_args,
            check=True,
        ).stdout.strip()

        expected_output = non_ctf_parts["@output"].content.strip()

        if actual_output != expected_output:
            raise btu_mctf.MCtfMismatchError(
                self.mctf_path,
                "Expected `sink.utils.counter` output",
                expected_output,
                "Actual `sink.utils.counter` output",
                actual_output,
            )


# pytest hook.
def pytest_collect_file(file_path: pathlib.Path, parent: pytest.Collector):
    return btu_mctf.collect_mctf_file(
        file_path, parent, _CounterMctfItem, log_label="succeed"
    )
