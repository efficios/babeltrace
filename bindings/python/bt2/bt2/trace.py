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
import bt2.value
import bt2.stream
import copy
import bt2


class _StreamClassIterator(collections.abc.Iterator):
    def __init__(self, trace):
        self._trace = trace
        self._at = 0

    def __next__(self):
        if self._at == len(self._trace):
            raise StopIteration

        sc_ptr = native_bt.trace_get_stream_class_by_index(self._trace._ptr,
                                                           self._at)
        assert(sc_ptr)
        id = native_bt.stream_class_get_id(sc_ptr)
        native_bt.put(sc_ptr)
        assert(id >= 0)
        self._at += 1
        return id


class _TraceStreams(collections.abc.Sequence):
    def __init__(self, trace):
        self._trace = trace

    def __len__(self):
        count = native_bt.trace_get_stream_count(self._trace._ptr)
        assert(count >= 0)
        return count

    def __getitem__(self, index):
        utils._check_uint64(index)

        if index >= len(self):
            raise IndexError

        stream_ptr = native_bt.trace_get_stream_by_index(self._trace._ptr,
                                                         index)
        assert(stream_ptr)
        return bt2.stream._create_from_ptr(stream_ptr)


class _TraceClockClassesIterator(collections.abc.Iterator):
    def __init__(self, trace_clock_classes):
        self._trace_clock_classes = trace_clock_classes
        self._at = 0

    def __next__(self):
        if self._at == len(self._trace_clock_classes):
            raise StopIteration

        trace_ptr = self._trace_clock_classes._trace._ptr
        cc_ptr = native_bt.trace_get_clock_class_by_index(trace_ptr, self._at)
        assert(cc_ptr)
        name = native_bt.clock_class_get_name(cc_ptr)
        native_bt.put(cc_ptr)
        assert(name is not None)
        self._at += 1
        return name


class _TraceClockClasses(collections.abc.Mapping):
    def __init__(self, trace):
        self._trace = trace

    def __getitem__(self, key):
        utils._check_str(key)
        cc_ptr = native_bt.trace_get_clock_class_by_name(self._trace._ptr, key)

        if cc_ptr is None:
            raise KeyError(key)

        return bt2.ClockClass._create_from_ptr(cc_ptr)

    def __len__(self):
        count = native_bt.trace_get_clock_class_count(self._trace._ptr)
        assert(count >= 0)
        return count

    def __iter__(self):
        return _TraceClockClassesIterator(self)


class _TraceEnvIterator(collections.abc.Iterator):
    def __init__(self, trace_env):
        self._trace_env = trace_env
        self._at = 0

    def __next__(self):
        if self._at == len(self._trace_env):
            raise StopIteration

        trace_ptr = self._trace_env._trace._ptr
        entry_name = native_bt.trace_get_environment_field_name_by_index(trace_ptr,
                                                                         self._at)
        assert(entry_name is not None)
        self._at += 1
        return entry_name


class _TraceEnv(collections.abc.MutableMapping):
    def __init__(self, trace):
        self._trace = trace

    def __getitem__(self, key):
        utils._check_str(key)
        value_ptr = native_bt.trace_get_environment_field_value_by_name(self._trace._ptr,
                                                                        key)

        if value_ptr is None:
            raise KeyError(key)

        return bt2.value._create_from_ptr(value_ptr)

    def __setitem__(self, key, value):
        utils._check_str(key)
        value = bt2.create_value(value)
        ret = native_bt.trace_set_environment_field(self._trace._ptr,
                                                    key, value._ptr)
        utils._handle_ret(ret, "cannot set trace class object's environment entry")

    def __delitem__(self, key):
        raise NotImplementedError

    def __len__(self):
        count = native_bt.trace_get_environment_field_count(self._trace._ptr)
        assert(count >= 0)
        return count

    def __iter__(self):
        return _TraceEnvIterator(self)


class Trace(object._Object, collections.abc.Mapping):
    def __init__(self, name=None, native_byte_order=None, env=None,
                 packet_header_field_class=None, clock_classes=None,
                 stream_classes=None):
        ptr = native_bt.trace_create()

        if ptr is None:
            raise bt2.CreationError('cannot create trace class object')

        super().__init__(ptr)

        if name is not None:
            self.name = name

        if native_byte_order is not None:
            self.native_byte_order = native_byte_order

        if packet_header_field_class is not None:
            self.packet_header_field_class = packet_header_field_class

        if env is not None:
            for key, value in env.items():
                self.env[key] = value

        if clock_classes is not None:
            for clock_class in clock_classes:
                self.add_clock_class(clock_class)

        if stream_classes is not None:
            for stream_class in stream_classes:
                self.add_stream_class(stream_class)

    def __getitem__(self, key):
        utils._check_int64(key)
        sc_ptr = native_bt.trace_get_stream_class_by_id(self._ptr, key)

        if sc_ptr is None:
            raise KeyError(key)

        return bt2.StreamClass._create_from_ptr(sc_ptr)

    def __len__(self):
        count = native_bt.trace_get_stream_class_count(self._ptr)
        assert(count >= 0)
        return count

    def __iter__(self):
        return _StreamClassIterator(self)

    def add_stream_class(self, stream_class):
        utils._check_type(stream_class, bt2.StreamClass)
        ret = native_bt.trace_add_stream_class(self._ptr, stream_class._ptr)
        utils._handle_ret(ret, "cannot add stream class object to trace class object")

    @property
    def name(self):
        return native_bt.trace_get_name(self._ptr)

    @name.setter
    def name(self, name):
        utils._check_str(name)
        ret = native_bt.trace_set_name(self._ptr, name)
        utils._handle_ret(ret, "cannot set trace class object's name")

    @property
    def native_byte_order(self):
        bo = native_bt.trace_get_native_byte_order(self._ptr)
        assert(bo >= 0)
        return bo

    @native_byte_order.setter
    def native_byte_order(self, native_byte_order):
        utils._check_int(native_byte_order)
        ret = native_bt.trace_set_native_byte_order(self._ptr, native_byte_order)
        utils._handle_ret(ret, "cannot set trace class object's native byte order")

    @property
    def is_static(self):
        is_static = native_bt.trace_is_static(self._ptr)
        return is_static > 0

    def set_is_static(self):
        ret = native_bt.trace_set_is_static(self._ptr)
        utils._handle_ret(ret, "cannot set trace object as static")

    @property
    def env(self):
        return _TraceEnv(self)

    @property
    def clock_classes(self):
        return _TraceClockClasses(self)

    def add_clock_class(self, clock_class):
        utils._check_type(clock_class, bt2.ClockClass)
        ret = native_bt.trace_add_clock_class(self._ptr, clock_class._ptr)
        utils._handle_ret(ret, "cannot add clock class object to trace class object")

    @property
    def streams(self):
        return _TraceStreams(self)

    @property
    def packet_header_field_class(self):
        fc_ptr = native_bt.trace_get_packet_header_type(self._ptr)

        if fc_ptr is None:
            return

        return bt2.field_class._create_from_ptr(fc_ptr)

    @packet_header_field_class.setter
    def packet_header_field_class(self, packet_header_field_class):
        packet_header_field_class_ptr = None

        if packet_header_field_class is not None:
            utils._check_type(packet_header_field_class, bt2.field_class._FieldClass)
            packet_header_field_class_ptr = packet_header_field_class._ptr

        ret = native_bt.trace_set_packet_header_type(self._ptr,
                                                     packet_header_field_class_ptr)
        utils._handle_ret(ret, "cannot set trace class object's packet header field class")

    def __eq__(self, other):
        if type(other) is not type(self):
            # not comparing apples to apples
            return False

        if self.addr == other.addr:
            return True

        self_stream_classes = list(self.values())
        self_clock_classes = list(self.clock_classes.values())
        self_env = {key: val for key, val in self.env.items()}
        other_stream_classes = list(other.values())
        other_clock_classes = list(other.clock_classes.values())
        other_env = {key: val for key, val in other.env.items()}
        self_props = (
            self_stream_classes,
            self_clock_classes,
            self_env,
            self.name,
            self.native_byte_order,
            self.packet_header_field_class,
        )
        other_props = (
            other_stream_classes,
            other_clock_classes,
            other_env,
            other.name,
            other.native_byte_order,
            other.packet_header_field_class,
        )
        return self_props == other_props

    def _copy(self, gen_copy_func, sc_copy_func):
        cpy = Trace()

        if self.name is not None:
            cpy.name = self.name

        cpy.packet_header_field_class = gen_copy_func(self.packet_header_field_class)

        for key, val in self.env.items():
            cpy.env[key] = gen_copy_func(val)

        for clock_class in self.clock_classes.values():
            cpy.add_clock_class(gen_copy_func(clock_class))

        for stream_class in self.values():
            cpy.add_stream_class(sc_copy_func(stream_class))

        return cpy

    def __copy__(self):
        return self._copy(lambda obj: obj, copy.copy)

    def __deepcopy__(self, memo):
        cpy = self._copy(copy.deepcopy, copy.deepcopy)
        memo[id(self)] = cpy
        return cpy
