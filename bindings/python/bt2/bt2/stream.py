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

from bt2 import native_bt, object, utils
import bt2.packet
import bt2.event
import abc
import bt2


def _create_from_ptr(stream_ptr):
    import bt2.ctf_writer

    if native_bt.stream_is_writer(stream_ptr):
        cls = bt2.ctf_writer._CtfWriterStream
    else:
        cls = _Stream

    return cls._create_from_ptr(stream_ptr)


class _StreamBase(object._SharedObject):
    @property
    def stream_class(self):
        stream_class_ptr = native_bt.stream_get_class(self._ptr)
        assert(stream_class_ptr)
        return bt2.StreamClass._create_from_ptr(stream_class_ptr)

    @property
    def name(self):
        return native_bt.stream_get_name(self._ptr)

    @property
    def id(self):
        id = native_bt.stream_get_id(self._ptr)
        return id if id >= 0 else None

    def __eq__(self, other):
        if self.addr == other.addr:
            return True

        return (self.name, self.id) == (other.name, other.id)


class _Stream(_StreamBase):
    def create_packet(self):
        packet_ptr = native_bt.packet_create(self._ptr)

        if packet_ptr is None:
            raise bt2.CreationError('cannot create packet object')

        return bt2.packet._Packet._create_from_ptr(packet_ptr)

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        return _StreamBase.__eq__(self, other)

    def _copy(self):
        return self.stream_class(self.name, self.id)

    def __copy__(self):
        return self._copy()

    def __deepcopy__(self, memo):
        cpy = self._copy()
        memo[id(self)] = cpy
        return cpy
