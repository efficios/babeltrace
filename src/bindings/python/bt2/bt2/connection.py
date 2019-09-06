# The MIT License (MIT)
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

from bt2 import native_bt
from bt2 import port as bt2_port
from bt2 import object as bt2_object


class _ConnectionConst(bt2_object._SharedObject):
    _get_ref = staticmethod(native_bt.connection_get_ref)
    _put_ref = staticmethod(native_bt.connection_put_ref)

    @property
    def downstream_port(self):
        port_ptr = native_bt.connection_borrow_downstream_port_const(self._ptr)
        return bt2_port._create_from_const_ptr_and_get_ref(
            port_ptr, native_bt.PORT_TYPE_INPUT
        )

    @property
    def upstream_port(self):
        port_ptr = native_bt.connection_borrow_upstream_port_const(self._ptr)
        return bt2_port._create_from_const_ptr_and_get_ref(
            port_ptr, native_bt.PORT_TYPE_OUTPUT
        )
