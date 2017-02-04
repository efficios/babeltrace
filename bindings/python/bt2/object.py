# The MIT License (MIT)
#
# Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
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
import abc


class _Object:
    def __init__(self, ptr):
        self._ptr = ptr

    @property
    def addr(self):
        return int(self._ptr)

    @classmethod
    def _create_from_ptr(cls, ptr):
        obj = cls.__new__(cls)
        obj._ptr = ptr
        return obj

    def _get(self):
        native_bt.get(self._ptr)

    def __del__(self):
        ptr = getattr(self, '_ptr', None)
        native_bt.put(ptr)

    def __repr__(self):
        return '<{}.{} object @ {}>'.format(self.__class__.__module__,
                                            self.__class__.__name__,
                                            hex(self.addr))


class _Freezable(metaclass=abc.ABCMeta):
    @property
    def is_frozen(self):
        return self._is_frozen()

    @property
    def frozen(self):
        return self.is_frozen

    def freeze(self):
        self._freeze()

    @abc.abstractmethod
    def _is_frozen(self):
        pass

    @abc.abstractmethod
    def _freeze(self):
        pass
