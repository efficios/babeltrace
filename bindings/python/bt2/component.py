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
import bt2.notification_iterator
import collections.abc
import sys
import bt2


# This class wraps a component class pointer. This component class could
# have been created by Python code, but since we only have the pointer,
# we can only wrap it in a generic way and lose the original Python
# class.
class _GenericComponentClass(object._Object):
    @property
    def name(self):
        return native_bt.component_class_get_name(self._ptr)

    @property
    def description(self):
        return native_bt.component_class_get_description(self._ptr)

    @property
    def help(self):
        return native_bt.component_class_get_help(self._ptr)

    def __call__(self, params=None, name=None):
        params = bt2.create_value(params)
        comp_ptr = native_bt.component_create_with_init_method_data(self._ptr,
                                                                    name,
                                                                    params._ptr,
                                                                    None)

        if comp_ptr is None:
            raise bt2.CreationError('cannot create component object')

        return _create_generic_component_from_ptr(comp_ptr)


class _GenericSourceComponentClass(_GenericComponentClass):
    pass


class _GenericFilterComponentClass(_GenericComponentClass):
    pass


class _GenericSinkComponentClass(_GenericComponentClass):
    pass


# This class holds the methods which are common to both generic
# component objects and Python user component objects. They use the
# internal native _ptr, however it was set, to call native API
# functions.
class _CommonComponentMethods:
    @property
    def name(self):
        return native_bt.component_get_name(self._ptr)

    @property
    def component_class(self):
        cc_ptr = native_bt.component_get_class(self._ptr)
        utils._handle_ptr(cc_ptr, "cannot get component object's class object")
        return _create_generic_component_class_from_ptr(cc_ptr)

    def _handle_status(self, status, gen_error_msg):
        if status == native_bt.COMPONENT_STATUS_END:
            raise bt2.Stop
        elif status == native_bt.COMPONENT_STATUS_AGAIN:
            raise bt2.TryAgain
        elif status == native_bt.COMPONENT_STATUS_UNSUPPORTED:
            raise bt2.UnsupportedFeature
        elif status < 0:
            raise bt2.Error(gen_error_msg)


class _CommonSourceComponentMethods(_CommonComponentMethods):
    def create_notification_iterator(self):
        iter_ptr = native_bt.component_source_create_notification_iterator_with_init_method_data(self._ptr, None)

        if iter_ptr is None:
            raise bt2.CreationError('cannot create notification iterator object')

        return bt2.notification_iterator._GenericNotificationIterator._create_from_ptr(iter_ptr)


class _CommonFilterComponentMethods(_CommonComponentMethods):
    def create_notification_iterator(self):
        iter_ptr = native_bt.component_filter_create_notification_iterator_with_init_method_data(self._ptr, None)

        if iter_ptr is None:
            raise bt2.CreationError('cannot create notification iterator object')

        return bt2.notification_iterator._GenericNotificationIterator._create_from_ptr(iter_ptr)

    def add_notification_iterator(self, notif_iter):
        utils._check_type(notif_iter, bt2.notification_iterator._GenericNotificationIteratorMethods)
        status = native_bt.component_filter_add_iterator(self._ptr, notif_iter._ptr)
        self._handle_status(status, 'unexpected error: cannot add notification iterator to filter component object')


class _CommonSinkComponentMethods(_CommonComponentMethods):
    def add_notification_iterator(self, notif_iter):
        utils._check_type(notif_iter, bt2.notification_iterator._GenericNotificationIteratorMethods)
        status = native_bt.component_sink_add_iterator(self._ptr, notif_iter._ptr)
        self._handle_status(status, 'unexpected error: cannot add notification iterator to sink component object')

    def consume(self):
        status = native_bt.component_sink_consume(self._ptr)
        self._handle_status(status, 'unexpected error: cannot consume sink component object')


# This is analogous to _GenericSourceComponentClass, but for source
# component objects.
class _GenericSourceComponent(object._Object, _CommonSourceComponentMethods):
    pass


# This is analogous to _GenericFilterComponentClass, but for filter
# component objects.
class _GenericFilterComponent(object._Object, _CommonFilterComponentMethods):
    pass


# This is analogous to _GenericSinkComponentClass, but for sink
# component objects.
class _GenericSinkComponent(object._Object, _CommonSinkComponentMethods):
    pass


_COMP_CLS_TYPE_TO_GENERIC_COMP_CLS = {
    native_bt.COMPONENT_CLASS_TYPE_SOURCE: _GenericSourceComponent,
    native_bt.COMPONENT_CLASS_TYPE_FILTER: _GenericFilterComponent,
    native_bt.COMPONENT_CLASS_TYPE_SINK: _GenericSinkComponent,
}


_COMP_CLS_TYPE_TO_GENERIC_COMP_CLS_CLS = {
    native_bt.COMPONENT_CLASS_TYPE_SOURCE: _GenericSourceComponentClass,
    native_bt.COMPONENT_CLASS_TYPE_FILTER: _GenericFilterComponentClass,
    native_bt.COMPONENT_CLASS_TYPE_SINK: _GenericSinkComponentClass,
}


def _create_generic_component_from_ptr(ptr):
    comp_cls_type = native_bt.component_get_class_type(ptr)
    return _COMP_CLS_TYPE_TO_GENERIC_COMP_CLS[comp_cls_type]._create_from_ptr(ptr)


def _create_generic_component_class_from_ptr(ptr):
    comp_cls_type = native_bt.component_class_get_type(ptr)
    return _COMP_CLS_TYPE_TO_GENERIC_COMP_CLS_CLS[comp_cls_type]._create_from_ptr(ptr)


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
# of the three base classes (UserSourceComponent, UserFilterComponent,
# or UserSinkComponent). Those base classes set this class
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
# The user-defined component class can also have a _destroy() method
# (do NOT use __del__()) to be notified when the component object is
# (really) destroyed.
#
# User-defined source and filter component classes must use the
# `notification_iterator_class` class parameter to specify the
# notification iterator class to use for this component class:
#
#     class MyNotificationIterator(bt2.UserNotificationIterator):
#         ...
#
#     class MySource(bt2.UserSourceComponent,
#                    notification_iterator_class=MyNotificationIterator):
#         ...
#
# This notification iterator class must inherit
# bt2.UserNotificationIterator, and it must define the _get() and
# _next() methods. The notification iterator class can also define an
# __init__() method: this method has access to the original Python
# component object which was used to create it as the `component`
# property. The notification iterator class can also define a _destroy()
# method (again, do NOT use __del__()): this is called when the
# notification iterator is (really) destroyed.
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
        if class_name in ('_UserComponent', 'UserSourceComponent', 'UserFilterComponent', 'UserSinkComponent'):
            return

        comp_cls_name = kwargs.get('name', class_name)
        comp_cls_descr = None
        comp_cls_help = None

        if hasattr(cls, '__doc__') and cls.__doc__ is not None:
            docstring = _trim_docstring(cls.__doc__)
            lines = docstring.splitlines()

            if len(lines) >= 1:
                comp_cls_descr = lines[0]

            if len(lines) >= 3:
                comp_cls_help = '\n'.join(lines[2:])

        iter_cls = kwargs.get('notification_iterator_class')

        if UserSourceComponent in bases:
            _UserComponentType._set_iterator_class(cls, iter_cls)
            has_seek_time = _UserComponentType._has_seek_to_time_method(cls._iter_cls)
            cc_ptr = native_bt.py3_component_class_source_create(cls,
                                                                 comp_cls_name,
                                                                 comp_cls_descr,
                                                                 comp_cls_help,
                                                                 has_seek_time)
        elif UserFilterComponent in bases:
            _UserComponentType._set_iterator_class(cls, iter_cls)
            has_seek_time = _UserComponentType._has_seek_to_time_method(cls._iter_cls)
            cc_ptr = native_bt.py3_component_class_filter_create(cls,
                                                                 comp_cls_name,
                                                                 comp_cls_descr,
                                                                 comp_cls_help,
                                                                 has_seek_time)
        elif UserSinkComponent in bases:
            if not hasattr(cls, '_consume'):
                raise bt2.IncompleteUserClassError("cannot create component class '{}': missing a _consume() method".format(class_name))

            cc_ptr = native_bt.py3_component_class_sink_create(cls,
                                                               comp_cls_name,
                                                               comp_cls_descr,
                                                               comp_cls_help)
        else:
            raise bt2.IncompleteUserClassError("cannot find a known component class base in the bases of '{}'".format(class_name))

        if cc_ptr is None:
            raise bt2.CreationError("cannot create component class '{}'".format(class_name))

        cls._cc_ptr = cc_ptr

    def __call__(cls, *args, **kwargs):
        # create instance
        self = cls.__new__(cls)

        # assign native component pointer received from caller
        self._ptr = kwargs.get('__comp_ptr')
        name = kwargs.get('name')

        if self._ptr is None:
            # called from Python code
            self._belongs_to_native_component = False

            # py3_component_create() will call self.__init__() with the
            # desired arguments and keyword arguments. This is needed
            # because functions such as
            # bt_component_sink_set_minimum_input_count() can only be
            # called _during_ the bt_component_create() call (in
            # Python words: during self.__init__()).
            #
            # The arguments and keyword arguments to use for
            # self.__init__() are put in the object itself to find them
            # from the bt_component_create() function.
            self._init_args = args
            self._init_kwargs = kwargs
            native_bt.py3_component_create(cls._cc_ptr, self, name)

            # At this point, self._ptr should be set to non-None. If
            # it's not, an error occured during the
            # native_bt.py3_component_create() call. We consider this a
            # creation error.
            if self._ptr is None:
                raise bt2.CreationError("cannot create component object from component class '{}'".format(cls.__name__))
        else:
            # Called from non-Python code (within
            # bt_component_create()): call __init__() here, after
            # removing the __comp_ptr keyword argument which is just for
            # this __call__() method.
            self._belongs_to_native_component = True
            del kwargs['__comp_ptr']

            # inject `name` into the keyword arguments
            kwargs['name'] = self.name
            self.__init__(*args, **kwargs)

        return self

    @staticmethod
    def _has_seek_to_time_method(iter_cls):
        return hasattr(iter_cls, '_seek_to_time')

    @staticmethod
    def _set_iterator_class(cls, iter_cls):
        if iter_cls is None:
            raise bt2.IncompleteUserClassError("cannot create component class '{}': missing notification iterator class".format(cls.__name__))

        if not issubclass(iter_cls, bt2.notification_iterator.UserNotificationIterator):
            raise bt2.IncompleteUserClassError("cannot create component class '{}': notification iterator class does not inherit bt2.UserNotificationIterator".format(cls.__name__))

        if not hasattr(iter_cls, '_get'):
            raise bt2.IncompleteUserClassError("cannot create component class '{}': notification iterator class is missing a _get() method".format(cls.__name__))

        if not hasattr(iter_cls, '_next'):
            raise bt2.IncompleteUserClassError("cannot create component class '{}': notification iterator class is missing a _next() method".format(cls.__name__))

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

    def __del__(cls):
        if hasattr(cls, '_cc_ptr'):
            native_bt.put(cls._cc_ptr)


class _ComponentInputNotificationIterators(collections.abc.Sequence):
    def __init__(self, comp, count_func, get_func):
        self._comp = comp
        self._count_func = count_func
        self._get_func = get_func

    def __len__(self):
        status, count = self._count_func(self._comp._ptr)
        utils._handle_ret(status, "cannot get component object's input notification iterator count")
        return count

    def __getitem__(self, index):
        utils._check_uint64(index)

        if index >= len(self):
            raise IndexError

        notif_iter_ptr = self._get_func(self._comp._ptr, index)
        utils._handle_ptr(notif_iter_ptr, "cannot get component object's input notification iterator")
        return bt2.notification_iterator._GenericNotificationIterator._create_from_ptr(notif_iter_ptr)


class _UserComponent(metaclass=_UserComponentType):
    @property
    def addr(self):
        return int(self._ptr)

    def __init__(self, *args, **kwargs):
        pass

    def _destroy(self):
        pass

    def __del__(self):
        if not self._belongs_to_native_component:
            self._belongs_to_native_component = True
            native_bt.py3_component_on_del(self)
            native_bt.put(self._ptr)


class UserSourceComponent(_UserComponent, _CommonSourceComponentMethods):
    pass


class UserFilterComponent(_UserComponent, _CommonFilterComponentMethods):
    def _set_minimum_input_notification_iterator_count(self, count):
        utils._check_uint64(count)
        status = native_bt.component_filter_set_minimum_input_count(self._ptr, count)
        self._handle_status(status, "unexpected error: cannot set filter component object's minimum input notification iterator count")

    _minimum_input_notification_iterator_count = property(fset=_set_minimum_input_notification_iterator_count)

    def _set_maximum_input_notification_iterator_count(self, count):
        utils._check_uint64(count)
        status = native_bt.component_filter_set_maximum_input_count(self._ptr, count)
        self._handle_status(status, "unexpected error: cannot set filter component object's maximum input notification iterator count")

    _maximum_input_notification_iterator_count = property(fset=_set_maximum_input_notification_iterator_count)

    @property
    def _input_notification_iterators(self):
        return _ComponentInputNotificationIterators(self,
                                                    native_bt.component_filter_get_input_count,
                                                    native_bt.component_filter_get_input_iterator_private)

    def _add_iterator_from_bt(self, notif_iter_ptr):
        notif_iter = bt2.notification_iterator._GenericNotificationIterator._create_from_ptr(notif_iter_ptr)
        self._add_notification_iterator(notif_iter)

    def _add_notification_iterator(self, notif_iter):
        pass


class UserSinkComponent(_UserComponent, _CommonSinkComponentMethods):
    def _set_minimum_input_notification_iterator_count(self, count):
        utils._check_uint64(count)
        status = native_bt.component_sink_set_minimum_input_count(self._ptr, count)
        self._handle_status(status, "unexpected error: cannot set sink component object's minimum input notification iterator count")

    _minimum_input_notification_iterator_count = property(fset=_set_minimum_input_notification_iterator_count)

    def _set_maximum_input_notification_iterator_count(self, count):
        utils._check_uint64(count)
        status = native_bt.component_sink_set_maximum_input_count(self._ptr, count)
        self._handle_status(status, "unexpected error: cannot set sink component object's maximum input notification iterator count")

    _maximum_input_notification_iterator_count = property(fset=_set_maximum_input_notification_iterator_count)

    @property
    def _input_notification_iterators(self):
        return _ComponentInputNotificationIterators(self,
                                                    native_bt.component_sink_get_input_count,
                                                    native_bt.component_sink_get_input_iterator_private)

    def _add_iterator_from_bt(self, notif_iter_ptr):
        notif_iter = bt2.notification_iterator._GenericNotificationIterator._create_from_ptr(notif_iter_ptr)
        self._add_notification_iterator(notif_iter)

    def _add_notification_iterator(self, notif_iter):
        pass
