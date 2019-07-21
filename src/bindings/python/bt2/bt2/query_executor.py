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
import bt2.interrupter
import bt2.component
import bt2.logging
import bt2


class QueryExecutor(object._SharedObject):
    _get_ref = staticmethod(native_bt.query_executor_get_ref)
    _put_ref = staticmethod(native_bt.query_executor_put_ref)

    def __init__(self):
        ptr = native_bt.query_executor_create()

        if ptr is None:
            raise bt2._MemoryError('cannot create query executor object')

        super().__init__(ptr)

    def add_interrupter(self, interrupter):
        utils._check_type(interrupter, bt2.interrupter.Interrupter)
        native_bt.query_executor_add_interrupter(self._ptr, interrupter._ptr)

    def interrupt(self):
        native_bt.query_executor_interrupt(self._ptr)

    @property
    def is_interrupted(self):
        is_interrupted = native_bt.query_executor_is_interrupted(self._ptr)
        return bool(is_interrupted)

    def query(
        self,
        component_class,
        object,
        params=None,
        logging_level=bt2.logging.LoggingLevel.NONE,
    ):
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

        utils._check_log_level(logging_level)
        cc_ptr = component_class._bt_component_class_ptr()

        status, result_ptr = native_bt.query_executor_query(
            self._ptr, cc_ptr, object, params_ptr, logging_level
        )
        utils._handle_func_status(status, 'cannot query component class')
        assert result_ptr
        return bt2.value._create_from_ptr(result_ptr)
