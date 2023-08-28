# SPDX-License-Identifier: MIT
#
# Copyright (c) 2019 Philippe Proulx <pproulx@efficios.com>

from bt2 import native_bt
from bt2 import object as bt2_object
from bt2 import error as bt2_error


class Interrupter(bt2_object._SharedObject):
    @staticmethod
    def _get_ref(ptr):
        native_bt.interrupter_get_ref(ptr)

    @staticmethod
    def _put_ref(ptr):
        native_bt.interrupter_put_ref(ptr)

    def __init__(self):
        ptr = native_bt.interrupter_create()

        if ptr is None:
            raise bt2_error._MemoryError("cannot create interrupter object")

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
