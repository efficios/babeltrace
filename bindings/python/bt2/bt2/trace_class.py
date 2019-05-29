# The MIT License (MIT)
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
# Copyright (c) 2018 Francis Deslauriers <francis.deslauriers@efficios.com>
# Copyright (c) 2019 Simon Marchi <simon.marchi@efficios.com>
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

__all__ = ['TraceClass']

import bt2
from bt2 import native_bt, utils, object
import uuid as uuidp
import collections.abc
import functools


class _TraceClassEnv(collections.abc.MutableMapping):
    def __init__(self, trace_class):
        self._trace_class = trace_class

    def __getitem__(self, key):
        utils._check_str(key)

        borrow_entry_fn = native_bt.trace_class_borrow_environment_entry_value_by_name_const
        value_ptr = borrow_entry_fn(self._trace_class._ptr, key)

        if value_ptr is None:
            raise KeyError(key)

        return bt2.value._create_from_ptr_and_get_ref(value_ptr)

    def __setitem__(self, key, value):
        if isinstance(value, str):
            set_env_entry_fn = native_bt.trace_class_set_environment_entry_string
        elif isinstance(value, int):
            set_env_entry_fn = native_bt.trace_class_set_environment_entry_integer
        else:
            raise TypeError('expected str or int, got {}'.format(type(value)))

        ret = set_env_entry_fn(self._trace_class._ptr, key, value)

        utils._handle_ret(ret, "cannot set trace class object's environment entry")

    def __delitem__(self, key):
        raise NotImplementedError

    def __len__(self):
        count = native_bt.trace_class_get_environment_entry_count(self._trace_class._ptr)
        assert count >= 0
        return count

    def __iter__(self):
        trace_class_ptr = self._trace_class_env._trace_class._ptr

        for idx in range(len(self)):
            borrow_entry_fn = native_bt.trace_class_borrow_environment_entry_by_index_const
            entry_name, _ = borrow_entry_fn(trace_class_ptr, idx)
            assert entry_name is not None
            yield entry_name


class _StreamClassIterator(collections.abc.Iterator):
    def __init__(self, trace_class):
        self._trace_class = trace_class
        self._at = 0

    def __next__(self):
        if self._at == len(self._trace_class):
            raise StopIteration

        borrow_stream_class_fn = native_bt.trace_class_borrow_stream_class_by_index_const
        sc_ptr = borrow_stream_class_fn(self._trace_class._ptr, self._at)
        assert sc_ptr
        id = native_bt.stream_class_get_id(sc_ptr)
        assert id >= 0
        self._at += 1
        return id


def _trace_class_destruction_listener_from_native(user_listener, trace_class_ptr):
    trace_class = bt2.trace_class.TraceClass._create_from_ptr_and_get_ref(trace_class_ptr)
    user_listener(trace_class)


class TraceClass(object._SharedObject, collections.abc.Mapping):
    _get_ref = staticmethod(native_bt.trace_class_get_ref)
    _put_ref = staticmethod(native_bt.trace_class_put_ref)

    @property
    def uuid(self):
        uuid_bytes = native_bt.trace_class_get_uuid(self._ptr)
        if uuid_bytes is None:
            return

        return uuidp.UUID(bytes=uuid_bytes)

    def _uuid(self, uuid):
        utils._check_type(uuid, uuidp.UUID)
        native_bt.trace_class_set_uuid(self._ptr, uuid.bytes)

    _uuid = property(fset=_uuid)

    # Instantiate a trace of this class.

    def __call__(self, name=None):
        trace_ptr = native_bt.trace_create(self._ptr)

        if trace_ptr is None:
            raise bt2.CreationError('cannot create trace class object')

        trace = bt2.trace.Trace._create_from_ptr(trace_ptr)

        if name is not None:
            trace._name = name

        return trace

    # Number of stream classes in this trace class.

    def __len__(self):
        count = native_bt.trace_class_get_stream_class_count(self._ptr)
        assert count >= 0
        return count

    # Get a stream class by stream id.

    def __getitem__(self, key):
        utils._check_uint64(key)

        sc_ptr = native_bt.trace_class_borrow_stream_class_by_id_const(self._ptr, key)
        if sc_ptr is None:
            raise KeyError(key)

        return bt2.StreamClass._create_from_ptr_and_get_ref(sc_ptr)

    def __iter__(self):
        for idx in range(len(self)):
            sc_ptr = native_bt.trace_class_borrow_stream_class_by_index_const(self._ptr, idx)
            assert sc_ptr is not None

            id = native_bt.stream_class_get_id(sc_ptr)
            assert id >= 0

            yield id

    @property
    def env(self):
        return _TraceClassEnv(self)

    def create_stream_class(self, id=None,
                            name=None,
                            packet_context_field_class=None,
                            event_common_context_field_class=None,
                            default_clock_class=None,
                            assigns_automatic_event_class_id=True,
                            assigns_automatic_stream_id=True,
                            packets_have_default_beginning_clock_snapshot=False,
                            packets_have_default_end_clock_snapshot=False):

        if self.assigns_automatic_stream_class_id:
            if id is not None:
                raise bt2.CreationError('id provided, but trace class assigns automatic stream class ids')

            sc_ptr = native_bt.stream_class_create(self._ptr)
        else:
            if id is None:
                raise bt2.CreationError('id not provided, but trace class does not assign automatic stream class ids')

            utils._check_uint64(id)
            sc_ptr = native_bt.stream_class_create_with_id(self._ptr, id)

        sc = bt2.stream_class.StreamClass._create_from_ptr(sc_ptr)

        if name is not None:
            sc._name = name

        if packet_context_field_class is not None:
            sc._packet_context_field_class = packet_context_field_class

        if event_common_context_field_class is not None:
            sc._event_common_context_field_class = event_common_context_field_class

        if default_clock_class is not None:
            sc._default_clock_class = default_clock_class

        sc._assigns_automatic_event_class_id = assigns_automatic_event_class_id
        sc._assigns_automatic_stream_id = assigns_automatic_stream_id
        sc._packets_have_default_beginning_clock_snapshot = packets_have_default_beginning_clock_snapshot
        sc._packets_have_default_end_clock_snapshot = packets_have_default_end_clock_snapshot

        return sc

    @property
    def assigns_automatic_stream_class_id(self):
        return native_bt.trace_class_assigns_automatic_stream_class_id(self._ptr)

    def _assigns_automatic_stream_class_id(self, auto_id):
        utils._check_bool(auto_id)
        return native_bt.trace_class_set_assigns_automatic_stream_class_id(self._ptr, auto_id)

    _assigns_automatic_stream_class_id = property(fset=_assigns_automatic_stream_class_id)

    # Field class creation methods.

    def _check_create_status(self, ptr, type_name):
        if ptr is None:
            raise bt2.CreationError(
                'cannot create {} field class'.format(type_name))

    def _create_integer_field_class(self, create_func, py_cls, type_name, field_value_range, preferred_display_base):
        field_class_ptr = create_func(self._ptr)
        self._check_create_status(field_class_ptr, type_name)

        field_class = py_cls._create_from_ptr(field_class_ptr)

        if field_value_range is not None:
            field_class._field_value_range = field_value_range

        if preferred_display_base is not None:
            field_class._preferred_display_base = preferred_display_base

        return field_class

    def create_signed_integer_field_class(self, field_value_range=None, preferred_display_base=None):
        return self._create_integer_field_class(native_bt.field_class_signed_integer_create,
                                                bt2.field_class._SignedIntegerFieldClass,
                                                'signed integer', field_value_range, preferred_display_base)

    def create_unsigned_integer_field_class(self, field_value_range=None, preferred_display_base=None):
        return self._create_integer_field_class(native_bt.field_class_unsigned_integer_create,
                                                bt2.field_class._UnsignedIntegerFieldClass,
                                                'unsigned integer', field_value_range, preferred_display_base)

    def create_signed_enumeration_field_class(self, field_value_range=None, preferred_display_base=None):
        return self._create_integer_field_class(native_bt.field_class_signed_enumeration_create,
                                                bt2.field_class._SignedEnumerationFieldClass,
                                                'signed enumeration', field_value_range, preferred_display_base)

    def create_unsigned_enumeration_field_class(self, field_value_range=None, preferred_display_base=None):
        return self._create_integer_field_class(native_bt.field_class_unsigned_enumeration_create,
                                                bt2.field_class._UnsignedEnumerationFieldClass,
                                                'unsigned enumeration', field_value_range, preferred_display_base)

    def create_real_field_class(self, is_single_precision=False):
        field_class_ptr = native_bt.field_class_real_create(self._ptr)
        self._check_create_status(field_class_ptr, 'real')

        field_class = bt2.field_class._RealFieldClass._create_from_ptr(field_class_ptr)

        field_class._is_single_precision = is_single_precision

        return field_class

    def create_structure_field_class(self):
        field_class_ptr = native_bt.field_class_structure_create(self._ptr)
        self._check_create_status(field_class_ptr, 'structure')

        return bt2.field_class._StructureFieldClass._create_from_ptr(field_class_ptr)

    def create_string_field_class(self):
        field_class_ptr = native_bt.field_class_string_create(self._ptr)
        self._check_create_status(field_class_ptr, 'string')

        return bt2.field_class._StringFieldClass._create_from_ptr(field_class_ptr)

    def create_static_array_field_class(self, elem_fc, length):
        utils._check_type(elem_fc, bt2.field_class._FieldClass)
        utils._check_uint64(length)
        ptr = native_bt.field_class_static_array_create(self._ptr, elem_fc._ptr, length)
        self._check_create_status(ptr, 'static array')

        return bt2.field_class._StaticArrayFieldClass._create_from_ptr_and_get_ref(ptr)

    def create_dynamic_array_field_class(self, elem_fc, length_fc=None):
        utils._check_type(elem_fc, bt2.field_class._FieldClass)
        ptr = native_bt.field_class_dynamic_array_create(self._ptr, elem_fc._ptr)
        self._check_create_status(ptr, 'dynamic array')
        obj = bt2.field_class._DynamicArrayFieldClass._create_from_ptr(ptr)

        if length_fc is not None:
            obj._length_field_class = length_fc

        return obj

    def create_variant_field_class(self, selector_fc=None):
        ptr = native_bt.field_class_variant_create(self._ptr)
        self._check_create_status(ptr, 'variant')
        obj = bt2.field_class._VariantFieldClass._create_from_ptr(ptr)

        if selector_fc is not None:
            obj._selector_field_class = selector_fc

        return obj

    # Add a listener to be called when the trace class is destroyed.

    def add_destruction_listener(self, listener):

        if not callable(listener):
            raise TypeError("'listener' parameter is not callable")

        fn = native_bt.py3_trace_class_add_destruction_listener
        listener_from_native = functools.partial(_trace_class_destruction_listener_from_native,
                                                 listener)

        listener_id = fn(self._ptr, listener_from_native)
        if listener_id is None:
            utils._raise_bt2_error('cannot add destruction listener to trace class object')

        return bt2._ListenerHandle(listener_id, self)
