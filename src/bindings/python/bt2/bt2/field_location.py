# SPDX-License-Identifier: MIT
#
# Copyright (c) 2024 EfficiOS Inc.

import enum
import typing

from bt2 import utils as bt2_utils
from bt2 import object as bt2_object
from bt2 import native_bt


class FieldLocationScope(enum.Enum):
    PACKET_CONTEXT = native_bt.FIELD_LOCATION_SCOPE_PACKET_CONTEXT
    EVENT_COMMON_CONTEXT = native_bt.FIELD_LOCATION_SCOPE_EVENT_COMMON_CONTEXT
    EVENT_SPECIFIC_CONTEXT = native_bt.FIELD_LOCATION_SCOPE_EVENT_SPECIFIC_CONTEXT
    EVENT_PAYLOAD = native_bt.FIELD_LOCATION_SCOPE_EVENT_PAYLOAD


class _FieldLocationConst(bt2_object._SharedObject, typing.Sequence[str]):
    @staticmethod
    def _get_ref(ptr):
        native_bt.field_location_get_ref(ptr)

    @staticmethod
    def _put_ref(ptr):
        native_bt.field_location_put_ref(ptr)

    @property
    def root_scope(self) -> FieldLocationScope:
        return FieldLocationScope(native_bt.field_location_get_root_scope(self._ptr))

    def __len__(self) -> int:
        return native_bt.field_location_get_item_count(self._ptr)

    def __getitem__(self, index: int) -> str:
        bt2_utils._check_int(index)

        if index < 0 or index >= len(self):
            raise IndexError("field location object index is out of range")

        return native_bt.field_location_get_item_by_index(self._ptr, index)
