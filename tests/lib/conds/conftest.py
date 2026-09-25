# SPDX-FileCopyrightText: 2020-2025 Philippe Proulx <pproulx@efficios.com>
# SPDX-License-Identifier: MIT

# pyright: strict, reportMissingTypeStubs=false

import os
import re
import signal
import logging
import pathlib
import subprocess
from typing import Any, Set, List, Tuple, Optional

import tjson
import pytest
import bt_tests_utils as btu

_logger = logging.getLogger(__name__)


# Condition trigger descriptor (base).
class _CondTriggerDescriptor:
    def __init__(self, trigger_name: str, cond_id: str):
        self._trigger_name = trigger_name
        self._cond_id = cond_id

    @property
    def trigger_name(self) -> str:
        return self._trigger_name

    @property
    def cond_id(self) -> str:
        return self._cond_id


# Precondition trigger descriptor.
class _PreCondTriggerDescriptor(_CondTriggerDescriptor):
    @property
    def type_str(self) -> str:
        return "pre"


# Postcondition trigger descriptor.
class _PostCondTriggerDescriptor(_CondTriggerDescriptor):
    @property
    def type_str(self) -> str:
        return "post"


# Parses condition trigger descriptors from JSON array.
def _cond_trigger_descriptors_from_json(
    json_descr_array: tjson.ArrayVal,
) -> List[_CondTriggerDescriptor]:
    descriptors: List[_CondTriggerDescriptor] = []
    descriptor_names: Set[str] = set()

    for json_descr in json_descr_array.iter(tjson.ObjVal):
        trigger_name = json_descr.at("name", tjson.StrVal).val

        if trigger_name in descriptor_names:
            raise ValueError(f"Duplicate condition trigger name `{trigger_name}`")

        cond_id = json_descr.at("cond-id", tjson.StrVal).val

        if cond_id.startswith("pre"):
            cond_type = _PreCondTriggerDescriptor
        elif cond_id.startswith("post"):
            cond_type = _PostCondTriggerDescriptor
        else:
            raise ValueError(f"Invalid condition ID `{cond_id}`")

        descriptors.append(cond_type(trigger_name, cond_id))
        descriptor_names.add(trigger_name)

    return descriptors


class _CondTriggerTestItem(pytest.Item):
    def __init__(
        self,
        *,
        descriptor: _CondTriggerDescriptor,
        conds_triggers_bin: pathlib.Path,
        **kwargs: Any,
    ):
        super().__init__(**kwargs)  # pyright: ignore[reportUnknownMemberType]
        self._descriptor = descriptor
        self._conds_triggers_bin = conds_triggers_bin

    def runtest(self) -> None:
        timeout = 5

        # Execute:
        #
        #     $ conds-triggers run <trigger-name>
        #
        # where `<trigger-name>` is the descriptor trigger name.
        _logger.info(f"Running the condition trigger `{self._descriptor.trigger_name}`")

        try:
            result = btu.run(
                btu.config_build_root_dir(self.config),
                self._conds_triggers_bin,
                ["run", self._descriptor.trigger_name],
                timeout=timeout,
            )
        except subprocess.TimeoutExpired:
            pytest.fail(
                f"Condition trigger `{self._descriptor.trigger_name}` hung for {timeout} seconds"
            )

        # Assert that the program aborted (only available on POSIX)
        if os.name == "posix":
            if result.returncode != -int(signal.SIGABRT):
                pytest.fail(
                    f"Expected return code {-int(signal.SIGABRT)} (SIGABRT); got {result.returncode}"
                )

        # Assert that the standard error text contains the condition ID
        if f"Condition ID: `{self._descriptor.cond_id}`" not in result.stderr:
            pytest.fail(
                f"Expected condition ID `{self._descriptor.cond_id}` not found in standard error stream"
            )

    def reportinfo(self) -> Tuple[pathlib.Path, None, str]:
        return self.path, None, self.name


class _CondTriggersTestFile(pytest.File):
    @staticmethod
    def _normalize_trigger_name(trigger_name: str) -> str:
        normalized = re.sub(r"\W", "_", trigger_name)
        return f"test_{normalized}"

    def collect(self) -> List[_CondTriggerTestItem]:
        # Build the path to the `conds-triggers` binary and make sure
        # it exists.
        conds_triggers_bin = btu.exe_path(
            btu.build_dir_of_source_file(
                btu.config_build_root_dir(self.config), __file__
            )
            / "conds-triggers.bin"
        )

        # Skip if the binary doesn't exist
        if not conds_triggers_bin.exists():
            pytest.skip(
                f"`{conds_triggers_bin}` binary doesn't exist",
                allow_module_level=True,
            )
        else:
            assert os.access(conds_triggers_bin, os.X_OK)

        # Create test items
        items: List[_CondTriggerTestItem] = []
        _logger.info(f"Listing condition triggers from `{conds_triggers_bin}`")

        for descriptor in _cond_trigger_descriptors_from_json(
            tjson.loads(
                btu.run(
                    btu.config_build_root_dir(self.config),
                    conds_triggers_bin,
                    ["list"],
                ).stdout,
                tjson.ArrayVal,
            )
        ):
            _logger.info(
                f"Adding test item for condition trigger `{descriptor.trigger_name}`"
            )
            items.append(
                _CondTriggerTestItem.from_parent(  # pyright: ignore[reportUnknownMemberType]
                    name=self._normalize_trigger_name(descriptor.trigger_name),
                    parent=self,
                    descriptor=descriptor,
                    conds_triggers_bin=conds_triggers_bin,
                )
            )

        return items


# pytest hook.
def pytest_collect_file(
    file_path: pathlib.Path, parent: pytest.Collector
) -> Optional[_CondTriggersTestFile]:
    if file_path.name == "conds-triggers.cpp":
        return _CondTriggersTestFile.from_parent(  # pyright: ignore[reportUnknownMemberType]
            parent=parent, path=file_path
        )
