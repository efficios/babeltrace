# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
# Copyright (c) 2018 Francis Deslauriers <francis.deslauriers@efficios.com>
# Copyright (c) 2019 Simon Marchi <simon.marchi@efficios.com>

import uuid as uuidp
import typing
import warnings
import functools

from bt2 import error as bt2_error
from bt2 import trace as bt2_trace
from bt2 import utils as bt2_utils
from bt2 import value as bt2_value
from bt2 import object as bt2_object
from bt2 import native_bt
from bt2 import clock_class as bt2_clock_class
from bt2 import field_class as bt2_field_class
from bt2 import stream_class as bt2_stream_class
from bt2 import field_location as bt2_field_location
from bt2 import user_attributes as bt2_user_attrs
from bt2 import integer_range_set as bt2_integer_range_set


def _trace_class_destruction_listener_from_native(
    user_listener, handle, trace_class_ptr
):
    user_listener(_TraceClassConst._create_from_ptr_and_get_ref(trace_class_ptr))
    handle._invalidate()


class _TraceClassConst(
    bt2_object._SharedObject,
    bt2_user_attrs._WithUserAttrsConst,
    typing.Mapping[int, bt2_stream_class._StreamClassConst],
):
    @staticmethod
    def _get_ref(ptr):
        native_bt.trace_class_get_ref(ptr)

    @staticmethod
    def _put_ref(ptr):
        native_bt.trace_class_put_ref(ptr)

    _borrow_stream_class_ptr_by_index = staticmethod(
        native_bt.trace_class_borrow_stream_class_by_index_const
    )
    _borrow_stream_class_ptr_by_id = staticmethod(
        native_bt.trace_class_borrow_stream_class_by_id_const
    )

    @staticmethod
    def _borrow_user_attributes_ptr(ptr):
        return native_bt.trace_class_borrow_user_attributes_const(ptr)

    _stream_class_pycls = bt2_stream_class._StreamClassConst

    # Number of stream classes in this trace class.

    def __len__(self):
        return native_bt.trace_class_get_stream_class_count(self._ptr)

    # Get a stream class by stream id.

    def __getitem__(self, key):
        bt2_utils._check_uint64(key)

        sc_ptr = self._borrow_stream_class_ptr_by_id(self._ptr, key)
        if sc_ptr is None:
            raise KeyError(key)

        return self._stream_class_pycls._create_from_ptr_and_get_ref(sc_ptr)

    def __iter__(self):
        for idx in range(len(self)):
            yield native_bt.stream_class_get_id(
                self._borrow_stream_class_ptr_by_index(self._ptr, idx)
            )

    @property
    def graph_mip_version(self) -> int:
        return native_bt.trace_class_get_graph_mip_version(self._ptr)

    @property
    def assigns_automatic_stream_class_id(self):
        return native_bt.trace_class_assigns_automatic_stream_class_id(self._ptr)

    # Add a listener to be called when the trace class is destroyed.

    def add_destruction_listener(self, listener):
        if not callable(listener):
            raise TypeError("'listener' parameter is not callable")

        handle = bt2_utils._ListenerHandle(self.addr)

        native_callable = functools.partial(
            _trace_class_destruction_listener_from_native, listener, handle
        )
        status, listener_id = native_bt.trace_class_add_destruction_listener(
            self._ptr, native_callable
        )
        bt2_utils._handle_func_status(
            status, "cannot add destruction listener to trace class object"
        )

        handle._set_listener_id(listener_id, native_callable)

        return handle

    def remove_destruction_listener(self, listener_handle):
        bt2_utils._check_type(listener_handle, bt2_utils._ListenerHandle)

        if listener_handle._addr != self.addr:
            raise ValueError(
                "This trace class destruction listener does not match the trace class object."
            )

        if listener_handle._listener_id is None:
            raise ValueError(
                "This trace class destruction listener was already removed."
            )

        bt2_utils._handle_func_status(
            native_bt.trace_class_remove_destruction_listener(
                self._ptr,
                listener_handle._listener_id,
                listener_handle._native_callable,
            )
        )
        listener_handle._invalidate()


_FieldClassT = typing.TypeVar("_FieldClassT", bound="bt2_field_class._FieldClass")
_IntegerFieldClassT = typing.TypeVar(
    "_IntegerFieldClassT", bound="bt2_field_class._IntegerFieldClass"
)


class _TraceClass(bt2_user_attrs._WithUserAttrs, _TraceClassConst):
    _borrow_stream_class_ptr_by_index = staticmethod(
        native_bt.trace_class_borrow_stream_class_by_index
    )
    _borrow_stream_class_ptr_by_id = staticmethod(
        native_bt.trace_class_borrow_stream_class_by_id
    )

    @staticmethod
    def _borrow_user_attributes_ptr(ptr):
        return native_bt.trace_class_borrow_user_attributes(ptr)

    _stream_class_pycls = bt2_stream_class._StreamClass

    # Instantiate a trace of this class.

    def __call__(
        self,
        name: typing.Optional[str] = None,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
        uuid: typing.Optional[uuidp.UUID] = None,
        environment: typing.Optional[
            typing.Mapping[str, typing.Union[str, int]]
        ] = None,
        namespace: typing.Optional[str] = None,
        uid: typing.Optional[str] = None,
    ) -> bt2_trace._Trace:
        trace_ptr = native_bt.trace_create(self._ptr)

        if trace_ptr is None:
            raise bt2_error._MemoryError("cannot create trace class object")

        trace = bt2_trace._Trace._create_from_ptr(trace_ptr)

        if namespace is not None:
            trace._set_namespace(namespace)

        if name is not None:
            trace._set_name(name)

        if uid is not None:
            trace._set_uid(uid)

        if user_attributes is not None:
            trace._set_user_attributes(user_attributes)

        if uuid is not None:
            trace._set_uuid(uuid)

        if environment is not None:
            for key, value in environment.items():
                trace.environment[key] = value

        return trace

    def create_stream_class(
        self,
        id: typing.Optional[int] = None,
        name: typing.Optional[str] = None,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
        packet_context_field_class: typing.Optional[
            bt2_field_class._StructureFieldClass
        ] = None,
        event_common_context_field_class: typing.Optional[
            bt2_field_class._StructureFieldClass
        ] = None,
        default_clock_class: typing.Optional[bt2_clock_class._ClockClass] = None,
        assigns_automatic_event_class_id: bool = True,
        assigns_automatic_stream_id: bool = True,
        supports_packets: bool = False,
        packets_have_beginning_default_clock_snapshot: bool = False,
        packets_have_end_default_clock_snapshot: bool = False,
        supports_discarded_events: bool = False,
        discarded_events_have_default_clock_snapshots: bool = False,
        supports_discarded_packets: bool = False,
        discarded_packets_have_default_clock_snapshots: bool = False,
        namespace: typing.Optional[str] = None,
        uid: typing.Optional[str] = None,
    ) -> bt2_stream_class._StreamClass:
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
                    "id provided, but trace class assigns automatic stream class ids"
                )

            sc_ptr = native_bt.stream_class_create(self._ptr)
        else:
            if id is None:
                raise ValueError(
                    "id not provided, but trace class does not assign automatic stream class ids"
                )

            bt2_utils._check_uint64(id)
            sc_ptr = native_bt.stream_class_create_with_id(self._ptr, id)

        sc = bt2_stream_class._StreamClass._create_from_ptr(sc_ptr)

        if namespace is not None:
            sc._set_namespace(namespace)

        if name is not None:
            sc._set_name(name)

        if uid is not None:
            sc._set_uid(uid)

        if user_attributes is not None:
            sc._set_user_attributes(user_attributes)

        if event_common_context_field_class is not None:
            sc._set_event_common_context_field_class(event_common_context_field_class)

        if default_clock_class is not None:
            sc._set_default_clock_class(default_clock_class)

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
            sc._set_packet_context_field_class(packet_context_field_class)

        sc._set_assigns_automatic_event_class_id(assigns_automatic_event_class_id)
        sc._set_assigns_automatic_stream_id(assigns_automatic_stream_id)
        sc._set_supports_discarded_events(
            supports_discarded_events, discarded_events_have_default_clock_snapshots
        )
        sc._set_supports_discarded_packets(
            supports_discarded_packets, discarded_packets_have_default_clock_snapshots
        )
        return sc

    @staticmethod
    def _set_user_attributes_ptr(obj_ptr, value_ptr):
        native_bt.trace_class_set_user_attributes(obj_ptr, value_ptr)

    def _set_assigns_automatic_stream_class_id(self, auto_id):
        bt2_utils._check_bool(auto_id)
        return native_bt.trace_class_set_assigns_automatic_stream_class_id(
            self._ptr, auto_id
        )

    def create_field_location(
        self, root_scope: bt2_field_location.FieldLocationScope, items: typing.List[str]
    ) -> bt2_field_location._FieldLocationConst:
        bt2_utils._check_mip_ge(self, "Field location", 1)
        bt2_utils._check_type(root_scope, bt2_field_location.FieldLocationScope)
        bt2_utils._check_type(items, list)

        if len(items) == 0:
            raise ValueError("`items` must not be empty")

        for item in items:
            bt2_utils._check_str(item)

        ptr = native_bt.field_location_create(self._ptr, root_scope.value, items)
        return bt2_field_location._FieldLocationConst._create_from_ptr(ptr)

    # Field class creation methods.

    @staticmethod
    def _check_and_wrap_field_class(
        ptr,
        type_name: str,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue],
        expected_type: typing.Type[_FieldClassT],
    ) -> _FieldClassT:
        if ptr is None:
            raise bt2_error._MemoryError(f"cannot create {type_name} field class")

        fc = bt2_field_class._create_field_class_from_ptr(ptr)
        assert type(fc) is expected_type

        if user_attributes is not None:
            fc._set_user_attributes(user_attributes)

        return fc

    def create_bool_field_class(
        self, user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None
    ) -> bt2_field_class._BoolFieldClass:
        return self._check_and_wrap_field_class(
            native_bt.field_class_bool_create(self._ptr),
            "boolean",
            user_attributes,
            bt2_field_class._BoolFieldClass,
        )

    def create_bit_array_field_class(
        self,
        length: int,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
        flags: typing.Optional[
            typing.Iterable[
                typing.Tuple[
                    str,
                    bt2_integer_range_set._UnsignedIntegerRangeSetConst,
                ]
            ]
        ] = None,
    ) -> bt2_field_class._BitArrayFieldClass:
        bt2_utils._check_uint64(length)

        if length < 1 or length > 64:
            raise ValueError(
                f"invalid length {length}: expecting a value in the [1, 64] range"
            )

        fc = self._check_and_wrap_field_class(
            native_bt.field_class_bit_array_create(self._ptr, length),
            "bit array",
            user_attributes,
            bt2_field_class._BitArrayFieldClass,
        )

        if flags:
            fc += flags

        return fc

    def _create_integer_field_class(
        self,
        create_func,
        type_name,
        field_value_range,
        preferred_display_base,
        user_attributes,
        expected_type: typing.Type[_IntegerFieldClassT],
    ) -> _IntegerFieldClassT:
        fc = self._check_and_wrap_field_class(
            create_func(self._ptr), type_name, user_attributes, expected_type
        )

        if field_value_range is not None:
            fc._set_field_value_range(field_value_range)

        if preferred_display_base is not None:
            fc._set_preferred_display_base(preferred_display_base)

        return fc

    def create_signed_integer_field_class(
        self,
        field_value_range: typing.Optional[int] = None,
        preferred_display_base: typing.Optional[
            bt2_field_class.IntegerDisplayBase
        ] = None,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
    ) -> bt2_field_class._SignedIntegerFieldClass:
        return self._create_integer_field_class(
            native_bt.field_class_integer_signed_create,
            "signed integer",
            field_value_range,
            preferred_display_base,
            user_attributes,
            bt2_field_class._SignedIntegerFieldClass,
        )

    def create_unsigned_integer_field_class(
        self,
        field_value_range: typing.Optional[int] = None,
        preferred_display_base: typing.Optional[
            bt2_field_class.IntegerDisplayBase
        ] = None,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
    ) -> bt2_field_class._UnsignedIntegerFieldClass:
        return self._create_integer_field_class(
            native_bt.field_class_integer_unsigned_create,
            "unsigned integer",
            field_value_range,
            preferred_display_base,
            user_attributes,
            bt2_field_class._UnsignedIntegerFieldClass,
        )

    def create_signed_enumeration_field_class(
        self,
        field_value_range: typing.Optional[int] = None,
        preferred_display_base: typing.Optional[
            bt2_field_class.IntegerDisplayBase
        ] = None,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
        mappings: typing.Optional[
            typing.Iterable[
                typing.Tuple[str, bt2_integer_range_set._SignedIntegerRangeSetConst]
            ]
        ] = None,
    ) -> bt2_field_class._SignedEnumerationFieldClass:
        fc = self._create_integer_field_class(
            native_bt.field_class_enumeration_signed_create,
            "signed enumeration",
            field_value_range,
            preferred_display_base,
            user_attributes,
            bt2_field_class._SignedEnumerationFieldClass,
        )

        if mappings is not None:
            fc += mappings

        return fc

    def create_unsigned_enumeration_field_class(
        self,
        field_value_range: typing.Optional[int] = None,
        preferred_display_base: typing.Optional[
            bt2_field_class.IntegerDisplayBase
        ] = None,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
        mappings: typing.Optional[
            typing.Iterable[
                typing.Tuple[str, bt2_integer_range_set._UnsignedIntegerRangeSetConst]
            ]
        ] = None,
    ) -> bt2_field_class._UnsignedEnumerationFieldClass:
        fc = self._create_integer_field_class(
            native_bt.field_class_enumeration_unsigned_create,
            "unsigned enumeration",
            field_value_range,
            preferred_display_base,
            user_attributes,
            bt2_field_class._UnsignedEnumerationFieldClass,
        )

        if mappings:
            fc += mappings

        return fc

    def create_single_precision_real_field_class(
        self, user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None
    ) -> bt2_field_class._SinglePrecisionRealFieldClass:
        return self._check_and_wrap_field_class(
            native_bt.field_class_real_single_precision_create(self._ptr),
            "single-precision real",
            user_attributes,
            bt2_field_class._SinglePrecisionRealFieldClass,
        )

    def create_double_precision_real_field_class(
        self, user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None
    ) -> bt2_field_class._DoublePrecisionRealFieldClass:
        return self._check_and_wrap_field_class(
            native_bt.field_class_real_double_precision_create(self._ptr),
            "double-precision real",
            user_attributes,
            bt2_field_class._DoublePrecisionRealFieldClass,
        )

    def create_structure_field_class(
        self,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
        members: typing.Optional[
            typing.Iterable[
                typing.Union[
                    typing.Tuple[str, bt2_field_class._FieldClass],
                    typing.Tuple[
                        str,
                        bt2_field_class._FieldClass,
                        typing.Optional[bt2_value._MapValueConst],
                    ],
                ]
            ]
        ] = None,
    ) -> bt2_field_class._StructureFieldClass:
        fc = self._check_and_wrap_field_class(
            native_bt.field_class_structure_create(self._ptr),
            "structure",
            user_attributes,
            bt2_field_class._StructureFieldClass,
        )

        if members is not None:
            fc += members

        return fc

    def create_string_field_class(
        self, user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None
    ) -> bt2_field_class._StringFieldClass:
        return self._check_and_wrap_field_class(
            native_bt.field_class_string_create(self._ptr),
            "string",
            user_attributes,
            bt2_field_class._StringFieldClass,
        )

    def create_static_array_field_class(
        self,
        elem_fc: bt2_field_class._FieldClass,
        length: int,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
    ) -> bt2_field_class._StaticArrayFieldClass:
        bt2_utils._check_type(elem_fc, bt2_field_class._FieldClass)
        bt2_utils._check_uint64(length)
        return self._check_and_wrap_field_class(
            native_bt.field_class_array_static_create(self._ptr, elem_fc._ptr, length),
            "static array",
            user_attributes,
            bt2_field_class._StaticArrayFieldClass,
        )

    @typing.overload
    def create_dynamic_array_field_class(
        self,
        elem_fc: bt2_field_class._FieldClass,
        length_fc: None = None,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
        length_field_location: typing.Optional[
            bt2_field_location._FieldLocationConst
        ] = None,
    ) -> bt2_field_class._DynamicArrayFieldClass: ...

    @typing.overload
    def create_dynamic_array_field_class(
        self,
        elem_fc: bt2_field_class._FieldClass,
        length_fc: bt2_field_class._UnsignedIntegerFieldClass,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
    ) -> bt2_field_class._DynamicArrayWithLengthFieldFieldClass: ...

    @typing.overload
    def create_dynamic_array_field_class(
        self,
        elem_fc: bt2_field_class._FieldClass,
        length_fc: bt2_field_class._UnsignedIntegerFieldClass,
        user_attributes: None,
        length_field_location: typing.Optional[bt2_field_location._FieldLocationConst],
    ) -> bt2_field_class._DynamicArrayWithLengthFieldFieldClass: ...

    @typing.overload
    def create_dynamic_array_field_class(
        self,
        elem_fc: bt2_field_class._FieldClass,
        length_fc: bt2_field_class._UnsignedIntegerFieldClass,
        user_attributes: bt2_value._MapValueConst,
        length_field_location: typing.Optional[bt2_field_location._FieldLocationConst],
    ) -> bt2_field_class._DynamicArrayWithLengthFieldFieldClass: ...

    def create_dynamic_array_field_class(
        self,
        elem_fc: bt2_field_class._FieldClass,
        length_fc: typing.Optional[bt2_field_class._UnsignedIntegerFieldClass] = None,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
        length_field_location: typing.Optional[
            bt2_field_location._FieldLocationConst
        ] = None,
    ) -> bt2_field_class._DynamicArrayFieldClass:
        bt2_utils._check_type(elem_fc, bt2_field_class._FieldClass)

        if self.graph_mip_version == 0:
            if length_field_location is not None:
                raise ValueError("length field location is not supported with MIP 0")

            if length_fc is not None:
                bt2_utils._check_type(
                    length_fc, bt2_field_class._UnsignedIntegerFieldClass
                )
                length_fc_ptr = length_fc._ptr
                expected_type = bt2_field_class._DynamicArrayWithLengthFieldFieldClass
            else:
                length_fc_ptr = None
                expected_type = bt2_field_class._DynamicArrayFieldClass

            return self._check_and_wrap_field_class(
                native_bt.field_class_array_dynamic_create(
                    self._ptr, elem_fc._ptr, length_fc_ptr
                ),
                "dynamic array",
                user_attributes,
                expected_type,
            )

        else:
            if length_fc is not None:
                raise ValueError(
                    f"length field class is not supported with MIP {self.graph_mip_version}"
                )

            if length_field_location is None:
                return self._check_and_wrap_field_class(
                    native_bt.field_class_array_dynamic_without_length_field_location_create(
                        self._ptr, elem_fc._ptr
                    ),
                    "dynamic array",
                    user_attributes,
                    bt2_field_class._DynamicArrayFieldClass,
                )
            else:
                bt2_utils._check_type(
                    length_field_location, bt2_field_location._FieldLocationConst
                )
                return self._check_and_wrap_field_class(
                    native_bt.field_class_array_dynamic_with_length_field_location_create(
                        self._ptr, elem_fc._ptr, length_field_location._ptr
                    ),
                    "dynamic array",
                    user_attributes,
                    bt2_field_class._DynamicArrayWithLengthFieldFieldClass,
                )

    def create_option_field_class_without_selector_field(
        self,
        content_fc: bt2_field_class._FieldClass,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
    ) -> bt2_field_class._OptionFieldClass:
        bt2_utils._check_type(content_fc, bt2_field_class._FieldClass)
        return self._check_and_wrap_field_class(
            (
                native_bt.field_class_option_without_selector_create
                if self.graph_mip_version == 0
                else native_bt.field_class_option_without_selector_field_location_create
            )(self._ptr, content_fc._ptr),
            "option",
            user_attributes,
            bt2_field_class._OptionFieldClass,
        )

    def create_option_without_selector_field_class(self, *args, **kwargs):
        warnings.warn(
            "`create_option_without_selector_field_class()` is deprecated; "
            "use `create_option_field_class_without_selector_field()` instead",
            DeprecationWarning,
        )
        return self.create_option_field_class_without_selector_field(*args, **kwargs)

    def create_option_field_class_with_bool_selector_field(
        self,
        content_fc: bt2_field_class._FieldClass,
        selector_fc: typing.Optional[bt2_field_class._FieldClass] = None,
        selector_is_reversed: bool = False,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
        selector_field_location: typing.Optional[
            bt2_field_location._FieldLocationConst
        ] = None,
    ) -> bt2_field_class._OptionWithBoolSelectorFieldClass:
        bt2_utils._check_type(content_fc, bt2_field_class._FieldClass)
        bt2_utils._check_bool(selector_is_reversed)

        if self.graph_mip_version == 0:
            selector_fc = bt2_utils._check_type(
                selector_fc, bt2_field_class._BoolFieldClass
            )

            if selector_field_location is not None:
                raise ValueError("selector field location is not supported with MIP 0")

            fc_ptr = native_bt.field_class_option_with_selector_field_bool_create(
                self._ptr, content_fc._ptr, selector_fc._ptr
            )

        else:
            selector_field_location = bt2_utils._check_type(
                selector_field_location, bt2_field_location._FieldLocationConst
            )

            if selector_fc is not None:
                raise ValueError(
                    f"selector field class is not supported with MIP {self.graph_mip_version}"
                )

            fc_ptr = (
                native_bt.field_class_option_with_selector_field_location_bool_create(
                    self._ptr, content_fc._ptr, selector_field_location._ptr
                )
            )

        fc = self._check_and_wrap_field_class(
            fc_ptr,
            "option",
            user_attributes,
            bt2_field_class._OptionWithBoolSelectorFieldClass,
        )
        fc._set_selector_is_reversed(selector_is_reversed)
        return fc

    def create_option_with_bool_selector_field_class(self, *args, **kwargs):
        warnings.warn(
            "`create_option_with_bool_selector_field_class()` is deprecated; "
            "use `create_option_field_class_with_bool_selector_field()` instead",
            DeprecationWarning,
        )
        return self.create_option_field_class_with_bool_selector_field(*args, **kwargs)

    @typing.overload
    def create_option_field_class_with_integer_selector_field(
        self,
        content_fc: bt2_field_class._FieldClass,
        selector_fc: bt2_field_class._UnsignedIntegerFieldClass,
        ranges: bt2_integer_range_set._UnsignedIntegerRangeSetConst,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
    ) -> bt2_field_class._OptionWithUnsignedIntegerSelectorFieldClass: ...

    @typing.overload
    def create_option_field_class_with_integer_selector_field(
        self,
        content_fc: bt2_field_class._FieldClass,
        selector_fc: bt2_field_class._SignedIntegerFieldClass,
        ranges: bt2_integer_range_set._SignedIntegerRangeSetConst,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
    ) -> bt2_field_class._OptionWithSignedIntegerSelectorFieldClass: ...

    def create_option_field_class_with_integer_selector_field(
        self,
        content_fc: bt2_field_class._FieldClass,
        selector_fc: bt2_field_class._IntegerFieldClass,
        ranges: bt2_integer_range_set._IntegerRangeSetConst,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
    ) -> bt2_field_class._OptionWithIntegerSelectorFieldClass:
        bt2_utils._check_mip_eq(
            self,
            "`_TraceClass.create_option_field_class_with_integer_selector_field()`",
            0,
        )
        bt2_utils._check_type(content_fc, bt2_field_class._FieldClass)
        bt2_utils._check_type(selector_fc, bt2_field_class._IntegerFieldClass)

        if len(ranges) == 0:
            raise ValueError("integer range set is empty")

        if isinstance(selector_fc, bt2_field_class._UnsignedIntegerFieldClass):
            bt2_utils._check_type(ranges, bt2_integer_range_set.UnsignedIntegerRangeSet)
            ptr = native_bt.field_class_option_with_selector_field_integer_unsigned_create(
                self._ptr, content_fc._ptr, selector_fc._ptr, ranges._ptr
            )
            expected_type = bt2_field_class._OptionWithUnsignedIntegerSelectorFieldClass
        else:
            bt2_utils._check_type(ranges, bt2_integer_range_set.SignedIntegerRangeSet)
            ptr = (
                native_bt.field_class_option_with_selector_field_integer_signed_create(
                    self._ptr, content_fc._ptr, selector_fc._ptr, ranges._ptr
                )
            )
            expected_type = bt2_field_class._OptionWithSignedIntegerSelectorFieldClass

        return self._check_and_wrap_field_class(
            ptr, "option", user_attributes, expected_type
        )

    def create_option_with_integer_selector_field_class(self, *args, **kwargs):
        warnings.warn(
            "`create_option_with_integer_selector_field_class()` is deprecated; "
            "use `create_option_field_class_with_integer_selector_field()` instead",
            DeprecationWarning,
        )
        return self.create_option_field_class_with_integer_selector_field(
            *args, **kwargs
        )

    def create_option_field_class_with_unsigned_integer_selector_field(
        self,
        content_fc: bt2_field_class._FieldClass,
        selector_field_location: bt2_field_location._FieldLocationConst,
        ranges: bt2_integer_range_set._UnsignedIntegerRangeSetConst,
        user_attributes: typing.Optional[bt2_value._MapValueConst] = None,
    ) -> bt2_field_class._OptionWithUnsignedIntegerSelectorFieldClass:
        bt2_utils._check_mip_ge(
            self,
            "`_TraceClass.create_option_field_class_with_unsigned_integer_selector_field()`",
            1,
        )
        bt2_utils._check_type(content_fc, bt2_field_class._FieldClass)
        bt2_utils._check_type(
            selector_field_location, bt2_field_location._FieldLocationConst
        )
        bt2_utils._check_type(ranges, bt2_integer_range_set.UnsignedIntegerRangeSet)

        if len(ranges) == 0:
            raise ValueError("integer range set is empty")

        return self._check_and_wrap_field_class(
            native_bt.field_class_option_with_selector_field_location_integer_unsigned_create(
                self._ptr, content_fc._ptr, selector_field_location._ptr, ranges._ptr
            ),
            "option",
            user_attributes,
            bt2_field_class._OptionWithUnsignedIntegerSelectorFieldClass,
        )

    def create_option_field_class_with_signed_integer_selector_field(
        self,
        content_fc: bt2_field_class._FieldClass,
        selector_field_location: bt2_field_location._FieldLocationConst,
        ranges: bt2_integer_range_set._SignedIntegerRangeSetConst,
        user_attributes: typing.Optional[bt2_value._MapValueConst] = None,
    ) -> bt2_field_class._OptionWithSignedIntegerSelectorFieldClass:
        bt2_utils._check_mip_ge(
            self,
            "`_TraceClass.create_option_field_class_with_signed_integer_selector_field()`",
            1,
        )
        bt2_utils._check_type(content_fc, bt2_field_class._FieldClass)
        bt2_utils._check_type(
            selector_field_location, bt2_field_location._FieldLocationConst
        )
        bt2_utils._check_type(ranges, bt2_integer_range_set.SignedIntegerRangeSet)

        if len(ranges) == 0:
            raise ValueError("integer range set is empty")

        return self._check_and_wrap_field_class(
            native_bt.field_class_option_with_selector_field_location_integer_signed_create(
                self._ptr, content_fc._ptr, selector_field_location._ptr, ranges._ptr
            ),
            "option",
            user_attributes,
            bt2_field_class._OptionWithSignedIntegerSelectorFieldClass,
        )

    @typing.overload
    def create_variant_field_class(
        self,
        selector_fc: None = None,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
        options: typing.Optional[
            typing.Iterable[
                typing.Union[
                    typing.Tuple[str, bt2_field_class._FieldClass],
                    typing.Tuple[
                        str,
                        bt2_field_class._FieldClass,
                        typing.Optional[bt2_value._MapValueConst],
                    ],
                ]
            ]
        ] = None,
    ) -> bt2_field_class._VariantFieldClassWithoutSelector: ...

    @typing.overload
    def create_variant_field_class(
        self,
        selector_fc: bt2_field_class._UnsignedIntegerFieldClass,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
        options: typing.Optional[
            typing.Iterable[
                typing.Union[
                    typing.Tuple[
                        str,
                        bt2_field_class._FieldClass,
                        bt2_integer_range_set._UnsignedIntegerRangeSetConst,
                    ],
                    typing.Tuple[
                        str,
                        bt2_field_class._FieldClass,
                        bt2_integer_range_set._UnsignedIntegerRangeSetConst,
                        typing.Optional[bt2_value._MapValueConst],
                    ],
                ]
            ]
        ] = None,
    ) -> bt2_field_class._VariantFieldClassWithUnsignedIntegerSelector: ...

    @typing.overload
    def create_variant_field_class(
        self,
        selector_fc: bt2_field_class._SignedIntegerFieldClass,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
        options: typing.Optional[
            typing.Iterable[
                typing.Union[
                    typing.Tuple[
                        str,
                        bt2_field_class._FieldClass,
                        bt2_integer_range_set._SignedIntegerRangeSetConst,
                    ],
                    typing.Tuple[
                        str,
                        bt2_field_class._FieldClass,
                        bt2_integer_range_set._SignedIntegerRangeSetConst,
                        typing.Optional[bt2_value._MapValueConst],
                    ],
                ]
            ]
        ] = None,
    ) -> bt2_field_class._VariantFieldClassWithSignedIntegerSelector: ...

    def create_variant_field_class(
        self,
        selector_fc: typing.Optional[bt2_field_class._IntegerFieldClass] = None,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
        options: typing.Optional[
            typing.Iterable[
                typing.Union[
                    typing.Tuple[str, bt2_field_class._FieldClass],
                    typing.Tuple[
                        str,
                        bt2_field_class._FieldClass,
                        typing.Optional[bt2_value._MapValueConst],
                    ],
                    typing.Tuple[
                        str,
                        bt2_field_class._FieldClass,
                        bt2_integer_range_set._IntegerRangeSetConst,
                    ],
                    typing.Tuple[
                        str,
                        bt2_field_class._FieldClass,
                        bt2_integer_range_set._IntegerRangeSetConst,
                        typing.Optional[bt2_value._MapValueConst],
                    ],
                ]
            ]
        ] = None,
    ) -> bt2_field_class._VariantFieldClass:
        bt2_utils._check_mip_eq(self, "`_TraceClass.create_variant_field_class()`", 0)

        if selector_fc is not None:
            bt2_utils._check_type(selector_fc, bt2_field_class._IntegerFieldClass)
            selector_fc_ptr = selector_fc._ptr
            expected_type = (
                bt2_field_class._VariantFieldClassWithUnsignedIntegerSelector
                if isinstance(selector_fc, bt2_field_class._UnsignedIntegerFieldClass)
                else bt2_field_class._VariantFieldClassWithSignedIntegerSelector
            )
        else:
            selector_fc_ptr = None
            expected_type = bt2_field_class._VariantFieldClassWithoutSelector

        fc = self._check_and_wrap_field_class(
            native_bt.field_class_variant_create(self._ptr, selector_fc_ptr),
            "variant",
            user_attributes,
            expected_type,
        )

        if options:
            # Rely on the type checks inside `__iadd__()`
            fc += options  # type: ignore

        return fc

    def create_variant_field_class_without_selector_field(
        self,
        user_attributes: typing.Optional[bt2_value._MapValueConst] = None,
        options: typing.Optional[
            typing.Iterable[
                typing.Union[
                    typing.Tuple[typing.Optional[str], bt2_field_class._FieldClass],
                    typing.Tuple[
                        typing.Optional[str],
                        bt2_field_class._FieldClass,
                        typing.Optional[bt2_value._MapValueConst],
                    ],
                ]
            ]
        ] = None,
    ) -> bt2_field_class._VariantFieldClassWithoutSelector:
        bt2_utils._check_mip_ge(self, "Variant without selector field location", 1)

        fc = self._check_and_wrap_field_class(
            native_bt.field_class_variant_without_selector_field_location_create(
                self._ptr
            ),
            "variant",
            user_attributes,
            bt2_field_class._VariantFieldClassWithoutSelector,
        )

        if options:
            fc += options

        return fc

    def _create_variant_field_class_with_integer_selector_field(
        self,
        create_func,
        selector_field_location: bt2_field_location._FieldLocationConst,
        user_attributes: typing.Optional[bt2_value._MapValueConst],
        options: typing.Optional[
            typing.Iterable[
                typing.Union[
                    typing.Tuple[
                        typing.Optional[str],
                        bt2_field_class._FieldClass,
                        bt2_integer_range_set._IntegerRangeSetConst,
                    ],
                    typing.Tuple[
                        typing.Optional[str],
                        bt2_field_class._FieldClass,
                        bt2_integer_range_set._IntegerRangeSetConst,
                        typing.Optional[bt2_value._MapValueConst],
                    ],
                ]
            ]
        ],
        expected_type: typing.Type[_FieldClassT],
    ):
        bt2_utils._check_mip_ge(self, "Variant with selector field location", 1)
        bt2_utils._check_type(
            selector_field_location, bt2_field_location._FieldLocationConst
        )

        fc = self._check_and_wrap_field_class(
            create_func(self._ptr, selector_field_location._ptr),
            "variant",
            user_attributes,
            expected_type,
        )

        if options:
            # Rely on the type checks inside `__iadd__()`
            fc += options  # type: ignore

        return fc

    def create_variant_field_class_with_unsigned_integer_selector_field(
        self,
        selector_field_location: bt2_field_location._FieldLocationConst,
        user_attributes: typing.Optional[bt2_value._MapValueConst] = None,
        options: typing.Optional[
            typing.Iterable[
                typing.Union[
                    typing.Tuple[
                        typing.Optional[str],
                        bt2_field_class._FieldClass,
                        bt2_integer_range_set._UnsignedIntegerRangeSetConst,
                    ],
                    typing.Tuple[
                        typing.Optional[str],
                        bt2_field_class._FieldClass,
                        bt2_integer_range_set._UnsignedIntegerRangeSetConst,
                        typing.Optional[bt2_value._MapValueConst],
                    ],
                ]
            ]
        ] = None,
    ) -> bt2_field_class._VariantFieldClassWithUnsignedIntegerSelector:
        return self._create_variant_field_class_with_integer_selector_field(
            native_bt.field_class_variant_with_selector_field_location_integer_unsigned_create,
            selector_field_location,
            user_attributes,
            options,
            bt2_field_class._VariantFieldClassWithUnsignedIntegerSelector,
        )

    def create_variant_field_class_with_signed_integer_selector_field(
        self,
        selector_field_location: bt2_field_location._FieldLocationConst,
        user_attributes: typing.Optional[bt2_value._MapValueConst] = None,
        options: typing.Optional[
            typing.Iterable[
                typing.Union[
                    typing.Tuple[
                        typing.Optional[str],
                        bt2_field_class._FieldClass,
                        bt2_integer_range_set._SignedIntegerRangeSetConst,
                    ],
                    typing.Tuple[
                        typing.Optional[str],
                        bt2_field_class._FieldClass,
                        bt2_integer_range_set._SignedIntegerRangeSetConst,
                        typing.Optional[bt2_value._MapValueConst],
                    ],
                ]
            ]
        ] = None,
    ) -> bt2_field_class._VariantFieldClassWithSignedIntegerSelector:
        return self._create_variant_field_class_with_integer_selector_field(
            native_bt.field_class_variant_with_selector_field_location_integer_signed_create,
            selector_field_location,
            user_attributes,
            options,
            bt2_field_class._VariantFieldClassWithSignedIntegerSelector,
        )

    def create_static_blob_field_class(
        self,
        length: int,
        media_type: typing.Optional[str] = None,
        user_attributes: typing.Optional[bt2_value._MapValueConst] = None,
    ) -> bt2_field_class._StaticBlobFieldClass:
        bt2_utils._check_mip_ge(self, "Static BLOB field class", 1)
        bt2_utils._check_uint64(length)

        fc = self._check_and_wrap_field_class(
            native_bt.field_class_blob_static_create(self._ptr, length),
            "static BLOB",
            user_attributes,
            bt2_field_class._StaticBlobFieldClass,
        )

        if media_type is not None:
            fc._set_media_type(media_type)

        return fc

    def create_dynamic_blob_field_class(
        self,
        length_field_location: typing.Optional[
            bt2_field_location._FieldLocationConst
        ] = None,
        media_type: typing.Optional[str] = None,
        user_attributes: typing.Optional[bt2_value._MapValueConst] = None,
    ) -> bt2_field_class._DynamicBlobFieldClass:
        bt2_utils._check_mip_ge(self, "Dynamic BLOB field class", 1)

        if length_field_location is None:
            ptr = (
                native_bt.field_class_blob_dynamic_without_length_field_location_create(
                    self._ptr
                )
            )
            expected_type = bt2_field_class._DynamicBlobFieldClass
        else:
            bt2_utils._check_type(
                length_field_location, bt2_field_location._FieldLocationConst
            )
            ptr = native_bt.field_class_blob_dynamic_with_length_field_location_create(
                self._ptr, length_field_location._ptr
            )
            expected_type = bt2_field_class._DynamicBlobFieldClassWithLengthField

        fc = self._check_and_wrap_field_class(
            ptr, "dynamic BLOB", user_attributes, expected_type
        )

        if media_type is not None:
            fc._set_media_type(media_type)

        return fc
