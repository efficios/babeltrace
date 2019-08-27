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
from bt2 import event_class as bt2_event_class
from bt2 import trace_class as bt2_trace_class
from bt2 import clock_class as bt2_clock_class
from bt2 import value as bt2_value
import collections.abc


class _StreamClassConst(object._SharedObject, collections.abc.Mapping):
    _get_ref = staticmethod(native_bt.stream_class_get_ref)
    _put_ref = staticmethod(native_bt.stream_class_put_ref)
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

    _event_class_cls = property(lambda _: bt2_event_class._EventClassConst)
    _trace_class_cls = property(lambda _: bt2_trace_class._TraceClassConst)
    _clock_class_cls = property(lambda _: bt2_clock_class._ClockClassConst)

    def __getitem__(self, key):
        utils._check_int64(key)
        ec_ptr = self._borrow_event_class_ptr_by_id(self._ptr, key)

        if ec_ptr is None:
            raise KeyError(key)

        return self._event_class_cls._create_from_ptr_and_get_ref(ec_ptr)

    def __len__(self):
        count = native_bt.stream_class_get_event_class_count(self._ptr)
        assert count >= 0
        return count

    def __iter__(self):
        for idx in range(len(self)):
            ec_ptr = self._borrow_event_class_ptr_by_index(self._ptr, idx)
            assert ec_ptr is not None

            id = native_bt.event_class_get_id(ec_ptr)
            assert id >= 0

            yield id

    @property
    def trace_class(self):
        tc_ptr = self._borrow_trace_class_ptr(self._ptr)

        if tc_ptr is not None:
            return self._trace_class_cls._create_from_ptr_and_get_ref(tc_ptr)

    @property
    def user_attributes(self):
        ptr = native_bt.stream_class_borrow_user_attributes(self._ptr)
        assert ptr is not None
        return bt2_value._create_from_ptr_and_get_ref(ptr)

    @property
    def name(self):
        return native_bt.stream_class_get_name(self._ptr)

    @property
    def assigns_automatic_event_class_id(self):
        return native_bt.stream_class_assigns_automatic_event_class_id(self._ptr)

    @property
    def assigns_automatic_stream_id(self):
        return native_bt.stream_class_assigns_automatic_stream_id(self._ptr)

    @property
    def supports_packets(self):
        return native_bt.stream_class_supports_packets(self._ptr)

    @property
    def packets_have_beginning_default_clock_snapshot(self):
        return native_bt.stream_class_packets_have_beginning_default_clock_snapshot(
            self._ptr
        )

    @property
    def packets_have_end_default_clock_snapshot(self):
        return native_bt.stream_class_packets_have_end_default_clock_snapshot(self._ptr)

    @property
    def supports_discarded_events(self):
        return native_bt.stream_class_supports_discarded_events(self._ptr)

    @property
    def discarded_events_have_default_clock_snapshots(self):
        return native_bt.stream_class_discarded_events_have_default_clock_snapshots(
            self._ptr
        )

    @property
    def supports_discarded_packets(self):
        return native_bt.stream_class_supports_discarded_packets(self._ptr)

    @property
    def discarded_packets_have_default_clock_snapshots(self):
        return native_bt.stream_class_discarded_packets_have_default_clock_snapshots(
            self._ptr
        )

    @property
    def id(self):
        id = native_bt.stream_class_get_id(self._ptr)

        if id < 0:
            return

        return id

    @property
    def packet_context_field_class(self):
        fc_ptr = self._borrow_packet_context_field_class_ptr(self._ptr)

        if fc_ptr is None:
            return

        return bt2_field_class._create_field_class_from_ptr_and_get_ref(fc_ptr)

    @property
    def event_common_context_field_class(self):
        fc_ptr = self._borrow_event_common_context_field_class_ptr(self._ptr)

        if fc_ptr is None:
            return

        return bt2_field_class._create_field_class_from_ptr_and_get_ref(fc_ptr)

    @property
    def default_clock_class(self):
        cc_ptr = self._borrow_default_clock_class_ptr(self._ptr)
        if cc_ptr is None:
            return

        return self._clock_class_cls._create_from_ptr_and_get_ref(cc_ptr)


class _StreamClass(_StreamClassConst):
    _get_ref = staticmethod(native_bt.stream_class_get_ref)
    _put_ref = staticmethod(native_bt.stream_class_put_ref)
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
    _event_class_cls = property(lambda s: bt2_event_class._EventClass)
    _trace_class_cls = property(lambda s: bt2_trace_class._TraceClass)
    _clock_class_cls = property(lambda s: bt2_clock_class._ClockClass)

    def create_event_class(
        self,
        id=None,
        name=None,
        user_attributes=None,
        log_level=None,
        emf_uri=None,
        specific_context_field_class=None,
        payload_field_class=None,
    ):
        if self.assigns_automatic_event_class_id:
            if id is not None:
                raise ValueError(
                    'id provided, but stream class assigns automatic event class ids'
                )

            ec_ptr = native_bt.event_class_create(self._ptr)
        else:
            if id is None:
                raise ValueError(
                    'id not provided, but stream class does not assign automatic event class ids'
                )

            utils._check_uint64(id)
            ec_ptr = native_bt.event_class_create_with_id(self._ptr, id)

        event_class = bt2_event_class._EventClass._create_from_ptr(ec_ptr)

        if name is not None:
            event_class._name = name

        if user_attributes is not None:
            event_class._user_attributes = user_attributes

        if log_level is not None:
            event_class._log_level = log_level

        if emf_uri is not None:
            event_class._emf_uri = emf_uri

        if specific_context_field_class is not None:
            event_class._specific_context_field_class = specific_context_field_class

        if payload_field_class is not None:
            event_class._payload_field_class = payload_field_class

        return event_class

    def _user_attributes(self, user_attributes):
        value = bt2_value.create_value(user_attributes)
        utils._check_type(value, bt2_value.MapValue)
        native_bt.stream_class_set_user_attributes(self._ptr, value._ptr)

    _user_attributes = property(fset=_user_attributes)

    def _name(self, name):
        utils._check_str(name)
        status = native_bt.stream_class_set_name(self._ptr, name)
        utils._handle_func_status(status, "cannot set stream class object's name")

    _name = property(fset=_name)

    def _assigns_automatic_event_class_id(self, auto_id):
        utils._check_bool(auto_id)
        return native_bt.stream_class_set_assigns_automatic_event_class_id(
            self._ptr, auto_id
        )

    _assigns_automatic_event_class_id = property(fset=_assigns_automatic_event_class_id)

    def _assigns_automatic_stream_id(self, auto_id):
        utils._check_bool(auto_id)
        return native_bt.stream_class_set_assigns_automatic_stream_id(
            self._ptr, auto_id
        )

    _assigns_automatic_stream_id = property(fset=_assigns_automatic_stream_id)

    def _set_supports_packets(self, supports, with_begin_cs=False, with_end_cs=False):
        utils._check_bool(supports)
        utils._check_bool(with_begin_cs)
        utils._check_bool(with_end_cs)

        if not supports and (with_begin_cs or with_end_cs):
            raise ValueError(
                'cannot not support packets, but have default clock snapshots'
            )

        if not supports and self.packet_context_field_class is not None:
            raise ValueError('stream class already has a packet context field class')

        native_bt.stream_class_set_supports_packets(
            self._ptr, supports, with_begin_cs, with_end_cs
        )

    def _set_supports_discarded_events(self, supports, with_cs=False):
        utils._check_bool(supports)
        utils._check_bool(with_cs)

        if not supports and with_cs:
            raise ValueError(
                'cannot not support discarded events, but have default clock snapshots'
            )

        native_bt.stream_class_set_supports_discarded_events(
            self._ptr, supports, with_cs
        )

    _supports_discarded_events = property(fset=_set_supports_discarded_events)

    def _set_supports_discarded_packets(self, supports, with_cs):
        utils._check_bool(supports)
        utils._check_bool(with_cs)

        if supports and not self.supports_packets:
            raise ValueError(
                'cannot support discarded packets, but not support packets'
            )

        if not supports and with_cs:
            raise ValueError(
                'cannot not support discarded packets, but have default clock snapshots'
            )

        native_bt.stream_class_set_supports_discarded_packets(
            self._ptr, supports, with_cs
        )

    _supports_discarded_packets = property(fset=_set_supports_discarded_packets)

    def _packet_context_field_class(self, packet_context_field_class):
        if packet_context_field_class is not None:
            utils._check_type(
                packet_context_field_class, bt2_field_class._StructureFieldClass
            )

            if not self.supports_packets:
                raise ValueError('stream class does not support packets')

            status = native_bt.stream_class_set_packet_context_field_class(
                self._ptr, packet_context_field_class._ptr
            )
            utils._handle_func_status(
                status, "cannot set stream class' packet context field class"
            )

    _packet_context_field_class = property(fset=_packet_context_field_class)

    def _event_common_context_field_class(self, event_common_context_field_class):
        if event_common_context_field_class is not None:
            utils._check_type(
                event_common_context_field_class, bt2_field_class._StructureFieldClass
            )

            set_context_fn = native_bt.stream_class_set_event_common_context_field_class
            status = set_context_fn(self._ptr, event_common_context_field_class._ptr)
            utils._handle_func_status(
                status, "cannot set stream class' event context field type"
            )

    _event_common_context_field_class = property(fset=_event_common_context_field_class)

    def _default_clock_class(self, clock_class):
        utils._check_type(clock_class, bt2_clock_class._ClockClass)
        native_bt.stream_class_set_default_clock_class(self._ptr, clock_class._ptr)

    _default_clock_class = property(fset=_default_clock_class)
