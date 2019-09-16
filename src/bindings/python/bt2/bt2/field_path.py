# The MIT License (MIT)
#
# Copyright (c) 2018 Francis Deslauriers <francis.deslauriers@efficios.com>
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

import collections
from bt2 import native_bt, object


class FieldPathScope:
    PACKET_CONTEXT = native_bt.FIELD_PATH_SCOPE_PACKET_CONTEXT
    EVENT_COMMON_CONTEXT = native_bt.FIELD_PATH_SCOPE_EVENT_COMMON_CONTEXT
    EVENT_SPECIFIC_CONTEXT = native_bt.FIELD_PATH_SCOPE_EVENT_SPECIFIC_CONTEXT
    EVENT_PAYLOAD = native_bt.FIELD_PATH_SCOPE_EVENT_PAYLOAD


class _FieldPathItem:
    pass


class _IndexFieldPathItem(_FieldPathItem):
    def __init__(self, index):
        self._index = index

    @property
    def index(self):
        return self._index


class _CurrentArrayElementFieldPathItem(_FieldPathItem):
    pass


class _CurrentOptionContentFieldPathItem(_FieldPathItem):
    pass


class _FieldPathConst(object._SharedObject, collections.abc.Iterable):
    _get_ref = staticmethod(native_bt.field_path_get_ref)
    _put_ref = staticmethod(native_bt.field_path_put_ref)

    @property
    def root_scope(self):
        scope = native_bt.field_path_get_root_scope(self._ptr)
        return _SCOPE_TO_OBJ[scope]

    def __len__(self):
        return native_bt.field_path_get_item_count(self._ptr)

    def __iter__(self):
        for idx in range(len(self)):
            item_ptr = native_bt.field_path_borrow_item_by_index_const(self._ptr, idx)
            assert item_ptr is not None
            item_type = native_bt.field_path_item_get_type(item_ptr)
            if item_type == native_bt.FIELD_PATH_ITEM_TYPE_INDEX:
                idx = native_bt.field_path_item_index_get_index(item_ptr)
                yield _IndexFieldPathItem(idx)
            elif item_type == native_bt.FIELD_PATH_ITEM_TYPE_CURRENT_ARRAY_ELEMENT:
                yield _CurrentArrayElementFieldPathItem()
            elif item_type == native_bt.FIELD_PATH_ITEM_TYPE_CURRENT_OPTION_CONTENT:
                yield _CurrentOptionContentFieldPathItem()
            else:
                assert False


_SCOPE_TO_OBJ = {
    native_bt.FIELD_PATH_SCOPE_PACKET_CONTEXT: FieldPathScope.PACKET_CONTEXT,
    native_bt.FIELD_PATH_SCOPE_EVENT_COMMON_CONTEXT: FieldPathScope.EVENT_COMMON_CONTEXT,
    native_bt.FIELD_PATH_SCOPE_EVENT_SPECIFIC_CONTEXT: FieldPathScope.EVENT_SPECIFIC_CONTEXT,
    native_bt.FIELD_PATH_SCOPE_EVENT_PAYLOAD: FieldPathScope.EVENT_PAYLOAD,
}
