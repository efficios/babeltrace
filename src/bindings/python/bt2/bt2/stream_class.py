# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

import typing

from bt2 import utils as bt2_utils
from bt2 import value as bt2_value
from bt2 import object as bt2_object
from bt2 import native_bt
from bt2 import clock_class as bt2_clock_class
from bt2 import event_class as bt2_event_class
from bt2 import field_class as bt2_field_class
from bt2 import user_attributes as bt2_user_attrs

if typing.TYPE_CHECKING:
    from bt2 import trace_class as bt2_trace_class


def _bt2_trace_class():
    from bt2 import trace_class as bt2_trace_class

    return bt2_trace_class


class _StreamClassConst(
    bt2_object._SharedObject,
    bt2_user_attrs._WithUserAttrsConst,
    typing.Mapping[int, bt2_event_class._EventClassConst],
):
    @staticmethod
    def _get_ref(ptr):
        native_bt.stream_class_get_ref(ptr)

    @staticmethod
    def _put_ref(ptr):
        native_bt.stream_class_put_ref(ptr)

    _borrow_event_class_ptr_by_id = staticmethod(
        native_bt.stream_class_borrow_event_class_by_id_const
    )
    _borrow_event_class_ptr_by_index = staticmethod(
        native_bt.stream_class_borrow_event_class_by_index_const
    )
    _borrow_trace_class_ptr = staticmethod(
        native_bt.stream_class_borrow_trace_class_const
    )
    _borrow_packet_context_field_class_ptr = staticmethod(
        native_bt.stream_class_borrow_packet_context_field_class_const
    )
    _borrow_event_common_context_field_class_ptr = staticmethod(
        native_bt.stream_class_borrow_event_common_context_field_class_const
    )
    _borrow_default_clock_class_ptr = staticmethod(
        native_bt.stream_class_borrow_default_clock_class_const
    )

    @property
    def graph_mip_version(self) -> int:
        return self.trace_class.graph_mip_version

    @staticmethod
    def _borrow_user_attributes_ptr(ptr):
        return native_bt.stream_class_borrow_user_attributes_const(ptr)

    _event_class_cls = property(lambda _: bt2_event_class._EventClassConst)
    _trace_class_cls = property(lambda _: _bt2_trace_class()._TraceClassConst)
    _clock_class_cls = property(lambda _: bt2_clock_class._ClockClassConst)

    def __getitem__(self, key: int) -> bt2_event_class._EventClassConst:
        bt2_utils._check_uint64(key)
        ec_ptr = self._borrow_event_class_ptr_by_id(self._ptr, key)

        if ec_ptr is None:
            raise KeyError(key)

        return self._event_class_cls._create_from_ptr_and_get_ref(ec_ptr)

    def __len__(self) -> int:
        return native_bt.stream_class_get_event_class_count(self._ptr)

    def __iter__(self) -> typing.Iterator[int]:
        for idx in range(len(self)):
            yield native_bt.event_class_get_id(
                self._borrow_event_class_ptr_by_index(self._ptr, idx)
            )

    @property
    def _trace_class(self):
        return self._trace_class_cls._create_from_ptr_and_get_ref(
            self._borrow_trace_class_ptr(self._ptr)
        )

    @property
    def trace_class(self) -> "bt2_trace_class._TraceClassConst":
        return self._trace_class

    @property
    def namespace(self) -> typing.Optional[str]:
        bt2_utils._check_mip_ge(self, "Stream class namespace", 1)
        return native_bt.stream_class_get_namespace(self._ptr)

    @property
    def name(self) -> typing.Optional[str]:
        return native_bt.stream_class_get_name(self._ptr)

    @property
    def uid(self) -> typing.Optional[str]:
        bt2_utils._check_mip_ge(self, "Stream class UID", 1)
        return native_bt.stream_class_get_uid(self._ptr)

    @property
    def assigns_automatic_event_class_id(self) -> bool:
        return native_bt.stream_class_assigns_automatic_event_class_id(self._ptr)

    @property
    def assigns_automatic_stream_id(self) -> bool:
        return native_bt.stream_class_assigns_automatic_stream_id(self._ptr)

    @property
    def supports_packets(self) -> bool:
        return native_bt.stream_class_supports_packets(self._ptr)

    @property
    def packets_have_beginning_default_clock_snapshot(self) -> bool:
        return native_bt.stream_class_packets_have_beginning_default_clock_snapshot(
            self._ptr
        )

    @property
    def packets_have_end_default_clock_snapshot(self) -> bool:
        return native_bt.stream_class_packets_have_end_default_clock_snapshot(self._ptr)

    @property
    def supports_discarded_events(self) -> bool:
        return native_bt.stream_class_supports_discarded_events(self._ptr)

    @property
    def discarded_events_have_default_clock_snapshots(self) -> bool:
        return native_bt.stream_class_discarded_events_have_default_clock_snapshots(
            self._ptr
        )

    @property
    def supports_discarded_packets(self) -> bool:
        return native_bt.stream_class_supports_discarded_packets(self._ptr)

    @property
    def discarded_packets_have_default_clock_snapshots(self) -> bool:
        return native_bt.stream_class_discarded_packets_have_default_clock_snapshots(
            self._ptr
        )

    @property
    def id(self) -> int:
        return native_bt.stream_class_get_id(self._ptr)

    @property
    def packet_context_field_class(
        self,
    ) -> typing.Optional[bt2_field_class._StructureFieldClassConst]:
        fc_ptr = self._borrow_packet_context_field_class_ptr(self._ptr)

        if fc_ptr is None:
            return

        return bt2_field_class._create_field_class_from_ptr_and_get_ref(fc_ptr)

    @property
    def event_common_context_field_class(
        self,
    ) -> typing.Optional[bt2_field_class._StructureFieldClassConst]:
        fc_ptr = self._borrow_event_common_context_field_class_ptr(self._ptr)

        if fc_ptr is None:
            return

        return bt2_field_class._create_field_class_from_ptr_and_get_ref(fc_ptr)

    @property
    def default_clock_class(self) -> typing.Optional[bt2_clock_class._ClockClassConst]:
        cc_ptr = self._borrow_default_clock_class_ptr(self._ptr)
        if cc_ptr is None:
            return

        return self._clock_class_cls._create_from_ptr_and_get_ref(cc_ptr)


class _StreamClass(bt2_user_attrs._WithUserAttrs, _StreamClassConst):
    @staticmethod
    def _get_ref(ptr):
        native_bt.stream_class_get_ref(ptr)

    @staticmethod
    def _put_ref(ptr):
        native_bt.stream_class_put_ref(ptr)

    _borrow_event_class_ptr_by_id = staticmethod(
        native_bt.stream_class_borrow_event_class_by_id
    )
    _borrow_event_class_ptr_by_index = staticmethod(
        native_bt.stream_class_borrow_event_class_by_index
    )
    _borrow_trace_class_ptr = staticmethod(native_bt.stream_class_borrow_trace_class)
    _borrow_packet_context_field_class_ptr = staticmethod(
        native_bt.stream_class_borrow_packet_context_field_class
    )
    _borrow_event_common_context_field_class_ptr = staticmethod(
        native_bt.stream_class_borrow_event_common_context_field_class
    )
    _borrow_default_clock_class_ptr = staticmethod(
        native_bt.stream_class_borrow_default_clock_class
    )

    @staticmethod
    def _borrow_user_attributes_ptr(ptr):
        return native_bt.stream_class_borrow_user_attributes(ptr)

    _event_class_cls = property(lambda s: bt2_event_class._EventClass)
    _trace_class_cls = property(lambda s: _bt2_trace_class()._TraceClass)
    _clock_class_cls = property(lambda s: bt2_clock_class._ClockClass)

    @property
    def trace_class(self) -> "bt2_trace_class._TraceClass":
        return self._trace_class

    def create_event_class(
        self,
        id: typing.Optional[int] = None,
        name: typing.Optional[str] = None,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
        log_level: typing.Optional[bt2_event_class.EventClassLogLevel] = None,
        emf_uri: typing.Optional[str] = None,
        specific_context_field_class: typing.Optional[
            bt2_field_class._StructureFieldClass
        ] = None,
        payload_field_class: typing.Optional[
            bt2_field_class._StructureFieldClass
        ] = None,
        namespace: typing.Optional[str] = None,
        uid: typing.Optional[str] = None,
    ) -> bt2_event_class._EventClass:
        # Validate parameters before we create the object.
        bt2_event_class._EventClass._validate_create_params(
            name,
            user_attributes,
            log_level,
            emf_uri,
            specific_context_field_class,
            payload_field_class,
        )

        if self.assigns_automatic_event_class_id:
            if id is not None:
                raise ValueError(
                    "id provided, but stream class assigns automatic event class ids"
                )

            ec_ptr = native_bt.event_class_create(self._ptr)
        else:
            if id is None:
                raise ValueError(
                    "id not provided, but stream class does not assign automatic event class ids"
                )

            bt2_utils._check_uint64(id)
            ec_ptr = native_bt.event_class_create_with_id(self._ptr, id)

        event_class = bt2_event_class._EventClass._create_from_ptr(ec_ptr)

        if namespace is not None:
            event_class._set_namespace(namespace)

        if name is not None:
            event_class._set_name(name)

        if uid is not None:
            event_class._set_uid(uid)

        if user_attributes is not None:
            event_class._set_user_attributes(user_attributes)

        if log_level is not None:
            event_class._set_log_level(log_level)

        if emf_uri is not None:
            event_class._set_emf_uri(emf_uri)

        if specific_context_field_class is not None:
            event_class._set_specific_context_field_class(specific_context_field_class)

        if payload_field_class is not None:
            event_class._set_payload_field_class(payload_field_class)

        return event_class

    @staticmethod
    def _set_user_attributes_ptr(obj_ptr, value_ptr):
        native_bt.stream_class_set_user_attributes(obj_ptr, value_ptr)

    def _set_namespace(self, namespace: str):
        bt2_utils._check_mip_ge(self, "Stream class namespace", 1)
        bt2_utils._check_str(namespace)
        bt2_utils._handle_func_status(
            native_bt.stream_class_set_namespace(self._ptr, namespace),
            "cannot set stream class object's namespace",
        )

    def _set_name(self, name):
        bt2_utils._handle_func_status(
            native_bt.stream_class_set_name(self._ptr, name),
            "cannot set stream class object's name",
        )

    def _set_uid(self, uid: str):
        bt2_utils._check_mip_ge(self, "Stream class UID", 1)
        bt2_utils._check_str(uid)
        bt2_utils._handle_func_status(
            native_bt.stream_class_set_uid(self._ptr, uid),
            "cannot set stream class object's UID",
        )

    def _set_assigns_automatic_event_class_id(self, auto_id):
        native_bt.stream_class_set_assigns_automatic_event_class_id(self._ptr, auto_id)

    def _set_assigns_automatic_stream_id(self, auto_id):
        native_bt.stream_class_set_assigns_automatic_stream_id(self._ptr, auto_id)

    def _set_supports_packets(self, supports, with_begin_cs=False, with_end_cs=False):
        native_bt.stream_class_set_supports_packets(
            self._ptr, supports, with_begin_cs, with_end_cs
        )

    def _set_supports_discarded_events(self, supports, with_cs=False):
        native_bt.stream_class_set_supports_discarded_events(
            self._ptr, supports, with_cs
        )

    def _set_supports_discarded_packets(self, supports, with_cs):
        native_bt.stream_class_set_supports_discarded_packets(
            self._ptr, supports, with_cs
        )

    def _set_packet_context_field_class(self, packet_context_field_class):
        bt2_utils._handle_func_status(
            native_bt.stream_class_set_packet_context_field_class(
                self._ptr, packet_context_field_class._ptr
            ),
            "cannot set stream class' packet context field class",
        )

    def _set_event_common_context_field_class(self, event_common_context_field_class):
        bt2_utils._handle_func_status(
            native_bt.stream_class_set_event_common_context_field_class(
                self._ptr, event_common_context_field_class._ptr
            ),
            "cannot set stream class' event context field type",
        )

    def _set_default_clock_class(self, clock_class):
        native_bt.stream_class_set_default_clock_class(self._ptr, clock_class._ptr)

    @classmethod
    def _validate_create_params(
        cls,
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
    ):
        # Name
        if name is not None:
            bt2_utils._check_str(name)

        # User attributes
        if user_attributes is not None:
            bt2_utils._check_type(
                bt2_value.create_value(user_attributes), bt2_value.MapValue
            )

        # Packet context field class
        if packet_context_field_class is not None:
            if not supports_packets:
                raise ValueError(
                    "cannot have a packet context field class without supporting packets"
                )

            bt2_utils._check_type(
                packet_context_field_class, bt2_field_class._StructureFieldClass
            )

        # Event common context field class
        if event_common_context_field_class is not None:
            bt2_utils._check_type(
                event_common_context_field_class, bt2_field_class._StructureFieldClass
            )

        # Default clock class
        if default_clock_class is not None:
            bt2_utils._check_type(default_clock_class, bt2_clock_class._ClockClass)

        # Assigns automatic event class id
        bt2_utils._check_bool(assigns_automatic_event_class_id)

        # Assigns automatic stream id
        bt2_utils._check_bool(assigns_automatic_stream_id)

        # Packets
        bt2_utils._check_bool(supports_packets)
        bt2_utils._check_bool(packets_have_beginning_default_clock_snapshot)
        bt2_utils._check_bool(packets_have_end_default_clock_snapshot)

        if not supports_packets:
            if packets_have_beginning_default_clock_snapshot:
                raise ValueError(
                    "cannot not support packets, but have packet beginning default clock snapshot"
                )
            if packets_have_end_default_clock_snapshot:
                raise ValueError(
                    "cannot not support packets, but have packet end default clock snapshots"
                )

        # Discarded events
        bt2_utils._check_bool(supports_discarded_events)
        bt2_utils._check_bool(discarded_events_have_default_clock_snapshots)

        if discarded_events_have_default_clock_snapshots:
            if not supports_discarded_events:
                raise ValueError(
                    "cannot not support discarded events, but have default clock snapshots for discarded event messages"
                )

            if default_clock_class is None:
                raise ValueError(
                    "cannot have no default clock class, but have default clock snapshots for discarded event messages"
                )

        # Discarded packets
        bt2_utils._check_bool(supports_discarded_packets)
        bt2_utils._check_bool(discarded_packets_have_default_clock_snapshots)

        if supports_discarded_packets and not supports_packets:
            raise ValueError(
                "cannot support discarded packets, but not support packets"
            )

        if discarded_packets_have_default_clock_snapshots:
            if not supports_discarded_packets:
                raise ValueError(
                    "cannot not support discarded packets, but have default clock snapshots for discarded packet messages"
                )

            if default_clock_class is None:
                raise ValueError(
                    "cannot have no default clock class, but have default clock snapshots for discarded packet messages"
                )
