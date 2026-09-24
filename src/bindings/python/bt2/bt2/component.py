# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

import sys
import uuid as uuidp
import typing

from bt2 import port as bt2_port
from bt2 import error as bt2_error
from bt2 import utils as bt2_utils
from bt2 import value as bt2_value
from bt2 import object as bt2_object
from bt2 import logging as bt2_logging
from bt2 import native_bt
from bt2 import clock_class as bt2_clock_class
from bt2 import trace_class as bt2_trace_class
from bt2 import query_executor as bt2_query_executor
from bt2 import message_iterator as bt2_message_iterator
from bt2 import integer_range_set as bt2_integer_range_set

# The types a user can pass as `params` when creating a component.
_ComponentParams = typing.Optional[bt2_value._ConvertibleToValue]


class _IncompleteUserClass(Exception):
    pass


# This class wraps a component class pointer. This component class could
# have been created by Python code, but since we only have the pointer,
# we can only wrap it in a generic way and lose the original Python
# class.
#
# Subclasses must implement some methods that this base class uses:
#
#   - _bt_as_component_class_ptr: static method, convert the passed component class
#     pointer to a 'bt_component_class *'.


class _ComponentClassConst(bt2_object._SharedObject):
    @property
    def name(self) -> str:
        return native_bt.component_class_get_name(
            self._bt_as_component_class_ptr(self._ptr)
        )

    @property
    def description(self) -> typing.Optional[str]:
        return native_bt.component_class_get_description(
            self._bt_as_component_class_ptr(self._ptr)
        )

    @property
    def help(self) -> typing.Optional[str]:
        return native_bt.component_class_get_help(
            self._bt_as_component_class_ptr(self._ptr)
        )

    def _bt_component_class_ptr(self):
        return self._bt_as_component_class_ptr(self._ptr)

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, _ComponentClassConst):
            try:
                if not issubclass(other, _UserComponent):
                    return False
            except TypeError:
                return False

        return self.addr == other.addr


class _SourceComponentClassConst(_ComponentClassConst):
    @staticmethod
    def _get_ref(ptr):
        native_bt.component_class_source_get_ref(ptr)

    @staticmethod
    def _put_ref(ptr):
        native_bt.component_class_source_put_ref(ptr)

    _bt_as_component_class_ptr = staticmethod(
        native_bt.component_class_source_as_component_class
    )


class _FilterComponentClassConst(_ComponentClassConst):
    @staticmethod
    def _get_ref(ptr):
        native_bt.component_class_filter_get_ref(ptr)

    @staticmethod
    def _put_ref(ptr):
        native_bt.component_class_filter_put_ref(ptr)

    _bt_as_component_class_ptr = staticmethod(
        native_bt.component_class_filter_as_component_class
    )


class _SinkComponentClassConst(_ComponentClassConst):
    @staticmethod
    def _get_ref(ptr):
        native_bt.component_class_sink_get_ref(ptr)

    @staticmethod
    def _put_ref(ptr):
        native_bt.component_class_sink_put_ref(ptr)

    _bt_as_component_class_ptr = staticmethod(
        native_bt.component_class_sink_as_component_class
    )


class _PortIterator(typing.Iterator[str]):
    def __init__(self, comp_ports):
        self._comp_ports = comp_ports
        self._at = 0

    def __next__(self) -> str:
        if self._at == len(self._comp_ports):
            raise StopIteration

        name = native_bt.port_get_name(
            self._comp_ports._port_pycls._as_port_ptr(
                self._comp_ports._borrow_port_ptr_at_index(
                    self._comp_ports._component_ptr, self._at
                )
            )
        )

        self._at += 1
        return name


_PortPyCls = typing.TypeVar("_PortPyCls", bound=bt2_port._PortConst)


class _ComponentPorts(typing.Mapping[str, _PortPyCls]):
    # component_ptr is a bt_component_source *, bt_component_filter * or
    # bt_component_sink *.  Its type must match the type expected by the
    # functions passed as arguments.

    def __init__(
        self,
        component_ptr,
        borrow_port_ptr_by_name,
        borrow_port_ptr_at_index,
        get_port_count,
        port_pycls,
    ):
        self._component_ptr = component_ptr
        self._borrow_port_ptr_by_name = borrow_port_ptr_by_name
        self._borrow_port_ptr_at_index = borrow_port_ptr_at_index
        self._get_port_count = get_port_count
        self._port_pycls = port_pycls

    # items() and values() are not provided by typing.Mapping as of Python 3.5
    # so we provide them ourselves.
    def items(self) -> typing.ItemsView[str, _PortPyCls]:
        return ((key, self[key]) for key in self)

    def values(self) -> typing.ValuesView[_PortPyCls]:
        return {key: self[key] for key in self}.values()

    def __getitem__(self, key: str) -> _PortPyCls:
        bt2_utils._check_str(key)
        port_ptr = self._borrow_port_ptr_by_name(self._component_ptr, key)

        if port_ptr is None:
            raise KeyError(key)

        return self._port_pycls._create_from_ptr_and_get_ref(port_ptr)

    def __len__(self) -> int:
        count = self._get_port_count(self._component_ptr)
        assert count >= 0
        return count

    def __iter__(self) -> typing.Iterator[str]:
        return _PortIterator(self)


# This class holds the methods which are common to both generic
# component objects and Python user component objects.
#
# Subclasses must provide these methods or property:
#
#   - _bt_borrow_component_class_ptr: static method, must return a pointer to the
#     specialized component class (e.g. 'bt_component_class_sink *') of the
#     passed specialized component pointer (e.g. 'bt_component_sink *').
#   - _bt_comp_cls_type: property, one of the native_bt.COMPONENT_CLASS_TYPE_*
#     constants.
#   - _bt_as_component_ptr: static method, must return the passed specialized
#     component pointer (e.g. 'bt_component_sink *') as a 'bt_component *'.


class _ComponentConst:
    @property
    def name(self) -> str:
        return native_bt.component_get_name(self._bt_as_component_ptr(self._ptr))

    @property
    def logging_level(self) -> bt2_logging.LoggingLevel:
        return bt2_logging.LoggingLevel(
            native_bt.component_get_logging_level(self._bt_as_component_ptr(self._ptr))
        )

    @property
    def cls(self) -> _ComponentClassConst:
        return _create_component_class_from_const_ptr_and_get_ref(
            self._bt_borrow_component_class_ptr(self._ptr), self._bt_comp_cls_type
        )

    def __eq__(self, other: object) -> bool:
        if not hasattr(other, "addr"):
            return False

        return self.addr == other.addr


class _SourceComponentConst(_ComponentConst):
    _bt_borrow_component_class_ptr = staticmethod(
        native_bt.component_source_borrow_class_const
    )
    _bt_comp_cls_type = native_bt.COMPONENT_CLASS_TYPE_SOURCE
    _bt_as_component_class_ptr = staticmethod(
        native_bt.component_class_source_as_component_class
    )
    _bt_as_component_ptr = staticmethod(native_bt.component_source_as_component_const)


class _FilterComponentConst(_ComponentConst):
    _bt_borrow_component_class_ptr = staticmethod(
        native_bt.component_filter_borrow_class_const
    )
    _bt_comp_cls_type = native_bt.COMPONENT_CLASS_TYPE_FILTER
    _bt_as_component_class_ptr = staticmethod(
        native_bt.component_class_filter_as_component_class
    )
    _bt_as_component_ptr = staticmethod(native_bt.component_filter_as_component_const)


class _SinkComponentConst(_ComponentConst):
    _bt_borrow_component_class_ptr = staticmethod(
        native_bt.component_sink_borrow_class_const
    )
    _bt_comp_cls_type = native_bt.COMPONENT_CLASS_TYPE_SINK
    _bt_as_component_class_ptr = staticmethod(
        native_bt.component_class_sink_as_component_class
    )
    _bt_as_component_ptr = staticmethod(native_bt.component_sink_as_component_const)


# This is analogous to _SourceComponentClassConst, but for source
# component objects.
class _GenericSourceComponentConst(bt2_object._SharedObject, _SourceComponentConst):
    @staticmethod
    def _get_ref(ptr):
        native_bt.component_source_get_ref(ptr)

    @staticmethod
    def _put_ref(ptr):
        native_bt.component_source_put_ref(ptr)

    @property
    def output_ports(self) -> typing.Mapping[str, bt2_port._OutputPortConst]:
        return _ComponentPorts(
            self._ptr,
            native_bt.component_source_borrow_output_port_by_name_const,
            native_bt.component_source_borrow_output_port_by_index_const,
            native_bt.component_source_get_output_port_count,
            bt2_port._OutputPortConst,
        )


# This is analogous to _FilterComponentClassConst, but for filter
# component objects.
class _GenericFilterComponentConst(bt2_object._SharedObject, _FilterComponentConst):
    @staticmethod
    def _get_ref(ptr):
        native_bt.component_filter_get_ref(ptr)

    @staticmethod
    def _put_ref(ptr):
        native_bt.component_filter_put_ref(ptr)

    @property
    def output_ports(self) -> typing.Mapping[str, bt2_port._OutputPortConst]:
        return _ComponentPorts(
            self._ptr,
            native_bt.component_filter_borrow_output_port_by_name_const,
            native_bt.component_filter_borrow_output_port_by_index_const,
            native_bt.component_filter_get_output_port_count,
            bt2_port._OutputPortConst,
        )

    @property
    def input_ports(self) -> typing.Mapping[str, bt2_port._InputPortConst]:
        return _ComponentPorts(
            self._ptr,
            native_bt.component_filter_borrow_input_port_by_name_const,
            native_bt.component_filter_borrow_input_port_by_index_const,
            native_bt.component_filter_get_input_port_count,
            bt2_port._InputPortConst,
        )


# This is analogous to _SinkComponentClassConst, but for sink
# component objects.
class _GenericSinkComponentConst(bt2_object._SharedObject, _SinkComponentConst):
    @staticmethod
    def _get_ref(ptr):
        native_bt.component_sink_get_ref(ptr)

    @staticmethod
    def _put_ref(ptr):
        native_bt.component_sink_put_ref(ptr)

    @property
    def input_ports(self) -> typing.Mapping[str, bt2_port._InputPortConst]:
        return _ComponentPorts(
            self._ptr,
            native_bt.component_sink_borrow_input_port_by_name_const,
            native_bt.component_sink_borrow_input_port_by_index_const,
            native_bt.component_sink_get_input_port_count,
            bt2_port._InputPortConst,
        )


_COMP_CLS_TYPE_TO_GENERIC_COMP_PYCLS = {
    native_bt.COMPONENT_CLASS_TYPE_SOURCE: _GenericSourceComponentConst,
    native_bt.COMPONENT_CLASS_TYPE_FILTER: _GenericFilterComponentConst,
    native_bt.COMPONENT_CLASS_TYPE_SINK: _GenericSinkComponentConst,
}


_COMP_CLS_TYPE_TO_GENERIC_COMP_CLS_PYCLS = {
    native_bt.COMPONENT_CLASS_TYPE_SOURCE: _SourceComponentClassConst,
    native_bt.COMPONENT_CLASS_TYPE_FILTER: _FilterComponentClassConst,
    native_bt.COMPONENT_CLASS_TYPE_SINK: _SinkComponentClassConst,
}


# Create a component Python object of type _GenericSourceComponentConst,
# _GenericFilterComponentConst or _GenericSinkComponentConst, depending on
# comp_cls_type.
#
#    Steals the reference to ptr from the caller.


def _create_component_from_const_ptr(ptr, comp_cls_type):
    return _COMP_CLS_TYPE_TO_GENERIC_COMP_PYCLS[comp_cls_type]._create_from_ptr(ptr)


# Same as the above, but acquire a new reference instead of stealing the
# reference from the caller.


def _create_component_from_const_ptr_and_get_ref(ptr, comp_cls_type):
    return _COMP_CLS_TYPE_TO_GENERIC_COMP_PYCLS[
        comp_cls_type
    ]._create_from_ptr_and_get_ref(ptr)


# Create a component class Python object of type
# _SourceComponentClassConst, _FilterComponentClassConst or
# _SinkComponentClassConst, depending on comp_cls_type.
#
# Acquires a new reference to ptr.


def _create_component_class_from_const_ptr_and_get_ref(ptr, comp_cls_type):
    return _COMP_CLS_TYPE_TO_GENERIC_COMP_CLS_PYCLS[
        comp_cls_type
    ]._create_from_ptr_and_get_ref(ptr)


def _trim_docstring(docstring):
    lines = docstring.expandtabs().splitlines()

    if len(lines) == 0:
        return ""

    indent = sys.maxsize

    if len(lines) > 1:
        for line in lines[1:]:
            stripped = line.lstrip()

            if stripped:
                indent = min(indent, len(line) - len(stripped))

    trimmed = [lines[0].strip()]

    if indent < sys.maxsize and len(lines) > 1:
        for line in lines[1:]:
            trimmed.append(line[indent:].rstrip())

    while trimmed and not trimmed[-1]:
        trimmed.pop()

    while trimmed and not trimmed[0]:
        trimmed.pop(0)

    return "\n".join(trimmed)


# Metaclass for component classes defined by Python code.
#
# The Python user can create a standard Python class which inherits one
# of the three base classes (_UserSourceComponent, _UserFilterComponent,
# or _UserSinkComponent). Those base classes set this class
# (_UserComponentType) as their metaclass.
#
# Once the body of a user-defined component class is executed, this
# metaclass is used to create and initialize the class. The metaclass
# creates a native BT component class of the corresponding type and
# associates it with this user-defined class. The metaclass also defines
# class methods like the `name` and `description` properties to match
# the _ComponentClassConst interface.
#
# The component class name which is used is either:
#
# * The `name` parameter of the class:
#
#       class MySink(bt2.SinkComponent, name='my-custom-sink'):
#           ...
#
# * If the `name` class parameter is not used: the name of the class
#   itself (`MySink` in the example above).
#
# The component class description which is used is the user-defined
# class's docstring:
#
#     class MySink(bt2.SinkComponent):
#         'Description goes here'
#         ...
#
# A user-defined Python component class can have an __init__() method
# which must accept the following parameters:
#
#     def __init__(self, config, params, obj):
#         ...
#
# The value of the `obj` parameter is what was passed as the `obj`
# parameter if the component was instantiated from Python with
# Graph.add_component().  If the component was not instantiated from
# Python, is is always `None`.
#
# The user-defined component class can also have a _user_finalize()
# method (do NOT use __del__()) to be notified when the component object
# is finalized.
#
# User-defined source and filter component classes must use the
# `message_iterator_class` class parameter to specify the
# message iterator class to use for this component class:
#
#     class MyMessageIterator(bt2._UserMessageIterator):
#         ...
#
#     class MySource(bt2._UserSourceComponent,
#                    message_iterator_class=MyMessageIterator):
#         ...
#
# This message iterator class must inherit bt2._UserMessageIterator.
# It can implement the __init__() method, which must accept the
# following parameters:
#
#     def __init__(self, config, port):
#         ...
#
# It can also implement the __next__() and _user_finalize() methods
# (again, do NOT use __del__()), which don't accept any parameters
# other than `self`.
#
# When the user-defined class is destroyed, this metaclass's __del__()
# method is called: the native BT component class pointer is put (not
# needed anymore, at least not by any Python code since all references
# are dropped for __del__() to be called).
class _UserComponentType(type):
    # __new__() is used to catch custom class parameters
    def __new__(meta_cls, class_name, bases, attrs, **kwargs):
        return super().__new__(meta_cls, class_name, bases, attrs)

    def __init__(cls, class_name, bases, namespace, **kwargs):
        super().__init__(class_name, bases, namespace)

        # skip our own bases; they are never directly instantiated by the user
        own_bases = (
            "_UserComponent",
            "_UserFilterSinkComponent",
            "_UserSourceComponent",
            "_UserFilterComponent",
            "_UserSinkComponent",
        )

        if class_name in own_bases:
            return

        comp_cls_name = kwargs.get("name", class_name)
        bt2_utils._check_str(comp_cls_name)
        comp_cls_descr = None
        comp_cls_help = None

        if hasattr(cls, "__doc__") and cls.__doc__ is not None:
            bt2_utils._check_str(cls.__doc__)
            docstring = _trim_docstring(cls.__doc__)
            lines = docstring.splitlines()

            if len(lines) >= 1:
                comp_cls_descr = lines[0]

            if len(lines) >= 3:
                comp_cls_help = "\n".join(lines[2:])

        iter_cls = kwargs.get("message_iterator_class")

        if _UserSourceComponent in bases:
            _UserComponentType._bt_set_iterator_class(cls, iter_cls)
            cc_ptr = native_bt.component_class_source_create(
                cls, comp_cls_name, comp_cls_descr, comp_cls_help
            )
        elif _UserFilterComponent in bases:
            _UserComponentType._bt_set_iterator_class(cls, iter_cls)
            cc_ptr = native_bt.component_class_filter_create(
                cls, comp_cls_name, comp_cls_descr, comp_cls_help
            )
        elif _UserSinkComponent in bases:
            if not hasattr(cls, "_user_consume"):
                raise _IncompleteUserClass(
                    f"cannot create component class '{class_name}': missing a _user_consume() method"
                )

            cc_ptr = native_bt.component_class_sink_create(
                cls, comp_cls_name, comp_cls_descr, comp_cls_help
            )
        else:
            raise _IncompleteUserClass(
                f"cannot find a known component class base in the bases of '{class_name}'"
            )

        cls._bt_cc_ptr = cc_ptr

    def _bt_init_from_native(cls, comp_ptr, params_ptr, obj):
        # create instance, not user-initialized yet
        self = cls.__new__(cls)

        # config object
        config = cls._config_pycls()

        # pointer to native self component object (weak/borrowed)
        self._bt_ptr = comp_ptr

        # call user's __init__() method
        self.__init__(
            config, bt2_value._create_from_const_ptr_and_get_ref(params_ptr), obj
        )
        return self

    def __call__(cls, *args, **kwargs):
        raise RuntimeError(
            "cannot directly instantiate a user component from a Python module"
        )

    @staticmethod
    def _bt_set_iterator_class(cls, iter_cls):
        if iter_cls is None:
            raise _IncompleteUserClass(
                f"cannot create component class '{cls.__name__}': missing message iterator class"
            )

        if not issubclass(iter_cls, bt2_message_iterator._UserMessageIterator):
            raise _IncompleteUserClass(
                f"cannot create component class '{cls.__name__}': message iterator class does not inherit bt2._UserMessageIterator"
            )

        if not hasattr(iter_cls, "__next__"):
            raise _IncompleteUserClass(
                f"cannot create component class '{cls.__name__}': message iterator class is missing a __next__() method"
            )

        if hasattr(iter_cls, "_user_can_seek_ns_from_origin") and not hasattr(
            iter_cls, "_user_seek_ns_from_origin"
        ):
            raise _IncompleteUserClass(
                f"cannot create component class '{cls.__name__}': message iterator class implements _user_can_seek_ns_from_origin but not _user_seek_ns_from_origin"
            )

        if hasattr(iter_cls, "_user_can_seek_beginning") and not hasattr(
            iter_cls, "_user_seek_beginning"
        ):
            raise _IncompleteUserClass(
                f"cannot create component class '{cls.__name__}': message iterator class implements _user_can_seek_beginning but not _user_seek_beginning"
            )

        cls._iter_cls = iter_cls

    @property
    def name(cls) -> str:
        return native_bt.component_class_get_name(
            cls._bt_as_component_class_ptr(cls._bt_cc_ptr)
        )

    @property
    def description(cls) -> typing.Optional[str]:
        return native_bt.component_class_get_description(
            cls._bt_as_component_class_ptr(cls._bt_cc_ptr)
        )

    @property
    def help(cls) -> typing.Optional[str]:
        return native_bt.component_class_get_help(
            cls._bt_as_component_class_ptr(cls._bt_cc_ptr)
        )

    @property
    def addr(cls) -> int:
        return int(cls._bt_cc_ptr)

    def _bt_get_supported_mip_versions_from_native(cls, params_ptr, obj, log_level):
        # this can raise, but the native side checks the exception
        range_set = cls._user_get_supported_mip_versions(
            bt2_value._create_from_const_ptr_and_get_ref(params_ptr), obj, log_level
        )

        if type(range_set) is not bt2_integer_range_set.UnsignedIntegerRangeSet:
            # this can raise, but the native side checks the exception
            range_set = bt2_integer_range_set.UnsignedIntegerRangeSet(range_set)

        # return new reference
        range_set._get_ref(range_set._ptr)
        return range_set._ptr

    def _user_get_supported_mip_versions(cls, params, obj, log_level):
        return [0]

    def _bt_query_from_native(
        cls, priv_query_exec_ptr, object_name, params_ptr, method_obj
    ):
        priv_query_exec = bt2_query_executor._PrivateQueryExecutor(priv_query_exec_ptr)

        try:
            # this can raise, but the native side checks the exception
            results = cls._user_query(
                priv_query_exec,
                object_name,
                bt2_value._create_from_const_ptr_and_get_ref(params_ptr),
                method_obj,
            )
        finally:
            # the private query executor is a private view on the query
            # executor; it's not a shared object (the library does not
            # offer an API to get/put a reference, just like "self"
            # objects) from this query's point of view, so invalidate
            # the object in case the user kept a reference and uses it
            # later
            priv_query_exec._invalidate()

        # this can raise, but the native side checks the exception
        results = bt2_value.create_value(results)

        if results is None:
            results_ptr = native_bt.value_null
        else:
            results_ptr = results._ptr

        # return new reference
        bt2_value._Value._get_ref(results_ptr)
        return results_ptr

    def _user_query(cls, priv_query_executor, object_name, params, method_obj):
        raise bt2_utils.UnknownObject

    def _bt_component_class_ptr(self):
        return self._bt_as_component_class_ptr(self._bt_cc_ptr)

    def __del__(cls):
        if hasattr(cls, "_bt_cc_ptr"):
            cc_ptr = cls._bt_as_component_class_ptr(cls._bt_cc_ptr)
            native_bt.component_class_put_ref(cc_ptr)
            native_bt.component_class_unregister_py_cls(cc_ptr)


# Configuration objects for components.
#
# These are passed in __init__ to allow components to change some configuration
# parameters during initialization and not after. As you can see, they are not
# used at the moment, but are there in case we want to add such parameters.


class _UserComponentConfiguration:
    pass


class _UserSourceComponentConfiguration(_UserComponentConfiguration):
    pass


class _UserFilterComponentConfiguration(_UserComponentConfiguration):
    pass


class _UserSinkComponentConfiguration(_UserComponentConfiguration):
    pass


# Subclasses must provide these methods or property:
#
#   - _bt_as_not_self_specific_component_ptr: static method, must return the passed
#     specialized self component pointer (e.g. 'bt_self_component_sink *') as a
#     specialized non-self pointer (e.g. 'bt_component_sink *').
#   - _bt_borrow_component_class_ptr: static method, must return a pointer to the
#     specialized component class (e.g. 'bt_component_class_sink *') of the
#     passed specialized component pointer (e.g. 'bt_component_sink *').
#   - _bt_comp_cls_type: property, one of the native_bt.COMPONENT_CLASS_TYPE_*
#     constants.


class _UserComponent(metaclass=_UserComponentType):
    @property
    def name(self) -> str:
        return native_bt.component_get_name(
            self._bt_as_component_ptr(
                self._bt_as_not_self_specific_component_ptr(self._bt_ptr)
            )
        )

    @property
    def logging_level(self) -> bt2_logging.LoggingLevel:
        return bt2_logging.LoggingLevel(
            native_bt.component_get_logging_level(
                self._bt_as_component_ptr(
                    self._bt_as_not_self_specific_component_ptr(self._bt_ptr)
                )
            )
        )

    @property
    def cls(self) -> _ComponentClassConst:
        return _create_component_class_from_const_ptr_and_get_ref(
            self._bt_borrow_component_class_ptr(
                self._bt_as_not_self_specific_component_ptr(self._bt_ptr)
            ),
            self._bt_comp_cls_type,
        )

    @property
    def addr(self) -> int:
        return int(self._bt_ptr)

    @property
    def _graph_mip_version(self) -> int:
        return native_bt.self_component_get_graph_mip_version(
            self._bt_as_self_component_ptr(self._bt_ptr)
        )

    def __init__(self, config, params, obj):
        pass

    def _user_finalize(self):
        pass

    def _user_port_connected(self, port, other_port):
        pass

    def _bt_port_connected_from_native(
        self, self_port_ptr, self_port_type, other_port_ptr
    ):
        self._user_port_connected(
            bt2_port._create_self_from_ptr_and_get_ref(self_port_ptr, self_port_type),
            bt2_port._create_from_const_ptr_and_get_ref(
                other_port_ptr,
                (
                    native_bt.PORT_TYPE_INPUT
                    if self_port_type == native_bt.PORT_TYPE_OUTPUT
                    else native_bt.PORT_TYPE_OUTPUT
                ),
            ),
        )

    def _create_trace_class(
        self,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
        assigns_automatic_stream_class_id: bool = True,
    ) -> bt2_trace_class._TraceClass:
        tc_ptr = native_bt.trace_class_create(
            self._bt_as_self_component_ptr(self._bt_ptr)
        )

        if tc_ptr is None:
            raise bt2_error._MemoryError("could not create trace class")

        tc = bt2_trace_class._TraceClass._create_from_ptr(tc_ptr)
        tc._set_assigns_automatic_stream_class_id(assigns_automatic_stream_class_id)

        if user_attributes is not None:
            tc._set_user_attributes(user_attributes)

        return tc

    def _create_clock_class(
        self,
        frequency: typing.Optional[int] = None,
        name: typing.Optional[str] = None,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
        description: typing.Optional[str] = None,
        precision: typing.Optional[int] = None,
        offset: typing.Optional[bt2_clock_class.ClockOffset] = None,
        origin_is_unix_epoch: typing.Optional[bool] = None,
        uuid: typing.Optional[uuidp.UUID] = None,
        accuracy: typing.Optional[int] = None,
        namespace: typing.Optional[str] = None,
        uid: typing.Optional[str] = None,
        origin: typing.Union[
            None,
            bt2_clock_class._UnixEpochClockOrigin,
            bt2_clock_class._UnknownClockOrigin,
            bt2_clock_class.ClockOrigin,
        ] = None,
    ) -> bt2_clock_class._ClockClass:
        ptr = self._bt_as_self_component_ptr(self._bt_ptr)
        cc_ptr = native_bt.clock_class_create(ptr)

        if cc_ptr is None:
            raise bt2_error._MemoryError("could not create clock class")

        cc = bt2_clock_class._ClockClass._create_from_ptr(cc_ptr)

        if frequency is not None:
            cc._set_frequency(frequency)

        if namespace is not None:
            cc._set_namespace(namespace)

        if name is not None:
            cc._set_name(name)

        if uid is not None:
            cc._set_uid(uid)

        if user_attributes is not None:
            cc._set_user_attributes(user_attributes)

        if description is not None:
            cc._set_description(description)

        if precision is not None:
            cc._set_precision(precision)

        if accuracy is not None:
            cc._set_accuracy(accuracy)

        if offset is not None:
            cc._set_offset(offset)

        if origin_is_unix_epoch is not None:
            bt2_utils._check_bool(origin_is_unix_epoch)

        if origin_is_unix_epoch is not None and origin is not None:
            raise ValueError(
                "only one of `origin_is_unix_epoch` and `origin` can be set"
            )

        if origin_is_unix_epoch is not None:
            if origin_is_unix_epoch:
                cc._set_origin(bt2_clock_class.unix_epoch_clock_origin)
            else:
                cc._set_origin(bt2_clock_class.unknown_clock_origin)

        if origin is not None:
            cc._set_origin(origin)

        if uuid is not None:
            cc._set_uuid(uuid)

        return cc


class _UserSourceComponent(_UserComponent, _SourceComponentConst):
    _bt_as_not_self_specific_component_ptr = staticmethod(
        native_bt.self_component_source_as_component_source
    )
    _bt_as_self_component_ptr = staticmethod(
        native_bt.self_component_source_as_self_component
    )
    _config_pycls = _UserSourceComponentConfiguration

    @property
    def _output_ports(self) -> typing.Mapping[str, bt2_port._UserComponentOutputPort]:
        def get_output_port_count(self_ptr):
            return native_bt.component_source_get_output_port_count(
                self._bt_as_not_self_specific_component_ptr(self_ptr)
            )

        return _ComponentPorts(
            self._bt_ptr,
            native_bt.self_component_source_borrow_output_port_by_name,
            native_bt.self_component_source_borrow_output_port_by_index,
            get_output_port_count,
            bt2_port._UserComponentOutputPort,
        )

    def _add_output_port(
        self, name: str, user_data: object = None
    ) -> bt2_port._UserComponentOutputPort:
        bt2_utils._check_str(name)

        if name in self._output_ports:
            raise ValueError(
                f"source component `{self.name}` already contains an output port named `{name}`"
            )

        return bt2_port._UserComponentOutputPort._create_from_ptr_and_get_ref(
            native_bt.self_component_source_add_output_port(
                self._bt_ptr, name, user_data
            )
        )


class _UserFilterComponent(_UserComponent, _FilterComponentConst):
    _bt_as_not_self_specific_component_ptr = staticmethod(
        native_bt.self_component_filter_as_component_filter
    )
    _bt_as_self_component_ptr = staticmethod(
        native_bt.self_component_filter_as_self_component
    )
    _config_pycls = _UserFilterComponentConfiguration

    @property
    def _output_ports(self) -> typing.Mapping[str, bt2_port._UserComponentOutputPort]:
        def get_output_port_count(self_ptr):
            return native_bt.component_filter_get_output_port_count(
                self._bt_as_not_self_specific_component_ptr(self_ptr)
            )

        return _ComponentPorts(
            self._bt_ptr,
            native_bt.self_component_filter_borrow_output_port_by_name,
            native_bt.self_component_filter_borrow_output_port_by_index,
            get_output_port_count,
            bt2_port._UserComponentOutputPort,
        )

    @property
    def _input_ports(self) -> typing.Mapping[str, bt2_port._UserComponentInputPort]:
        def get_input_port_count(self_ptr):
            return native_bt.component_filter_get_input_port_count(
                self._bt_as_not_self_specific_component_ptr(self_ptr)
            )

        return _ComponentPorts(
            self._bt_ptr,
            native_bt.self_component_filter_borrow_input_port_by_name,
            native_bt.self_component_filter_borrow_input_port_by_index,
            get_input_port_count,
            bt2_port._UserComponentInputPort,
        )

    def _add_output_port(
        self, name: str, user_data: object = None
    ) -> bt2_port._UserComponentOutputPort:
        bt2_utils._check_str(name)

        if name in self._output_ports:
            raise ValueError(
                f"filter component `{self.name}` already contains an output port named `{name}`"
            )

        return bt2_port._UserComponentOutputPort._create_from_ptr_and_get_ref(
            native_bt.self_component_filter_add_output_port(
                self._bt_ptr, name, user_data
            )
        )

    def _add_input_port(
        self, name: str, user_data: object = None
    ) -> bt2_port._UserComponentInputPort:
        bt2_utils._check_str(name)

        if name in self._input_ports:
            raise ValueError(
                f"filter component `{self.name}` already contains an input port named `{name}`"
            )

        return bt2_port._UserComponentInputPort._create_from_ptr_and_get_ref(
            native_bt.self_component_filter_add_input_port(
                self._bt_ptr, name, user_data
            )
        )


class _UserSinkComponent(_UserComponent, _SinkComponentConst):
    _bt_as_not_self_specific_component_ptr = staticmethod(
        native_bt.self_component_sink_as_component_sink
    )
    _bt_as_self_component_ptr = staticmethod(
        native_bt.self_component_sink_as_self_component
    )
    _config_pycls = _UserSinkComponentConfiguration

    def _bt_graph_is_configured_from_native(self):
        self._user_graph_is_configured()

    def _user_graph_is_configured(self):
        pass

    @property
    def _input_ports(self) -> typing.Mapping[str, bt2_port._UserComponentInputPort]:
        def get_input_port_count(self_ptr):
            return native_bt.component_sink_get_input_port_count(
                self._bt_as_not_self_specific_component_ptr(self_ptr)
            )

        return _ComponentPorts(
            self._bt_ptr,
            native_bt.self_component_sink_borrow_input_port_by_name,
            native_bt.self_component_sink_borrow_input_port_by_index,
            get_input_port_count,
            bt2_port._UserComponentInputPort,
        )

    def _add_input_port(
        self, name: str, user_data: object = None
    ) -> bt2_port._UserComponentInputPort:
        bt2_utils._check_str(name)

        if name in self._input_ports:
            raise ValueError(
                f"sink component `{self.name}` already contains an input port named `{name}`"
            )

        return bt2_port._UserComponentInputPort._create_from_ptr_and_get_ref(
            native_bt.self_component_sink_add_input_port(self._bt_ptr, name, user_data)
        )

    def _create_message_iterator(
        self, input_port: bt2_port._UserComponentInputPort
    ) -> bt2_message_iterator._UserComponentInputPortMessageIterator:
        bt2_utils._check_type(input_port, bt2_port._UserComponentInputPort)

        if not input_port.is_connected:
            raise ValueError("input port is not connected")

        return bt2_message_iterator._UserComponentInputPortMessageIterator(
            native_bt.message_iterator_create_from_sink_component(
                self._bt_ptr, input_port._ptr
            )
        )

    @property
    def _is_interrupted(self) -> bool:
        return native_bt.self_component_sink_is_interrupted(self._bt_ptr)
