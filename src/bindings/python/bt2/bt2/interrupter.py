# The MIT License (MIT)
#
# Copyright (c) 2019 Philippe Proulx <pproulx@efficios.com>
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
import bt2


class Interrupter(object._SharedObject):
    _get_ref = staticmethod(native_bt.interrupter_get_ref)
    _put_ref = staticmethod(native_bt.interrupter_put_ref)

    def __init__(self):
        ptr = native_bt.interrupter_create()

        if ptr is None:
            raise bt2._MemoryError('cannot create interrupter object')

        super().__init__(ptr)

    @property
    def is_set(self):
        return bool(native_bt.interrupter_is_set(self._ptr))

    def __bool__(self):
        return self.is_set

    def set(self):
        native_bt.interrupter_set(self._ptr)

    def reset(self):
        native_bt.interrupter_reset(self._ptr)
