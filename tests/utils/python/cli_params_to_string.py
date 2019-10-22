#
# Copyright (C) 2019 EfficiOS Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; only version 2
# of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#

import bt2


def to_string(p):
    # Print BT values in a predictable way (the order of map entries) and with
    # additional information (u suffix to differentiate unsigned integers from
    # signed integers).

    if type(p) is bt2._ArrayValueConst:
        s = '[{}]'.format(', '.join([to_string(x) for x in p]))
    elif type(p) is bt2._MapValueConst:
        s = '{{{}}}'.format(
            ', '.join([k + '=' + to_string(p[k]) for k in sorted(p.keys())])
        )
    elif type(p) is bt2._UnsignedIntegerValueConst:
        s = str(p) + 'u'
    elif (
        type(p)
        in (
            bt2._StringValueConst,
            bt2._SignedIntegerValueConst,
            bt2._RealValueConst,
            bt2._BoolValueConst,
        )
        or p is None
    ):
        s = str(p)

    else:
        raise TypeError('Unexpected type', type(p))

    return s
