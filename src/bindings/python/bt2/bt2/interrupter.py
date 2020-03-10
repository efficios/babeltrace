# SPDX-License-Identifier: MIT
#
# Copyright (c) 2019 Philippe Proulx <pproulx@efficios.com>

from bt2 import native_bt, object
import bt2


class Interrupter(object._SharedObject):
    _get_ref = staticmethod(native_bt.interrupter_get_ref)
    _put_ref = staticmethod(native_bt.interrupter_put_ref)

    def __init__(self):
        ptr = native_bt.interrupter_create()

        if ptr is None:
            raise bt2._MemoryError('cannot create interrupter object')

        super().__init__(ptr)

    @property
    def is_set(self):
        return bool(native_bt.interrupter_is_set(self._ptr))

    def __bool__(self):
        return self.is_set

    def set(self):
        native_bt.interrupter_set(self._ptr)

    def reset(self):
        native_bt.interrupter_reset(self._ptr)
