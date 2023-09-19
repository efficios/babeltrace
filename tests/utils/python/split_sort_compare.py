# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2020 Francis Deslauriers <francis.deslauriers@efficios.com>

import re
import sys


def main():
    expected = sys.argv[1]
    actual = sys.argv[2]
    sorted_expected = "".join(sorted(re.findall(r"\w+|\W+", expected.strip())))
    sorted_actual = "".join(sorted(re.findall(r"\w+|\W+", actual.strip())))

    if sorted_expected == sorted_actual:
        status = 0
    else:
        status = 1

    sys.exit(status)


if __name__ == "__main__":
    main()
