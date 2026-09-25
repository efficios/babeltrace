# SPDX-FileCopyrightText: 2019 Philippe Proulx <pproulx@efficios.com>
# SPDX-FileCopyrightText: 2025 EfficiOS Inc.
# SPDX-License-Identifier: GPL-2.0-only

import bt2
import pytest
import bt_tests_utils as btu


def _find_expect_file(expect_dir, test_name, ctf_version, mip_version):
    # Try different naming patterns in order of specificity
    candidates = [
        expect_dir / f"{test_name}-ctf{ctf_version}-mip{mip_version}.expect",
        expect_dir / f"{test_name}-ctf{ctf_version}.expect",
        expect_dir / f"{test_name}-mip{mip_version}.expect",
        expect_dir / f"{test_name}.expect",
    ]

    for candidate in candidates:
        if candidate.exists():
            return candidate

    raise FileNotFoundError(
        f"Could not find expectation file for test `{test_name}`, CTF `{ctf_version}`, MIP `{mip_version}`"
    )


@pytest.mark.parametrize(
    ["ctf_version", "mip_version"],
    # Valid CTF/MIP combinations: CTF 1.8 with any MIP, or any CTF with
    # MIP 1 (CTF 2 + MIP 0 is invalid).
    [
        pytest.param(1, 0, id="ctf-1.8-mip-0"),
        pytest.param(1, 1, id="ctf-1.8-mip-1"),
        pytest.param(2, 1, id="ctf-2-mip-1"),
    ],
)
@pytest.mark.parametrize(
    ["test_name", "trace_name", "details_params"],
    [
        ("default", "wk-heartbeat-u", {}),
        ("default-compact", "wk-heartbeat-u", {"compact": True}),
        (
            "default-compact-without-metadata",
            "wk-heartbeat-u",
            {"compact": True, "with-metadata": False},
        ),
        (
            "default-compact-without-time",
            "wk-heartbeat-u",
            {"compact": True, "with-time": False},
        ),
        ("default-without-data", "wk-heartbeat-u", {"with-data": False}),
        (
            "default-without-data-without-metadata",
            "wk-heartbeat-u",
            {"with-data": False, "with-metadata": False},
        ),
        ("default-without-metadata", "wk-heartbeat-u", {"with-metadata": False}),
        ("default-without-time", "wk-heartbeat-u", {"with-time": False}),
        ("default-without-trace-name", "wk-heartbeat-u", {"with-trace-name": False}),
        # `with-uuid` only applies to MIP 0 and `with-uid` only to MIP 1
        (
            "default-without-uuid",
            "wk-heartbeat-u",
            {"with-uuid": False, "with-uid": False},
        ),
        ("no-packet-context", "no-packet-context", {}),
    ],
)
def test_details_succeed(
    ctf_traces_dir,
    test_name,
    trace_name,
    details_params,
    ctf_version,
    mip_version,
):
    # Always disable stream name (contains absolute paths from `src.ctf.fs`)
    params = {"with-stream-name": False}
    params.update(details_params)

    btu.convert_sink_text_details_test(
        bt2.AutoSourceComponentSpec(
            str(ctf_traces_dir / str(ctf_version) / "succeed" / trace_name),
            params={"trace-name": "the-trace"},
        ),
        _find_expect_file(
            btu.this_src_dir(__file__),
            test_name,
            ctf_version,
            mip_version,
        ),
        details_params=params,
        mip_version=mip_version,
    )


# Message iterator emitting a single event within a named stream of a
# named stream class of a named trace.
#
# With MIP 1, the trace and the stream class also have a namespace and
# a UID.
class _NamesMsgIter(bt2._UserMessageIterator):
    def __init__(self, config, port):
        tc = port.user_data._create_trace_class()
        mip1_kwargs = {}

        if port.user_data._graph_mip_version >= 1:
            mip1_kwargs = {"namespace": "the-ns", "uid": "the-uid"}

        sc = tc.create_stream_class(name="the-sc", **mip1_kwargs)
        ec = sc.create_event_class(name="the-ec")
        stream = tc(name="the-trace", **mip1_kwargs).create_stream(
            sc, name="the-stream"
        )
        self._msgs = [
            self._create_stream_beginning_message(stream),
            self._create_event_message(ec, stream),
            self._create_stream_end_message(stream),
        ]

    def __next__(self):
        if len(self._msgs) == 0:
            raise StopIteration

        return self._msgs.pop(0)


class _NamesSrc(bt2._UserSourceComponent, message_iterator_class=_NamesMsgIter):
    def __init__(self, config, params, obj):
        self._add_output_port("out", self)


@pytest.mark.parametrize(
    ["test_name", "mip_version", "details_params"],
    [
        ("names", 0, {}),
        ("names", 1, {}),
        ("names-without-stream-class-name", 0, {"with-stream-class-name": False}),
        ("names-without-stream-class-name", 1, {"with-stream-class-name": False}),
        (
            "names-without-stream-class-namespace",
            1,
            {"with-stream-class-namespace": False},
        ),
        ("names-without-stream-name", 1, {"with-stream-name": False}),
        ("names-without-uid", 1, {"with-uid": False}),
        (
            "names-without-names",
            0,
            {
                "with-trace-name": False,
                "with-stream-class-name": False,
                "with-stream-name": False,
            },
        ),
        (
            "names-without-names",
            1,
            {
                "with-trace-name": False,
                "with-stream-class-name": False,
                "with-stream-name": False,
            },
        ),
    ],
)
def test_details_names(test_name, mip_version, details_params):
    btu.convert_sink_text_details_test(
        bt2.ComponentSpec(_NamesSrc),
        btu.this_src_dir(__file__) / f"{test_name}-mip{mip_version}.expect",
        details_params=details_params,
        mip_version=mip_version,
    )
