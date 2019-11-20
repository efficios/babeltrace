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
import collections.abc
from bt2 import value as bt2_value
from bt2 import stream as bt2_stream
from bt2 import stream_class as bt2_stream_class
import bt2
import functools
import uuid as uuidp


def _bt2_trace_class():
    from bt2 import trace_class as bt2_trace_class

    return bt2_trace_class


class _TraceEnvironmentConst(collections.abc.Mapping):
    _create_value_from_ptr_and_get_ref = staticmethod(
        bt2_value._create_from_const_ptr_and_get_ref
    )

    def __init__(self, trace):
        self._trace = trace

    def __getitem__(self, key):
        utils._check_str(key)

        borrow_entry_fn = native_bt.trace_borrow_environment_entry_value_by_name_const
        value_ptr = borrow_entry_fn(self._trace._ptr, key)

        if value_ptr is None:
            raise KeyError(key)

        return self._create_value_from_ptr_and_get_ref(value_ptr)

    def __len__(self):
        count = native_bt.trace_get_environment_entry_count(self._trace._ptr)
        assert count >= 0
        return count

    def __iter__(self):
        trace_ptr = self._trace._ptr

        for idx in range(len(self)):
            borrow_entry_fn = native_bt.trace_borrow_environment_entry_by_index_const
            entry_name, _ = borrow_entry_fn(trace_ptr, idx)
            assert entry_name is not None
            yield entry_name


class _TraceEnvironment(_TraceEnvironmentConst, collections.abc.MutableMapping):
    _create_value_from_ptr_and_get_ref = staticmethod(
        bt2_value._create_from_ptr_and_get_ref
    )

    def __setitem__(self, key, value):
        if isinstance(value, str):
            set_env_entry_fn = native_bt.trace_set_environment_entry_string
        elif isinstance(value, int):
            set_env_entry_fn = native_bt.trace_set_environment_entry_integer
        else:
            raise TypeError('expected str or int, got {}'.format(type(value)))

        status = set_env_entry_fn(self._trace._ptr, key, value)
        utils._handle_func_status(status, "cannot set trace object's environment entry")

    def __delitem__(self, key):
        raise NotImplementedError


class _TraceConst(object._SharedObject, collections.abc.Mapping):
    _get_ref = staticmethod(native_bt.trace_get_ref)
    _put_ref = staticmethod(native_bt.trace_put_ref)
    _borrow_stream_ptr_by_id = staticmethod(native_bt.trace_borrow_stream_by_id_const)
    _borrow_stream_ptr_by_index = staticmethod(
        native_bt.trace_borrow_stream_by_index_const
    )
    _borrow_class_ptr = staticmethod(native_bt.trace_borrow_class_const)
    _borrow_user_attributes_ptr = staticmethod(
        native_bt.trace_borrow_user_attributes_const
    )
    _create_value_from_ptr_and_get_ref = staticmethod(
        bt2_value._create_from_const_ptr_and_get_ref
    )
    _stream_pycls = property(lambda _: bt2_stream._StreamConst)
    _trace_class_pycls = property(lambda _: _bt2_trace_class()._TraceClassConst)
    _trace_env_pycls = property(lambda _: _TraceEnvironmentConst)

    def __len__(self):
        count = native_bt.trace_get_stream_count(self._ptr)
        assert count >= 0
        return count

    def __getitem__(self, id):
        utils._check_uint64(id)

        stream_ptr = self._borrow_stream_ptr_by_id(self._ptr, id)

        if stream_ptr is None:
            raise KeyError(id)

        return self._stream_pycls._create_from_ptr_and_get_ref(stream_ptr)

    def __iter__(self):
        for idx in range(len(self)):
            stream_ptr = self._borrow_stream_ptr_by_index(self._ptr, idx)
            assert stream_ptr is not None

            id = native_bt.stream_get_id(stream_ptr)
            assert id >= 0

            yield id

    @property
    def cls(self):
        trace_class_ptr = self._borrow_class_ptr(self._ptr)
        assert trace_class_ptr is not None
        return self._trace_class_pycls._create_from_ptr_and_get_ref(trace_class_ptr)

    @property
    def user_attributes(self):
        ptr = self._borrow_user_attributes_ptr(self._ptr)
        assert ptr is not None
        return self._create_value_from_ptr_and_get_ref(ptr)

    @property
    def name(self):
        return native_bt.trace_get_name(self._ptr)

    @property
    def uuid(self):
        uuid_bytes = native_bt.trace_get_uuid(self._ptr)
        if uuid_bytes is None:
            return

        return uuidp.UUID(bytes=uuid_bytes)

    @property
    def environment(self):
        return self._trace_env_pycls(self)

    def add_destruction_listener(self, listener):
        '''Add a listener to be called when the trace is destroyed.'''
        if not callable(listener):
            raise TypeError("'listener' parameter is not callable")

        handle = utils._ListenerHandle(self.addr)

        fn = native_bt.bt2_trace_add_destruction_listener
        listener_from_native = functools.partial(
            _trace_destruction_listener_from_native, listener, handle
        )

        status, listener_id = fn(self._ptr, listener_from_native)
        utils._handle_func_status(
            status, 'cannot add destruction listener to trace object'
        )

        handle._set_listener_id(listener_id)

        return handle

    def remove_destruction_listener(self, listener_handle):
        utils._check_type(listener_handle, utils._ListenerHandle)

        if listener_handle._addr != self.addr:
            raise ValueError(
                'This trace destruction listener does not match the trace object.'
            )

        if listener_handle._listener_id is None:
            raise ValueError('This trace destruction listener was already removed.')

        status = native_bt.trace_remove_destruction_listener(
            self._ptr, listener_handle._listener_id
        )
        utils._handle_func_status(status)
        listener_handle._invalidate()


class _Trace(_TraceConst):
    _borrow_stream_ptr_by_id = staticmethod(native_bt.trace_borrow_stream_by_id)
    _borrow_stream_ptr_by_index = staticmethod(native_bt.trace_borrow_stream_by_index)
    _borrow_class_ptr = staticmethod(native_bt.trace_borrow_class)
    _borrow_user_attributes_ptr = staticmethod(native_bt.trace_borrow_user_attributes)
    _create_value_from_ptr_and_get_ref = staticmethod(
        bt2_value._create_from_ptr_and_get_ref
    )
    _stream_pycls = property(lambda _: bt2_stream._Stream)
    _trace_class_pycls = property(lambda _: _bt2_trace_class()._TraceClass)
    _trace_env_pycls = property(lambda _: _TraceEnvironment)

    def _name(self, name):
        utils._check_str(name)
        status = native_bt.trace_set_name(self._ptr, name)
        utils._handle_func_status(status, "cannot set trace class object's name")

    _name = property(fset=_name)

    def _user_attributes(self, user_attributes):
        value = bt2_value.create_value(user_attributes)
        utils._check_type(value, bt2_value.MapValue)
        native_bt.trace_set_user_attributes(self._ptr, value._ptr)

    _user_attributes = property(fset=_user_attributes)

    def _uuid(self, uuid):
        utils._check_type(uuid, uuidp.UUID)
        native_bt.trace_set_uuid(self._ptr, uuid.bytes)

    _uuid = property(fset=_uuid)

    def create_stream(self, stream_class, id=None, name=None, user_attributes=None):
        utils._check_type(stream_class, bt2_stream_class._StreamClass)

        if stream_class.assigns_automatic_stream_id:
            if id is not None:
                raise ValueError(
                    "id provided, but stream class assigns automatic stream ids"
                )

            stream_ptr = native_bt.stream_create(stream_class._ptr, self._ptr)
        else:
            if id is None:
                raise ValueError(
                    "id not provided, but stream class does not assign automatic stream ids"
                )

            utils._check_uint64(id)
            stream_ptr = native_bt.stream_create_with_id(
                stream_class._ptr, self._ptr, id
            )

        if stream_ptr is None:
            raise bt2._MemoryError('cannot create stream object')

        stream = bt2_stream._Stream._create_from_ptr(stream_ptr)

        if name is not None:
            stream._name = name

        if user_attributes is not None:
            stream._user_attributes = user_attributes

        return stream


def _trace_destruction_listener_from_native(user_listener, handle, trace_ptr):
    trace = _TraceConst._create_from_ptr_and_get_ref(trace_ptr)
    user_listener(trace)
    handle._invalidate()
