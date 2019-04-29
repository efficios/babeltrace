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
import bt2.message
import collections.abc
import bt2.component
import bt2


class _MessageIterator(collections.abc.Iterator):
    def _handle_status(self, status, gen_error_msg):
        if status == native_bt.MESSAGE_ITERATOR_STATUS_CANCELED:
            raise bt2.MessageIteratorCanceled
        elif status == native_bt.MESSAGE_ITERATOR_STATUS_AGAIN:
            raise bt2.TryAgain
        elif status == native_bt.MESSAGE_ITERATOR_STATUS_END:
            raise bt2.Stop
        elif status == native_bt.MESSAGE_ITERATOR_STATUS_UNSUPPORTED:
            raise bt2.UnsupportedFeature
        elif status < 0:
            raise bt2.Error(gen_error_msg)

    def __next__(self):
        raise NotImplementedError


class _GenericMessageIterator(object._SharedObject, _MessageIterator):
    def _get_msg(self):
        msg_ptr = native_bt.message_iterator_get_message(self._ptr)
        utils._handle_ptr(msg_ptr, "cannot get message iterator object's current message object")
        return bt2.message._create_from_ptr(msg_ptr)

    def _next(self):
        status = native_bt.message_iterator_next(self._ptr)
        self._handle_status(status,
                            'unexpected error: cannot advance the message iterator')

    def __next__(self):
        self._next()
        return self._get_msg()


class _PrivateConnectionMessageIterator(_GenericMessageIterator):
    @property
    def component(self):
        comp_ptr = native_bt.private_connection_message_iterator_get_component(self._ptr)
        assert(comp_ptr)
        return bt2.component._create_generic_component_from_ptr(comp_ptr)


class _OutputPortMessageIterator(_GenericMessageIterator):
    pass


class _UserMessageIterator(_MessageIterator):
    def __new__(cls, ptr):
        # User iterator objects are always created by the native side,
        # that is, never instantiated directly by Python code.
        #
        # The native code calls this, then manually calls
        # self.__init__() without the `ptr` argument. The user has
        # access to self.component during this call, thanks to this
        # self._ptr argument being set.
        #
        # self._ptr is NOT owned by this object here, so there's nothing
        # to do in __del__().
        self = super().__new__(cls)
        self._ptr = ptr
        return self

    def __init__(self):
        pass

    @property
    def _component(self):
        return native_bt.py3_get_user_component_from_user_msg_iter(self._ptr)

    @property
    def addr(self):
        return int(self._ptr)

    def _finalize(self):
        pass

    def __next__(self):
        raise bt2.Stop

    def _next_from_native(self):
        # this can raise anything: it's catched by the native part
        try:
            msg = next(self)
        except StopIteration:
            raise bt2.Stop
        except:
            raise

        utils._check_type(msg, bt2.message._Message)

        # take a new reference for the native part
        msg._get()
        return int(msg._ptr)
