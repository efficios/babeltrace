# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

import enum
import typing

from bt2 import utils as bt2_utils
from bt2 import value as bt2_value
from bt2 import object as bt2_object
from bt2 import native_bt
from bt2 import field_class as bt2_field_class
from bt2 import user_attributes as bt2_user_attrs

if typing.TYPE_CHECKING:
    from bt2 import stream_class as bt2_stream_class


def _bt2_stream_class():
    from bt2 import stream_class as bt2_stream_class

    return bt2_stream_class


class EventClassLogLevel(enum.Enum):
    EMERGENCY = native_bt.EVENT_CLASS_LOG_LEVEL_EMERGENCY
    ALERT = native_bt.EVENT_CLASS_LOG_LEVEL_ALERT
    CRITICAL = native_bt.EVENT_CLASS_LOG_LEVEL_CRITICAL
    ERROR = native_bt.EVENT_CLASS_LOG_LEVEL_ERROR
    WARNING = native_bt.EVENT_CLASS_LOG_LEVEL_WARNING
    NOTICE = native_bt.EVENT_CLASS_LOG_LEVEL_NOTICE
    INFO = native_bt.EVENT_CLASS_LOG_LEVEL_INFO
    DEBUG_SYSTEM = native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM
    DEBUG_PROGRAM = native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM
    DEBUG_PROCESS = native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS
    DEBUG_MODULE = native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE
    DEBUG_UNIT = native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT
    DEBUG_FUNCTION = native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION
    DEBUG_LINE = native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_LINE
    DEBUG = native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG


class _EventClassConst(bt2_object._SharedObject, bt2_user_attrs._WithUserAttrsConst):
    @staticmethod
    def _get_ref(ptr):
        native_bt.event_class_get_ref(ptr)

    @staticmethod
    def _put_ref(ptr):
        native_bt.event_class_put_ref(ptr)

    _borrow_stream_class_ptr = staticmethod(
        native_bt.event_class_borrow_stream_class_const
    )
    _borrow_specific_context_field_class_ptr = staticmethod(
        native_bt.event_class_borrow_specific_context_field_class_const
    )
    _borrow_payload_field_class_ptr = staticmethod(
        native_bt.event_class_borrow_payload_field_class_const
    )
    _borrow_user_attributes_ptr = staticmethod(
        native_bt.event_class_borrow_user_attributes_const
    )
    _create_field_class_from_ptr_and_get_ref = staticmethod(
        bt2_field_class._create_field_class_from_const_ptr_and_get_ref
    )
    _stream_class_pycls = property(lambda s: _bt2_stream_class()._StreamClassConst)

    @property
    def graph_mip_version(self) -> int:
        return self.stream_class.graph_mip_version

    @property
    def _stream_class(self):
        return self._stream_class_pycls._create_from_ptr_and_get_ref(
            self._borrow_stream_class_ptr(self._ptr)
        )

    @property
    def stream_class(self) -> "bt2_stream_class._StreamClassConst":
        return self._stream_class

    @property
    def namespace(self) -> typing.Optional[str]:
        bt2_utils._check_mip_ge(self, "Event class namespace", 1)
        return native_bt.event_class_get_namespace(self._ptr)

    @property
    def name(self) -> typing.Optional[str]:
        return native_bt.event_class_get_name(self._ptr)

    @property
    def uid(self) -> typing.Optional[str]:
        bt2_utils._check_mip_ge(self, "Event class UID", 1)
        return native_bt.event_class_get_uid(self._ptr)

    @property
    def id(self) -> int:
        return native_bt.event_class_get_id(self._ptr)

    @property
    def log_level(self) -> typing.Optional[EventClassLogLevel]:
        log_level = native_bt.event_class_get_log_level(self._ptr)

        if log_level is None:
            return None

        return EventClassLogLevel(log_level)

    @property
    def emf_uri(self) -> typing.Optional[str]:
        return native_bt.event_class_get_emf_uri(self._ptr)

    @property
    def specific_context_field_class(
        self,
    ) -> typing.Optional[bt2_field_class._StructureFieldClassConst]:
        fc_ptr = self._borrow_specific_context_field_class_ptr(self._ptr)

        if fc_ptr is None:
            return

        return self._create_field_class_from_ptr_and_get_ref(fc_ptr)

    @property
    def payload_field_class(
        self,
    ) -> typing.Optional[bt2_field_class._StructureFieldClassConst]:
        fc_ptr = self._borrow_payload_field_class_ptr(self._ptr)

        if fc_ptr is None:
            return

        return self._create_field_class_from_ptr_and_get_ref(fc_ptr)


class _EventClass(bt2_user_attrs._WithUserAttrs, _EventClassConst):
    _borrow_stream_class_ptr = staticmethod(native_bt.event_class_borrow_stream_class)
    _borrow_specific_context_field_class_ptr = staticmethod(
        native_bt.event_class_borrow_specific_context_field_class
    )
    _borrow_payload_field_class_ptr = staticmethod(
        native_bt.event_class_borrow_payload_field_class
    )
    _borrow_user_attributes_ptr = staticmethod(
        native_bt.event_class_borrow_user_attributes
    )
    _create_field_class_from_ptr_and_get_ref = staticmethod(
        bt2_field_class._create_field_class_from_ptr_and_get_ref
    )
    _stream_class_pycls = property(lambda s: _bt2_stream_class()._StreamClass)

    @staticmethod
    def _set_user_attributes_ptr(obj_ptr, value_ptr):
        native_bt.event_class_set_user_attributes(obj_ptr, value_ptr)

    @property
    def stream_class(self) -> "bt2_stream_class._StreamClass":
        return self._stream_class

    def _set_namespace(self, namespace: str):
        bt2_utils._check_str(namespace)
        bt2_utils._handle_func_status(
            native_bt.event_class_set_namespace(self._ptr, namespace),
            "cannot set event class object's namespace",
        )

    def _set_name(self, name: str):
        bt2_utils._check_str(name)
        bt2_utils._handle_func_status(
            native_bt.event_class_set_name(self._ptr, name),
            "cannot set event class object's name",
        )

    def _set_uid(self, uid: str):
        bt2_utils._check_mip_ge(self, "Event class UID", 1)
        bt2_utils._check_str(uid)
        bt2_utils._handle_func_status(
            native_bt.event_class_set_uid(self._ptr, uid),
            "cannot set event class object's UID",
        )

    def _set_log_level(self, log_level):
        bt2_utils._check_type(log_level, EventClassLogLevel)
        native_bt.event_class_set_log_level(self._ptr, log_level.value)

    def _set_emf_uri(self, emf_uri):
        bt2_utils._handle_func_status(
            native_bt.event_class_set_emf_uri(self._ptr, emf_uri),
            "cannot set event class object's EMF URI",
        )

    def _set_specific_context_field_class(self, context_field_class):
        bt2_utils._handle_func_status(
            native_bt.event_class_set_specific_context_field_class(
                self._ptr, context_field_class._ptr
            ),
            "cannot set event class object's context field class",
        )

    def _set_payload_field_class(self, payload_field_class):
        bt2_utils._handle_func_status(
            native_bt.event_class_set_payload_field_class(
                self._ptr, payload_field_class._ptr
            ),
            "cannot set event class object's payload field class",
        )

    @staticmethod
    def _validate_create_params(
        name,
        user_attributes,
        log_level,
        emf_uri,
        specific_context_field_class,
        payload_field_class,
    ):
        if name is not None:
            bt2_utils._check_str(name)

        if user_attributes is not None:
            value = bt2_value.create_value(user_attributes)
            bt2_utils._check_type(value, bt2_value.MapValue)

        if log_level is not None:
            log_levels = (
                EventClassLogLevel.EMERGENCY,
                EventClassLogLevel.ALERT,
                EventClassLogLevel.CRITICAL,
                EventClassLogLevel.ERROR,
                EventClassLogLevel.WARNING,
                EventClassLogLevel.NOTICE,
                EventClassLogLevel.INFO,
                EventClassLogLevel.DEBUG_SYSTEM,
                EventClassLogLevel.DEBUG_PROGRAM,
                EventClassLogLevel.DEBUG_PROCESS,
                EventClassLogLevel.DEBUG_MODULE,
                EventClassLogLevel.DEBUG_UNIT,
                EventClassLogLevel.DEBUG_FUNCTION,
                EventClassLogLevel.DEBUG_LINE,
                EventClassLogLevel.DEBUG,
            )

            if log_level not in log_levels:
                raise ValueError(f"'{log_level}' is not a valid log level")

        if emf_uri is not None:
            bt2_utils._check_str(emf_uri)

        if specific_context_field_class is not None:
            bt2_utils._check_type(
                specific_context_field_class, bt2_field_class._StructureFieldClass
            )

        if payload_field_class is not None:
            bt2_utils._check_type(
                payload_field_class, bt2_field_class._StructureFieldClass
            )
