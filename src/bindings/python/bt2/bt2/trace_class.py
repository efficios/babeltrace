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

from bt2 import native_bt, utils, object
from bt2 import stream_class as bt2_stream_class
from bt2 import field_class as bt2_field_class
from bt2 import integer_range_set as bt2_integer_range_set
from bt2 import trace as bt2_trace
from bt2 import value as bt2_value
import collections.abc
import functools
import bt2


def _trace_class_destruction_listener_from_native(
    user_listener, handle, trace_class_ptr
):
    trace_class = _TraceClass._create_from_ptr_and_get_ref(trace_class_ptr)
    user_listener(trace_class)
    handle._invalidate()


class _TraceClassConst(object._SharedObject, collections.abc.Mapping):
    _get_ref = staticmethod(native_bt.trace_class_get_ref)
    _put_ref = staticmethod(native_bt.trace_class_put_ref)
    _borrow_stream_class_ptr_by_index = staticmethod(
        native_bt.trace_class_borrow_stream_class_by_index_const
    )
    _borrow_stream_class_ptr_by_id = staticmethod(
        native_bt.trace_class_borrow_stream_class_by_id_const
    )
    _borrow_user_attributes_ptr = staticmethod(
        native_bt.trace_class_borrow_user_attributes_const
    )
    _stream_class_pycls = bt2_stream_class._StreamClassConst
    _create_value_from_ptr_and_get_ref = staticmethod(
        bt2_value._create_from_const_ptr_and_get_ref
    )

    @property
    def user_attributes(self):
        ptr = self._borrow_user_attributes_ptr(self._ptr)
        assert ptr is not None
        return self._create_value_from_ptr_and_get_ref(ptr)

    # Number of stream classes in this trace class.

    def __len__(self):
        count = native_bt.trace_class_get_stream_class_count(self._ptr)
        assert count >= 0
        return count

    # Get a stream class by stream id.

    def __getitem__(self, key):
        utils._check_uint64(key)

        sc_ptr = self._borrow_stream_class_ptr_by_id(self._ptr, key)
        if sc_ptr is None:
            raise KeyError(key)

        return self._stream_class_pycls._create_from_ptr_and_get_ref(sc_ptr)

    def __iter__(self):
        for idx in range(len(self)):
            sc_ptr = self._borrow_stream_class_ptr_by_index(self._ptr, idx)
            assert sc_ptr is not None

            id = native_bt.stream_class_get_id(sc_ptr)
            assert id >= 0

            yield id

    @property
    def assigns_automatic_stream_class_id(self):
        return native_bt.trace_class_assigns_automatic_stream_class_id(self._ptr)

    # Add a listener to be called when the trace class is destroyed.

    def add_destruction_listener(self, listener):

        if not callable(listener):
            raise TypeError("'listener' parameter is not callable")

        handle = utils._ListenerHandle(self.addr)

        listener_from_native = functools.partial(
            _trace_class_destruction_listener_from_native, listener, handle
        )

        fn = native_bt.bt2_trace_class_add_destruction_listener
        status, listener_id = fn(self._ptr, listener_from_native)
        utils._handle_func_status(
            status, 'cannot add destruction listener to trace class object'
        )

        handle._set_listener_id(listener_id)

        return handle

    def remove_destruction_listener(self, listener_handle):
        utils._check_type(listener_handle, utils._ListenerHandle)

        if listener_handle._addr != self.addr:
            raise ValueError(
                'This trace class destruction listener does not match the trace class object.'
            )

        if listener_handle._listener_id is None:
            raise ValueError(
                'This trace class destruction listener was already removed.'
            )

        status = native_bt.trace_class_remove_destruction_listener(
            self._ptr, listener_handle._listener_id
        )
        utils._handle_func_status(status)
        listener_handle._invalidate()


class _TraceClass(_TraceClassConst):
    _borrow_stream_class_ptr_by_index = staticmethod(
        native_bt.trace_class_borrow_stream_class_by_index
    )
    _borrow_stream_class_ptr_by_id = staticmethod(
        native_bt.trace_class_borrow_stream_class_by_id
    )
    _borrow_user_attributes_ptr = staticmethod(
        native_bt.trace_class_borrow_user_attributes
    )
    _stream_class_pycls = bt2_stream_class._StreamClass
    _create_value_from_ptr_and_get_ref = staticmethod(
        bt2_value._create_from_ptr_and_get_ref
    )

    # Instantiate a trace of this class.

    def __call__(self, name=None, user_attributes=None, uuid=None, environment=None):
        trace_ptr = native_bt.trace_create(self._ptr)

        if trace_ptr is None:
            raise bt2._MemoryError('cannot create trace class object')

        trace = bt2_trace._Trace._create_from_ptr(trace_ptr)

        if name is not None:
            trace._name = name

        if user_attributes is not None:
            trace._user_attributes = user_attributes

        if uuid is not None:
            trace._uuid = uuid

        if environment is not None:
            for key, value in environment.items():
                trace.environment[key] = value

        return trace

    def create_stream_class(
        self,
        id=None,
        name=None,
        user_attributes=None,
        packet_context_field_class=None,
        event_common_context_field_class=None,
        default_clock_class=None,
        assigns_automatic_event_class_id=True,
        assigns_automatic_stream_id=True,
        supports_packets=False,
        packets_have_beginning_default_clock_snapshot=False,
        packets_have_end_default_clock_snapshot=False,
        supports_discarded_events=False,
        discarded_events_have_default_clock_snapshots=False,
        supports_discarded_packets=False,
        discarded_packets_have_default_clock_snapshots=False,
    ):
        # Validate parameters before we create the object.
        bt2_stream_class._StreamClass._validate_create_params(
            name,
            user_attributes,
            packet_context_field_class,
            event_common_context_field_class,
            default_clock_class,
            assigns_automatic_event_class_id,
            assigns_automatic_stream_id,
            supports_packets,
            packets_have_beginning_default_clock_snapshot,
            packets_have_end_default_clock_snapshot,
            supports_discarded_events,
            discarded_events_have_default_clock_snapshots,
            supports_discarded_packets,
            discarded_packets_have_default_clock_snapshots,
        )

        if self.assigns_automatic_stream_class_id:
            if id is not None:
                raise ValueError(
                    'id provided, but trace class assigns automatic stream class ids'
                )

            sc_ptr = native_bt.stream_class_create(self._ptr)
        else:
            if id is None:
                raise ValueError(
                    'id not provided, but trace class does not assign automatic stream class ids'
                )

            utils._check_uint64(id)
            sc_ptr = native_bt.stream_class_create_with_id(self._ptr, id)

        sc = bt2_stream_class._StreamClass._create_from_ptr(sc_ptr)

        if name is not None:
            sc._name = name

        if user_attributes is not None:
            sc._user_attributes = user_attributes

        if event_common_context_field_class is not None:
            sc._event_common_context_field_class = event_common_context_field_class

        if default_clock_class is not None:
            sc._default_clock_class = default_clock_class

        # call after `sc._default_clock_class` because, if
        # `packets_have_beginning_default_clock_snapshot` or
        # `packets_have_end_default_clock_snapshot` is true, then this
        # stream class needs a default clock class already.
        sc._set_supports_packets(
            supports_packets,
            packets_have_beginning_default_clock_snapshot,
            packets_have_end_default_clock_snapshot,
        )

        # call after sc._set_supports_packets() because, if
        # `packet_context_field_class` is not `None`, then this stream
        # class needs to support packets already.
        if packet_context_field_class is not None:
            sc._packet_context_field_class = packet_context_field_class

        sc._assigns_automatic_event_class_id = assigns_automatic_event_class_id
        sc._assigns_automatic_stream_id = assigns_automatic_stream_id
        sc._set_supports_discarded_events(
            supports_discarded_events, discarded_events_have_default_clock_snapshots
        )
        sc._set_supports_discarded_packets(
            supports_discarded_packets, discarded_packets_have_default_clock_snapshots
        )
        return sc

    def _user_attributes(self, user_attributes):
        value = bt2_value.create_value(user_attributes)
        utils._check_type(value, bt2_value.MapValue)
        native_bt.trace_class_set_user_attributes(self._ptr, value._ptr)

    _user_attributes = property(fset=_user_attributes)

    def _assigns_automatic_stream_class_id(self, auto_id):
        utils._check_bool(auto_id)
        return native_bt.trace_class_set_assigns_automatic_stream_class_id(
            self._ptr, auto_id
        )

    _assigns_automatic_stream_class_id = property(
        fset=_assigns_automatic_stream_class_id
    )

    # Field class creation methods.

    def _check_field_class_create_status(self, ptr, type_name):
        if ptr is None:
            raise bt2._MemoryError('cannot create {} field class'.format(type_name))

    @staticmethod
    def _set_field_class_user_attrs(fc, user_attributes):
        if user_attributes is not None:
            fc._user_attributes = user_attributes

    def create_bool_field_class(self, user_attributes=None):
        field_class_ptr = native_bt.field_class_bool_create(self._ptr)
        self._check_field_class_create_status(field_class_ptr, 'boolean')
        fc = bt2_field_class._BoolFieldClass._create_from_ptr(field_class_ptr)
        self._set_field_class_user_attrs(fc, user_attributes)
        return fc

    def create_bit_array_field_class(self, length, user_attributes=None):
        utils._check_uint64(length)

        if length < 1 or length > 64:
            raise ValueError(
                'invalid length {}: expecting a value in the [1, 64] range'.format(
                    length
                )
            )

        field_class_ptr = native_bt.field_class_bit_array_create(self._ptr, length)
        self._check_field_class_create_status(field_class_ptr, 'bit array')
        fc = bt2_field_class._BitArrayFieldClass._create_from_ptr(field_class_ptr)
        self._set_field_class_user_attrs(fc, user_attributes)
        return fc

    def _create_integer_field_class(
        self,
        create_func,
        py_cls,
        type_name,
        field_value_range,
        preferred_display_base,
        user_attributes,
    ):
        field_class_ptr = create_func(self._ptr)
        self._check_field_class_create_status(field_class_ptr, type_name)

        field_class = py_cls._create_from_ptr(field_class_ptr)

        if field_value_range is not None:
            field_class._field_value_range = field_value_range

        if preferred_display_base is not None:
            field_class._preferred_display_base = preferred_display_base

        self._set_field_class_user_attrs(field_class, user_attributes)
        return field_class

    def create_signed_integer_field_class(
        self, field_value_range=None, preferred_display_base=None, user_attributes=None
    ):
        return self._create_integer_field_class(
            native_bt.field_class_integer_signed_create,
            bt2_field_class._SignedIntegerFieldClass,
            'signed integer',
            field_value_range,
            preferred_display_base,
            user_attributes,
        )

    def create_unsigned_integer_field_class(
        self, field_value_range=None, preferred_display_base=None, user_attributes=None
    ):
        return self._create_integer_field_class(
            native_bt.field_class_integer_unsigned_create,
            bt2_field_class._UnsignedIntegerFieldClass,
            'unsigned integer',
            field_value_range,
            preferred_display_base,
            user_attributes,
        )

    def create_signed_enumeration_field_class(
        self, field_value_range=None, preferred_display_base=None, user_attributes=None
    ):
        return self._create_integer_field_class(
            native_bt.field_class_enumeration_signed_create,
            bt2_field_class._SignedEnumerationFieldClass,
            'signed enumeration',
            field_value_range,
            preferred_display_base,
            user_attributes,
        )

    def create_unsigned_enumeration_field_class(
        self, field_value_range=None, preferred_display_base=None, user_attributes=None
    ):
        return self._create_integer_field_class(
            native_bt.field_class_enumeration_unsigned_create,
            bt2_field_class._UnsignedEnumerationFieldClass,
            'unsigned enumeration',
            field_value_range,
            preferred_display_base,
            user_attributes,
        )

    def create_single_precision_real_field_class(self, user_attributes=None):
        field_class_ptr = native_bt.field_class_real_single_precision_create(self._ptr)
        self._check_field_class_create_status(field_class_ptr, 'single-precision real')

        field_class = bt2_field_class._SinglePrecisionRealFieldClass._create_from_ptr(
            field_class_ptr
        )

        self._set_field_class_user_attrs(field_class, user_attributes)

        return field_class

    def create_double_precision_real_field_class(self, user_attributes=None):
        field_class_ptr = native_bt.field_class_real_double_precision_create(self._ptr)
        self._check_field_class_create_status(field_class_ptr, 'double-precision real')

        field_class = bt2_field_class._DoublePrecisionRealFieldClass._create_from_ptr(
            field_class_ptr
        )

        self._set_field_class_user_attrs(field_class, user_attributes)

        return field_class

    def create_structure_field_class(self, user_attributes=None):
        field_class_ptr = native_bt.field_class_structure_create(self._ptr)
        self._check_field_class_create_status(field_class_ptr, 'structure')
        fc = bt2_field_class._StructureFieldClass._create_from_ptr(field_class_ptr)
        self._set_field_class_user_attrs(fc, user_attributes)
        return fc

    def create_string_field_class(self, user_attributes=None):
        field_class_ptr = native_bt.field_class_string_create(self._ptr)
        self._check_field_class_create_status(field_class_ptr, 'string')
        fc = bt2_field_class._StringFieldClass._create_from_ptr(field_class_ptr)
        self._set_field_class_user_attrs(fc, user_attributes)
        return fc

    def create_static_array_field_class(self, elem_fc, length, user_attributes=None):
        utils._check_type(elem_fc, bt2_field_class._FieldClass)
        utils._check_uint64(length)
        ptr = native_bt.field_class_array_static_create(self._ptr, elem_fc._ptr, length)
        self._check_field_class_create_status(ptr, 'static array')
        fc = bt2_field_class._StaticArrayFieldClass._create_from_ptr_and_get_ref(ptr)
        self._set_field_class_user_attrs(fc, user_attributes)
        return fc

    def create_dynamic_array_field_class(
        self, elem_fc, length_fc=None, user_attributes=None
    ):
        utils._check_type(elem_fc, bt2_field_class._FieldClass)
        length_fc_ptr = None

        if length_fc is not None:
            utils._check_type(length_fc, bt2_field_class._UnsignedIntegerFieldClass)
            length_fc_ptr = length_fc._ptr

        ptr = native_bt.field_class_array_dynamic_create(
            self._ptr, elem_fc._ptr, length_fc_ptr
        )
        self._check_field_class_create_status(ptr, 'dynamic array')
        fc = bt2_field_class._create_field_class_from_ptr_and_get_ref(ptr)
        self._set_field_class_user_attrs(fc, user_attributes)
        return fc

    def create_option_without_selector_field_class(
        self, content_fc, user_attributes=None
    ):
        utils._check_type(content_fc, bt2_field_class._FieldClass)
        ptr = native_bt.field_class_option_without_selector_create(
            self._ptr, content_fc._ptr
        )
        self._check_field_class_create_status(ptr, 'option')
        fc = bt2_field_class._create_field_class_from_ptr_and_get_ref(ptr)
        self._set_field_class_user_attrs(fc, user_attributes)
        return fc

    def create_option_with_bool_selector_field_class(
        self, content_fc, selector_fc, selector_is_reversed=False, user_attributes=None
    ):
        utils._check_type(content_fc, bt2_field_class._FieldClass)
        utils._check_bool(selector_is_reversed)
        utils._check_type(selector_fc, bt2_field_class._BoolFieldClass)
        ptr = native_bt.field_class_option_with_selector_field_bool_create(
            self._ptr, content_fc._ptr, selector_fc._ptr
        )
        self._check_field_class_create_status(ptr, 'option')
        fc = bt2_field_class._create_field_class_from_ptr_and_get_ref(ptr)
        self._set_field_class_user_attrs(fc, user_attributes)
        fc._selector_is_reversed = selector_is_reversed
        return fc

    def create_option_with_integer_selector_field_class(
        self, content_fc, selector_fc, ranges, user_attributes=None
    ):
        utils._check_type(content_fc, bt2_field_class._FieldClass)
        utils._check_type(selector_fc, bt2_field_class._IntegerFieldClass)

        if len(ranges) == 0:
            raise ValueError('integer range set is empty')

        if isinstance(selector_fc, bt2_field_class._UnsignedIntegerFieldClass):
            utils._check_type(ranges, bt2_integer_range_set.UnsignedIntegerRangeSet)
            ptr = native_bt.field_class_option_with_selector_field_integer_unsigned_create(
                self._ptr, content_fc._ptr, selector_fc._ptr, ranges._ptr
            )
        else:
            utils._check_type(ranges, bt2_integer_range_set.SignedIntegerRangeSet)
            ptr = (
                native_bt.field_class_option_with_selector_field_integer_signed_create(
                    self._ptr, content_fc._ptr, selector_fc._ptr, ranges._ptr
                )
            )

        self._check_field_class_create_status(ptr, 'option')
        fc = bt2_field_class._create_field_class_from_ptr_and_get_ref(ptr)
        self._set_field_class_user_attrs(fc, user_attributes)
        return fc

    def create_variant_field_class(self, selector_fc=None, user_attributes=None):
        selector_fc_ptr = None

        if selector_fc is not None:
            utils._check_type(selector_fc, bt2_field_class._IntegerFieldClass)
            selector_fc_ptr = selector_fc._ptr

        ptr = native_bt.field_class_variant_create(self._ptr, selector_fc_ptr)
        self._check_field_class_create_status(ptr, 'variant')
        fc = bt2_field_class._create_field_class_from_ptr_and_get_ref(ptr)
        self._set_field_class_user_attrs(fc, user_attributes)
        return fc
