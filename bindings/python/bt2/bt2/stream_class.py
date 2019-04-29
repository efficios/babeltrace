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

from bt2 import native_bt, object, utils
import bt2.field_class
import collections.abc
import bt2.ctf_writer
import bt2.stream
import copy
import bt2


class _EventClassIterator(collections.abc.Iterator):
    def __init__(self, stream_class):
        self._stream_class = stream_class
        self._at = 0

    def __next__(self):
        if self._at == len(self._stream_class):
            raise StopIteration

        ec_ptr = native_bt.stream_class_get_event_class_by_index(self._stream_class._ptr,
                                                                 self._at)
        assert(ec_ptr)
        ev_id = native_bt.event_class_get_id(ec_ptr)
        native_bt.put(ec_ptr)
        utils._handle_ret(ev_id, "cannot get event class object's ID")
        self._at += 1
        return ev_id


class StreamClass(object._SharedObject, collections.abc.Mapping):
    def __init__(self, name=None, id=None, packet_context_field_class=None,
                 event_header_field_class=None, event_context_field_class=None,
                 event_classes=None):
        ptr = native_bt.stream_class_create_empty(None)

        if ptr is None:
            raise bt2.CreationError('cannot create stream class object')

        super().__init__(ptr)

        if name is not None:
            self.name = name

        if id is not None:
            self.id = id

        if packet_context_field_class is not None:
            self.packet_context_field_class = packet_context_field_class

        if event_header_field_class is not None:
            self.event_header_field_class = event_header_field_class

        if event_context_field_class is not None:
            self.event_context_field_class = event_context_field_class

        if event_classes is not None:
            for event_class in event_classes:
                self.add_event_class(event_class)

    def __getitem__(self, key):
        utils._check_int64(key)
        ec_ptr = native_bt.stream_class_get_event_class_by_id(self._ptr,
                                                              key)

        if ec_ptr is None:
            raise KeyError(key)

        return bt2.EventClass._create_from_ptr(ec_ptr)

    def __len__(self):
        count = native_bt.stream_class_get_event_class_count(self._ptr)
        assert(count >= 0)
        return count

    def __iter__(self):
        return _EventClassIterator(self)

    def add_event_class(self, event_class):
        utils._check_type(event_class, bt2.EventClass)
        ret = native_bt.stream_class_add_event_class(self._ptr, event_class._ptr)
        utils._handle_ret(ret, "cannot add event class object to stream class object's")

    @property
    def trace(self):
        tc_ptr = native_bt.stream_class_get_trace(self._ptr)

        if tc_ptr is not None:
            return bt2.Trace._create_from_ptr(tc_ptr)

    @property
    def name(self):
        return native_bt.stream_class_get_name(self._ptr)

    @name.setter
    def name(self, name):
        utils._check_str(name)
        ret = native_bt.stream_class_set_name(self._ptr, name)
        utils._handle_ret(ret, "cannot set stream class object's name")

    @property
    def id(self):
        id = native_bt.stream_class_get_id(self._ptr)

        if id < 0:
            return

        return id

    @id.setter
    def id(self, id):
        utils._check_int64(id)
        ret = native_bt.stream_class_set_id(self._ptr, id)
        utils._handle_ret(ret, "cannot set stream class object's ID")

    @property
    def clock(self):
        clock_ptr = native_bt.stream_class_get_clock(self._ptr)

        if clock_ptr is None:
            return

        return bt2.ctf_writer.CtfWriterClock._create_from_ptr(clock_ptr)

    @clock.setter
    def clock(self, clock):
        utils._check_type(clock, bt2.ctf_writer.CtfWriterClock)
        ret = native_bt.stream_class_set_clock(self._ptr, clock._ptr)
        utils._handle_ret(ret, "cannot set stream class object's CTF writer clock object")

    @property
    def packet_context_field_class(self):
        fc_ptr = native_bt.stream_class_get_packet_context_type(self._ptr)

        if fc_ptr is None:
            return

        return bt2.field_class._create_from_ptr(fc_ptr)

    @packet_context_field_class.setter
    def packet_context_field_class(self, packet_context_field_class):
        packet_context_field_class_ptr = None

        if packet_context_field_class is not None:
            utils._check_type(packet_context_field_class, bt2.field_class._FieldClass)
            packet_context_field_class_ptr = packet_context_field_class._ptr

        ret = native_bt.stream_class_set_packet_context_type(self._ptr,
                                                             packet_context_field_class_ptr)
        utils._handle_ret(ret, "cannot set stream class object's packet context field class")

    @property
    def event_header_field_class(self):
        fc_ptr = native_bt.stream_class_get_event_header_type(self._ptr)

        if fc_ptr is None:
            return

        return bt2.field_class._create_from_ptr(fc_ptr)

    @event_header_field_class.setter
    def event_header_field_class(self, event_header_field_class):
        event_header_field_class_ptr = None

        if event_header_field_class is not None:
            utils._check_type(event_header_field_class, bt2.field_class._FieldClass)
            event_header_field_class_ptr = event_header_field_class._ptr

        ret = native_bt.stream_class_set_event_header_type(self._ptr,
                                                           event_header_field_class_ptr)
        utils._handle_ret(ret, "cannot set stream class object's event header field class")

    @property
    def event_context_field_class(self):
        fc_ptr = native_bt.stream_class_get_event_context_type(self._ptr)

        if fc_ptr is None:
            return

        return bt2.field_class._create_from_ptr(fc_ptr)

    @event_context_field_class.setter
    def event_context_field_class(self, event_context_field_class):
        event_context_field_class_ptr = None

        if event_context_field_class is not None:
            utils._check_type(event_context_field_class, bt2.field_class._FieldClass)
            event_context_field_class_ptr = event_context_field_class._ptr

        ret = native_bt.stream_class_set_event_context_type(self._ptr,
                                                            event_context_field_class_ptr)
        utils._handle_ret(ret, "cannot set stream class object's event context field class")

    def __call__(self, name=None, id=None):
        if name is not None:
            utils._check_str(name)

        if id is None:
            stream_ptr = native_bt.stream_create(self._ptr, name)
        else:
            stream_ptr = native_bt.stream_create_with_id(self._ptr, name, id)

        if stream_ptr is None:
            raise bt2.CreationError('cannot create stream object')

        return bt2.stream._create_from_ptr(stream_ptr)

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        if self.addr == other.addr:
            return True

        self_event_classes = list(self.values())
        other_event_classes = list(other.values())
        self_props = (
            self_event_classes,
            self.name,
            self.id,
            self.packet_context_field_class,
            self.event_header_field_class,
            self.event_context_field_class,
            self.clock,
        )
        other_props = (
            other_event_classes,
            other.name,
            other.id,
            other.packet_context_field_class,
            other.event_header_field_class,
            other.event_context_field_class,
            other.clock,
        )

        return self_props == other_props

    def _copy(self, fc_copy_func, ev_copy_func):
        cpy = StreamClass()

        if self.id is not None:
            cpy.id = self.id

        if self.name is not None:
            cpy.name = self.name

        if self.clock is not None:
            cpy.clock = self.clock

        cpy.packet_context_field_class = fc_copy_func(self.packet_context_field_class)
        cpy.event_header_field_class = fc_copy_func(self.event_header_field_class)
        cpy.event_context_field_class = fc_copy_func(self.event_context_field_class)

        for event_class in self.values():
            cpy.add_event_class(ev_copy_func(event_class))

        return cpy

    def __copy__(self):
        return self._copy(lambda fc: fc, copy.copy)

    def __deepcopy__(self, memo):
        cpy = self._copy(copy.deepcopy, copy.deepcopy)
        memo[id(self)] = cpy
        return cpy
