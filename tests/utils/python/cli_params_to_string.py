# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 EfficiOS Inc.
#

import bt2


def to_string(p):
    # Print BT values in a predictable way (the order of map entries) and with
    # additional information (u suffix to differentiate unsigned integers from
    # signed integers).

    if type(p) is bt2._ArrayValueConst:
        s = "[{}]".format(", ".join([to_string(x) for x in p]))
    elif type(p) is bt2._MapValueConst:
        s = "{{{}}}".format(
            ", ".join([k + "=" + to_string(p[k]) for k in sorted(p.keys())])
        )
    elif type(p) is bt2._UnsignedIntegerValueConst:
        s = str(p) + "u"
    elif type(p) is bt2._RealValueConst:
        s = "{:.7f}".format(float(p))
    elif (
        type(p)
        in (
            bt2._StringValueConst,
            bt2._SignedIntegerValueConst,
            bt2._BoolValueConst,
        )
        or p is None
    ):
        s = str(p)

    else:
        raise TypeError("Unexpected type", type(p))

    return s
