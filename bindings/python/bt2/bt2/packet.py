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
import bt2.field
import bt2.stream
import copy
import abc
import bt2


class _Packet(object._SharedObject):
    @property
    def stream(self):
        stream_ptr = native_bt.packet_get_stream(self._ptr)
        assert(stream_ptr)
        return bt2.stream._Stream._create_from_ptr(stream_ptr)

    @property
    def header_field(self):
        field_ptr = native_bt.packet_get_header(self._ptr)

        if field_ptr is None:
            return

        return bt2.field._create_from_ptr(field_ptr)

    @header_field.setter
    def header_field(self, header_field):
        header_field_ptr = None

        if header_field is not None:
            utils._check_type(header_field, bt2.field._Field)
            header_field_ptr = header_field._ptr

        ret = native_bt.packet_set_header(self._ptr, header_field_ptr)
        utils._handle_ret(ret, "cannot set packet object's header field")

    @property
    def context_field(self):
        field_ptr = native_bt.packet_get_context(self._ptr)

        if field_ptr is None:
            return

        return bt2.field._create_from_ptr(field_ptr)

    @context_field.setter
    def context_field(self, context_field):
        context_field_ptr = None

        if context_field is not None:
            utils._check_type(context_field, bt2.field._Field)
            context_field_ptr = context_field._ptr

        ret = native_bt.packet_set_context(self._ptr, context_field_ptr)
        utils._handle_ret(ret, "cannot set packet object's context field")

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        if self.addr == other.addr:
            return True

        self_props = (
            self.header_field,
            self.context_field,
        )
        other_props = (
            other.header_field,
            other.context_field,
        )
        return self_props == other_props

    def _copy(self, copy_func):
        cpy = self.stream.create_packet()
        cpy.header_field = copy_func(self.header_field)
        cpy.context_field = copy_func(self.context_field)
        return cpy

    def __copy__(self):
        return self._copy(copy.copy)

    def __deepcopy__(self, memo):
        cpy = self._copy(copy.deepcopy)
        memo[id(self)] = cpy
        return cpy
