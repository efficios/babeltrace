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
from bt2 import field_class as bt2_field_class
from bt2 import value as bt2_value


def _bt2_stream_class():
    from bt2 import stream_class as bt2_stream_class

    return bt2_stream_class


class EventClassLogLevel:
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


class _EventClassConst(object._SharedObject):
    _get_ref = staticmethod(native_bt.event_class_get_ref)
    _put_ref = staticmethod(native_bt.event_class_put_ref)
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
    _create_value_from_ptr_and_get_ref = staticmethod(
        bt2_value._create_from_const_ptr_and_get_ref
    )
    _stream_class_pycls = property(lambda s: _bt2_stream_class()._StreamClassConst)

    @property
    def stream_class(self):
        sc_ptr = self._borrow_stream_class_ptr(self._ptr)

        if sc_ptr is not None:
            return self._stream_class_pycls._create_from_ptr_and_get_ref(sc_ptr)

    @property
    def user_attributes(self):
        ptr = self._borrow_user_attributes_ptr(self._ptr)
        assert ptr is not None
        return self._create_value_from_ptr_and_get_ref(ptr)

    @property
    def name(self):
        return native_bt.event_class_get_name(self._ptr)

    @property
    def id(self):
        id = native_bt.event_class_get_id(self._ptr)
        return id if id >= 0 else None

    @property
    def log_level(self):
        is_available, log_level = native_bt.event_class_get_log_level(self._ptr)

        if is_available != native_bt.PROPERTY_AVAILABILITY_AVAILABLE:
            return None

        return _EVENT_CLASS_LOG_LEVEL_TO_OBJ[log_level]

    @property
    def emf_uri(self):
        return native_bt.event_class_get_emf_uri(self._ptr)

    @property
    def specific_context_field_class(self):
        fc_ptr = self._borrow_specific_context_field_class_ptr(self._ptr)

        if fc_ptr is None:
            return

        return self._create_field_class_from_ptr_and_get_ref(fc_ptr)

    @property
    def payload_field_class(self):
        fc_ptr = self._borrow_payload_field_class_ptr(self._ptr)

        if fc_ptr is None:
            return

        return self._create_field_class_from_ptr_and_get_ref(fc_ptr)


class _EventClass(_EventClassConst):
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
    _create_value_from_ptr_and_get_ref = staticmethod(
        bt2_value._create_from_ptr_and_get_ref
    )
    _stream_class_pycls = property(lambda s: _bt2_stream_class()._StreamClass)

    def _user_attributes(self, user_attributes):
        value = bt2_value.create_value(user_attributes)
        native_bt.event_class_set_user_attributes(self._ptr, value._ptr)

    _user_attributes = property(fset=_user_attributes)

    def _name(self, name):
        return native_bt.event_class_set_name(self._ptr, name)

    _name = property(fset=_name)

    def _log_level(self, log_level):
        native_bt.event_class_set_log_level(self._ptr, log_level)

    _log_level = property(fset=_log_level)

    def _emf_uri(self, emf_uri):
        status = native_bt.event_class_set_emf_uri(self._ptr, emf_uri)
        utils._handle_func_status(status, "cannot set event class object's EMF URI")

    _emf_uri = property(fset=_emf_uri)

    def _specific_context_field_class(self, context_field_class):
        status = native_bt.event_class_set_specific_context_field_class(
            self._ptr, context_field_class._ptr
        )
        utils._handle_func_status(
            status, "cannot set event class object's context field class"
        )

    _specific_context_field_class = property(fset=_specific_context_field_class)

    def _payload_field_class(self, payload_field_class):
        status = native_bt.event_class_set_payload_field_class(
            self._ptr, payload_field_class._ptr
        )
        utils._handle_func_status(
            status, "cannot set event class object's payload field class"
        )

    _payload_field_class = property(fset=_payload_field_class)

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
            utils._check_str(name)

        if user_attributes is not None:
            value = bt2_value.create_value(user_attributes)
            utils._check_type(value, bt2_value.MapValue)

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
                raise ValueError("'{}' is not a valid log level".format(log_level))

        if emf_uri is not None:
            utils._check_str(emf_uri)

        if specific_context_field_class is not None:
            utils._check_type(
                specific_context_field_class, bt2_field_class._StructureFieldClass
            )

        if payload_field_class is not None:
            utils._check_type(payload_field_class, bt2_field_class._StructureFieldClass)


_EVENT_CLASS_LOG_LEVEL_TO_OBJ = {
    native_bt.EVENT_CLASS_LOG_LEVEL_EMERGENCY: EventClassLogLevel.EMERGENCY,
    native_bt.EVENT_CLASS_LOG_LEVEL_ALERT: EventClassLogLevel.ALERT,
    native_bt.EVENT_CLASS_LOG_LEVEL_CRITICAL: EventClassLogLevel.CRITICAL,
    native_bt.EVENT_CLASS_LOG_LEVEL_ERROR: EventClassLogLevel.ERROR,
    native_bt.EVENT_CLASS_LOG_LEVEL_WARNING: EventClassLogLevel.WARNING,
    native_bt.EVENT_CLASS_LOG_LEVEL_NOTICE: EventClassLogLevel.NOTICE,
    native_bt.EVENT_CLASS_LOG_LEVEL_INFO: EventClassLogLevel.INFO,
    native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM: EventClassLogLevel.DEBUG_SYSTEM,
    native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM: EventClassLogLevel.DEBUG_PROGRAM,
    native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS: EventClassLogLevel.DEBUG_PROCESS,
    native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE: EventClassLogLevel.DEBUG_MODULE,
    native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT: EventClassLogLevel.DEBUG_UNIT,
    native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION: EventClassLogLevel.DEBUG_FUNCTION,
    native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG_LINE: EventClassLogLevel.DEBUG_LINE,
    native_bt.EVENT_CLASS_LOG_LEVEL_DEBUG: EventClassLogLevel.DEBUG,
}
