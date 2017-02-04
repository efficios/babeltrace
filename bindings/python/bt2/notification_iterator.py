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
import bt2.notification
import collections.abc
import bt2.component
import bt2


class NotificationIteratorSeekOrigin:
    BEGIN = native_bt.NOTIFICATION_ITERATOR_SEEK_ORIGIN_BEGIN
    CURRENT = native_bt.NOTIFICATION_ITERATOR_SEEK_ORIGIN_CURRENT
    END = native_bt.NOTIFICATION_ITERATOR_SEEK_ORIGIN_END


class _GenericNotificationIteratorMethods(collections.abc.Iterator):
    @property
    def notification(self):
        notif_ptr = native_bt.notification_iterator_get_notification(self._ptr)
        utils._handle_ptr(notif_ptr, "cannot get notification iterator object's current notification object")
        return bt2.notification._create_from_ptr(notif_ptr)

    def _handle_status(self, status, gen_error_msg):
        if status == native_bt.NOTIFICATION_ITERATOR_STATUS_END:
            raise bt2.Stop
        elif status == native_bt.NOTIFICATION_ITERATOR_STATUS_UNSUPPORTED:
            raise bt2.UnsupportedFeature
        elif status < 0:
            raise bt2.Error(gen_error_msg)

    def next(self):
        status = native_bt.notification_iterator_next(self._ptr)
        self._handle_status(status,
                            'unexpected error: cannot go to the next notification')

    def __next__(self):
        self.next()
        return self.notification

    def seek_to_time(self, origin, time):
        utils._check_int64(origin)
        utils._check_int64(time)
        status = native_bt.notification_iterator_seek_time(self._ptr, origin,
                                                           time)
        self._handle_status(status,
                            'unexpected error: cannot seek notification iterator to time')


class _GenericNotificationIterator(object._Object, _GenericNotificationIteratorMethods):
    @property
    def component(self):
        comp_ptr = native_bt.notification_iterator_get_component(self._ptr)
        utils._handle_ptr(comp_ptr, "cannot get notification iterator object's component object")
        return bt2.component._create_generic_component_from_ptr(comp_ptr)


class UserNotificationIterator(_GenericNotificationIteratorMethods):
    def __new__(cls, ptr):
        # User iterator objects are always created by the BT system,
        # that is, never instantiated directly by Python code.
        #
        # The system calls this, then manually calls self.__init__()
        # without the `ptr` argument. The user has access to
        # self.component during this call, thanks to this self._ptr
        # argument being set.
        #
        # self._ptr is NOT owned by this object here, so there's nothing
        # to do in __del__().
        self = super().__new__(cls)
        self._ptr = ptr
        return self

    def __init__(self):
        pass

    @property
    def component(self):
        return native_bt.py3_get_component_from_notif_iter(self._ptr)

    @property
    def addr(self):
        return int(self._ptr)

    def _destroy(self):
        pass

    def _get_from_bt(self):
        # this can raise anything: it's catched by the system
        notif = self._get()
        utils._check_type(notif, bt2.notification._Notification)

        # steal the underlying native notification object for the caller
        notif_ptr = notif._ptr
        notif._ptr = None
        return int(notif_ptr)
