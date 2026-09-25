# SPDX-FileCopyrightText: 2017 Julien Desfossez <jdesfossez@efficios.com>
# SPDX-FileCopyrightText: 2025 EfficiOS Inc.
# SPDX-License-Identifier: GPL-2.0-only

import pytest
import bt_tests_cli_utils as btu_cli


# Counts the number of non-empty lines in `stdout`.
def _count_output_lines(stdout):
    if not stdout.strip():
        return 0

    return len(stdout.strip().split("\n"))


# Environment variables to make the local time zone EST (UTC-05:00).
#
# See <https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html>
# to learn more.
_EST_ENV = {"TZ": "EST5"}


# Runs the CLI on the `wk-heartbeat-u` trace with the additional
# arguments `cli_args` and returns the number of printed events.
def _trimmed_event_count(ctf_traces_dir, build_root_dir, cli_args, extra_env=None):
    result = btu_cli.run_cli(
        build_root_dir,
        [str(ctf_traces_dir / "1/succeed/wk-heartbeat-u")] + cli_args,
        check=True,
        extra_env=extra_env,
    )
    return _count_output_lines(result.stdout)


_BEGIN = "48:17.587029529"
_END = "48:17.588680018"


# Trims with `--begin` and/or `--end`, relative (time only) or absolute
# (date and time), with either the GMT or the EST time zone.
#
# `begin` and `end` are `(hour, rest)` pairs, `hour` being a GMT hour.
@pytest.mark.parametrize("absolute", [False, True], ids=["relative", "absolute"])
@pytest.mark.parametrize(
    ["tz_args", "tz_env", "hour_offset"],
    [
        pytest.param(["--clock-gmt"], None, 0, id="gmt"),
        pytest.param([], _EST_ENV, -5, id="est"),
    ],
)
@pytest.mark.parametrize(
    ["expected_event_count", "begin", "end"],
    [
        pytest.param(18, (17, _BEGIN), None, id="begin"),
        pytest.param(9, None, (17, _END), id="end"),
        pytest.param(7, (17, _BEGIN), (17, _END), id="begin-and-end"),
        pytest.param(0, (18, _BEGIN), None, id="begin-out-of-range"),
        pytest.param(0, None, (16, _END), id="end-out-of-range"),
    ],
)
def test_trimmer_range(
    ctf_traces_dir,
    build_root_dir,
    absolute,
    tz_args,
    tz_env,
    hour_offset,
    expected_event_count,
    begin,
    end,
):
    def fmt_time(time):
        hour, rest = time
        text = "{:02}:{}".format(hour + hour_offset, rest)
        return "2012-10-29 " + text if absolute else text

    cli_args = list(tz_args)

    if begin is not None:
        cli_args += ["--begin", fmt_time(begin)]

    if end is not None:
        cli_args += ["--end", fmt_time(end)]

    assert (
        _trimmed_event_count(ctf_traces_dir, build_root_dir, cli_args, tz_env)
        == expected_event_count
    )


# Various format tests (using EST timezone). We sometimes apply a clock
# offset to make the events of the trace span two different seconds or
# minutes.
@pytest.mark.parametrize(
    ["expected_event_count", "cli_args"],
    [
        pytest.param(
            13,
            ["--begin=2012-10-29 12:48:17.588"],
            id="date-time-format-partial-nanosecond-precision",
        ),
        pytest.param(
            11,
            ["--clock-offset-ns=411282268", "--begin=2012-10-29 12:48:18"],
            id="date-time-format-second-precision",
        ),
        pytest.param(
            11,
            [
                "--clock-offset=42",
                "--clock-offset-ns=411282268",
                "--begin=2012-10-29 12:49",
            ],
            id="date-time-format-minute-precision",
        ),
        pytest.param(
            11,
            ["--begin=1351532897.588717732"],
            id="seconds-from-origin-format-nanosecond-precision",
        ),
        pytest.param(
            11,
            ["--begin=1351532897.58871773"],
            id="seconds-from-origin-format-partial-nanosecond-precision",
        ),
        pytest.param(
            11,
            ["--clock-offset-ns=411282268", "--begin=1351532898"],
            id="seconds-from-origin-format-second-precision",
        ),
    ],
)
def test_trimmer_formats(
    ctf_traces_dir, build_root_dir, expected_event_count, cli_args
):
    assert (
        _trimmed_event_count(ctf_traces_dir, build_root_dir, cli_args, _EST_ENV)
        == expected_event_count
    )


@pytest.mark.parametrize(
    "cli_args",
    [
        pytest.param(
            ["--begin=2012-10-29 12:48:17.1231231231"],
            id="date-time-format-too-many-nanosecond-digits",
        ),
        pytest.param(
            ["--begin=2012-10-29 12:48:17."],
            id="date-time-format-missing-nanoseconds",
        ),
        pytest.param(
            ["--begin=2012-10-29 12:48:123"],
            id="date-time-format-seconds-with-too-many-digits",
        ),
        pytest.param(
            ["--begin=2012-10-29 12:48:1"],
            id="date-time-format-seconds-with-missing-digit",
        ),
        pytest.param(
            ["--begin=2012-10-29 12:489:17"],
            id="date-time-format-minutes-with-too-many-digits",
        ),
        pytest.param(
            ["--begin=2012-10-29 12:4:17"],
            id="date-time-format-minutes-with-missing-digit",
        ),
        pytest.param(
            ["--begin=2012-10-29 123:48:17"],
            id="date-time-format-hours-with-too-many-digits",
        ),
        pytest.param(
            ["--begin=2012-10-29 2:48:17"],
            id="date-time-format-hours-with-missing-digit",
        ),
        pytest.param(
            ["--begin=2012-10-29 12:48:"],
            id="date-time-format-missing-seconds",
        ),
        pytest.param(
            ["--begin=2012-10-29 12:"],
            id="date-time-format-missing-minutes-1",
        ),
        pytest.param(
            ["--begin=2012-10-29 12"],
            id="date-time-format-missing-minutes-2",
        ),
        pytest.param(
            ["--begin=2012-10-29 "],
            id="date-time-format-missing-time",
        ),
        pytest.param(
            ["--begin=2012-10-291"],
            id="date-time-format-day-with-too-many-digits",
        ),
        pytest.param(
            ["--begin=2012-10-2"],
            id="date-time-format-day-with-missing-digit",
        ),
        pytest.param(
            ["--begin=2012-101-29"],
            id="date-time-format-month-with-too-many-digits",
        ),
        pytest.param(
            ["--begin=2012-1-29"],
            id="date-time-format-month-with-missing-digit",
        ),
        pytest.param(
            ["--begin=20121-10-29"],
            id="date-time-format-year-with-too-many-digits",
        ),
        pytest.param(
            ["--begin=12-10-29"],
            id="date-time-format-year-with-missing-digits",
        ),
        pytest.param(
            ["--begin=2012-10-"],
            id="date-time-format-missing-day-1",
        ),
        pytest.param(
            ["--begin=2012-10"],
            id="date-time-format-missing-day-2",
        ),
        pytest.param(
            ["--begin=1351532898.1231231231"],
            id="seconds-from-origin-format-too-many-nanosecond-digits",
        ),
        pytest.param(
            ["--begin=1351532898."],
            id="seconds-from-origin-format-missing-nanoseconds",
        ),
    ],
)
def test_trimmer_failure(ctf_traces_dir, build_root_dir, cli_args):
    result = btu_cli.run_cli(
        build_root_dir,
        [str(ctf_traces_dir / "1/succeed/wk-heartbeat-u")] + cli_args,
        check=False,
        extra_env={**_EST_ENV, "BABELTRACE_FLT_UTILS_TRIMMER_LOG_LEVEL": "E"},
    )
    assert result.returncode != 0
    assert _count_output_lines(result.stdout) == 0
    assert result.stderr, "Nothing on standard error"
    assert "Invalid date/time format" in result.stderr
