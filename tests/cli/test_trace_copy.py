# SPDX-FileCopyrightText: 2017 Julien Desfossez <jdesfossez@efficios.com>
# SPDX-FileCopyrightText: 2025 EfficiOS Inc.
# SPDX-License-Identifier: GPL-2.0-only

import pytest
import bt_tests_cli_utils as btu_cli


# Returns a list of (CTF version, trace path) tuples for all
# succeed traces.
def _succeed_traces(ctf_traces_dir):
    traces = []

    for ctf_version in 1, 2:
        traces_dir = ctf_traces_dir / str(ctf_version) / "succeed"

        for trace_path in sorted(traces_dir.iterdir()):
            traces.append((ctf_version, trace_path))

    return traces


def pytest_generate_tests(metafunc):
    if "ctf_version" in metafunc.fixturenames and "trace_path" in metafunc.fixturenames:
        traces = _succeed_traces(metafunc.config.ctf_traces_dir)
        metafunc.parametrize(
            ["ctf_version", "trace_path"],
            traces,
            ids=[f"{p.name}-ctf-{'1.8' if v == 1 else '2'}" for v, p in traces],
        )


# Checks if the trace output has duplicate timestamps.
#
# Timestamps are in the first column (for example, `[123:456]`).
#
# Returns `True` if there are duplicate timestamps.
def _has_duplicate_timestamps(output_lines):
    if not output_lines:
        return False

    # Only check if output starts with timestamp format
    if not output_lines[0].startswith("["):
        return False

    # Extract timestamps (first column)
    timestamps = []

    for line in output_lines:
        parts = line.split()

        if parts:
            timestamps.append(parts[0])

    # Check for duplicates by comparing unique count to total count
    return len(set(timestamps)) != len(timestamps)


# Tests that copying a trace with `sink.ctf.fs` produces an
# identical trace.
#
# For each CTF version and each success trace:
#
# 1. Read original trace with `--no-delta`, capture output.
# 2. If empty output, skip (empty trace).
# 3. Check for duplicate timestamps; if found, sort output for comparison.
# 4. Copy trace using `sink.ctf.fs`.
# 5. Read copied trace with `--no-delta` and compare with original.
def test_trace_copy(build_root_dir, ctf_version, trace_path, tmp_path):
    trace_name = trace_path.name

    # Read original trace with `--no-delta`
    orig_result = btu_cli.run_cli(
        build_root_dir,
        ["--no-delta", str(trace_path)],
        check=True,
    )

    orig_output = orig_result.stdout
    orig_lines = orig_output.strip().split("\n") if orig_output.strip() else []

    # Skip empty trace
    if not orig_lines or (len(orig_lines) == 1 and orig_lines[0] == ""):
        pytest.skip(f"Empty trace `{trace_name}`: nothing to copy")

    # Check for duplicate timestamps and sort if needed
    needs_sorting = _has_duplicate_timestamps(orig_lines)

    if needs_sorting:
        orig_lines = sorted(orig_lines)

    # Copy trace using `sink.ctf.fs`
    btu_cli.run_cli(
        build_root_dir,
        [
            str(trace_path),
            "-c",
            "sink.ctf.fs",
            btu_cli.CliParams({"path": str(tmp_path), "ctf-version": str(ctf_version)}),
        ],
        check=True,
    )

    # Read copied trace with `--no-delta` and compare
    copied_result = btu_cli.run_cli(
        build_root_dir,
        ["--no-delta", str(tmp_path)],
        check=True,
    )
    copied_output = copied_result.stdout
    copied_lines = copied_output.strip().split("\n") if copied_output.strip() else []

    if needs_sorting:
        copied_lines = sorted(copied_lines)

    # Compare original and copied trace output
    assert orig_lines == copied_lines
