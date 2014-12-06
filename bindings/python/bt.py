import babeltrace.nativebt as nbt
import collections
import os
from datetime import datetime
from uuid import UUID


class TraceCollection:
    """
    The TraceCollection is the object that contains all currently opened traces.
    """

    def __init__(self):
        self._tc = nbt._bt_context_create()

    def __del__(self):
        nbt._bt_context_put(self._tc)

    def add_trace(self, path, format_str):
        """
        Add a trace by path to the TraceCollection.

        Open a trace.

        path is the path to the trace, it is not recursive.
        If "path" is None, stream_list is used instead as a list
        of mmap streams to open for the trace.

        format is a string containing the format name in which the trace was
        produced.

        Return: the corresponding TraceHandle on success or None on error.
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
        Open a trace recursively.

        Find each trace present in the subdirectory starting from the given
        path, and add them to the TraceCollection.

        Return a dict of TraceHandle instances (the full path is the key).
        Return None on error.
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
        Remove a trace from the TraceCollection.
        Effectively closing the trace.
        """

        try:
            nbt._bt_context_remove_trace(self._tc, trace_handle._id)
        except AttributeError:
            raise TypeError("in remove_trace, argument 2 must be a TraceHandle instance")

    @property
    def events(self):
        """
        Generator function to iterate over the events of open in the current
        TraceCollection.

        Due to limitations of the native Babeltrace API, only one event
        may be "alive" at a time (i.e. a user should never store a copy
        of the events returned by this function for ulterior use). Users
        shall make sure to copy the information they need from an event
        before accessing the next one.

        Furthermore, event objects become invalid when the generator goes
        out of scope as the underlying iterator will be reclaimed. Using an
        event after the the generator has gone out of scope may result in a
        crash or data corruption.
        """

        begin_pos_ptr = nbt._bt_iter_pos()
        end_pos_ptr = nbt._bt_iter_pos()
        begin_pos_ptr.type = nbt.SEEK_BEGIN
        end_pos_ptr.type = nbt.SEEK_LAST

        for event in self._events(begin_pos_ptr, end_pos_ptr):
            yield event

    def events_timestamps(self, timestamp_begin, timestamp_end):
        """
        Generator function to iterate over the events of open in the current
        TraceCollection from timestamp_begin to timestamp_end.
        """

        begin_pos_ptr = nbt._bt_iter_pos()
        end_pos_ptr = nbt._bt_iter_pos()
        begin_pos_ptr.type = end_pos_ptr.type = nbt.SEEK_TIME
        begin_pos_ptr.u.seek_time = timestamp_begin
        end_pos_ptr.u.seek_time = timestamp_end

        for event in self._events(begin_pos_ptr, end_pos_ptr):
            yield event

    @property
    def timestamp_begin(self):
        pos_ptr = nbt._bt_iter_pos()
        pos_ptr.type = nbt.SEEK_BEGIN

        return self._timestamp_at_pos(pos_ptr)

    @property
    def timestamp_end(self):
        pos_ptr = nbt._bt_iter_pos()
        pos_ptr.type = nbt.SEEK_LAST

        return self._timestamp_at_pos(pos_ptr)

    def _timestamp_at_pos(self, pos_ptr):
        ctf_it_ptr = nbt._bt_ctf_iter_create(self._tc, pos_ptr, pos_ptr)

        if ctf_it_ptr is None:
            raise NotImplementedError("Creation of multiple iterators is unsupported.")

        ev_ptr = nbt._bt_ctf_iter_read_event(ctf_it_ptr)
        nbt._bt_ctf_iter_destroy(ctf_it_ptr)

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


def print_format_list(babeltrace_file):
    """
    Print a list of available formats to file.

    babeltrace_file must be a File instance opened in write mode.
    """

    try:
        if babeltrace_file._file is not None:
            nbt._bt_print_format_list(babeltrace_file._file)
    except AttributeError:
        raise TypeError("in print_format_list, argument 1 must be a File instance")


# Based on enum bt_clock_type in clock-type.h
class _ClockType:
    CLOCK_CYCLES = 0
    CLOCK_REAL = 1


class TraceHandle:
    """
    The TraceHandle allows the user to manipulate a trace file directly.
    It is a unique identifier representing a trace file.
    Do not instantiate.
    """

    def __init__(self):
        raise NotImplementedError("TraceHandle cannot be instantiated")

    def __repr__(self):
        return "Babeltrace TraceHandle: trace_id('{0}')".format(self._id)

    @property
    def id(self):
        """Return the TraceHandle id."""

        return self._id

    @property
    def path(self):
        """Return the path of a TraceHandle."""

        return nbt._bt_trace_handle_get_path(self._trace_collection._tc,
                                             self._id)

    @property
    def timestamp_begin(self):
        """Return the creation time of the buffers of a trace."""

        return nbt._bt_trace_handle_get_timestamp_begin(self._trace_collection._tc,
                                                        self._id,
                                                        _ClockType.CLOCK_REAL)

    @property
    def timestamp_end(self):
        """Return the destruction timestamp of the buffers of a trace."""

        return nbt._bt_trace_handle_get_timestamp_end(self._trace_collection._tc,
                                                      self._id,
                                                      _ClockType.CLOCK_REAL)

    @property
    def events(self):
        """
        Generator returning all events (EventDeclaration) in a trace.
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


class CTFStringEncoding:
    NONE = 0
    UTF8 = 1
    ASCII = 2
    UNKNOWN = 3


# Based on the enum in ctf-writer/writer.h
class ByteOrder:
    BYTE_ORDER_NATIVE = 0
    BYTE_ORDER_LITTLE_ENDIAN = 1
    BYTE_ORDER_BIG_ENDIAN = 2
    BYTE_ORDER_NETWORK = 3
    BYTE_ORDER_UNKNOWN = 4  # Python-specific entry


# enum equivalent, accessible constants
# These are taken directly from ctf/events.h
# All changes to enums must also be made here
class CTFTypeId:
    UNKNOWN = 0
    INTEGER = 1
    FLOAT = 2
    ENUM = 3
    STRING = 4
    STRUCT = 5
    UNTAGGED_VARIANT = 6
    VARIANT = 7
    ARRAY = 8
    SEQUENCE = 9
    NR_CTF_TYPES = 10

    def type_name(id):
        name = "UNKNOWN_TYPE"
        constants = [
            attr for attr in dir(CTFTypeId) if not callable(
                getattr(
                    CTFTypeId,
                    attr)) and not attr.startswith("__")]

        for attr in constants:
            if getattr(CTFTypeId, attr) == id:
                name = attr
                break

        return name


class CTFScope:
    TRACE_PACKET_HEADER = 0
    STREAM_PACKET_CONTEXT = 1
    STREAM_EVENT_HEADER = 2
    STREAM_EVENT_CONTEXT = 3
    EVENT_CONTEXT = 4
    EVENT_FIELDS = 5

    def scope_name(scope):
        name = "UNKNOWN_SCOPE"
        constants = [
            attr for attr in dir(CTFScope) if not callable(
                getattr(
                    CTFScope,
                    attr)) and not attr.startswith("__")]

        for attr in constants:
            if getattr(CTFScope, attr) == scope:
                name = attr
                break

        return name


# Priority of the scopes when searching for event fields
_scopes = [
    CTFScope.EVENT_FIELDS,
    CTFScope.EVENT_CONTEXT,
    CTFScope.STREAM_EVENT_CONTEXT,
    CTFScope.STREAM_EVENT_HEADER,
    CTFScope.STREAM_PACKET_CONTEXT,
    CTFScope.TRACE_PACKET_HEADER
]


class Event(collections.Mapping):
    """
    This class represents an event from the trace.
    It is obtained using the TraceCollection generator functions.
    Do not instantiate.
    """

    def __init__(self):
        raise NotImplementedError("Event cannot be instantiated")

    @property
    def name(self):
        """Return the name of the event or None on error."""

        return nbt._bt_ctf_event_name(self._e)

    @property
    def cycles(self):
        """
        Return the timestamp of the event as written in
        the packet (in cycles) or -1ULL on error.
        """

        return nbt._bt_ctf_get_cycles(self._e)

    @property
    def timestamp(self):
        """
        Return the timestamp of the event offset with the
        system clock source or -1ULL on error.
        """

        return nbt._bt_ctf_get_timestamp(self._e)

    @property
    def datetime(self):
        """
        Return a datetime object based on the event's
        timestamp. Note that the datetime class' precision
        is limited to microseconds.
        """

        return datetime.fromtimestamp(self.timestamp / 1E9)

    def field_with_scope(self, field_name, scope):
        """
        Get field_name's value in scope.
        None is returned if no field matches field_name.
        """

        if scope not in _scopes:
            raise ValueError("Invalid scope provided")

        field = self._field_with_scope(field_name, scope)

        if field is not None:
            return field.value

    def field_list_with_scope(self, scope):
        """Return a list of field names in scope."""

        if scope not in _scopes:
            raise ValueError("Invalid scope provided")

        field_names = []

        for field in self._field_list_with_scope(scope):
            field_names.append(field.name)

        return field_names

    @property
    def handle(self):
        """
        Get the TraceHandle associated with this event
        Return None on error
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
        Get the TraceCollection associated with this event.
        Return None on error.
        """

        trace_collection = TraceCollection()
        trace_collection._tc = nbt._bt_ctf_event_get_context(self._e)

        if trace_collection._tc is not None:
            return trace_collection

    def __getitem__(self, field_name):
        """
        Get field_name's value. If the field_name exists in multiple
        scopes, the first field found is returned. The scopes are searched
        in the following order:
        1) EVENT_FIELDS
        2) EVENT_CONTEXT
        3) STREAM_EVENT_CONTEXT
        4) STREAM_EVENT_HEADER
        5) STREAM_PACKET_CONTEXT
        6) TRACE_PACKET_HEADER
        None is returned if no field matches field_name.

        Use field_with_scope() to explicitly access fields in a given
        scope.
        """

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
        """Return a list of field names."""

        field_names = set()

        for scope in _scopes:
            for name in self.field_list_with_scope(scope):
                field_names.add(name)

        return list(field_names)

    def get(self, field_name, default=None):
        field = self._field(field_name)

        if field is None:
            return default

        return field.value

    def items(self):
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
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return repr(self.value)


class EventDeclaration:
    """Event declaration class.  Do not instantiate."""

    MAX_UINT64 = 0xFFFFFFFFFFFFFFFF

    def __init__(self):
        raise NotImplementedError("EventDeclaration cannot be instantiated")

    @property
    def name(self):
        """Return the name of the event or None on error"""

        return nbt._bt_ctf_get_decl_event_name(self._ed)

    @property
    def id(self):
        """Return the event-ID of the event or -1 on error"""

        id = nbt._bt_ctf_get_decl_event_id(self._ed)

        if id == self.MAX_UINT64:
            id = -1

        return id

    @property
    def fields(self):
        """
        Generator returning all FieldDeclarations of an event, going through
        each scope in the following order:
        1) EVENT_FIELDS
        2) EVENT_CONTEXT
        3) STREAM_EVENT_CONTEXT
        4) STREAM_EVENT_HEADER
        5) STREAM_PACKET_CONTEXT
        6) TRACE_PACKET_HEADER
        """

        for scope in _scopes:
            for declaration in self.fields_scope(scope):
                yield declaration

    def fields_scope(self, scope):
        """
        Generator returning FieldDeclarations of the current event in scope.
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
    """Field declaration class. Do not instantiate."""

    def __init__(self):
        raise NotImplementedError("FieldDeclaration cannot be instantiated")

    def __repr__(self):
        return "({0}) {1} {2}".format(CTFScope.scope_name(self.scope),
                                      CTFTypeId.type_name(self.type),
                                      self.name)

    @property
    def name(self):
        """Return the name of a FieldDeclaration or None on error."""

        return self._name

    @property
    def type(self):
        """
        Return the FieldDeclaration's type. One of the entries in class
        CTFTypeId.
        """

        return nbt._bt_ctf_field_type(self._fd)

    @property
    def scope(self):
        """
        Return the FieldDeclaration's scope.
        """

        return self._s


class IntegerFieldDeclaration(FieldDeclaration):
    """Do not instantiate."""

    def __init__(self):
        raise NotImplementedError("IntegerFieldDeclaration cannot be instantiated")

    @property
    def signedness(self):
        """
        Return the signedness of an integer:
        0 if unsigned; 1 if signed; -1 on error.
        """

        return nbt._bt_ctf_get_int_signedness(self._fd)

    @property
    def base(self):
        """Return the base of an int or a negative value on error."""

        return nbt._bt_ctf_get_int_base(self._fd)

    @property
    def byte_order(self):
        """
        Return the byte order. One of class ByteOrder's entries.
        """

        ret = nbt._bt_ctf_get_int_byte_order(self._fd)

        if ret == 1234:
            return ByteOrder.BYTE_ORDER_LITTLE_ENDIAN
        elif ret == 4321:
            return ByteOrder.BYTE_ORDER_BIG_ENDIAN
        else:
            return ByteOrder.BYTE_ORDER_UNKNOWN

    @property
    def length(self):
        """
        Return the size, in bits, of an int or a negative
        value on error.
        """

        return nbt._bt_ctf_get_int_len(self._fd)

    @property
    def encoding(self):
        """
        Return the encoding. One of class CTFStringEncoding's entries.
        Return a negative value on error.
        """

        return nbt._bt_ctf_get_encoding(self._fd)


class EnumerationFieldDeclaration(FieldDeclaration):
    """Do not instantiate."""

    def __init__(self):
        raise NotImplementedError("EnumerationFieldDeclaration cannot be instantiated")


class ArrayFieldDeclaration(FieldDeclaration):
    """Do not instantiate."""

    def __init__(self):
        raise NotImplementedError("ArrayFieldDeclaration cannot be instantiated")

    @property
    def length(self):
        """
        Return the length of an array or a negative
        value on error.
        """

        return nbt._bt_ctf_get_array_len(self._fd)

    @property
    def element_declaration(self):
        """
        Return element declaration.
        """

        field_decl_ptr = nbt._bt_python_get_array_element_declaration(self._fd)

        return _create_field_declaration(field_decl_ptr, "", self.scope)


class SequenceFieldDeclaration(FieldDeclaration):
    """Do not instantiate."""

    def __init__(self):
        raise NotImplementedError("SequenceFieldDeclaration cannot be instantiated")

    @property
    def element_declaration(self):
        """
        Return element declaration.
        """

        field_decl_ptr = nbt._bt_python_get_sequence_element_declaration(self._fd)

        return _create_field_declaration(field_decl_ptr, "", self.scope)


class FloatFieldDeclaration(FieldDeclaration):
    """Do not instantiate."""

    def __init__(self):
        raise NotImplementedError("FloatFieldDeclaration cannot be instantiated")


class StructureFieldDeclaration(FieldDeclaration):
    """Do not instantiate."""

    def __init__(self):
        raise NotImplementedError("StructureFieldDeclaration cannot be instantiated")


class StringFieldDeclaration(FieldDeclaration):
    """Do not instantiate."""

    def __init__(self):
        raise NotImplementedError("StringFieldDeclaration cannot be instantiated")


class VariantFieldDeclaration(FieldDeclaration):
    """Do not instantiate."""

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

    if type == CTFTypeId.INTEGER:
        declaration = IntegerFieldDeclaration.__new__(IntegerFieldDeclaration)
    elif type == CTFTypeId.ENUM:
        declaration = EnumerationFieldDeclaration.__new__(EnumerationFieldDeclaration)
    elif type == CTFTypeId.ARRAY:
        declaration = ArrayFieldDeclaration.__new__(ArrayFieldDeclaration)
    elif type == CTFTypeId.SEQUENCE:
        declaration = SequenceFieldDeclaration.__new__(SequenceFieldDeclaration)
    elif type == CTFTypeId.FLOAT:
        declaration = FloatFieldDeclaration.__new__(FloatFieldDeclaration)
    elif type == CTFTypeId.STRUCT:
        declaration = StructureFieldDeclaration.__new__(StructureFieldDeclaration)
    elif type == CTFTypeId.STRING:
        declaration = StringFieldDeclaration.__new__(StringFieldDeclaration)
    elif type == CTFTypeId.VARIANT:
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

        if id == CTFTypeId.STRING:
            value = self._get_str()
        elif id == CTFTypeId.ARRAY:
            element_decl = self.declaration.element_declaration

            if ((element_decl.type == CTFTypeId.INTEGER
                    and element_decl.length == 8)
                    and (element_decl.encoding == CTFStringEncoding.ASCII or element_decl.encoding == CTFStringEncoding.UTF8)):
                value = nbt._bt_python_get_array_string(self._d)
            else:
                value = []

                for i in range(self.declaration.length):
                    element = self._get_array_element_at(i)
                    value.append(element.value)
        elif id == CTFTypeId.INTEGER:
            if self.declaration.signedness == 0:
                value = self._get_uint64()
            else:
                value = self._get_int64()
        elif id == CTFTypeId.ENUM:
            value = self._get_enum_str()
        elif id == CTFTypeId.SEQUENCE:
            element_decl = self.declaration.element_declaration

            if ((element_decl.type == CTFTypeId.INTEGER
                    and element_decl.length == 8)
                    and (element_decl.encoding == CTFStringEncoding.ASCII or element_decl.encoding == CTFStringEncoding.UTF8)):
                value = nbt._bt_python_get_sequence_string(self._d)
            else:
                seq_len = self._get_sequence_len()
                value = []

                for i in range(seq_len):
                    evDef = self._get_sequence_element_at(i)
                    value.append(evDef.value)
        elif id == CTFTypeId.FLOAT:
            value = self._get_float()
        elif id == CTFTypeId.VARIANT:
            variant = _Definition.__new__(_Definition)
            variant._d = self._get_variant()
            value = variant.value
        elif id == CTFTypeId.STRUCT:
            value = {}

            for i in range(self._get_struct_field_count()):
                member = _Definition(self._get_struct_field_at(i), self.scope)
                value[member.name] = member.value

        if field_error():
            raise FieldError(
                "Error occurred while accessing field {} of type {}".format(
                    self.name,
                    CTFTypeId.type_name(id)))

        return value

    @property
    def scope(self):
        """Return the scope of a field or None on error."""

        return self._s


class CTFWriter:
    # Used to compare to -1ULL in error checks
    _MAX_UINT64 = 0xFFFFFFFFFFFFFFFF

    class EnumerationMapping:
        """
        Enumeration mapping class. start and end values are inclusive.
        """

        def __init__(self, name, start, end):
            self.name = name
            self.start = start
            self.end = end

    class Clock:
        def __init__(self, name):
            self._c = nbt._bt_ctf_clock_create(name)

            if self._c is None:
                raise ValueError("Invalid clock name.")

        def __del__(self):
            nbt._bt_ctf_clock_put(self._c)

        @property
        def name(self):
            """
            Get the clock's name.
            """

            name = nbt._bt_ctf_clock_get_name(self._c)

            if name is None:
                raise ValueError("Invalid clock instance.")

            return name

        @property
        def description(self):
            """
            Get the clock's description. None if unset.
            """

            return nbt._bt_ctf_clock_get_description(self._c)

        @description.setter
        def description(self, desc):
            """
            Set the clock's description. The description appears in the clock's TSDL
            meta-data.
            """

            ret = nbt._bt_ctf_clock_set_description(self._c, str(desc))

            if ret < 0:
                raise ValueError("Invalid clock description.")

        @property
        def frequency(self):
            """
            Get the clock's frequency (Hz).
            """

            freq = nbt._bt_ctf_clock_get_frequency(self._c)

            if freq == CTFWriter._MAX_UINT64:
                raise ValueError("Invalid clock instance")

            return freq

        @frequency.setter
        def frequency(self, freq):
            """
            Set the clock's frequency (Hz).
            """

            ret = nbt._bt_ctf_clock_set_frequency(self._c, freq)

            if ret < 0:
                raise ValueError("Invalid frequency value.")

        @property
        def precision(self):
            """
            Get the clock's precision (in clock ticks).
            """

            precision = nbt._bt_ctf_clock_get_precision(self._c)

            if precision == CTFWriter._MAX_UINT64:
                raise ValueError("Invalid clock instance")

            return precision

        @precision.setter
        def precision(self, precision):
            """
            Set the clock's precision (in clock ticks).
            """

            ret = nbt._bt_ctf_clock_set_precision(self._c, precision)

        @property
        def offset_seconds(self):
            """
            Get the clock's offset in seconds from POSIX.1 Epoch.
            """

            offset_s = nbt._bt_ctf_clock_get_offset_s(self._c)

            if offset_s == CTFWriter._MAX_UINT64:
                raise ValueError("Invalid clock instance")

            return offset_s

        @offset_seconds.setter
        def offset_seconds(self, offset_s):
            """
            Set the clock's offset in seconds from POSIX.1 Epoch.
            """

            ret = nbt._bt_ctf_clock_set_offset_s(self._c, offset_s)

            if ret < 0:
                raise ValueError("Invalid offset value.")

        @property
        def offset(self):
            """
            Get the clock's offset in ticks from POSIX.1 Epoch + offset in seconds.
            """

            offset = nbt._bt_ctf_clock_get_offset(self._c)

            if offset == CTFWriter._MAX_UINT64:
                raise ValueError("Invalid clock instance")

            return offset

        @offset.setter
        def offset(self, offset):
            """
            Set the clock's offset in ticks from POSIX.1 Epoch + offset in seconds.
            """

            ret = nbt._bt_ctf_clock_set_offset(self._c, offset)

            if ret < 0:
                raise ValueError("Invalid offset value.")

        @property
        def absolute(self):
            """
            Get a clock's absolute attribute. A clock is absolute if the clock
            is a global reference across the trace's other clocks.
            """

            is_absolute = nbt._bt_ctf_clock_get_is_absolute(self._c)

            if is_absolute == -1:
                raise ValueError("Invalid clock instance")

            return False if is_absolute == 0 else True

        @absolute.setter
        def absolute(self, is_absolute):
            """
            Set a clock's absolute attribute. A clock is absolute if the clock
            is a global reference across the trace's other clocks.
            """

            ret = nbt._bt_ctf_clock_set_is_absolute(self._c, int(is_absolute))

            if ret < 0:
                raise ValueError("Could not set the clock's absolute attribute.")

        @property
        def uuid(self):
            """
            Get a clock's UUID (an object of type UUID).
            """

            uuid_list = []

            for i in range(16):
                ret, value = nbt._bt_python_ctf_clock_get_uuid_index(self._c, i)

                if ret < 0:
                    raise ValueError("Invalid clock instance")

                uuid_list.append(value)

            return UUID(bytes=bytes(uuid_list))

        @uuid.setter
        def uuid(self, uuid):
            """
            Set a clock's UUID (an object of type UUID).
            """

            uuid_bytes = uuid.bytes

            if len(uuid_bytes) != 16:
                raise ValueError("Invalid UUID provided. UUID length must be 16 bytes")

            for i in range(len(uuid_bytes)):
                ret = nbt._bt_python_ctf_clock_set_uuid_index(self._c, i,
                                                              uuid_bytes[i])

                if ret < 0:
                    raise ValueError("Invalid clock instance")

        @property
        def time(self):
            """
            Get the current time in nanoseconds since the clock's origin (offset and
            offset_s attributes).
            """

            time = nbt._bt_ctf_clock_get_time(self._c)

            if time == CTFWriter._MAX_UINT64:
                raise ValueError("Invalid clock instance")

            return time

        @time.setter
        def time(self, time):
            """
            Set the current time in nanoseconds since the clock's origin (offset and
            offset_s attributes). The clock's value will be sampled as events are
            appended to a stream.
            """

            ret = nbt._bt_ctf_clock_set_time(self._c, time)

            if ret < 0:
                raise ValueError("Invalid time value.")

    class FieldDeclaration:
        """
        FieldDeclaration should not be instantiated directly. Instantiate
        one of the concrete FieldDeclaration classes.
        """

        class IntegerBase:
            # These values are based on the bt_ctf_integer_base enum
            # declared in event-types.h.
            INTEGER_BASE_UNKNOWN = -1
            INTEGER_BASE_BINARY = 2
            INTEGER_BASE_OCTAL = 8
            INTEGER_BASE_DECIMAL = 10
            INTEGER_BASE_HEXADECIMAL = 16

        def __init__(self):
            if self._ft is None:
                raise ValueError("FieldDeclaration creation failed.")

        def __del__(self):
            nbt._bt_ctf_field_type_put(self._ft)

        @staticmethod
        def _create_field_declaration_from_native_instance(
                native_field_declaration):
            type_dict = {
                CTFTypeId.INTEGER: CTFWriter.IntegerFieldDeclaration,
                CTFTypeId.FLOAT: CTFWriter.FloatFieldDeclaration,
                CTFTypeId.ENUM: CTFWriter.EnumerationFieldDeclaration,
                CTFTypeId.STRING: CTFWriter.StringFieldDeclaration,
                CTFTypeId.STRUCT: CTFWriter.StructureFieldDeclaration,
                CTFTypeId.VARIANT: CTFWriter.VariantFieldDeclaration,
                CTFTypeId.ARRAY: CTFWriter.ArrayFieldDeclaration,
                CTFTypeId.SEQUENCE: CTFWriter.SequenceFieldDeclaration
            }

            field_type_id = nbt._bt_ctf_field_type_get_type_id(native_field_declaration)

            if field_type_id == CTFTypeId.UNKNOWN:
                raise TypeError("Invalid field instance")

            declaration = CTFWriter.Field.__new__(CTFWriter.Field)
            declaration._ft = native_field_declaration
            declaration.__class__ = type_dict[field_type_id]

            return declaration

        @property
        def alignment(self):
            """
            Get the field declaration's alignment. Returns -1 on error.
            """

            return nbt._bt_ctf_field_type_get_alignment(self._ft)

        @alignment.setter
        def alignment(self, alignment):
            """
            Set the field declaration's alignment. Defaults to 1 (bit-aligned). However,
            some types, such as structures and string, may impose other alignment
            constraints.
            """

            ret = nbt._bt_ctf_field_type_set_alignment(self._ft, alignment)

            if ret < 0:
                raise ValueError("Invalid alignment value.")

        @property
        def byte_order(self):
            """
            Get the field declaration's byte order. One of the ByteOrder's constant.
            """

            return nbt._bt_ctf_field_type_get_byte_order(self._ft)

        @byte_order.setter
        def byte_order(self, byte_order):
            """
            Set the field declaration's byte order. Use constants defined in the ByteOrder
            class.
            """

            ret = nbt._bt_ctf_field_type_set_byte_order(self._ft, byte_order)

            if ret < 0:
                raise ValueError("Could not set byte order value.")

    class IntegerFieldDeclaration(FieldDeclaration):
        def __init__(self, size):
            """
            Create a new integer field declaration of the given size.
            """
            self._ft = nbt._bt_ctf_field_type_integer_create(size)
            super().__init__()

        @property
        def size(self):
            """
            Get an integer's size.
            """

            ret = nbt._bt_ctf_field_type_integer_get_size(self._ft)

            if ret < 0:
                raise ValueError("Could not get Integer's size attribute.")
            else:
                return ret

        @property
        def signed(self):
            """
            Get an integer's signedness attribute.
            """

            ret = nbt._bt_ctf_field_type_integer_get_signed(self._ft)

            if ret < 0:
                raise ValueError("Could not get Integer's signed attribute.")
            elif ret > 0:
                return True
            else:
                return False

        @signed.setter
        def signed(self, signed):
            """
            Set an integer's signedness attribute.
            """

            ret = nbt._bt_ctf_field_type_integer_set_signed(self._ft, signed)

            if ret < 0:
                raise ValueError("Could not set Integer's signed attribute.")

        @property
        def base(self):
            """
            Get the integer's base used to pretty-print the resulting trace.
            Returns a constant from the FieldDeclaration.IntegerBase class.
            """

            return nbt._bt_ctf_field_type_integer_get_base(self._ft)

        @base.setter
        def base(self, base):
            """
            Set the integer's base used to pretty-print the resulting trace.
            The base must be a constant of the FieldDeclarationIntegerBase class.
            """

            ret = nbt._bt_ctf_field_type_integer_set_base(self._ft, base)

            if ret < 0:
                raise ValueError("Could not set Integer's base.")

        @property
        def encoding(self):
            """
            Get the integer's encoding (one of the constants of the
            CTFStringEncoding class).
            Returns a constant from the CTFStringEncoding class.
            """

            return nbt._bt_ctf_field_type_integer_get_encoding(self._ft)

        @encoding.setter
        def encoding(self, encoding):
            """
            An integer encoding may be set to signal that the integer must be printed
            as a text character. Must be a constant from the CTFStringEncoding class.
            """

            ret = nbt._bt_ctf_field_type_integer_set_encoding(self._ft, encoding)

            if ret < 0:
                raise ValueError("Could not set Integer's encoding.")

    class EnumerationFieldDeclaration(FieldDeclaration):
        def __init__(self, integer_type):
            """
            Create a new enumeration field declaration with the given underlying container type.
            """
            isinst = isinstance(integer_type, CTFWriter.IntegerFieldDeclaration)

            if integer_type is None or not isinst:
                raise TypeError("Invalid integer container.")

            self._ft = nbt._bt_ctf_field_type_enumeration_create(integer_type._ft)
            super().__init__()

        @property
        def container(self):
            """
            Get the enumeration's underlying container type.
            """

            ret = nbt._bt_ctf_field_type_enumeration_get_container_type(self._ft)

            if ret is None:
                raise TypeError("Invalid enumeration declaration")

            return CTFWriter.FieldDeclaration._create_field_declaration_from_native_instance(ret)

        def add_mapping(self, name, range_start, range_end):
            """
            Add a mapping to the enumeration. The range's values are inclusive.
            """

            if range_start < 0 or range_end < 0:
                ret = nbt._bt_ctf_field_type_enumeration_add_mapping(self._ft,
                                                                     str(name),
                                                                     range_start,
                                                                     range_end)
            else:
                ret = nbt._bt_ctf_field_type_enumeration_add_mapping_unsigned(self._ft,
                                                                              str(name),
                                                                              range_start,
                                                                              range_end)

            if ret < 0:
                raise ValueError("Could not add mapping to enumeration declaration.")

        @property
        def mappings(self):
            """
            Generator returning instances of EnumerationMapping.
            """

            signed = self.container.signed

            count = nbt._bt_ctf_field_type_enumeration_get_mapping_count(self._ft)

            for i in range(count):
                if signed:
                    ret = nbt._bt_python_ctf_field_type_enumeration_get_mapping(self._ft, i)
                else:
                    ret = nbt._bt_python_ctf_field_type_enumeration_get_mapping_unsigned(self._ft, i)

                if len(ret) != 3:
                    msg = "Could not get Enumeration mapping at index {}".format(i)
                    raise TypeError(msg)

                name, range_start, range_end = ret
                yield CTFWriter.EnumerationMapping(name, range_start, range_end)

        def get_mapping_by_name(self, name):
            """
            Get a mapping by name (EnumerationMapping).
            """

            index = nbt._bt_ctf_field_type_enumeration_get_mapping_index_by_name(self._ft, name)

            if index < 0:
                return None

            if self.container.signed:
                ret = nbt._bt_python_ctf_field_type_enumeration_get_mapping(self._ft, index)
            else:
                ret = nbt._bt_python_ctf_field_type_enumeration_get_mapping_unsigned(self._ft, index)

            if len(ret) != 3:
                msg = "Could not get Enumeration mapping at index {}".format(i)
                raise TypeError(msg)

            name, range_start, range_end = ret

            return CTFWriter.EnumerationMapping(name, range_start, range_end)

        def get_mapping_by_value(self, value):
            """
            Get a mapping by value (EnumerationMapping).
            """

            if value < 0:
                index = nbt._bt_ctf_field_type_enumeration_get_mapping_index_by_value(self._ft, value)
            else:
                index = nbt._bt_ctf_field_type_enumeration_get_mapping_index_by_unsigned_value(self._ft, value)

            if index < 0:
                return None

            if self.container.signed:
                ret = nbt._bt_python_ctf_field_type_enumeration_get_mapping(self._ft, index)
            else:
                ret = nbt._bt_python_ctf_field_type_enumeration_get_mapping_unsigned(self._ft, index)

            if len(ret) != 3:
                msg = "Could not get Enumeration mapping at index {}".format(i)
                raise TypeError(msg)

            name, range_start, range_end = ret

            return CTFWriter.EnumerationMapping(name, range_start, range_end)

    class FloatFieldDeclaration(FieldDeclaration):
        FLT_EXP_DIG = 8
        DBL_EXP_DIG = 11
        FLT_MANT_DIG = 24
        DBL_MANT_DIG = 53

        def __init__(self):
            """
            Create a new floating point field declaration.
            """

            self._ft = nbt._bt_ctf_field_type_floating_point_create()
            super().__init__()

        @property
        def exponent_digits(self):
            """
            Get the number of exponent digits used to store the floating point field.
            """

            ret = nbt._bt_ctf_field_type_floating_point_get_exponent_digits(self._ft)

            if ret < 0:
                raise TypeError(
                    "Could not get Floating point exponent digit count")

            return ret

        @exponent_digits.setter
        def exponent_digits(self, exponent_digits):
            """
            Set the number of exponent digits to use to store the floating point field.
            The only values currently supported are FLT_EXP_DIG and DBL_EXP_DIG which
            are defined as constants of this class.
            """

            ret = nbt._bt_ctf_field_type_floating_point_set_exponent_digits(self._ft,
                                                                            exponent_digits)

            if ret < 0:
                raise ValueError("Could not set exponent digit count.")

        @property
        def mantissa_digits(self):
            """
            Get the number of mantissa digits used to store the floating point field.
            """

            ret = nbt._bt_ctf_field_type_floating_point_get_mantissa_digits(self._ft)

            if ret < 0:
                raise TypeError("Could not get Floating point mantissa digit count")

            return ret

        @mantissa_digits.setter
        def mantissa_digits(self, mantissa_digits):
            """
            Set the number of mantissa digits to use to store the floating point field.
            The only values currently supported are FLT_MANT_DIG and DBL_MANT_DIG which
            are defined as constants of this class.
            """

            ret = nbt._bt_ctf_field_type_floating_point_set_mantissa_digits(self._ft,
                                                                            mantissa_digits)

            if ret < 0:
                raise ValueError("Could not set mantissa digit count.")

    class StructureFieldDeclaration(FieldDeclaration):
        def __init__(self):
            """
            Create a new structure field declaration.
            """

            self._ft = nbt._bt_ctf_field_type_structure_create()
            super().__init__()

        def add_field(self, field_type, field_name):
            """
            Add a field of type "field_type" to the structure.
            """

            ret = nbt._bt_ctf_field_type_structure_add_field(self._ft,
                                                             field_type._ft,
                                                             str(field_name))

            if ret < 0:
                raise ValueError("Could not add field to structure.")

        @property
        def fields(self):
            """
            Generator returning the structure's field as tuples of (field name, field declaration).
            """

            count = nbt._bt_ctf_field_type_structure_get_field_count(self._ft)

            if count < 0:
                raise TypeError("Could not get Structure field count")

            for i in range(count):
                field_name = nbt._bt_python_ctf_field_type_structure_get_field_name(self._ft, i)

                if field_name is None:
                    msg = "Could not get Structure field name at index {}".format(i)
                    raise TypeError(msg)

                field_type_native = nbt._bt_python_ctf_field_type_structure_get_field_type(self._ft, i)

                if field_type_native is None:
                    msg = "Could not get Structure field type at index {}".format(i)
                    raise TypeError(msg)

                field_type = CTFWriter.FieldDeclaration._create_field_declaration_from_native_instance(field_type_native)
                yield (field_name, field_type)

        def get_field_by_name(self, name):
            """
            Get a field declaration by name (FieldDeclaration).
            """

            field_type_native = nbt._bt_ctf_field_type_structure_get_field_type_by_name(self._ft, name)

            if field_type_native is None:
                msg = "Could not find Structure field with name {}".format(name)
                raise TypeError(msg)

            return CTFWriter.FieldDeclaration._create_field_declaration_from_native_instance(field_type_native)

    class VariantFieldDeclaration(FieldDeclaration):
        def __init__(self, enum_tag, tag_name):
            """
            Create a new variant field declaration.
            """

            isinst = isinstance(enum_tag, CTFWriter.EnumerationFieldDeclaration)
            if enum_tag is None or not isinst:
                raise TypeError("Invalid tag type; must be of type EnumerationFieldDeclaration.")

            self._ft = nbt._bt_ctf_field_type_variant_create(enum_tag._ft,
                                                             str(tag_name))
            super().__init__()

        @property
        def tag_name(self):
            """
            Get the variant's tag name.
            """

            ret = nbt._bt_ctf_field_type_variant_get_tag_name(self._ft)

            if ret is None:
                raise TypeError("Could not get Variant tag name")

            return ret

        @property
        def tag_type(self):
            """
            Get the variant's tag type.
            """

            ret = nbt._bt_ctf_field_type_variant_get_tag_type(self._ft)

            if ret is None:
                raise TypeError("Could not get Variant tag type")

            return CTFWriter.FieldDeclaration._create_field_declaration_from_native_instance(ret)

        def add_field(self, field_type, field_name):
            """
            Add a field of type "field_type" to the variant.
            """

            ret = nbt._bt_ctf_field_type_variant_add_field(self._ft,
                                                           field_type._ft,
                                                           str(field_name))

            if ret < 0:
                raise ValueError("Could not add field to variant.")

        @property
        def fields(self):
            """
            Generator returning the variant's field as tuples of (field name, field declaration).
            """

            count = nbt._bt_ctf_field_type_variant_get_field_count(self._ft)

            if count < 0:
                raise TypeError("Could not get Variant field count")

            for i in range(count):
                field_name = nbt._bt_python_ctf_field_type_variant_get_field_name(self._ft, i)

                if field_name is None:
                    msg = "Could not get Variant field name at index {}".format(i)
                    raise TypeError(msg)

                field_type_native = nbt._bt_python_ctf_field_type_variant_get_field_type(self._ft, i)

                if field_type_native is None:
                    msg = "Could not get Variant field type at index {}".format(i)
                    raise TypeError(msg)

                field_type = CTFWriter.FieldDeclaration._create_field_declaration_from_native_instance(field_type_native)
                yield (field_name, field_type)

        def get_field_by_name(self, name):
            """
            Get a field declaration by name (FieldDeclaration).
            """

            field_type_native = nbt._bt_ctf_field_type_variant_get_field_type_by_name(self._ft,
                                                                                      name)

            if field_type_native is None:
                msg = "Could not find Variant field with name {}".format(name)
                raise TypeError(msg)

            return CTFWriter.FieldDeclaration._create_field_declaration_from_native_instance(field_type_native)

        def get_field_from_tag(self, tag):
            """
            Get a field declaration from tag (EnumerationField).
            """

            field_type_native = nbt._bt_ctf_field_type_variant_get_field_type_from_tag(self._ft, tag._f)

            if field_type_native is None:
                msg = "Could not find Variant field with tag value {}".format(tag.value)
                raise TypeError(msg)

            return CTFWriter.FieldDeclaration._create_field_declaration_from_native_instance(field_type_native)

    class ArrayFieldDeclaration(FieldDeclaration):
        def __init__(self, element_type, length):
            """
            Create a new array field declaration.
            """

            self._ft = nbt._bt_ctf_field_type_array_create(element_type._ft,
                                                           length)
            super().__init__()

        @property
        def element_type(self):
            """
            Get the array's element type.
            """

            ret = nbt._bt_ctf_field_type_array_get_element_type(self._ft)

            if ret is None:
                raise TypeError("Could not get Array element type")

            return CTFWriter.FieldDeclaration._create_field_declaration_from_native_instance(ret)

        @property
        def length(self):
            """
            Get the array's length.
            """

            ret = nbt._bt_ctf_field_type_array_get_length(self._ft)

            if ret < 0:
                raise TypeError("Could not get Array length")

            return ret

    class SequenceFieldDeclaration(FieldDeclaration):
        def __init__(self, element_type, length_field_name):
            """
            Create a new sequence field declaration.
            """

            self._ft = nbt._bt_ctf_field_type_sequence_create(element_type._ft,
                                                              str(length_field_name))
            super().__init__()

        @property
        def element_type(self):
            """
            Get the sequence's element type.
            """

            ret = nbt._bt_ctf_field_type_sequence_get_element_type(self._ft)

            if ret is None:
                raise TypeError("Could not get Sequence element type")

            return CTFWriter.FieldDeclaration._create_field_declaration_from_native_instance(ret)

        @property
        def length_field_name(self):
            """
            Get the sequence's length field name.
            """

            ret = nbt._bt_ctf_field_type_sequence_get_length_field_name(self._ft)

            if ret is None:
                raise TypeError("Could not get Sequence length field name")

            return ret

    class StringFieldDeclaration(FieldDeclaration):
        def __init__(self):
            """
            Create a new string field declaration.
            """

            self._ft = nbt._bt_ctf_field_type_string_create()
            super().__init__()

        @property
        def encoding(self):
            """
            Get a string declaration's encoding (a constant from the CTFStringEncoding class).
            """

            return nbt._bt_ctf_field_type_string_get_encoding(self._ft)

        @encoding.setter
        def encoding(self, encoding):
            """
            Set a string declaration's encoding. Must be a constant from the CTFStringEncoding class.
            """

            ret = nbt._bt_ctf_field_type_string_set_encoding(self._ft, encoding)
            if ret < 0:
                raise ValueError("Could not set string encoding.")

    @staticmethod
    def create_field(field_type):
        """
        Create an instance of a field.
        """
        isinst = isinstance(field_type, CTFWriter.FieldDeclaration)

        if field_type is None or not isinst:
            raise TypeError("Invalid field_type. Type must be a FieldDeclaration-derived class.")

        if isinstance(field_type, CTFWriter.IntegerFieldDeclaration):
            return CTFWriter.IntegerField(field_type)
        elif isinstance(field_type, CTFWriter.EnumerationFieldDeclaration):
            return CTFWriter.EnumerationField(field_type)
        elif isinstance(field_type, CTFWriter.FloatFieldDeclaration):
            return CTFWriter.FloatFieldingPoint(field_type)
        elif isinstance(field_type, CTFWriter.StructureFieldDeclaration):
            return CTFWriter.StructureField(field_type)
        elif isinstance(field_type, CTFWriter.VariantFieldDeclaration):
            return CTFWriter.VariantField(field_type)
        elif isinstance(field_type, CTFWriter.ArrayFieldDeclaration):
            return CTFWriter.ArrayField(field_type)
        elif isinstance(field_type, CTFWriter.SequenceFieldDeclaration):
            return CTFWriter.SequenceField(field_type)
        elif isinstance(field_type, CTFWriter.StringFieldDeclaration):
            return CTFWriter.StringField(field_type)

    class Field:
        """
        Base class, do not instantiate.
        """

        def __init__(self, field_type):
            if not isinstance(field_type, CTFWriter.FieldDeclaration):
                raise TypeError("Invalid field_type argument.")

            self._f = nbt._bt_ctf_field_create(field_type._ft)

            if self._f is None:
                raise ValueError("Field creation failed.")

        def __del__(self):
            nbt._bt_ctf_field_put(self._f)

        @staticmethod
        def _create_field_from_native_instance(native_field_instance):
            type_dict = {
                CTFTypeId.INTEGER: CTFWriter.IntegerField,
                CTFTypeId.FLOAT: CTFWriter.FloatFieldingPoint,
                CTFTypeId.ENUM: CTFWriter.EnumerationField,
                CTFTypeId.STRING: CTFWriter.StringField,
                CTFTypeId.STRUCT: CTFWriter.StructureField,
                CTFTypeId.VARIANT: CTFWriter.VariantField,
                CTFTypeId.ARRAY: CTFWriter.ArrayField,
                CTFTypeId.SEQUENCE: CTFWriter.SequenceField
            }

            field_type = nbt._bt_python_get_field_type(native_field_instance)

            if field_type == CTFTypeId.UNKNOWN:
                raise TypeError("Invalid field instance")

            field = CTFWriter.Field.__new__(CTFWriter.Field)
            field._f = native_field_instance
            field.__class__ = type_dict[field_type]

            return field

        @property
        def declaration(self):
            native_field_type = nbt._bt_ctf_field_get_type(self._f)

            if native_field_type is None:
                raise TypeError("Invalid field instance")
            return CTFWriter.FieldDeclaration._create_field_declaration_from_native_instance(
                native_field_type)

    class IntegerField(Field):
        @property
        def value(self):
            """
            Get an integer field's value.
            """

            signedness = nbt._bt_python_field_integer_get_signedness(self._f)

            if signedness < 0:
                raise TypeError("Invalid integer instance.")

            if signedness == 0:
                ret, value = nbt._bt_ctf_field_unsigned_integer_get_value(self._f)
            else:
                ret, value = nbt._bt_ctf_field_signed_integer_get_value(self._f)

            if ret < 0:
                raise ValueError("Could not get integer field value.")

            return value

        @value.setter
        def value(self, value):
            """
            Set an integer field's value.
            """

            if not isinstance(value, int):
                raise TypeError("IntegerField's value must be an int")

            signedness = nbt._bt_python_field_integer_get_signedness(self._f)
            if signedness < 0:
                raise TypeError("Invalid integer instance.")

            if signedness == 0:
                ret = nbt._bt_ctf_field_unsigned_integer_set_value(self._f, value)
            else:
                ret = nbt._bt_ctf_field_signed_integer_set_value(self._f, value)

            if ret < 0:
                raise ValueError("Could not set integer field value.")

    class EnumerationField(Field):
        @property
        def container(self):
            """
            Return the enumeration's underlying container field (an integer field).
            """

            container = CTFWriter.IntegerField.__new__(CTFWriter.IntegerField)
            container._f = nbt._bt_ctf_field_enumeration_get_container(self._f)

            if container._f is None:
                raise TypeError("Invalid enumeration field type.")

            return container

        @property
        def value(self):
            """
            Get the enumeration field's mapping name.
            """

            value = nbt._bt_ctf_field_enumeration_get_mapping_name(self._f)

            if value is None:
                raise ValueError("Could not get enumeration's mapping name.")

            return value

        @value.setter
        def value(self, value):
            """
            Set the enumeration field's value. Must be an integer as mapping names
            may be ambiguous.
            """

            if not isinstance(value, int):
                raise TypeError("EnumerationField value must be an int")

            self.container.value = value

    class FloatFieldingPoint(Field):
        @property
        def value(self):
            """
            Get a floating point field's value.
            """

            ret, value = nbt._bt_ctf_field_floating_point_get_value(self._f)

            if ret < 0:
                raise ValueError("Could not get floating point field value.")

            return value

        @value.setter
        def value(self, value):
            """
            Set a floating point field's value.
            """

            if not isinstance(value, int) and not isinstance(value, float):
                raise TypeError("Value must be either a float or an int")

            ret = nbt._bt_ctf_field_floating_point_set_value(self._f, float(value))

            if ret < 0:
                raise ValueError("Could not set floating point field value.")

    class StructureField(Field):
        def field(self, field_name):
            """
            Get the structure's field corresponding to the provided field name.
            """

            native_instance = nbt._bt_ctf_field_structure_get_field(self._f,
                                                                    str(field_name))

            if native_instance is None:
                raise ValueError("Invalid field_name provided.")

            return CTFWriter.Field._create_field_from_native_instance(native_instance)

    class VariantField(Field):
        def field(self, tag):
            """
            Return the variant's selected field. The "tag" field is the selector enum field.
            """

            native_instance = nbt._bt_ctf_field_variant_get_field(self._f, tag._f)

            if native_instance is None:
                raise ValueError("Invalid tag provided.")

            return CTFWriter.Field._create_field_from_native_instance(native_instance)

    class ArrayField(Field):
        def field(self, index):
            """
            Return the array's field at position "index".
            """

            native_instance = nbt._bt_ctf_field_array_get_field(self._f, index)

            if native_instance is None:
                raise IndexError("Invalid index provided.")

            return CTFWriter.Field._create_field_from_native_instance(native_instance)

    class SequenceField(Field):
        @property
        def length(self):
            """
            Get the sequence's length field (IntegerField).
            """

            native_instance = nbt._bt_ctf_field_sequence_get_length(self._f)

            if native_instance is None:
                length = -1

            return CTFWriter.Field._create_field_from_native_instance(native_instance)

        @length.setter
        def length(self, length_field):
            """
            Set the sequence's length field (IntegerField).
            """

            if not isinstance(length_field, CTFWriter.IntegerField):
                raise TypeError("Invalid length field.")

            if length_field.declaration.signed:
                raise TypeError("Sequence field length must be unsigned")

            ret = nbt._bt_ctf_field_sequence_set_length(self._f, length_field._f)

            if ret < 0:
                raise ValueError("Could not set sequence length.")

        def field(self, index):
            """
            Return the sequence's field at position "index".
            """

            native_instance = nbt._bt_ctf_field_sequence_get_field(self._f, index)

            if native_instance is None:
                raise ValueError("Could not get sequence element at index.")

            return CTFWriter.Field._create_field_from_native_instance(native_instance)

    class StringField(Field):
        @property
        def value(self):
            """
            Get a string field's value.
            """

            return nbt._bt_ctf_field_string_get_value(self._f)

        @value.setter
        def value(self, value):
            """
            Set a string field's value.
            """

            ret = nbt._bt_ctf_field_string_set_value(self._f, str(value))

            if ret < 0:
                raise ValueError("Could not set string field value.")

    class EventClass:
        def __init__(self, name):
            """
            Create a new event class of the given name.
            """

            self._ec = nbt._bt_ctf_event_class_create(name)

            if self._ec is None:
                raise ValueError("Event class creation failed.")

        def __del__(self):
            nbt._bt_ctf_event_class_put(self._ec)

        def add_field(self, field_type, field_name):
            """
            Add a field of type "field_type" to the event class.
            """

            ret = nbt._bt_ctf_event_class_add_field(self._ec, field_type._ft,
                                                    str(field_name))

            if ret < 0:
                raise ValueError("Could not add field to event class.")

        @property
        def name(self):
            """
            Get the event class' name.
            """

            name = nbt._bt_ctf_event_class_get_name(self._ec)

            if name is None:
                raise TypeError("Could not get EventClass name")

            return name

        @property
        def id(self):
            """
            Get the event class' id. Returns a negative value if unset.
            """

            id = nbt._bt_ctf_event_class_get_id(self._ec)

            if id < 0:
                raise TypeError("Could not get EventClass id")

            return id

        @id.setter
        def id(self, id):
            """
            Set the event class' id. Throws a TypeError if the event class
            is already registered to a stream class.
            """

            ret = nbt._bt_ctf_event_class_set_id(self._ec, id)

            if ret < 0:
                raise TypeError("Can't change an Event Class's id after it has been assigned to a stream class")

        @property
        def stream_class(self):
            """
            Get the event class' stream class. Returns None if unset.
            """
            stream_class_native = nbt._bt_ctf_event_class_get_stream_class(self._ec)

            if stream_class_native is None:
                return None

            stream_class = CTFWriter.StreamClass.__new__(CTFWriter.StreamClass)
            stream_class._sc = stream_class_native

            return stream_class

        @property
        def fields(self):
            """
            Generator returning the event class' fields as tuples of (field name, field declaration).
            """

            count = nbt._bt_ctf_event_class_get_field_count(self._ec)

            if count < 0:
                raise TypeError("Could not get EventClass' field count")

            for i in range(count):
                field_name = nbt._bt_python_ctf_event_class_get_field_name(self._ec, i)

                if field_name is None:
                    msg = "Could not get EventClass' field name at index {}".format(i)
                    raise TypeError(msg)

                field_type_native = nbt._bt_python_ctf_event_class_get_field_type(self._ec, i)

                if field_type_native is None:
                    msg = "Could not get EventClass' field type at index {}".format(i)
                    raise TypeError(msg)

                field_type = CTFWriter.FieldDeclaration._create_field_declaration_from_native_instance(field_type_native)
                yield (field_name, field_type)

        def get_field_by_name(self, name):
            """
            Get a field declaration by name (FieldDeclaration).
            """

            field_type_native = nbt._bt_ctf_event_class_get_field_by_name(self._ec, name)

            if field_type_native is None:
                msg = "Could not find EventClass field with name {}".format(name)
                raise TypeError(msg)

            return CTFWriter.FieldDeclaration._create_field_declaration_from_native_instance(field_type_native)

    class Event:
        def __init__(self, event_class):
            """
            Create a new event of the given event class.
            """

            if not isinstance(event_class, CTFWriter.EventClass):
                raise TypeError("Invalid event_class argument.")

            self._e = nbt._bt_ctf_event_create(event_class._ec)

            if self._e is None:
                raise ValueError("Event creation failed.")

        def __del__(self):
            nbt._bt_ctf_event_put(self._e)

        @property
        def event_class(self):
            """
            Get the event's class.
            """

            event_class_native = nbt._bt_ctf_event_get_class(self._e)

            if event_class_native is None:
                return None

            event_class = CTFWriter.EventClass.__new__(CTFWriter.EventClass)
            event_class._ec = event_class_native

            return event_class

        def clock(self):
            """
            Get a clock from event. Returns None if the event's class
            is not registered to a stream class.
            """

            clock_instance = nbt._bt_ctf_event_get_clock(self._e)

            if clock_instance is None:
                return None

            clock = CTFWriter.Clock.__new__(CTFWriter.Clock)
            clock._c = clock_instance

            return clock

        def payload(self, field_name):
            """
            Get a field from event.
            """

            native_instance = nbt._bt_ctf_event_get_payload(self._e,
                                                            str(field_name))

            if native_instance is None:
                raise ValueError("Could not get event payload.")

            return CTFWriter.Field._create_field_from_native_instance(native_instance)

        def set_payload(self, field_name, value_field):
            """
            Set a manually created field as an event's payload.
            """

            if not isinstance(value, CTFWriter.Field):
                raise TypeError("Invalid value type.")

            ret = nbt._bt_ctf_event_set_payload(self._e, str(field_name),
                                                value_field._f)

            if ret < 0:
                raise ValueError("Could not set event field payload.")

    class StreamClass:
        def __init__(self, name):
            """
            Create a new stream class of the given name.
            """

            self._sc = nbt._bt_ctf_stream_class_create(name)

            if self._sc is None:
                raise ValueError("Stream class creation failed.")

        def __del__(self):
            nbt._bt_ctf_stream_class_put(self._sc)

        @property
        def name(self):
            """
            Get a stream class' name.
            """

            name = nbt._bt_ctf_stream_class_get_name(self._sc)

            if name is None:
                raise TypeError("Could not get StreamClass name")

            return name

        @property
        def clock(self):
            """
            Get a stream class' clock.
            """

            clock_instance = nbt._bt_ctf_stream_class_get_clock(self._sc)

            if clock_instance is None:
                return None

            clock = CTFWriter.Clock.__new__(CTFWriter.Clock)
            clock._c = clock_instance

            return clock

        @clock.setter
        def clock(self, clock):
            """
            Assign a clock to a stream class.
            """

            if not isinstance(clock, CTFWriter.Clock):
                raise TypeError("Invalid clock type.")

            ret = nbt._bt_ctf_stream_class_set_clock(self._sc, clock._c)

            if ret < 0:
                raise ValueError("Could not set stream class clock.")

        @property
        def id(self):
            """
            Get a stream class' id.
            """

            ret = nbt._bt_ctf_stream_class_get_id(self._sc)

            if ret < 0:
                raise TypeError("Could not get StreamClass id")

            return ret

        @id.setter
        def id(self, id):
            """
            Assign an id to a stream class.
            """

            ret = nbt._bt_ctf_stream_class_set_id(self._sc, id)

            if ret < 0:
                raise TypeError("Could not set stream class id.")

        @property
        def event_classes(self):
            """
            Generator returning the stream class' event classes.
            """

            count = nbt._bt_ctf_stream_class_get_event_class_count(self._sc)

            if count < 0:
                raise TypeError("Could not get StreamClass' event class count")

            for i in range(count):
                event_class_native = nbt._bt_ctf_stream_class_get_event_class(self._sc, i)

                if event_class_native is None:
                    msg = "Could not get StreamClass' event class at index {}".format(i)
                    raise TypeError(msg)

                event_class = CTFWriter.EventClass.__new__(CTFWriter.EventClass)
                event_class._ec = event_class_native
                yield event_class

        def add_event_class(self, event_class):
            """
            Add an event class to a stream class. New events can be added even after a
            stream has been instantiated and events have been appended. However, a stream
            will not accept events of a class that has not been added to the stream
            class beforehand.
            """

            if not isinstance(event_class, CTFWriter.EventClass):
                raise TypeError("Invalid event_class type.")

            ret = nbt._bt_ctf_stream_class_add_event_class(self._sc,
                                                           event_class._ec)

            if ret < 0:
                raise ValueError("Could not add event class.")

        @property
        def packet_context_type(self):
            """
            Get the StreamClass' packet context type (StructureFieldDeclaration)
            """

            field_type_native = nbt._bt_ctf_stream_class_get_packet_context_type(self._sc)

            if field_type_native is None:
                raise ValueError("Invalid StreamClass")

            field_type = CTFWriter.FieldDeclaration._create_field_declaration_from_native_instance(field_type_native)

            return field_type

        @packet_context_type.setter
        def packet_context_type(self, field_type):
            """
            Set a StreamClass' packet context type. Must be of type
            StructureFieldDeclaration.
            """

            if not isinstance(field_type, CTFWriter.StructureFieldDeclaration):
                raise TypeError("field_type argument must be of type StructureFieldDeclaration.")

            ret = nbt._bt_ctf_stream_class_set_packet_context_type(self._sc,
                                                                   field_type._ft)

            if ret < 0:
                raise ValueError("Failed to set packet context type.")

    class Stream:
        def __init__(self, stream_class):
            """
            Create a stream of the given class.
            """

            if not isinstance(stream_class, CTFWriter.StreamClass):
                raise TypeError("Invalid stream_class argument must be of type StreamClass.")

            self._s = nbt._bt_ctf_stream_create(stream_class._sc)

            if self._s is None:
                raise ValueError("Stream creation failed.")

        def __del__(self):
            nbt._bt_ctf_stream_put(self._s)

        @property
        def discarded_events(self):
            """
            Get a stream's discarded event count.
            """

            ret, count = nbt._bt_ctf_stream_get_discarded_events_count(self._s)

            if ret < 0:
                raise ValueError("Could not get the stream's discarded events count")

            return count

        def append_discarded_events(self, event_count):
            """
            Increase the current packet's discarded event count.
            """

            nbt._bt_ctf_stream_append_discarded_events(self._s, event_count)

        def append_event(self, event):
            """
            Append "event" to the stream's current packet. The stream's associated clock
            will be sampled during this call. The event shall not be modified after
            being appended to a stream.
            """

            ret = nbt._bt_ctf_stream_append_event(self._s, event._e)

            if ret < 0:
                raise ValueError("Could not append event to stream.")

        @property
        def packet_context(self):
            """
            Get a Stream's packet context field (a StructureField).
            """

            native_field = nbt._bt_ctf_stream_get_packet_context(self._s)

            if native_field is None:
                raise ValueError("Invalid Stream.")

            return CTFWriter.Field._create_field_from_native_instance(native_field)

        @packet_context.setter
        def packet_context(self, field):
            """
            Set a Stream's packet context field (must be a StructureField).
            """

            if not isinstance(field, CTFWriter.StructureField):
                raise TypeError("Argument field must be of type StructureField")

            ret = nbt._bt_ctf_stream_set_packet_context(self._s, field._f)

            if ret < 0:
                raise ValueError("Invalid packet context field.")

        def flush(self):
            """
            The stream's current packet's events will be flushed to disk. Events
            subsequently appended to the stream will be added to a new packet.
            """

            ret = nbt._bt_ctf_stream_flush(self._s)

            if ret < 0:
                raise ValueError("Could not flush stream.")

    class Writer:
        def __init__(self, path):
            """
            Create a new writer that will produce a trace in the given path.
            """

            self._w = nbt._bt_ctf_writer_create(path)

            if self._w is None:
                raise ValueError("Writer creation failed.")

        def __del__(self):
            nbt._bt_ctf_writer_put(self._w)

        def create_stream(self, stream_class):
            """
            Create a new stream instance and register it to the writer.
            """

            if not isinstance(stream_class, CTFWriter.StreamClass):
                raise TypeError("Invalid stream_class type.")

            stream = CTFWriter.Stream.__new__(CTFWriter.Stream)
            stream._s = nbt._bt_ctf_writer_create_stream(self._w, stream_class._sc)

            return stream

        def add_environment_field(self, name, value):
            """
            Add an environment field to the trace.
            """

            ret = nbt._bt_ctf_writer_add_environment_field(self._w, str(name),
                                                           str(value))

            if ret < 0:
                raise ValueError("Could not add environment field to trace.")

        def add_clock(self, clock):
            """
            Add a clock to the trace. Clocks assigned to stream classes must be
            registered to the writer.
            """

            ret = nbt._bt_ctf_writer_add_clock(self._w, clock._c)

            if ret < 0:
                raise ValueError("Could not add clock to Writer.")

        @property
        def metadata(self):
            """
            Get the trace's TSDL meta-data.
            """

            return nbt._bt_ctf_writer_get_metadata_string(self._w)

        def flush_metadata(self):
            """
            Flush the trace's metadata to the metadata file.
            """

            nbt._bt_ctf_writer_flush_metadata(self._w)

        @property
        def byte_order(self):
            """
            Get the trace's byte order. Must be a constant from the ByteOrder
            class.
            """

            raise NotImplementedError("Getter not implemented.")

        @byte_order.setter
        def byte_order(self, byte_order):
            """
            Set the trace's byte order. Must be a constant from the ByteOrder
            class. Defaults to the host machine's endianness
            """

            ret = nbt._bt_ctf_writer_set_byte_order(self._w, byte_order)

            if ret < 0:
                raise ValueError("Could not set trace's byte order.")
