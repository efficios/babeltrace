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
import bt2.field_types
import collections.abc
import bt2.values
import bt2.event
import copy
import bt2


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


class EventClass(object._Object):
    def __init__(self, name, id=None, log_level=None, emf_uri=None,
                 context_field_type=None, payload_field_type=None):
        utils._check_str(name)
        ptr = native_bt.event_class_create(name)

        if ptr is None:
            raise bt2.CreationError('cannot create event class object')

        super().__init__(ptr)

        if id is not None:
            self.id = id

        if log_level is not None:
            self.log_level = log_level

        if emf_uri is not None:
            self.emf_uri = emf_uri

        if context_field_type is not None:
            self.context_field_type = context_field_type

        if payload_field_type is not None:
            self.payload_field_type = payload_field_type

    @property
    def stream_class(self):
        sc_ptr = native_bt.event_class_get_stream_class(self._ptr)

        if sc_ptr is not None:
            return bt2.StreamClass._create_from_ptr(sc_ptr)

    @property
    def name(self):
        return native_bt.event_class_get_name(self._ptr)

    @property
    def id(self):
        id = native_bt.event_class_get_id(self._ptr)
        return id if id >= 0 else None

    @id.setter
    def id(self, id):
        utils._check_int64(id)
        ret = native_bt.event_class_set_id(self._ptr, id)
        utils._handle_ret(ret, "cannot set event class object's ID")

    @property
    def log_level(self):
        log_level = native_bt.event_class_get_log_level(self._ptr)
        return log_level if log_level >= 0 else None

    @log_level.setter
    def log_level(self, log_level):
        log_levels = (
            EventClassLogLevel.UNSPECIFIED,
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

        ret = native_bt.event_class_set_log_level(self._ptr, log_level)
        utils._handle_ret(ret, "cannot set event class object's log level")

    @property
    def emf_uri(self):
        return native_bt.event_class_get_emf_uri(self._ptr)

    @emf_uri.setter
    def emf_uri(self, emf_uri):
        utils._check_str(emf_uri)
        ret = native_bt.event_class_set_emf_uri(self._ptr, emf_uri)
        utils._handle_ret(ret, "cannot set event class object's EMF URI")

    @property
    def context_field_type(self):
        ft_ptr = native_bt.event_class_get_context_type(self._ptr)

        if ft_ptr is None:
            return

        return bt2.field_types._create_from_ptr(ft_ptr)

    @context_field_type.setter
    def context_field_type(self, context_field_type):
        context_field_type_ptr = None

        if context_field_type is not None:
            utils._check_type(context_field_type, bt2.field_types._FieldType)
            context_field_type_ptr = context_field_type._ptr

        ret = native_bt.event_class_set_context_type(self._ptr, context_field_type_ptr)
        utils._handle_ret(ret, "cannot set event class object's context field type")

    @property
    def payload_field_type(self):
        ft_ptr = native_bt.event_class_get_payload_type(self._ptr)

        if ft_ptr is None:
            return

        return bt2.field_types._create_from_ptr(ft_ptr)

    @payload_field_type.setter
    def payload_field_type(self, payload_field_type):
        payload_field_type_ptr = None

        if payload_field_type is not None:
            utils._check_type(payload_field_type, bt2.field_types._FieldType)
            payload_field_type_ptr = payload_field_type._ptr

        ret = native_bt.event_class_set_payload_type(self._ptr, payload_field_type_ptr)
        utils._handle_ret(ret, "cannot set event class object's payload field type")

    def __call__(self):
        event_ptr = native_bt.event_create(self._ptr)

        if event_ptr is None:
            raise bt2.CreationError('cannot create event field object')

        return bt2.event._create_from_ptr(event_ptr)

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        if self.addr == other.addr:
            return True

        self_props = (
            self.name,
            self.id,
            self.log_level,
            self.emf_uri,
            self.context_field_type,
            self.payload_field_type
        )
        other_props = (
            other.name,
            other.id,
            other.log_level,
            other.emf_uri,
            other.context_field_type,
            other.payload_field_type
        )
        return self_props == other_props

    def _copy(self, ft_copy_func):
        cpy = EventClass(self.name)
        cpy.id = self.id

        if self.log_level is not None:
            cpy.log_level = self.log_level

        if self.emf_uri is not None:
            cpy.emf_uri = self.emf_uri

        cpy.context_field_type = ft_copy_func(self.context_field_type)
        cpy.payload_field_type = ft_copy_func(self.payload_field_type)
        return cpy

    def __copy__(self):
        return self._copy(lambda ft: ft)

    def __deepcopy__(self, memo):
        cpy = self._copy(copy.deepcopy)
        memo[id(self)] = cpy
        return cpy
