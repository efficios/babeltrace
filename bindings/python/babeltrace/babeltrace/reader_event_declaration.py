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
import babeltrace.reader_field_declaration as field_declaration
import collections

def _create_event_declaration(event_class):
    event_declaration = EventDeclaration.__new__(EventDeclaration)
    event_declaration._event_class = event_class
    return event_declaration

class EventDeclaration:
    """
    An event declaration contains the properties of a class of events,
    that is, the common properties and fields layout of all the actual
    recorded events associated with this declaration.

    This class is not meant to be instantiated by the user. It is
    returned by :attr:`TraceHandle.events`.
    """
    def __init__(self):
        raise NotImplementedError("EventDeclaration cannot be instantiated")

    def _get_scope_field_type(self, scope):
        try:
            ec = self._event_class
            if scope is common.CTFScope.EVENT_FIELDS:
                return ec.payload_field_type

            if scope is common.CTFScope.EVENT_CONTEXT:
                return ec.context_field_type

            if scope is common.CTFScope.STREAM_EVENT_CONTEXT:
                return ec.stream_class.event_context_field_type

            if scope is common.CTFScope.STREAM_EVENT_HEADER:
                return ec.stream_class.event_header_field_type

            if scope is common.CTFScope.STREAM_PACKET_CONTEXT:
                return ec.stream_class.packet_context_field_type

            if scope is common.CTFScope.TRACE_PACKET_HEADER:
                return ec.stream_class.trace.packet_header_field_type
        except bt2.Error:
            return

        raise ValueError("Invalid scope provided")

    @property
    def name(self):
        """
        Event name, or ``None`` on error.
        """

        return self._event_class.name

    @property
    def id(self):
        """
        Event numeric ID, or -1 on error.
        """

        return self._event_class.id

    @property
    def fields(self):
        """
        Generates all the field declarations of this event, going
        through each scope in the following order:

        1. Event fields (:attr:`babeltrace.common.CTFScope.EVENT_FIELDS`)
        2. Event context (:attr:`babeltrace.common.CTFScope.EVENT_CONTEXT`)
        3. Stream event context (:attr:`babeltrace.common.CTFScope.STREAM_EVENT_CONTEXT`)
        4. Event header (:attr:`babeltrace.common.CTFScope.STREAM_EVENT_HEADER`)
        5. Packet context (:attr:`babeltrace.common.CTFScope.STREAM_PACKET_CONTEXT`)
        6. Packet header (:attr:`babeltrace.common.CTFScope.TRACE_PACKET_HEADER`)

        All the generated field declarations inherit
        :class:`FieldDeclaration`, and are among:

        * :class:`IntegerFieldDeclaration`
        * :class:`FloatFieldDeclaration`
        * :class:`EnumerationFieldDeclaration`
        * :class:`StringFieldDeclaration`
        * :class:`ArrayFieldDeclaration`
        * :class:`SequenceFieldDeclaration`
        * :class:`StructureFieldDeclaration`
        * :class:`VariantFieldDeclaration`
        """

        for scope in _SCOPES:
            for declaration in self.fields_scope(scope):
                yield declaration

    def fields_scope(self, scope):
        """
        Generates all the field declarations of the event's scope
        *scope*.

        *scope* must be one of :class:`babeltrace.common.CTFScope` constants.

        All the generated field declarations inherit
        :class:`FieldDeclaration`, and are among:

        * :class:`IntegerFieldDeclaration`
        * :class:`FloatFieldDeclaration`
        * :class:`EnumerationFieldDeclaration`
        * :class:`StringFieldDeclaration`
        * :class:`ArrayFieldDeclaration`
        * :class:`SequenceFieldDeclaration`
        * :class:`StructureFieldDeclaration`
        * :class:`VariantFieldDeclaration`
        """

        scope_field_type = self._get_scope_field_type(scope)
        for name, field_type in scope_field_type.items():
            yield field_declaration._create_field_declaration(field_type, name,
                                                              scope)

# Priority of the scopes when searching for event fields
_SCOPES = [
    common.CTFScope.EVENT_FIELDS,
    common.CTFScope.EVENT_CONTEXT,
    common.CTFScope.STREAM_EVENT_CONTEXT,
    common.CTFScope.STREAM_EVENT_HEADER,
    common.CTFScope.STREAM_PACKET_CONTEXT,
    common.CTFScope.TRACE_PACKET_HEADER
]
