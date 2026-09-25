# SPDX-FileCopyrightText: 2019-2025 Efficios, Inc.
# SPDX-License-Identifier: GPL-2.0-only

import pathlib
import tempfile

import bt2
import pytest
import bt_tests_utils as btu


def _find_expect_file(expect_dir, test_name, ctf_version, mip_version):
    # Try different naming patterns in order of specificity
    candidates = [
        (expect_dir / f"trace-{test_name}-ctf{ctf_version}-mip{mip_version}.expect"),
        expect_dir / f"trace-{test_name}-ctf{ctf_version}.expect",
        expect_dir / f"trace-{test_name}-mip{mip_version}.expect",
        expect_dir / f"trace-{test_name}.expect",
    ]

    for candidate in candidates:
        if candidate.exists():
            return candidate

    raise FileNotFoundError(
        f"Could not find expectation file for test `{test_name}`, CTF `{ctf_version}`, MIP `{mip_version}`"
    )


# Valid CTF/MIP combinations: CTF 1.8 with any MIP, or any CTF with
# MIP 1 (CTF 2 + MIP 0 is invalid).
_CTF_MIP_COMBOS = [
    pytest.param(1, 0, id="ctf-1.8-mip-0"),
    pytest.param(1, 1, id="ctf-1.8-mip-1"),
    pytest.param(2, 1, id="ctf-2-mip-1"),
]


@pytest.fixture(scope="module")
def expect_dir():
    return btu.this_src_dir(__file__)


# Test for generated trace using the `gen-trace-simple` binary.
def test_generated_trace_simple(build_root_dir, expect_dir, tmp_path):
    trace_dir = tmp_path / "trace"
    trace_dir.mkdir()

    # Generate trace using `gen-trace-simple.bin`
    btu.run(
        build_root_dir,
        btu.build_dir_of_source_file(build_root_dir, __file__) / "gen-trace-simple.bin",
        [str(trace_dir)],
        check=True,
    )

    # Validate contents
    btu.convert_sink_text_details_test(
        trace_dir,
        expect_dir / "trace-simple.expect",
        details_params={
            "with-trace-name": False,
            "with-stream-name": False,
            "with-uuid": False,
            "with-uid": False,
        },
    )


# Traces which exist in both CTF 1.8 and CTF 2, tested with each valid
# CTF/MIP combination.
_MULTI_CTF_VERSION_TRACE_NAMES = [
    "smalltrace",
    "2packets",
    "barectf-event-before-packet",
    "session-rotation",
    "lttng-tracefile-rotation",
    "array-align-elem",
    "struct-array-align-elem",
    "meta-ctx-sequence",
]


@pytest.mark.parametrize(
    ["trace_name", "ctf_version", "mip_version"],
    [
        pytest.param(trace_name, *combo.values, id=f"{trace_name}-{combo.id}")
        for trace_name in _MULTI_CTF_VERSION_TRACE_NAMES
        for combo in _CTF_MIP_COMBOS
    ]
    + [
        # Traces which exist in a single CTF version
        pytest.param(
            "meta-clk-cls-before-trace-cls",
            2,
            1,
            id="meta-clk-cls-before-trace-cls-ctf-2-mip-1",
        ),
        pytest.param("def-clk-freq", 1, 0, id="def-clk-freq-ctf-1.8-mip-0"),
        pytest.param("def-clk-freq", 1, 1, id="def-clk-freq-ctf-1.8-mip-1"),
    ],
)
def test_trace(ctf_traces_dir, expect_dir, trace_name, ctf_version, mip_version):
    btu.convert_sink_text_details_test(
        ctf_traces_dir / str(ctf_version) / "succeed" / trace_name,
        _find_expect_file(expect_dir, trace_name, ctf_version, mip_version),
        details_params={"with-trace-name": False, "with-stream-name": False},
        mip_version=mip_version,
    )


# Test for packetized metadata with trailing byte.
def test_meta_trailing_byte(ctf_traces_dir, dummy_comp_cls, capfd):
    btu.convert(
        bt2.AutoSourceComponentSpec(
            str(ctf_traces_dir / "1/succeed/meta-trailing-byte"),
            logging_level=bt2.LoggingLevel.WARNING,
        ),
        btu.SinkComponentSpec(dummy_comp_cls),
    )

    assert (
        "Remaining buffer isn't large enough to hold a packet header"
        in capfd.readouterr().err
    )


# Test that packet end messages are correctly emitted for traces with
# event records occurring after the packet end timestamp.
@pytest.mark.parametrize(["ctf_version", "mip_version"], _CTF_MIP_COMBOS)
@pytest.mark.parametrize("trace_name", ["lttng-event-after-packet", "lttng-crash"])
def test_packet_end(
    ctf_traces_dir, expect_dir, details_comp_cls, trace_name, ctf_version, mip_version
):
    with tempfile.TemporaryDirectory(prefix="bt-test-packet-end-") as temp_dir:
        temp_path = pathlib.Path(temp_dir) / "output.txt"

        btu.convert(
            ctf_traces_dir / str(ctf_version) / "succeed" / trace_name,
            btu.SinkComponentSpec(
                details_comp_cls,
                {
                    "path": str(temp_path),
                    "with-trace-name": False,
                    "with-stream-name": False,
                    "with-metadata": False,
                    "compact": True,
                    "color": "never",
                },
            ),
            mip_version=mip_version,
        )

        # Filter only `Packet end` lines from output
        filtered_output = "\n".join(
            line
            for line in temp_path.read_text(encoding="utf-8").splitlines()
            if "Packet end" in line
        )

        expected = (
            _find_expect_file(expect_dir, trace_name, ctf_version, mip_version)
            .read_text(encoding="utf-8")
            .strip()
        )

        assert filtered_output.strip() == expected


# Test for clock offset going back in time (generated from
# `.mctf` files).
def test_clock_offset_goes_back_in_time(expect_dir, materialize_mctf):
    mctf_dir = expect_dir / "clock-offset-goes-back-in-time"
    btu.convert_sink_text_details_test(
        [
            materialize_mctf(mctf_dir / "chunk1.mctf").path,
            materialize_mctf(mctf_dir / "chunk2.mctf").path,
        ],
        expect_dir / "trace-clock-offset-goes-back-in-time.expect",
        details_params={"with-stream-name": False},
    )


# Test the `force-clock-class-origin-unix-epoch` parameter with
# two traces.
@pytest.mark.parametrize(["ctf_version", "mip_version"], _CTF_MIP_COMBOS)
def test_force_origin_unix_epoch(ctf_traces_dir, expect_dir, ctf_version, mip_version):
    btu.convert_sink_text_details_test(
        [
            bt2.AutoSourceComponentSpec(
                str(ctf_traces_dir / str(ctf_version) / "succeed/2packets"),
                params={"force-clock-class-origin-unix-epoch": True},
            ),
            bt2.AutoSourceComponentSpec(
                str(
                    ctf_traces_dir
                    / str(ctf_version)
                    / "succeed/barectf-event-before-packet"
                ),
                params={"force-clock-class-origin-unix-epoch": True},
            ),
        ],
        _find_expect_file(
            expect_dir, "2packets-barectf-event-before-packet", ctf_version, mip_version
        ),
        details_params={
            "with-trace-name": False,
            "with-stream-name": False,
            "with-metadata": True,
            "compact": True,
        },
        mip_version=mip_version,
    )
