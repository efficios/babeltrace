# The MIT License (MIT)
#
# Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
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
import bt2.field_types
import collections.abc
import bt2.values
import bt2.event
import copy
import bt2


class _EventClassAttributesIterator(collections.abc.Iterator):
    def __init__(self, attributes):
        self._attributes = attributes
        self._at = 0

    def __next__(self):
        if self._at == len(self._attributes):
            raise StopIteration

        name = native_bt.ctf_event_class_get_attribute_name(self._attributes._event_class_ptr,
                                                            self._at)
        utils._handle_ptr("cannot get event class object's attribute name")
        self._at += 1
        return name


class _EventClassAttributes(collections.abc.MutableMapping):
    def __init__(self, event_class_ptr):
        self._event_class_ptr = event_class_ptr

    def __getitem__(self, key):
        utils._check_str(key)
        value_ptr = native_bt.ctf_event_class_get_attribute_value_by_name(self._event_class_ptr,
                                                                          key)

        if value_ptr is None:
            raise KeyError(key)

        return bt2.values._create_from_ptr(value_ptr)

    def __setitem__(self, key, value):
        utils._check_str(key)
        value = bt2.create_value(value)
        ret = native_bt.ctf_event_class_set_attribute(self._event_class_ptr, key,
                                                      value._ptr)
        utils._handle_ret(ret, "cannot set event class object's attribute")

    def __delitem__(self, key):
        raise NotImplementedError

    def __len__(self):
        count = native_bt.ctf_event_class_get_attribute_count(self._event_class_ptr)
        utils._handle_ret(count, "cannot get event class object's attribute count")
        return count

    def __iter__(self):
        return _EventClassAttributesIterator(self)


class EventClass(object._Object):
    def __init__(self, name, id=None, context_field_type=None,
                 payload_field_type=None, attributes=None):
        utils._check_str(name)
        ptr = native_bt.ctf_event_class_create(name)

        if ptr is None:
            raise bt2.CreationError('cannot create event class object')

        super().__init__(ptr)

        if id is not None:
            self.id = id

        if context_field_type is not None:
            self.context_field_type = context_field_type

        if payload_field_type is not None:
            self.payload_field_type = payload_field_type

        if attributes is not None:
            for name, value in attributes.items():
                self.attributes[name] = value

    @property
    def stream_class(self):
        sc_ptr = native_bt.ctf_event_class_get_stream_class(self._ptr)

        if sc_ptr is not None:
            return bt2.StreamClass._create_from_ptr(sc_ptr)

    @property
    def attributes(self):
        return _EventClassAttributes(self._ptr)

    @property
    def name(self):
        return native_bt.ctf_event_class_get_name(self._ptr)

    @property
    def id(self):
        id = native_bt.ctf_event_class_get_id(self._ptr)

        if utils._is_m1ull(id):
            raise bt2.Error("cannot get event class object's ID")

        return id

    @id.setter
    def id(self, id):
        utils._check_int64(id)
        ret = native_bt.ctf_event_class_set_id(self._ptr, id)
        utils._handle_ret(ret, "cannot set event class object's ID")

    @property
    def context_field_type(self):
        ft_ptr = native_bt.ctf_event_class_get_context_type(self._ptr)

        if ft_ptr is None:
            return

        return bt2.field_types._create_from_ptr(ft_ptr)

    @context_field_type.setter
    def context_field_type(self, context_field_type):
        context_field_type_ptr = None

        if context_field_type is not None:
            utils._check_type(context_field_type, bt2.field_types._FieldType)
            context_field_type_ptr = context_field_type._ptr

        ret = native_bt.ctf_event_class_set_context_type(self._ptr, context_field_type_ptr)
        utils._handle_ret(ret, "cannot set event class object's context field type")

    @property
    def payload_field_type(self):
        ft_ptr = native_bt.ctf_event_class_get_payload_type(self._ptr)

        if ft_ptr is None:
            return

        return bt2.field_types._create_from_ptr(ft_ptr)

    @payload_field_type.setter
    def payload_field_type(self, payload_field_type):
        payload_field_type_ptr = None

        if payload_field_type is not None:
            utils._check_type(payload_field_type, bt2.field_types._FieldType)
            payload_field_type_ptr = payload_field_type._ptr

        ret = native_bt.ctf_event_class_set_payload_type(self._ptr, payload_field_type_ptr)
        utils._handle_ret(ret, "cannot set event class object's payload field type")

    def __call__(self):
        event_ptr = native_bt.ctf_event_create(self._ptr)

        if event_ptr is None:
            raise bt2.CreationError('cannot create event field object')

        return bt2.event._create_from_ptr(event_ptr)

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        if self.addr == other.addr:
            return True

        self_attributes = {name: val for name, val in self.attributes.items()}
        other_attributes = {name: val for name, val in other.attributes.items()}
        self_props = (
            self_attributes,
            self.name,
            self.id,
            self.context_field_type,
            self.payload_field_type
        )
        other_props = (
            other_attributes,
            other.name,
            other.id,
            other.context_field_type,
            other.payload_field_type
        )
        return self_props == other_props

    def _copy(self, ft_copy_func):
        cpy = EventClass(self.name)
        cpy.id = self.id

        for name, value in self.attributes.items():
            cpy.attributes[name] = value

        cpy.context_field_type = ft_copy_func(self.context_field_type)
        cpy.payload_field_type = ft_copy_func(self.payload_field_type)
        return cpy

    def __copy__(self):
        return self._copy(lambda ft: ft)

    def __deepcopy__(self, memo):
        cpy = self._copy(copy.deepcopy)
        memo[id(self)] = cpy
        return cpy
