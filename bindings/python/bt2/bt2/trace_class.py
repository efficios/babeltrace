# The MIT License (MIT)
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
# Copyright (c) 2018 Francis Deslauriers <francis.deslauriers@efficios.com>
# Copyright (c) 2019 Simon Marchi <simon.marchi@efficios.com>
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

__all__ = ['TraceClass']

import bt2
from bt2 import native_bt, utils, object
import uuid as uuidp
import collections.abc
import functools


class _TraceClassEnv(collections.abc.MutableMapping):
    def __init__(self, trace_class):
        self._trace_class = trace_class

    def __getitem__(self, key):
        utils._check_str(key)

        borrow_entry_fn = native_bt.trace_class_borrow_environment_entry_value_by_name_const
        value_ptr = borrow_entry_fn(self._trace_class._ptr, key)

        if value_ptr is None:
            raise KeyError(key)

        return bt2.value._create_from_ptr_and_get_ref(value_ptr)

    def __setitem__(self, key, value):
        if isinstance(value, str):
            set_env_entry_fn = native_bt.trace_class_set_environment_entry_string
        elif isinstance(value, int):
            set_env_entry_fn = native_bt.trace_class_set_environment_entry_integer
        else:
            raise TypeError('expected str or int, got {}'.format(type(value)))

        ret = set_env_entry_fn(self._trace_class._ptr, key, value)

        utils._handle_ret(ret, "cannot set trace class object's environment entry")

    def __delitem__(self, key):
        raise NotImplementedError

    def __len__(self):
        count = native_bt.trace_class_get_environment_entry_count(self._trace_class._ptr)
        assert count >= 0
        return count

    def __iter__(self):
        trace_class_ptr = self._trace_class_env._trace_class._ptr

        for idx in range(len(self)):
            borrow_entry_fn = native_bt.trace_class_borrow_environment_entry_by_index_const
            entry_name, _ = borrow_entry_fn(trace_class_ptr, idx)
            assert entry_name is not None
            yield entry_name


class _StreamClassIterator(collections.abc.Iterator):
    def __init__(self, trace_class):
        self._trace_class = trace_class
        self._at = 0

    def __next__(self):
        if self._at == len(self._trace_class):
            raise StopIteration

        borrow_stream_class_fn = native_bt.trace_class_borrow_stream_class_by_index_const
        sc_ptr = borrow_stream_class_fn(self._trace_class._ptr, self._at)
        assert sc_ptr
        id = native_bt.stream_class_get_id(sc_ptr)
        assert id >= 0
        self._at += 1
        return id


def _trace_class_destruction_listener_from_native(user_listener, trace_class_ptr):
    trace_class = bt2.trace_class.TraceClass._create_from_ptr_and_get_ref(trace_class_ptr)
    user_listener(trace_class)


class TraceClass(object._SharedObject, collections.abc.Mapping):
    _get_ref = staticmethod(native_bt.trace_class_get_ref)
    _put_ref = staticmethod(native_bt.trace_class_put_ref)

    @property
    def uuid(self):
        uuid_bytes = native_bt.trace_class_get_uuid(self._ptr)
        if uuid_bytes is None:
            return

        return uuidp.UUID(bytes=uuid_bytes)

    def _uuid(self, uuid):
        utils._check_type(uuid, uuidp.UUID)
        native_bt.trace_class_set_uuid(self._ptr, uuid.bytes)

    _uuid = property(fset=_uuid)

    # Number of stream classes in this trace class.

    def __len__(self):
        count = native_bt.trace_class_get_stream_class_count(self._ptr)
        assert count >= 0
        return count

    # Get a stream class by stream id.

    def __getitem__(self, key):
        utils._check_uint64(key)

        sc_ptr = native_bt.trace_class_borrow_stream_class_by_id_const(self._ptr, key)
        if sc_ptr is None:
            raise KeyError(key)

        return bt2.StreamClass._create_from_ptr_and_get_ref(sc_ptr)

    def __iter__(self):
        for idx in range(len(self)):
            sc_ptr = native_bt.trace_class_borrow_stream_class_by_index_const(self._ptr, idx)
            assert sc_ptr is not None

            id = native_bt.stream_class_get_id(sc_ptr)
            assert id >= 0

            yield id

    @property
    def env(self):
        return _TraceClassEnv(self)

    def create_stream_class(self, id=None):

        if self.assigns_automatic_stream_class_id:
            if id is not None:
                raise bt2.CreationError('id provided, but trace class assigns automatic stream class ids')

            sc_ptr = native_bt.stream_class_create(self._ptr)
        else:
            if id is None:
                raise bt2.CreationError('id not provided, but trace class does not assign automatic stream class ids')

            utils._check_uint64(id)
            sc_ptr = native_bt.stream_class_create_with_id(self._ptr, id)

        return bt2.StreamClass._create_from_ptr(sc_ptr)

    @property
    def assigns_automatic_stream_class_id(self):
        return native_bt.trace_class_assigns_automatic_stream_class_id(self._ptr)

    def _assigns_automatic_stream_class_id(self, auto_id):
        utils._check_bool(auto_id)
        return native_bt.trace_class_set_assigns_automatic_stream_class_id(self._ptr, auto_id)

    _assigns_automatic_stream_class_id = property(fset=_assigns_automatic_stream_class_id)

    # Add a listener to be called when the trace class is destroyed.

    def add_destruction_listener(self, listener):

        if not callable(listener):
            raise TypeError("'listener' parameter is not callable")

        fn = native_bt.py3_trace_class_add_destruction_listener
        listener_from_native = functools.partial(_trace_class_destruction_listener_from_native,
                                                 listener)

        listener_id = fn(self._ptr, listener_from_native)
        if listener_id is None:
            utils._raise_bt2_error('cannot add destruction listener to trace class object')

        return bt2._ListenerHandle(listener_id, self)
