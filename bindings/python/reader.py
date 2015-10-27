# reader.py
#
# Babeltrace reader interface Python module
#
# Copyright 2012-2015 EfficiOS Inc.
#
# Author: Danny Serres <danny.serres@efficios.com>
# Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import babeltrace.nativebt as nbt
import babeltrace.common as common
import collections
import os
from datetime import datetime


class TraceCollection:
    """
    A :class:`TraceCollection` is a collection of opened traces.

    Once a trace collection is created, you can add traces to the
    collection by using the :meth:`add_trace` or
    :meth:`add_traces_recursive`, and then iterate on the merged
    events using :attr:`events`.

    You may use :meth:`remove_trace` to close and remove a specific
    trace from a trace collection.
    """

    def __init__(self):
        """
        Creates an empty trace collection.
        """

        self._tc = nbt._bt_context_create()

    def __del__(self):
        nbt._bt_context_put(self._tc)

    def add_trace(self, path, format_str):
        """
        Adds a trace to the trace collection.

        *path* is the exact path of the trace on the filesystem.

        *format_str* is a string indicating the type of trace to
        add. ``ctf`` is currently the only supported trace format.

        Returns the corresponding :class:`TraceHandle` instance for
        this opened trace on success, or ``None`` on error.

        This function **does not** recurse directories to find a
        trace.  See :meth:`add_traces_recursive` for a recursive
        version of this function.
        """

        ret = nbt._bt_context_add_trace(self._tc, path, format_str,
                                        None, None, None)

        if ret < 0:
            return None

        th = TraceHandle.__new__(TraceHandle)
        th._id = ret
        th._trace_collection = self

        return th

    def add_traces_recursive(self, path, format_str):
        """
        Adds traces to this trace collection by recursively searching
        in the *path* directory.

        *format_str* is a string indicating the type of trace to add.
        ``ctf`` is currently the only supported trace format.

        Returns a :class:`dict` object mapping full paths to trace
        handles for each trace found, or ``None`` on error.

        See also :meth:`add_trace`.
        """

        trace_handles = {}
        noTrace = True
        error = False

        for fullpath, dirs, files in os.walk(path):
            if "metadata" in files:
                trace_handle = self.add_trace(fullpath, format_str)

                if trace_handle is None:
                    error = True
                    continue

                trace_handles[fullpath] = trace_handle
                noTrace = False

        if noTrace and error:
            return None

        return trace_handles

    def remove_trace(self, trace_handle):
        """
        Removes a trace from the trace collection using its trace
        handle *trace_handle*.

        :class:`TraceHandle` objects are returned by :meth:`add_trace`
        and :meth:`add_traces_recursive`.
        """

        try:
            nbt._bt_context_remove_trace(self._tc, trace_handle._id)
        except AttributeError:
            raise TypeError("in remove_trace, argument 2 must be a TraceHandle instance")

    @property
    def events(self):
        """
        Generates the ordered :class:`Event` objects of all the opened
        traces contained in this trace collection.

        Due to limitations of the native Babeltrace API, only one event
        may be "alive" at a given time, i.e. a user **should never**
        store a copy of the events returned by this function for
        ulterior use. Users shall make sure to copy the information
        they need *from* an event before accessing the next one.
        """

        begin_pos_ptr = nbt._bt_python_create_iter_pos()
        end_pos_ptr = nbt._bt_python_create_iter_pos()
        begin_pos_ptr.type = nbt.SEEK_BEGIN
        end_pos_ptr.type = nbt.SEEK_LAST

        for event in self._events(begin_pos_ptr, end_pos_ptr):
            yield event

        nbt._bt_iter_free_pos(begin_pos_ptr);
        nbt._bt_iter_free_pos(end_pos_ptr);

    def events_timestamps(self, timestamp_begin, timestamp_end):
        """
        Generates the ordered :class:`Event` objects of all the opened
        traces contained in this trace collection from *timestamp_begin*
        to *timestamp_end*.

        *timestamp_begin* and *timestamp_end* are given in nanoseconds
        since Epoch.

        See :attr:`events` for notes and limitations.
        """

        begin_pos_ptr = nbt._bt_python_create_iter_pos()
        end_pos_ptr = nbt._bt_python_create_iter_pos()
        begin_pos_ptr.type = end_pos_ptr.type = nbt.SEEK_TIME
        begin_pos_ptr.u.seek_time = timestamp_begin
        end_pos_ptr.u.seek_time = timestamp_end

        for event in self._events(begin_pos_ptr, end_pos_ptr):
            yield event

        nbt._bt_iter_free_pos(begin_pos_ptr);
        nbt._bt_iter_free_pos(end_pos_ptr);

    @property
    def timestamp_begin(self):
        """
        Begin timestamp of this trace collection (nanoseconds since
        Epoch).
        """

        pos_ptr = nbt._bt_iter_pos()
        pos_ptr.type = nbt.SEEK_BEGIN

        return self._timestamp_at_pos(pos_ptr)

    @property
    def timestamp_end(self):
        """
        End timestamp of this trace collection (nanoseconds since
        Epoch).
        """

        pos_ptr = nbt._bt_iter_pos()
        pos_ptr.type = nbt.SEEK_LAST

        return self._timestamp_at_pos(pos_ptr)

    def _timestamp_at_pos(self, pos_ptr):
        ctf_it_ptr = nbt._bt_ctf_iter_create(self._tc, pos_ptr, pos_ptr)

        if ctf_it_ptr is None:
            raise NotImplementedError("Creation of multiple iterators is unsupported.")

        ev_ptr = nbt._bt_ctf_iter_read_event(ctf_it_ptr)
        nbt._bt_ctf_iter_destroy(ctf_it_ptr)

        ev = Event.__new__(Event)
        ev._e = ev_ptr

        return ev.timestamp

    def _events(self, begin_pos_ptr, end_pos_ptr):
        ctf_it_ptr = nbt._bt_ctf_iter_create(self._tc, begin_pos_ptr, end_pos_ptr)

        if ctf_it_ptr is None:
            raise NotImplementedError("Creation of multiple iterators is unsupported.")

        while True:
            ev_ptr = nbt._bt_ctf_iter_read_event(ctf_it_ptr)

            if ev_ptr is None:
                break

            ev = Event.__new__(Event)
            ev._e = ev_ptr

            try:
                yield ev
            except GeneratorExit:
                break

            ret = nbt._bt_iter_next(nbt._bt_ctf_get_iter(ctf_it_ptr))

            if ret != 0:
                break

        nbt._bt_ctf_iter_destroy(ctf_it_ptr)


# Based on enum bt_clock_type in clock-type.h
class _ClockType:
    CLOCK_CYCLES = 0
    CLOCK_REAL = 1


class TraceHandle:
    """
    A :class:`TraceHandle` is a handle allowing the user to manipulate
    a specific trace directly. It is a unique identifier representing a
    trace, and is not meant to be instantiated by the user.
    """

    def __init__(self):
        raise NotImplementedError("TraceHandle cannot be instantiated")

    def __repr__(self):
        return "Babeltrace TraceHandle: trace_id('{0}')".format(self._id)

    @property
    def id(self):
        """
        Numeric ID of this trace handle.
        """

        return self._id

    @property
    def path(self):
        """
        Path of the underlying trace.
        """

        return nbt._bt_trace_handle_get_path(self._trace_collection._tc,
                                             self._id)

    @property
    def timestamp_begin(self):
        """
        Buffers creation timestamp (nanoseconds since Epoch) of the
        underlying trace.
        """

        return nbt._bt_trace_handle_get_timestamp_begin(self._trace_collection._tc,
                                                        self._id,
                                                        _ClockType.CLOCK_REAL)

    @property
    def timestamp_end(self):
        """
        Buffers destruction timestamp (nanoseconds since Epoch) of the
        underlying trace.
        """

        return nbt._bt_trace_handle_get_timestamp_end(self._trace_collection._tc,
                                                      self._id,
                                                      _ClockType.CLOCK_REAL)

    @property
    def events(self):
        """
        Generates all the :class:`EventDeclaration` objects of the
        underlying trace.
        """

        ret = nbt._bt_python_event_decl_listcaller(self.id,
                                                   self._trace_collection._tc)

        if not isinstance(ret, list):
            return

        ptr_list, count = ret

        for i in range(count):
            tmp = EventDeclaration.__new__(EventDeclaration)
            tmp._ed = nbt._bt_python_decl_one_from_list(ptr_list, i)
            yield tmp




# Priority of the scopes when searching for event fields
_scopes = [
    common.CTFScope.EVENT_FIELDS,
    common.CTFScope.EVENT_CONTEXT,
    common.CTFScope.STREAM_EVENT_CONTEXT,
    common.CTFScope.STREAM_EVENT_HEADER,
    common.CTFScope.STREAM_PACKET_CONTEXT,
    common.CTFScope.TRACE_PACKET_HEADER
]


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

        return nbt._bt_ctf_event_name(self._e)

    @property
    def cycles(self):
        """
        Event timestamp in cycles or -1 on error.
        """

        return nbt._bt_ctf_get_cycles(self._e)

    @property
    def timestamp(self):
        """
        Event timestamp (nanoseconds since Epoch) or -1 on error.
        """

        return nbt._bt_ctf_get_timestamp(self._e)

    @property
    def datetime(self):
        """
        Event timestamp as a standard :class:`datetime.datetime`
        object.

        Note that the :class:`datetime.datetime` class' precision
        is limited to microseconds, whereas :attr:`timestamp` provides
        the event's timestamp with a nanosecond resolution.
        """

        return datetime.fromtimestamp(self.timestamp / 1E9)

    def field_with_scope(self, field_name, scope):
        """
        Returns the value of a field named *field_name* within the
        scope *scope*, or ``None`` if the field cannot be found.

        *scope* must be one of :class:`babeltrace.common.CTFScope`
        constants.
        """

        if scope not in _scopes:
            raise ValueError("Invalid scope provided")

        field = self._field_with_scope(field_name, scope)

        if field is not None:
            return field.value

    def field_list_with_scope(self, scope):
        """
        Returns a list of field names in the scope *scope*.
        """

        if scope not in _scopes:
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

        ret = nbt._bt_ctf_event_get_handle_id(self._e)

        if ret < 0:
            return None

        th = TraceHandle.__new__(TraceHandle)
        th._id = ret
        th._trace_collection = self.get_trace_collection()

        return th

    @property
    def trace_collection(self):
        """
        :class:`TraceCollection` object containing this event, or
        ``None`` on error.
        """

        trace_collection = TraceCollection()
        trace_collection._tc = nbt._bt_ctf_event_get_context(self._e)

        if trace_collection._tc is not None:
            return trace_collection

    def __getitem__(self, field_name):
        field = self._field(field_name)

        if field is not None:
            return field.value

        raise KeyError(field_name)

    def __iter__(self):
        for key in self.keys():
            yield key

    def __len__(self):
        count = 0

        for scope in _scopes:
            scope_ptr = nbt._bt_ctf_get_top_level_scope(self._e, scope)
            ret = nbt._bt_python_field_listcaller(self._e, scope_ptr)

            if isinstance(ret, list):
                count += ret[1]

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

        for scope in _scopes:
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

    def _field_with_scope(self, field_name, scope):
        scope_ptr = nbt._bt_ctf_get_top_level_scope(self._e, scope)

        if scope_ptr is None:
            return None

        definition_ptr = nbt._bt_ctf_get_field(self._e, scope_ptr, field_name)

        if definition_ptr is None:
            return None

        field = _Definition(definition_ptr, scope)

        return field

    def _field(self, field_name):
        field = None

        for scope in _scopes:
            field = self._field_with_scope(field_name, scope)

            if field is not None:
                break

        return field

    def _field_list_with_scope(self, scope):
        fields = []
        scope_ptr = nbt._bt_ctf_get_top_level_scope(self._e, scope)

        # Returns a list [list_ptr, count]. If list_ptr is NULL, SWIG will only
        # provide the "count" return value
        count = 0
        list_ptr = None
        ret = nbt._bt_python_field_listcaller(self._e, scope_ptr)

        if isinstance(ret, list):
            list_ptr, count = ret

        for i in range(count):
            definition_ptr = nbt._bt_python_field_one_from_list(list_ptr, i)

            if definition_ptr is not None:
                definition = _Definition(definition_ptr, scope)
                fields.append(definition)

        return fields


class FieldError(Exception):
    """
    Field error, raised when the value of a field cannot be accessed.
    """

    def __init__(self, value):
        self.value = value

    def __str__(self):
        return repr(self.value)


class EventDeclaration:
    """
    An event declaration contains the properties of a class of events,
    that is, the common properties and fields layout of all the actual
    recorded events associated with this declaration.

    This class is not meant to be instantiated by the user. It is
    returned by :attr:`TraceHandle.events`.
    """

    MAX_UINT64 = 0xFFFFFFFFFFFFFFFF

    def __init__(self):
        raise NotImplementedError("EventDeclaration cannot be instantiated")

    @property
    def name(self):
        """
        Event name, or ``None`` on error.
        """

        return nbt._bt_ctf_get_decl_event_name(self._ed)

    @property
    def id(self):
        """
        Event numeric ID, or -1 on error.
        """

        id = nbt._bt_ctf_get_decl_event_id(self._ed)

        if id == self.MAX_UINT64:
            id = -1

        return id

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

        for scope in _scopes:
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
        ret = nbt._by_python_field_decl_listcaller(self._ed, scope)

        if not isinstance(ret, list):
            return

        list_ptr, count = ret

        for i in range(count):
            field_decl_ptr = nbt._bt_python_field_decl_one_from_list(list_ptr, i)

            if field_decl_ptr is not None:
                decl_ptr = nbt._bt_ctf_get_decl_from_field_decl(field_decl_ptr)
                name = nbt._bt_ctf_get_decl_field_name(field_decl_ptr)
                field_declaration = _create_field_declaration(decl_ptr, name,
                                                              scope)
                yield field_declaration


class FieldDeclaration:
    """
    Base class for concrete field declarations.

    This class is not meant to be instantiated by the user.
    """

    def __init__(self):
        raise NotImplementedError("FieldDeclaration cannot be instantiated")

    def __repr__(self):
        return "({0}) {1} {2}".format(common.CTFScope.scope_name(self.scope),
                                      common.CTFTypeId.type_name(self.type),
                                      self.name)

    @property
    def name(self):
        """
        Field name, or ``None`` on error.
        """

        return self._name

    @property
    def type(self):
        """
        Field type (one of :class:`babeltrace.common.CTFTypeId`
        constants).
        """

        return nbt._bt_ctf_field_type(self._fd)

    @property
    def scope(self):
        """
        Field scope (one of:class:`babeltrace.common.CTFScope`
        constants).
        """

        return self._s


class IntegerFieldDeclaration(FieldDeclaration):
    """
    Integer field declaration.
    """

    def __init__(self):
        raise NotImplementedError("IntegerFieldDeclaration cannot be instantiated")

    @property
    def signedness(self):
        """
        0 if this integer is unsigned, 1 if signed, or -1 on error.
        """

        return nbt._bt_ctf_get_int_signedness(self._fd)

    @property
    def base(self):
        """
        Integer base (:class:`int`), or a negative value on error.
        """

        return nbt._bt_ctf_get_int_base(self._fd)

    @property
    def byte_order(self):
        """
        Integer byte order (one of
        :class:`babeltrace.common.ByteOrder` constants).
        """

        ret = nbt._bt_ctf_get_int_byte_order(self._fd)

        if ret == 1234:
            return common.ByteOrder.BYTE_ORDER_LITTLE_ENDIAN
        elif ret == 4321:
            return common.ByteOrder.BYTE_ORDER_BIG_ENDIAN
        else:
            return common.ByteOrder.BYTE_ORDER_UNKNOWN

    @property
    def size(self):
        """
        Integer size in bits, or a negative value on error.
        """
        return nbt._bt_ctf_get_int_len(self._fd)

    @property
    def length(self):
        return self.size

    @property
    def encoding(self):
        """
        Integer encoding (one of
        :class:`babeltrace.common.CTFStringEncoding` constants).
        """

        return nbt._bt_ctf_get_encoding(self._fd)


class EnumerationFieldDeclaration(FieldDeclaration):
    """
    Enumeration field declaration.

    .. note::

       As of this version, this class is missing some properties.
    """

    def __init__(self):
        raise NotImplementedError("EnumerationFieldDeclaration cannot be instantiated")


class ArrayFieldDeclaration(FieldDeclaration):
    """
    Static array field declaration.
    """

    def __init__(self):
        raise NotImplementedError("ArrayFieldDeclaration cannot be instantiated")

    @property
    def length(self):
        """
        Fixed length of this static array (number of contained
        elements), or a negative value on error.
        """

        return nbt._bt_ctf_get_array_len(self._fd)

    @property
    def element_declaration(self):
        """
        Field declaration of the underlying element.
        """

        field_decl_ptr = nbt._bt_python_get_array_element_declaration(self._fd)

        return _create_field_declaration(field_decl_ptr, "", self.scope)


class SequenceFieldDeclaration(FieldDeclaration):
    """
    Sequence (dynamic array) field declaration.

    .. note::

       As of this version, this class is missing some properties.
    """

    def __init__(self):
        raise NotImplementedError("SequenceFieldDeclaration cannot be instantiated")

    @property
    def element_declaration(self):
        """
        Field declaration of the underlying element.
        """

        field_decl_ptr = nbt._bt_python_get_sequence_element_declaration(self._fd)

        return _create_field_declaration(field_decl_ptr, "", self.scope)


class FloatFieldDeclaration(FieldDeclaration):
    """
    Floating point number field declaration.

    .. note::

       As of this version, this class is missing some properties.
    """

    def __init__(self):
        raise NotImplementedError("FloatFieldDeclaration cannot be instantiated")


class StructureFieldDeclaration(FieldDeclaration):
    """
    Structure (ordered map of field names to field declarations) field
    declaration.

    .. note::

       As of this version, this class is missing some properties.
    """

    def __init__(self):
        raise NotImplementedError("StructureFieldDeclaration cannot be instantiated")


class StringFieldDeclaration(FieldDeclaration):
    """
    String (NULL-terminated array of bytes) field declaration.

    .. note::

       As of this version, this class is missing some properties.
    """

    def __init__(self):
        raise NotImplementedError("StringFieldDeclaration cannot be instantiated")


class VariantFieldDeclaration(FieldDeclaration):
    """
    Variant (dynamic selection between different types) field declaration.

    .. note::

       As of this version, this class is missing some properties.
    """

    def __init__(self):
        raise NotImplementedError("VariantFieldDeclaration cannot be instantiated")


def field_error():
    """
    Return the last error code encountered while
    accessing a field and reset the error flag.
    Return 0 if no error, a negative value otherwise.
    """

    return nbt._bt_ctf_field_get_error()


def _create_field_declaration(declaration_ptr, name, scope):
    """
    Private field declaration factory.
    """

    if declaration_ptr is None:
        raise ValueError("declaration_ptr must be valid")
    if scope not in _scopes:
        raise ValueError("Invalid scope provided")

    type = nbt._bt_ctf_field_type(declaration_ptr)
    declaration = None

    if type == common.CTFTypeId.INTEGER:
        declaration = IntegerFieldDeclaration.__new__(IntegerFieldDeclaration)
    elif type == common.CTFTypeId.ENUM:
        declaration = EnumerationFieldDeclaration.__new__(EnumerationFieldDeclaration)
    elif type == common.CTFTypeId.ARRAY:
        declaration = ArrayFieldDeclaration.__new__(ArrayFieldDeclaration)
    elif type == common.CTFTypeId.SEQUENCE:
        declaration = SequenceFieldDeclaration.__new__(SequenceFieldDeclaration)
    elif type == common.CTFTypeId.FLOAT:
        declaration = FloatFieldDeclaration.__new__(FloatFieldDeclaration)
    elif type == common.CTFTypeId.STRUCT:
        declaration = StructureFieldDeclaration.__new__(StructureFieldDeclaration)
    elif type == common.CTFTypeId.STRING:
        declaration = StringFieldDeclaration.__new__(StringFieldDeclaration)
    elif type == common.CTFTypeId.VARIANT:
        declaration = VariantFieldDeclaration.__new__(VariantFieldDeclaration)
    else:
        return declaration

    declaration._fd = declaration_ptr
    declaration._s = scope
    declaration._name = name

    return declaration


class _Definition:
    def __init__(self, definition_ptr, scope):
        self._d = definition_ptr
        self._s = scope

        if scope not in _scopes:
            ValueError("Invalid scope provided")

    @property
    def name(self):
        """Return the name of a field or None on error."""

        return nbt._bt_ctf_field_name(self._d)

    @property
    def type(self):
        """Return the type of a field or -1 if unknown."""

        return nbt._bt_ctf_field_type(nbt._bt_ctf_get_decl_from_def(self._d))

    @property
    def declaration(self):
        """Return the associated Definition object."""

        return _create_field_declaration(
            nbt._bt_ctf_get_decl_from_def(self._d), self.name, self.scope)

    def _get_enum_str(self):
        """
        Return the string matching the current enumeration.
        Return None on error.
        """

        return nbt._bt_ctf_get_enum_str(self._d)

    def _get_array_element_at(self, index):
        """
        Return the array's element at position index.
        Return None on error
        """

        array_ptr = nbt._bt_python_get_array_from_def(self._d)

        if array_ptr is None:
            return None

        definition_ptr = nbt._bt_array_index(array_ptr, index)

        if definition_ptr is None:
            return None

        return _Definition(definition_ptr, self.scope)

    def _get_sequence_len(self):
        """
        Return the len of a sequence or a negative
        value on error.
        """

        seq = nbt._bt_python_get_sequence_from_def(self._d)

        return nbt._bt_sequence_len(seq)

    def _get_sequence_element_at(self, index):
        """
        Return the sequence's element at position index,
        otherwise return None
        """

        seq = nbt._bt_python_get_sequence_from_def(self._d)

        if seq is not None:
            definition_ptr = nbt._bt_sequence_index(seq, index)

            if definition_ptr is not None:
                return _Definition(definition_ptr, self.scope)

    def _get_uint64(self):
        """
        Return the value associated with the field.
        If the field does not exist or is not of the type requested,
        the value returned is undefined. To check if an error occured,
        use the field_error() function after accessing a field.
        """

        return nbt._bt_ctf_get_uint64(self._d)

    def _get_int64(self):
        """
        Return the value associated with the field.
        If the field does not exist or is not of the type requested,
        the value returned is undefined. To check if an error occured,
        use the field_error() function after accessing a field.
        """

        return nbt._bt_ctf_get_int64(self._d)

    def _get_char_array(self):
        """
        Return the value associated with the field.
        If the field does not exist or is not of the type requested,
        the value returned is undefined. To check if an error occurred,
        use the field_error() function after accessing a field.
        """

        return nbt._bt_ctf_get_char_array(self._d)

    def _get_str(self):
        """
        Return the value associated with the field.
        If the field does not exist or is not of the type requested,
        the value returned is undefined. To check if an error occurred,
        use the field_error() function after accessing a field.
        """

        return nbt._bt_ctf_get_string(self._d)

    def _get_float(self):
        """
        Return the value associated with the field.
        If the field does not exist or is not of the type requested,
        the value returned is undefined. To check if an error occurred,
        use the field_error() function after accessing a field.
        """

        return nbt._bt_ctf_get_float(self._d)

    def _get_variant(self):
        """
        Return the variant's selected field.
        If the field does not exist or is not of the type requested,
        the value returned is undefined. To check if an error occurred,
        use the field_error() function after accessing a field.
        """

        return nbt._bt_ctf_get_variant(self._d)

    def _get_struct_field_count(self):
        """
        Return the number of fields contained in the structure.
        If the field does not exist or is not of the type requested,
        the value returned is undefined.
        """

        return nbt._bt_ctf_get_struct_field_count(self._d)

    def _get_struct_field_at(self, i):
        """
        Return the structure's field at position i.
        If the field does not exist or is not of the type requested,
        the value returned is undefined. To check if an error occurred,
        use the field_error() function after accessing a field.
        """

        return nbt._bt_ctf_get_struct_field_index(self._d, i)

    @property
    def value(self):
        """
        Return the value associated with the field according to its type.
        Return None on error.
        """

        id = self.type
        value = None

        if id == common.CTFTypeId.STRING:
            value = self._get_str()
        elif id == common.CTFTypeId.ARRAY:
            element_decl = self.declaration.element_declaration

            if ((element_decl.type == common.CTFTypeId.INTEGER
                    and element_decl.length == 8)
                    and (element_decl.encoding == common.CTFStringEncoding.ASCII or element_decl.encoding == common.CTFStringEncoding.UTF8)):
                value = nbt._bt_python_get_array_string(self._d)
            else:
                value = []

                for i in range(self.declaration.length):
                    element = self._get_array_element_at(i)
                    value.append(element.value)
        elif id == common.CTFTypeId.INTEGER:
            if self.declaration.signedness == 0:
                value = self._get_uint64()
            else:
                value = self._get_int64()
        elif id == common.CTFTypeId.ENUM:
            value = self._get_enum_str()
        elif id == common.CTFTypeId.SEQUENCE:
            element_decl = self.declaration.element_declaration

            if ((element_decl.type == common.CTFTypeId.INTEGER
                    and element_decl.length == 8)
                    and (element_decl.encoding == common.CTFStringEncoding.ASCII or element_decl.encoding == common.CTFStringEncoding.UTF8)):
                value = nbt._bt_python_get_sequence_string(self._d)
            else:
                seq_len = self._get_sequence_len()
                value = []

                for i in range(seq_len):
                    evDef = self._get_sequence_element_at(i)
                    value.append(evDef.value)
        elif id == common.CTFTypeId.FLOAT:
            value = self._get_float()
        elif id == common.CTFTypeId.VARIANT:
            variant = _Definition.__new__(_Definition)
            variant._d = self._get_variant()
            value = variant.value
        elif id == common.CTFTypeId.STRUCT:
            value = {}

            for i in range(self._get_struct_field_count()):
                member = _Definition(self._get_struct_field_at(i), self.scope)
                value[member.name] = member.value

        if field_error():
            raise FieldError(
                "Error occurred while accessing field {} of type {}".format(
                    self.name,
                    common.CTFTypeId.type_name(id)))

        return value

    @property
    def scope(self):
        """Return the scope of a field or None on error."""

        return self._s
