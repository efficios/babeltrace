# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

import uuid as uuidp
import typing

from bt2 import utils as bt2_utils
from bt2 import object as bt2_object
from bt2 import native_bt
from bt2 import user_attributes as bt2_user_attrs


class ClockOffset:
    def __init__(self, seconds: int = 0, cycles: int = 0):
        bt2_utils._check_int64(seconds)
        bt2_utils._check_uint64(cycles)
        self._seconds = seconds
        self._cycles = cycles

    @property
    def seconds(self) -> int:
        return self._seconds

    @property
    def cycles(self) -> int:
        return self._cycles

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, self.__class__):
            # not comparing apples to apples
            return False

        return (self.seconds, self.cycles) == (other.seconds, other.cycles)


ClockClassOffset = ClockOffset


class ClockOrigin:
    def __init__(
        self,
        namespace: typing.Optional[str],
        name: str,
        uid: str,
    ):
        if namespace is not None:
            bt2_utils._check_str(namespace)

        bt2_utils._check_str(name)
        bt2_utils._check_str(uid)

        self._namespace = namespace
        self._name = name
        self._uid = uid

    @property
    def namespace(self) -> typing.Optional[str]:
        return self._namespace

    @property
    def name(self) -> str:
        return self._name

    @property
    def uid(self) -> str:
        return self._uid


class _UnixEpochClockOrigin:
    pass


class _UnknownClockOrigin:
    pass


unix_epoch_clock_origin = _UnixEpochClockOrigin()
unknown_clock_origin = _UnknownClockOrigin()


class _ClockClassConst(bt2_object._SharedObject, bt2_user_attrs._WithUserAttrsConst):
    @staticmethod
    def _get_ref(ptr):
        native_bt.clock_class_get_ref(ptr)

    @staticmethod
    def _put_ref(ptr):
        native_bt.clock_class_put_ref(ptr)

    @staticmethod
    def _borrow_user_attributes_ptr(ptr):
        return native_bt.clock_class_borrow_user_attributes_const(ptr)

    @property
    def namespace(self) -> typing.Optional[str]:
        bt2_utils._check_mip_ge(self, "Clock class namespace", 1)
        return native_bt.clock_class_get_namespace(self._ptr)

    @property
    def name(self) -> typing.Optional[str]:
        return native_bt.clock_class_get_name(self._ptr)

    @property
    def uid(self) -> typing.Optional[str]:
        bt2_utils._check_mip_ge(self, "Clock class UID", 1)
        return native_bt.clock_class_get_uid(self._ptr)

    @property
    def description(self) -> typing.Optional[str]:
        return native_bt.clock_class_get_description(self._ptr)

    @property
    def frequency(self) -> int:
        return native_bt.clock_class_get_frequency(self._ptr)

    @property
    def precision(self) -> typing.Optional[int]:
        return native_bt.clock_class_get_opt_precision(self._ptr)

    @property
    def accuracy(self) -> typing.Optional[int]:
        bt2_utils._check_mip_ge(self, "Clock class accuracy", 1)
        return native_bt.clock_class_get_accuracy(self._ptr)

    @property
    def offset(self) -> ClockOffset:
        offset_s, offset_cycles = native_bt.clock_class_get_offset(self._ptr)
        return ClockOffset(offset_s, offset_cycles)

    @property
    def origin_is_unix_epoch(self) -> bool:
        return native_bt.clock_class_origin_is_unix_epoch(self._ptr)

    @property
    def origin(
        self,
    ) -> typing.Union[_UnixEpochClockOrigin, _UnknownClockOrigin, ClockOrigin]:
        if native_bt.clock_class_origin_is_unix_epoch(self._ptr):
            return unix_epoch_clock_origin

        if not native_bt.clock_class_origin_is_known(self._ptr):
            return unknown_clock_origin

        return ClockOrigin(
            native_bt.clock_class_get_origin_namespace(self._ptr),
            native_bt.clock_class_get_origin_name(self._ptr),
            native_bt.clock_class_get_origin_uid(self._ptr),
        )

    @property
    def uuid(self) -> typing.Optional[uuidp.UUID]:
        bt2_utils._check_mip_eq(self, "Clock class UUID", 0)
        uuid_bytes = native_bt.clock_class_get_uuid(self._ptr)

        if uuid_bytes is None:
            return

        return uuidp.UUID(bytes=uuid_bytes)

    @property
    def graph_mip_version(self) -> int:
        return native_bt.clock_class_get_graph_mip_version(self._ptr)

    def cycles_to_ns_from_origin(self, cycles: int) -> int:
        bt2_utils._check_uint64(cycles)
        return native_bt.clock_class_cycles_to_ns_from_origin(self._ptr, cycles)


class _ClockClass(bt2_user_attrs._WithUserAttrs, _ClockClassConst):
    @staticmethod
    def _borrow_user_attributes_ptr(ptr):
        return native_bt.clock_class_borrow_user_attributes(ptr)

    @staticmethod
    def _set_user_attributes_ptr(obj_ptr, value_ptr):
        native_bt.clock_class_set_user_attributes(obj_ptr, value_ptr)

    def _set_namespace(self, namespace: str):
        bt2_utils._check_mip_ge(self, "Clock class namespace", 1)
        bt2_utils._check_str(namespace)
        bt2_utils._handle_func_status(
            native_bt.clock_class_set_namespace(self._ptr, namespace),
            "cannot set clock class object's namespace",
        )

    def _set_name(self, name):
        bt2_utils._check_str(name)
        bt2_utils._handle_func_status(
            native_bt.clock_class_set_name(self._ptr, name),
            "cannot set clock class object's name",
        )

    def _set_uid(self, uid: str):
        bt2_utils._check_mip_ge(self, "Clock class UID", 1)
        bt2_utils._check_str(uid)
        bt2_utils._handle_func_status(
            native_bt.clock_class_set_uid(self._ptr, uid),
            "cannot set clock class object's UID",
        )

    def _set_description(self, description):
        bt2_utils._check_str(description)
        bt2_utils._handle_func_status(
            native_bt.clock_class_set_description(self._ptr, description),
            "cannot set clock class object's description",
        )

    def _set_frequency(self, frequency):
        bt2_utils._check_uint64(frequency)
        native_bt.clock_class_set_frequency(self._ptr, frequency)

    def _set_precision(self, precision):
        bt2_utils._check_uint64(precision)
        native_bt.clock_class_set_precision(self._ptr, precision)

    def _set_accuracy(self, accuracy):
        bt2_utils._check_mip_ge(self, "Clock class accuracy", 1)
        bt2_utils._check_uint64(accuracy)
        native_bt.clock_class_set_accuracy(self._ptr, accuracy)

    def _set_offset(self, offset):
        bt2_utils._check_type(offset, ClockOffset)
        native_bt.clock_class_set_offset(self._ptr, offset.seconds, offset.cycles)

    def _set_origin(
        self,
        origin: typing.Union[_UnixEpochClockOrigin, _UnknownClockOrigin, ClockOrigin],
    ):
        if origin is unix_epoch_clock_origin:
            return native_bt.clock_class_set_origin_unix_epoch(self._ptr)

        if origin is unknown_clock_origin:
            return native_bt.clock_class_set_origin_unknown(self._ptr)

        origin = bt2_utils._check_type(origin, ClockOrigin)
        bt2_utils._check_mip_ge(self, "Custom clock class origin", 1)
        native_bt.clock_class_set_origin(
            self._ptr, origin.namespace, origin.name, origin.uid
        )

    def _set_uuid(self, uuid):
        bt2_utils._check_mip_eq(self, "Clock class UUID", 0)
        bt2_utils._check_type(uuid, uuidp.UUID)
        native_bt.clock_class_set_uuid(self._ptr, uuid.bytes)
