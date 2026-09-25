# SPDX-FileCopyrightText: 2019 Philippe Proulx <pproulx@efficios.com>
# SPDX-FileCopyrightText: 2025 EfficiOS Inc.
# SPDX-License-Identifier: GPL-2.0-only

import pytest
import bt_tests_utils as btu


@pytest.mark.parametrize(
    ["name", "ctf_version", "is_generated"],
    [
        pytest.param("float", "1", True, id="float-ctf-1.8"),
        pytest.param("double", "1", True, id="double-ctf-1.8"),
        pytest.param("float", "2", True, id="float-ctf-2"),
        pytest.param("double", "2", True, id="double-ctf-2"),
        pytest.param(
            "meta-variant-no-underscore", "1", False, id="meta-variant-no-underscore"
        ),
        pytest.param(
            "meta-variant-one-underscore", "1", False, id="meta-variant-one-underscore"
        ),
        pytest.param(
            "meta-variant-reserved-keywords",
            "1",
            False,
            id="meta-variant-reserved-keywords",
        ),
        pytest.param(
            "meta-variant-same-with-underscore",
            "1",
            False,
            id="meta-variant-same-with-underscore",
        ),
        pytest.param(
            "meta-variant-two-underscores",
            "1",
            False,
            id="meta-variant-two-underscores",
        ),
    ],
)
def test_sink_ctf_fs_succeed(
    name,
    ctf_version,
    is_generated,
    build_root_dir,
    ctf_traces_dir,
    tmp_path,
    sink_ctf_comp_cls,
):
    # Get input trace directory (generate or use existing trace)
    if is_generated:
        # Generate trace using `gen-trace-*` binary
        gen_trace_dir = tmp_path / "gen-trace"
        gen_trace_dir.mkdir()
        btu.run(
            build_root_dir,
            (
                btu.build_dir_of_source_file(build_root_dir, __file__)
                / f"gen-trace-{name}.bin"
            ),
            [str(gen_trace_dir)],
            check=True,
        )
        in_trace_dir = gen_trace_dir
    else:
        # Use existing trace
        in_trace_dir = ctf_traces_dir / "1/succeed" / name

    # Convert trace to CTF using `sink.ctf.fs`
    out_trace_dir = tmp_path / "out-trace"
    btu.convert(
        in_trace_dir,
        btu.SinkComponentSpec(
            sink_ctf_comp_cls,
            {"path": str(out_trace_dir), "quiet": True, "ctf-version": ctf_version},
        ),
    )

    # Read back the converted trace using `sink.text.details`
    # and compare.
    #
    # The expectation doesn't depend on the CTF version: the output of
    # `sink.text.details` must be the same whatever the version of the
    # written trace.
    btu.convert_sink_text_details_test(
        out_trace_dir,
        btu.this_src_dir(__file__) / f"trace-{name}.expect",
        details_params={
            "with-uuid": False,
            "with-uid": False,
            "with-trace-name": False,
            "with-stream-name": False,
        },
    )
