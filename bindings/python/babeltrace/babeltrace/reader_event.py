# The MIT License (MIT)
#
# Copyright (c) 2013-2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

import bt2
import babeltrace.common as common
import babeltrace.reader_field_definition as field_definition
import datetime
import collections

def _create_event(event_notification, trace_handle=None, trace_collection=None):
    event = Event.__new__(Event)
    event._event_notification = event_notification
    event._trace_handle = trace_handle
    event._trace_collection = trace_collection
    return event

class Event(collections.Mapping):
    """
    An :class:`Event` object represents a trace event. :class:`Event`
    objects are returned by :attr:`TraceCollection.events` and are
    not meant to be instantiated by the user.

    :class:`Event` has a :class:`dict`-like interface for accessing
    an event's field value by field name:

    .. code-block:: python

       event['my_field']

    If a field name exists in multiple scopes, the value of the first
    field found is returned. The scopes are searched in the following
    order:

    1. Event fields (:attr:`babeltrace.common.CTFScope.EVENT_FIELDS`)
    2. Event context (:attr:`babeltrace.common.CTFScope.EVENT_CONTEXT`)
    3. Stream event context (:attr:`babeltrace.common.CTFScope.STREAM_EVENT_CONTEXT`)
    4. Event header (:attr:`babeltrace.common.CTFScope.STREAM_EVENT_HEADER`)
    5. Packet context (:attr:`babeltrace.common.CTFScope.STREAM_PACKET_CONTEXT`)
    6. Packet header (:attr:`babeltrace.common.CTFScope.TRACE_PACKET_HEADER`)

    It is still possible to obtain a field's value from a specific
    scope using :meth:`field_with_scope`.

    Field values are returned as native Python types, that is:

    +-----------------------+----------------------------------+
    | Field type            | Python type                      |
    +=======================+==================================+
    | Integer               | :class:`int`                     |
    +-----------------------+----------------------------------+
    | Floating point number | :class:`float`                   |
    +-----------------------+----------------------------------+
    | Enumeration           | :class:`str` (enumeration label) |
    +-----------------------+----------------------------------+
    | String                | :class:`str`                     |
    +-----------------------+----------------------------------+
    | Array                 | :class:`list` of native Python   |
    |                       | objects                          |
    +-----------------------+----------------------------------+
    | Sequence              | :class:`list` of native Python   |
    |                       | objects                          |
    +-----------------------+----------------------------------+
    | Structure             | :class:`dict` mapping field      |
    |                       | names to native Python objects   |
    +-----------------------+----------------------------------+

    For example, printing the third element of a sequence named ``seq``
    in a structure named ``my_struct`` of the ``event``'s field named
    ``my_field`` is done this way:

    .. code-block:: python

       print(event['my_field']['my_struct']['seq'][2])
    """

    def __init__(self):
        raise NotImplementedError("Event cannot be instantiated")

    @property
    def name(self):
        """
        Event name or ``None`` on error.
        """

        try:
            return self._event_notification.event.name
        except bt2.Error:
            pass

    def _clock_value(self):
        cc_prio_map = self._event_notification.clock_class_priority_map
        clock_class = cc_prio_map.highest_priority_clock_class
        if not clock_class:
            return

        return self._event_notification.event.clock_value(clock_class)

    @property
    def cycles(self):
        """
        Event timestamp in cycles or -1 on error.
        """

        try:
            clock_value = self._clock_value()
        except bt2.Error:
            return -1

        if clock_value is not None:
            return clock_value.cycles
        else:
            return -1

    @property
    def timestamp(self):
        """
        Event timestamp (nanoseconds since Epoch).
        """

        try:
            clock_value = self._clock_value()
        except bt2.Error:
            raise RuntimeError("Failed to get event timestamp")

        if clock_value is not None:
            return clock_value.ns_from_epoch
        else:
            raise RuntimeError("Failed to get event timestamp")

    @property
    def datetime(self):
        """
        Event timestamp as a standard :class:`datetime.datetime`
        object.

        Note that the :class:`datetime.datetime` class' precision
        is limited to microseconds, whereas :attr:`timestamp` provides
        the event's timestamp with a nanosecond resolution.
        """

        return datetime.date.fromtimestamp(self.timestamp / 1E9)

    def field_with_scope(self, field_name, scope):
        """
        Returns the value of a field named *field_name* within the
        scope *scope*, or ``None`` if the field cannot be found.

        *scope* must be one of :class:`babeltrace.common.CTFScope`
        constants.
        """

        if scope not in _SCOPES:
            raise ValueError("Invalid scope provided")

        field = self._field_with_scope(field_name, scope)

        if field is not None:
            return field.value

    def field_list_with_scope(self, scope):
        """
        Returns a list of field names in the scope *scope*.
        """

        if scope not in _SCOPES:
            raise ValueError("Invalid scope provided")

        field_names = []

        for field in self._field_list_with_scope(scope):
            field_names.append(field.name)

        return field_names

    @property
    def handle(self):
        """
        :class:`TraceHandle` object containing this event, or ``None``
        on error.
        """

        try:
            return self._trace_handle
        except AttributeError:
            return None

    @property
    def trace_collection(self):
        """
        :class:`TraceCollection` object containing this event, or
        ``None`` on error.
        """

        try:
            return self._trace_collection
        except AttributeError:
            return

    def __getitem__(self, field_name):
        field = self._field(field_name)
        if field is None:
            raise KeyError(field_name)
        return field.value

    def __iter__(self):
        for key in self.keys():
            yield key

    def __len__(self):
        count = 0
        for scope in _SCOPES:
            scope_field = self._get_scope_field(scope)
            if scope_field is not None and isinstance(scope_field,
                                                      bt2._StructureField):
                count += len(scope_field)
        return count

    def __contains__(self, field_name):
        return self._field(field_name) is not None

    def keys(self):
        """
        Returns the list of field names.

        Note: field names are unique within the returned list, although
        a field name could exist in multiple scopes. Use
        :meth:`field_list_with_scope` to obtain the list of field names
        of a given scope.
        """

        field_names = set()

        for scope in _SCOPES:
            for name in self.field_list_with_scope(scope):
                field_names.add(name)

        return list(field_names)

    def get(self, field_name, default=None):
        """
        Returns the value of the field named *field_name*, or *default*
        when not found.

        See :class:`Event` note about how fields are retrieved by
        name when multiple fields share the same name in different
        scopes.
        """

        field = self._field(field_name)

        if field is None:
            return default

        return field.value

    def items(self):
        """
        Generates pairs of (field name, field value).

        This method iterates :meth:`keys` to find field names, which
        means some fields could be unavailable if other fields share
        their names in scopes with higher priorities.
        """

        for field in self.keys():
            yield (field, self[field])

    def _get_scope_field(self, scope):
        try:
            event = self._event_notification.event
            if scope is common.CTFScope.EVENT_FIELDS:
                return event.payload_field

            if scope is common.CTFScope.EVENT_CONTEXT:
                return event.context_field

            if scope is common.CTFScope.STREAM_EVENT_CONTEXT:
                return event.stream_event_context_field

            if scope is common.CTFScope.STREAM_EVENT_HEADER:
                return event.header_field

            if scope is common.CTFScope.STREAM_PACKET_CONTEXT:
                return event.packet.context_field

            if scope is common.CTFScope.TRACE_PACKET_HEADER:
                return event.packet.header_field
        except bt2.Error:
            return

        raise ValueError("Invalid scope provided")

    def _field_with_scope(self, field_name, scope):
        scope_field = self._get_scope_field(scope)
        if scope_field is not None:
            try:
                bt2_field = scope_field[field_name]
                if bt2_field is not None:
                    return field_definition._Definition(scope, bt2_field,
                                                        field_name)
            except (KeyError, bt2.Error):
                return None

    def _field(self, field_name):
        for scope in _SCOPES:
            field = self._field_with_scope(field_name, scope)
            if field is not None:
                return field

    def _field_list_with_scope(self, scope):
        fields = []
        scope_field = self._get_scope_field(scope)

        if scope_field is None or not isinstance(scope_field,
                                                 bt2._StructureField):
            return fields

        for name, field in scope_field.items():
            fields.append(field_definition._Definition(scope, field, name))

        return fields


# Priority of the scopes when searching for event fields
_SCOPES = [
    common.CTFScope.EVENT_FIELDS,
    common.CTFScope.EVENT_CONTEXT,
    common.CTFScope.STREAM_EVENT_CONTEXT,
    common.CTFScope.STREAM_EVENT_HEADER,
    common.CTFScope.STREAM_PACKET_CONTEXT,
    common.CTFScope.TRACE_PACKET_HEADER
]
