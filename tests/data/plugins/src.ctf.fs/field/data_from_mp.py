# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2023 EfficiOS Inc.
#
# pyright: strict, reportTypeCommentUsage=false

import os
import string
import argparse

import normand
import moultipart


def _make_ctf_1_metadata(payload_fc: str):
    payload_fc = payload_fc.strip()

    if "@" in payload_fc:
        payload_fc = payload_fc.replace("@", "root")
    else:
        payload_fc += " root"

    return string.Template(
        """\
/* CTF 1.8 */

trace {
    major = 1;
    minor = 8;
    byte_order = le;
};

typealias integer { size = 8; } := u8;
typealias integer { size = 16; } := u16;
typealias integer { size = 32; } := u32;
typealias integer { size = 64; } := u64;
typealias integer { size = 8; byte_order = le; } := u8le;
typealias integer { size = 16; byte_order = le; } := u16le;
typealias integer { size = 32; byte_order = le; } := u32le;
typealias integer { size = 64; byte_order = le; } := u64le;
typealias integer { size = 8; byte_order = be; } := u8be;
typealias integer { size = 16; byte_order = be; } := u16be;
typealias integer { size = 32; byte_order = be; } := u32be;
typealias integer { size = 64; byte_order = be; } := u64be;
typealias integer { signed = true; size = 8; } := i8;
typealias integer { signed = true; size = 16; } := i16;
typealias integer { signed = true; size = 32; } := i32;
typealias integer { signed = true; size = 64; } := i64;
typealias integer { signed = true; size = 8; byte_order = le; } := i8le;
typealias integer { signed = true; size = 16; byte_order = le; } := i16le;
typealias integer { signed = true; size = 32; byte_order = le; } := i32le;
typealias integer { signed = true; size = 64; byte_order = le; } := i64le;
typealias integer { signed = true; size = 8; byte_order = be; } := i8be;
typealias integer { signed = true; size = 16; byte_order = be; } := i16be;
typealias integer { signed = true; size = 32; byte_order = be; } := i32be;
typealias integer { signed = true; size = 64; byte_order = be; } := i64be;
typealias floating_point { exp_dig = 8; mant_dig = 24; } := flt32;
typealias floating_point { exp_dig = 11; mant_dig = 53; } := flt64;
typealias floating_point { exp_dig = 8; mant_dig = 24; byte_order = le; } := flt32le;
typealias floating_point { exp_dig = 11; mant_dig = 53; byte_order = le; } := flt64le;
typealias floating_point { exp_dig = 8; mant_dig = 24; byte_order = be; } := flt32be;
typealias floating_point { exp_dig = 11; mant_dig = 53; byte_order = be; } := flt64be;
typealias string { encoding = UTF8; } := nt_str;

event {
    name = the_event;
    fields := struct {
        ${payload_fc};
    };
};
"""
    ).substitute(payload_fc=payload_fc)


def _make_ctf_1_data(normand_text: str):
    # Default to little-endian because that's also the TSDL default in
    # _make_ctf_1_metadata() above.
    return normand.parse("!le\n" + normand_text).data


def _create_files_from_mp(mp_path: str, output_dir: str):
    trace_dir = os.path.join(output_dir, "trace")
    expect_path = os.path.join(output_dir, "expect")
    metadata_path = os.path.join(trace_dir, "metadata")
    data_path = os.path.join(trace_dir, "data")
    os.makedirs(trace_dir, exist_ok=True)

    with open(mp_path, "r") as f:
        parts = moultipart.parse(f)

    with open(metadata_path, "w") as f:
        f.write(_make_ctf_1_metadata(parts[0].content))

    with open(data_path, "wb") as f:
        f.write(_make_ctf_1_data(parts[1].content))

    with open(expect_path, "w") as f:
        f.write(parts[2].content)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "mp_path", metavar="MP-PATH", help="moultipart document to process"
    )
    parser.add_argument(
        "output_dir",
        metavar="OUTPUT-DIR",
        help="output directory for the CTF trace and expectation file",
    )
    args = parser.parse_args()
    _create_files_from_mp(args.mp_path, args.output_dir)
