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
import bt2.message_iterator
import collections.abc
import bt2.value
import traceback
import bt2.port
import sys
import bt2
import os


_env_var = os.environ.get('BABELTRACE_PYTHON_BT2_NO_TRACEBACK')
_NO_PRINT_TRACEBACK = _env_var == '1'


# This class wraps a component class pointer. This component class could
# have been created by Python code, but since we only have the pointer,
# we can only wrap it in a generic way and lose the original Python
# class.
class _GenericComponentClass(object._Object):
    @property
    def name(self):
        name = native_bt.component_class_get_name(self._ptr)
        assert(name is not None)
        return name

    @property
    def description(self):
        return native_bt.component_class_get_description(self._ptr)

    @property
    def help(self):
        return native_bt.component_class_get_help(self._ptr)

    def __eq__(self, other):
        if not isinstance(other, _GenericComponentClass):
            try:
                if not issubclass(other, _UserComponent):
                    return False
            except TypeError:
                return False

        return self.addr == other.addr


class _GenericSourceComponentClass(_GenericComponentClass):
    pass


class _GenericFilterComponentClass(_GenericComponentClass):
    pass


class _GenericSinkComponentClass(_GenericComponentClass):
    pass


def _handle_component_status(status, gen_error_msg):
    if status == native_bt.COMPONENT_STATUS_END:
        raise bt2.Stop
    elif status == native_bt.COMPONENT_STATUS_AGAIN:
        raise bt2.TryAgain
    elif status == native_bt.COMPONENT_STATUS_UNSUPPORTED:
        raise bt2.UnsupportedFeature
    elif status == native_bt.COMPONENT_STATUS_REFUSE_PORT_CONNECTION:
        raise bt2.PortConnectionRefused
    elif status == native_bt.COMPONENT_STATUS_GRAPH_IS_CANCELED:
        raise bt2.GraphCanceled
    elif status < 0:
        raise bt2.Error(gen_error_msg)


class _PortIterator(collections.abc.Iterator):
    def __init__(self, comp_ports):
        self._comp_ports = comp_ports
        self._at = 0

    def __next__(self):
        if self._at == len(self._comp_ports):
            raise StopIteration

        comp_ports = self._comp_ports
        comp_ptr = comp_ports._component._ptr
        port_ptr = comp_ports._get_port_at_index_fn(comp_ptr, self._at)
        assert(port_ptr)

        if comp_ports._is_private:
            port_pub_ptr = native_bt.port_from_private(port_ptr)
            name = native_bt.port_get_name(port_pub_ptr)
            native_bt.put(port_pub_ptr)
        else:
            name = native_bt.port_get_name(port_ptr)

        assert(name is not None)
        native_bt.put(port_ptr)
        self._at += 1
        return name


class _ComponentPorts(collections.abc.Mapping):
    def __init__(self, is_private, component,
                 get_port_by_name_fn, get_port_at_index_fn,
                 get_port_count_fn):
        self._is_private = is_private
        self._component = component
        self._get_port_by_name_fn = get_port_by_name_fn
        self._get_port_at_index_fn = get_port_at_index_fn
        self._get_port_count_fn = get_port_count_fn

    def __getitem__(self, key):
        utils._check_str(key)
        port_ptr = self._get_port_by_name_fn(self._component._ptr, key)

        if port_ptr is None:
            raise KeyError(key)

        if self._is_private:
            return bt2.port._create_private_from_ptr(port_ptr)
        else:
            return bt2.port._create_from_ptr(port_ptr)

    def __len__(self):
        if self._is_private:
            pub_ptr = native_bt.component_from_private(self._component._ptr)
            count = self._get_port_count_fn(pub_ptr)
            native_bt.put(pub_ptr)
        else:
            count = self._get_port_count_fn(self._component._ptr)

        assert(count >= 0)
        return count

    def __iter__(self):
        return _PortIterator(self)


# This class holds the methods which are common to both generic
# component objects and Python user component objects. They use the
# internal native _ptr, however it was set, to call native API
# functions.
class _Component:
    @property
    def name(self):
        name = native_bt.component_get_name(self._ptr)
        assert(name is not None)
        return name

    @property
    def graph(self):
        ptr = native_bt.component_get_graph(self._ptr)
        assert(ptr)
        return bt2.Graph._create_from_ptr(ptr)

    @property
    def component_class(self):
        cc_ptr = native_bt.component_get_class(self._ptr)
        assert(cc_ptr)
        return _create_generic_component_class_from_ptr(cc_ptr)

    def __eq__(self, other):
        if not hasattr(other, 'addr'):
            return False

        return self.addr == other.addr


class _SourceComponent(_Component):
    pass


class _FilterComponent(_Component):
    pass


class _SinkComponent(_Component):
    pass


# This is analogous to _GenericSourceComponentClass, but for source
# component objects.
class _GenericSourceComponent(object._Object, _SourceComponent):
    @property
    def output_ports(self):
        return _ComponentPorts(False, self,
                               native_bt.component_source_get_output_port_by_name,
                               native_bt.component_source_get_output_port_by_index,
                               native_bt.component_source_get_output_port_count)


# This is analogous to _GenericFilterComponentClass, but for filter
# component objects.
class _GenericFilterComponent(object._Object, _FilterComponent):
    @property
    def output_ports(self):
        return _ComponentPorts(False, self,
                               native_bt.component_filter_get_output_port_by_name,
                               native_bt.component_filter_get_output_port_by_index,
                               native_bt.component_filter_get_output_port_count)

    @property
    def input_ports(self):
        return _ComponentPorts(False, self,
                               native_bt.component_filter_get_input_port_by_name,
                               native_bt.component_filter_get_input_port_by_index,
                               native_bt.component_filter_get_input_port_count)


# This is analogous to _GenericSinkComponentClass, but for sink
# component objects.
class _GenericSinkComponent(object._Object, _SinkComponent):
    @property
    def input_ports(self):
        return _ComponentPorts(False, self,
                               native_bt.component_sink_get_input_port_by_name,
                               native_bt.component_sink_get_input_port_by_index,
                               native_bt.component_sink_get_input_port_count)


_COMP_CLS_TYPE_TO_GENERIC_COMP_PYCLS = {
    native_bt.COMPONENT_CLASS_TYPE_SOURCE: _GenericSourceComponent,
    native_bt.COMPONENT_CLASS_TYPE_FILTER: _GenericFilterComponent,
    native_bt.COMPONENT_CLASS_TYPE_SINK: _GenericSinkComponent,
}


_COMP_CLS_TYPE_TO_GENERIC_COMP_CLS_PYCLS = {
    native_bt.COMPONENT_CLASS_TYPE_SOURCE: _GenericSourceComponentClass,
    native_bt.COMPONENT_CLASS_TYPE_FILTER: _GenericFilterComponentClass,
    native_bt.COMPONENT_CLASS_TYPE_SINK: _GenericSinkComponentClass,
}


def _create_generic_component_from_ptr(ptr):
    comp_cls_type = native_bt.component_get_class_type(ptr)
    return _COMP_CLS_TYPE_TO_GENERIC_COMP_PYCLS[comp_cls_type]._create_from_ptr(ptr)


def _create_generic_component_class_from_ptr(ptr):
    comp_cls_type = native_bt.component_class_get_type(ptr)
    return _COMP_CLS_TYPE_TO_GENERIC_COMP_CLS_PYCLS[comp_cls_type]._create_from_ptr(ptr)


def _trim_docstring(docstring):
    lines = docstring.expandtabs().splitlines()
    indent = sys.maxsize

    for line in lines[1:]:
        stripped = line.lstrip()

        if stripped:
            indent = min(indent, len(line) - len(stripped))

    trimmed = [lines[0].strip()]

    if indent < sys.maxsize:
        for line in lines[1:]:
            trimmed.append(line[indent:].rstrip())

    while trimmed and not trimmed[-1]:
        trimmed.pop()

    while trimmed and not trimmed[0]:
        trimmed.pop(0)

    return '\n'.join(trimmed)


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
# the _GenericComponentClass interface.
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
# which must at least accept the `params` and `name` arguments:
#
#     def __init__(self, params, name, something_else):
#         ...
#
# The user-defined component class can also have a _finalize() method
# (do NOT use __del__()) to be notified when the component object is
# finalized.
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
# This message iterator class must inherit
# bt2._UserMessageIterator, and it must define the _get() and
# _next() methods. The message iterator class can also define an
# __init__() method: this method has access to the original Python
# component object which was used to create it as the `component`
# property. The message iterator class can also define a
# _finalize() method (again, do NOT use __del__()): this is called when
# the message iterator is (really) destroyed.
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
            '_UserComponent',
            '_UserFilterSinkComponent',
            '_UserSourceComponent',
            '_UserFilterComponent',
            '_UserSinkComponent',
        )

        if class_name in own_bases:
            return

        comp_cls_name = kwargs.get('name', class_name)
        utils._check_str(comp_cls_name)
        comp_cls_descr = None
        comp_cls_help = None

        if hasattr(cls, '__doc__') and cls.__doc__ is not None:
            utils._check_str(cls.__doc__)
            docstring = _trim_docstring(cls.__doc__)
            lines = docstring.splitlines()

            if len(lines) >= 1:
                comp_cls_descr = lines[0]

            if len(lines) >= 3:
                comp_cls_help = '\n'.join(lines[2:])

        iter_cls = kwargs.get('message_iterator_class')

        if _UserSourceComponent in bases:
            _UserComponentType._set_iterator_class(cls, iter_cls)
            cc_ptr = native_bt.py3_component_class_source_create(cls,
                                                                 comp_cls_name,
                                                                 comp_cls_descr,
                                                                 comp_cls_help)
        elif _UserFilterComponent in bases:
            _UserComponentType._set_iterator_class(cls, iter_cls)
            cc_ptr = native_bt.py3_component_class_filter_create(cls,
                                                                 comp_cls_name,
                                                                 comp_cls_descr,
                                                                 comp_cls_help)
        elif _UserSinkComponent in bases:
            if not hasattr(cls, '_consume'):
                raise bt2.IncompleteUserClass("cannot create component class '{}': missing a _consume() method".format(class_name))

            cc_ptr = native_bt.py3_component_class_sink_create(cls,
                                                               comp_cls_name,
                                                               comp_cls_descr,
                                                               comp_cls_help)
        else:
            raise bt2.IncompleteUserClass("cannot find a known component class base in the bases of '{}'".format(class_name))

        if cc_ptr is None:
            raise bt2.CreationError("cannot create component class '{}'".format(class_name))

        cls._cc_ptr = cc_ptr

    def _init_from_native(cls, comp_ptr, params_ptr):
        # create instance, not user-initialized yet
        self = cls.__new__(cls)

        # pointer to native private component object (weak/borrowed)
        self._ptr = comp_ptr

        # call user's __init__() method
        if params_ptr is not None:
            native_bt.get(params_ptr)
            params = bt2.value._create_from_ptr(params_ptr)
        else:
            params = None

        self.__init__(params)
        return self

    def __call__(cls, *args, **kwargs):
        raise bt2.Error('cannot directly instantiate a user component from a Python module')

    @staticmethod
    def _set_iterator_class(cls, iter_cls):
        if iter_cls is None:
            raise bt2.IncompleteUserClass("cannot create component class '{}': missing message iterator class".format(cls.__name__))

        if not issubclass(iter_cls, bt2.message_iterator._UserMessageIterator):
            raise bt2.IncompleteUserClass("cannot create component class '{}': message iterator class does not inherit bt2._UserMessageIterator".format(cls.__name__))

        if not hasattr(iter_cls, '__next__'):
            raise bt2.IncompleteUserClass("cannot create component class '{}': message iterator class is missing a __next__() method".format(cls.__name__))

        cls._iter_cls = iter_cls

    @property
    def name(cls):
        return native_bt.component_class_get_name(cls._cc_ptr)

    @property
    def description(cls):
        return native_bt.component_class_get_description(cls._cc_ptr)

    @property
    def help(cls):
        return native_bt.component_class_get_help(cls._cc_ptr)

    @property
    def addr(cls):
        return int(cls._cc_ptr)

    def _query_from_native(cls, query_exec_ptr, obj, params_ptr):
        # this can raise, in which case the native call to
        # bt_component_class_query() returns NULL
        if params_ptr is not None:
            native_bt.get(params_ptr)
            params = bt2.value._create_from_ptr(params_ptr)
        else:
            params = None

        native_bt.get(query_exec_ptr)
        query_exec = bt2.QueryExecutor._create_from_ptr(query_exec_ptr)

        # this can raise, but the native side checks the exception
        results = cls._query(query_exec, obj, params)

        if results is NotImplemented:
            return results

        # this can raise, but the native side checks the exception
        results = bt2.create_value(results)

        if results is None:
            results_addr = int(native_bt.value_null)
        else:
            # return new reference
            results._get()
            results_addr = int(results._ptr)

        return results_addr

    def _query(cls, query_executor, obj, params):
        # BT catches this and returns NULL to the user
        return NotImplemented

    def __eq__(self, other):
        if not hasattr(other, 'addr'):
            return False

        return self.addr == other.addr

    def __del__(cls):
        if hasattr(cls, '_cc_ptr'):
            native_bt.put(cls._cc_ptr)


class _UserComponent(metaclass=_UserComponentType):
    @property
    def name(self):
        pub_ptr = native_bt.component_from_private(self._ptr)
        name = native_bt.component_get_name(pub_ptr)
        native_bt.put(pub_ptr)
        assert(name is not None)
        return name

    @property
    def graph(self):
        pub_ptr = native_bt.component_from_private(self._ptr)
        ptr = native_bt.component_get_graph(pub_ptr)
        native_bt.put(pub_ptr)
        assert(ptr)
        return bt2.Graph._create_from_ptr(ptr)

    @property
    def component_class(self):
        pub_ptr = native_bt.component_from_private(self._ptr)
        cc_ptr = native_bt.component_get_class(pub_ptr)
        native_bt.put(pub_ptr)
        assert(cc_ptr)
        return _create_generic_component_class_from_ptr(cc_ptr)

    @property
    def addr(self):
        return int(self._ptr)

    def __init__(self, params=None):
        pass

    def _finalize(self):
        pass

    def _accept_port_connection(self, port, other_port):
        return True

    def _accept_port_connection_from_native(self, port_ptr, other_port_ptr):
        native_bt.get(port_ptr)
        native_bt.get(other_port_ptr)
        port = bt2.port._create_private_from_ptr(port_ptr)
        other_port = bt2.port._create_from_ptr(other_port_ptr)
        res = self._accept_port_connection(port, other_port_ptr)

        if type(res) is not bool:
            raise TypeError("'{}' is not a 'bool' object")

        return res

    def _port_connected(self, port, other_port):
        pass

    def _port_connected_from_native(self, port_ptr, other_port_ptr):
        native_bt.get(port_ptr)
        native_bt.get(other_port_ptr)
        port = bt2.port._create_private_from_ptr(port_ptr)
        other_port = bt2.port._create_from_ptr(other_port_ptr)

        try:
            self._port_connected(port, other_port)
        except:
            if not _NO_PRINT_TRACEBACK:
                traceback.print_exc()

    def _port_disconnected(self, port):
        pass

    def _port_disconnected_from_native(self, port_ptr):
        native_bt.get(port_ptr)
        port = bt2.port._create_private_from_ptr(port_ptr)

        try:
            self._port_disconnected(port)
        except:
            if not _NO_PRINT_TRACEBACK:
                traceback.print_exc()


class _UserSourceComponent(_UserComponent, _SourceComponent):
    @property
    def _output_ports(self):
        return _ComponentPorts(True, self,
                               native_bt.private_component_source_get_output_private_port_by_name,
                               native_bt.private_component_source_get_output_private_port_by_index,
                               native_bt.component_source_get_output_port_count)

    def _add_output_port(self, name):
        utils._check_str(name)
        fn = native_bt.private_component_source_add_output_private_port
        comp_status, priv_port_ptr = fn(self._ptr, name, None)
        _handle_component_status(comp_status,
                                 'cannot add output port to source component object')
        assert(priv_port_ptr)
        return bt2.port._create_private_from_ptr(priv_port_ptr)


class _UserFilterComponent(_UserComponent, _FilterComponent):
    @property
    def _output_ports(self):
        return _ComponentPorts(True, self,
                               native_bt.private_component_filter_get_output_private_port_by_name,
                               native_bt.private_component_filter_get_output_private_port_by_index,
                               native_bt.component_filter_get_output_port_count)

    @property
    def _input_ports(self):
        return _ComponentPorts(True, self,
                               native_bt.private_component_filter_get_input_private_port_by_name,
                               native_bt.private_component_filter_get_input_private_port_by_index,
                               native_bt.component_filter_get_input_port_count)

    def _add_output_port(self, name):
        utils._check_str(name)
        fn = native_bt.private_component_filter_add_output_private_port
        comp_status, priv_port_ptr = fn(self._ptr, name, None)
        _handle_component_status(comp_status,
                                 'cannot add output port to filter component object')
        assert(priv_port_ptr)
        return bt2.port._create_private_from_ptr(priv_port_ptr)

    def _add_input_port(self, name):
        utils._check_str(name)
        fn = native_bt.private_component_filter_add_input_private_port
        comp_status, priv_port_ptr = fn(self._ptr, name, None)
        _handle_component_status(comp_status,
                                 'cannot add input port to filter component object')
        assert(priv_port_ptr)
        return bt2.port._create_private_from_ptr(priv_port_ptr)


class _UserSinkComponent(_UserComponent, _SinkComponent):
    @property
    def _input_ports(self):
        return _ComponentPorts(True, self,
                               native_bt.private_component_sink_get_input_private_port_by_name,
                               native_bt.private_component_sink_get_input_private_port_by_index,
                               native_bt.component_sink_get_input_port_count)

    def _add_input_port(self, name):
        utils._check_str(name)
        fn = native_bt.private_component_sink_add_input_private_port
        comp_status, priv_port_ptr = fn(self._ptr, name, None)
        _handle_component_status(comp_status,
                                 'cannot add input port to sink component object')
        assert(priv_port_ptr)
        return bt2.port._create_private_from_ptr(priv_port_ptr)
