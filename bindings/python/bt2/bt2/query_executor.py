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
import bt2.component
import bt2


class QueryExecutor(object._Object):
    def _handle_status(self, status, gen_error_msg):
        if status == native_bt.QUERY_STATUS_AGAIN:
            raise bt2.TryAgain
        elif status == native_bt.QUERY_STATUS_EXECUTOR_CANCELED:
            raise bt2.QueryExecutorCanceled
        elif status == native_bt.QUERY_STATUS_INVALID_OBJECT:
            raise bt2.InvalidQueryObject
        elif status == native_bt.QUERY_STATUS_INVALID_PARAMS:
            raise bt2.InvalidQueryParams
        elif status < 0:
            raise bt2.Error(gen_error_msg)

    def __init__(self):
        ptr = native_bt.query_executor_create()

        if ptr is None:
            raise bt2.CreationError('cannot create query executor object')

        super().__init__(ptr)

    def cancel(self):
        status = native_bt.query_executor_cancel(self._ptr)
        self._handle_status(status, 'cannot cancel query executor object')

    @property
    def is_canceled(self):
        is_canceled = native_bt.query_executor_is_canceled(self._ptr)
        assert(is_canceled >= 0)
        return is_canceled > 0

    def query(self, component_class, object, params=None):
        if not isinstance(component_class, bt2.component._GenericComponentClass):
            err = False

            try:
                if not issubclass(component_class, bt2.component._UserComponent):
                    err = True
            except TypeError:
                err = True

            if err:
                o = component_class
                raise TypeError("'{}' is not a component class object".format(o))

        utils._check_str(object)

        if params is None:
            params_ptr = native_bt.value_null
        else:
            params = bt2.create_value(params)
            params_ptr = params._ptr

        if isinstance(component_class, bt2.component._GenericComponentClass):
            cc_ptr = component_class._ptr
        else:
            cc_ptr = component_class._cc_ptr

        status, result_ptr = native_bt.query_executor_query(self._ptr, cc_ptr,
                                                            object, params_ptr)
        self._handle_status(status, 'cannot query component class')
        assert(result_ptr)
        return bt2.value._create_from_ptr(result_ptr)

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        return self.addr == other.addr
