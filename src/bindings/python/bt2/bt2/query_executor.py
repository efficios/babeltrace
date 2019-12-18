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
from bt2 import interrupter as bt2_interrupter
from bt2 import value as bt2_value
import bt2


def _bt2_component():
    from bt2 import component as bt2_component

    return bt2_component


class _QueryExecutorCommon:
    @property
    def _common_ptr(self):
        return self._as_query_executor_ptr()

    @property
    def is_interrupted(self):
        is_interrupted = native_bt.query_executor_is_interrupted(self._common_ptr)
        return bool(is_interrupted)

    @property
    def logging_level(self):
        return native_bt.query_executor_get_logging_level(self._common_ptr)


class QueryExecutor(object._SharedObject, _QueryExecutorCommon):
    _get_ref = staticmethod(native_bt.query_executor_get_ref)
    _put_ref = staticmethod(native_bt.query_executor_put_ref)

    def _as_query_executor_ptr(self):
        return self._ptr

    def __init__(self, component_class, object_name, params=None, method_obj=None):
        if not isinstance(component_class, _bt2_component()._ComponentClassConst):
            err = False

            try:
                if not issubclass(component_class, _bt2_component()._UserComponent):
                    err = True
            except TypeError:
                err = True

            if err:
                o = component_class
                raise TypeError("'{}' is not a component class object".format(o))

        utils._check_str(object_name)

        if params is None:
            params_ptr = native_bt.value_null
        else:
            params = bt2.create_value(params)
            params_ptr = params._ptr

        cc_ptr = component_class._bt_component_class_ptr()
        assert cc_ptr is not None

        if method_obj is not None and not native_bt.bt2_is_python_component_class(
            cc_ptr
        ):
            raise ValueError(
                'cannot pass a Python object to a non-Python component class'
            )

        ptr = native_bt.bt2_query_executor_create(
            cc_ptr, object_name, params_ptr, method_obj
        )

        if ptr is None:
            raise bt2._MemoryError('cannot create query executor object')

        super().__init__(ptr)

        # Keep a reference of `method_obj` as the native query executor
        # does not have any. This ensures that, when this object's
        # query() method is called, the Python object still exists.
        self._method_obj = method_obj

    def add_interrupter(self, interrupter):
        utils._check_type(interrupter, bt2_interrupter.Interrupter)
        native_bt.query_executor_add_interrupter(self._ptr, interrupter._ptr)

    @property
    def default_interrupter(self):
        ptr = native_bt.query_executor_borrow_default_interrupter(self._ptr)
        return bt2_interrupter.Interrupter._create_from_ptr_and_get_ref(ptr)

    def _set_logging_level(self, log_level):
        utils._check_log_level(log_level)
        status = native_bt.query_executor_set_logging_level(self._ptr, log_level)
        utils._handle_func_status(status, "cannot set query executor's logging level")

    logging_level = property(
        fget=_QueryExecutorCommon.logging_level, fset=_set_logging_level
    )

    @property
    def is_interrupted(self):
        is_interrupted = native_bt.query_executor_is_interrupted(self._ptr)
        return bool(is_interrupted)

    def query(self):
        status, result_ptr = native_bt.query_executor_query(self._ptr)
        utils._handle_func_status(status, 'cannot query component class')
        assert result_ptr is not None
        return bt2_value._create_from_const_ptr(result_ptr)


class _PrivateQueryExecutor(_QueryExecutorCommon):
    def __init__(self, ptr):
        self._ptr = ptr

    def _check_validity(self):
        if self._ptr is None:
            raise RuntimeError('this object is not valid anymore')

    def _as_query_executor_ptr(self):
        self._check_validity()
        return native_bt.private_query_executor_as_query_executor_const(self._ptr)

    def _invalidate(self):
        self._ptr = None
