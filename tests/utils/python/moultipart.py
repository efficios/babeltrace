# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2023 EfficiOS Inc.
#
# pyright: strict, reportTypeCommentUsage=false

import re
from typing import TextIO


# One part of a moultipart document.
#
# For example, for this part of which the header is at line 37:
#
#     --- Another Oscar Wilde quote
#     I can resist everything except temptation.
#
# The corresponding `Part` object is:
#
#     Part('Another Oscar Wilde quote',
#          'I can resist everything except temptation',
#          38)
class Part:
    def __init__(self, header_info: str, content: str, first_content_line_no: int):
        self._header_info = header_info
        self._content = content
        self._first_content_line_no = first_content_line_no

    @property
    def header_info(self):
        return self._header_info

    @property
    def content(self):
        return self._content

    # Number of the first line, relative to the beginning of the
    # containing moultipart document, of the content of this part.
    @property
    def first_content_line_no(self):
        return self._first_content_line_no

    def __repr__(self):
        return "Part({}, {}, {})".format(
            repr(self.header_info), repr(self.content), self.first_content_line_no
        )


def _try_parse_header(line: str):
    m = re.match(r"---(\s*| .+)$", line)

    if m is None:
        return

    return m.group(1).strip()


# Parses the moultipart document file `in_file` and returns its parts
# (list of `Part` objects).
#
# A moultipart document is a sequence of parts.
#
# A moutlipart part is:
#
# 1. A header line, that is, in this order:
#
#    a) Exactly `---`.
#    b) Zero or more spaces.
#    c) Optional: custom information until the end of the line.
#
# 2. Zero or more lines of text which aren't header lines.
#
# For example, consider the following moultipart document:
#
#     --- Victoria
#     Parenteau
#     ---
#     Taillon
#     --- This part is empty
#     --- Josianne
#     Gervais
#
# Then this function would return the following part objects:
#
#     [
#         Part('Victoria',           'Parenteau\n', 2),
#         Part('',                   'Taillon\n',   4),
#         Part('This part is empty', '',            6),
#         Part('Josianne',           'Gervais\n',   7),
#     ]
#
# Raises `RuntimeError` on any parsing error.
def parse(in_file: TextIO):
    # Read the first header
    cur_part_content = ""
    cur_first_content_line_no = 2
    parts = []  # type: list[Part]
    line_no = 1
    line = next(in_file)
    cur_part_header_info = _try_parse_header(line)

    if cur_part_header_info is None:
        raise RuntimeError(
            "Expecting header line starting with `---`, got `{}`".format(
                line.strip("\n")
            )
        )

    for line in in_file:
        line_no += 1
        maybe_part_header_info = _try_parse_header(line)

        if maybe_part_header_info is not None:
            # New header
            parts.append(
                Part(
                    cur_part_header_info,
                    cur_part_content,
                    cur_first_content_line_no,
                )
            )
            cur_part_content = ""
            cur_part_header_info = maybe_part_header_info
            cur_first_content_line_no = line_no + 1
            continue

        # Accumulate content lines
        cur_part_content += line

    # Last part (always exists)
    parts.append(
        Part(
            cur_part_header_info,
            cur_part_content,
            cur_first_content_line_no,
        )
    )

    return parts


if __name__ == "__main__":
    import sys
    import pprint

    with open(sys.argv[1]) as f:
        pprint.pprint(parse(f))
