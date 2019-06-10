# The MIT License (MIT)
#
# Copyright (c) 2016-2017 Philippe Proulx <pproulx@efficios.com>
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

from bt2 import native_bt, object
import bt2.field
import bt2


class _Packet(object._SharedObject):
    _get_ref = staticmethod(native_bt.packet_get_ref)
    _put_ref = staticmethod(native_bt.packet_put_ref)

    @property
    def stream(self):
        stream_ptr = native_bt.packet_borrow_stream(self._ptr)
        assert stream_ptr is not None
        return bt2.stream._Stream._create_from_ptr_and_get_ref(stream_ptr)

    @property
    def context_field(self):
        field_ptr = native_bt.packet_borrow_context_field(self._ptr)

        if field_ptr is None:
            return

        return bt2.field._create_field_from_ptr(field_ptr, self._ptr,
                                                self._get_ref,
                                                self._put_ref)
