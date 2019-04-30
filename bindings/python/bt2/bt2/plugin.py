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
import collections.abc
import bt2.component
import os.path
import bt2


def find_plugins(path, recurse=True):
    utils._check_str(path)
    utils._check_bool(recurse)
    plugin_set_ptr = None

    if os.path.isfile(path):
        plugin_set_ptr = native_bt.plugin_find_all_from_file(path)
    elif os.path.isdir(path):
        plugin_set_ptr = native_bt.plugin_find_all_from_dir(path, int(recurse))

    if plugin_set_ptr is None:
        return

    return _PluginSet._create_from_ptr(plugin_set_ptr)


def find_plugin(name):
    utils._check_str(name)
    ptr = native_bt.plugin_find(name)

    if ptr is None:
        return

    return _Plugin._create_from_ptr(ptr)


class _PluginSet(object._SharedObject, collections.abc.Sequence):
    _put_ref = native_bt.plugin_set_put_ref
    _get_ref = native_bt.plugin_set_get_ref

    def __len__(self):
        count = native_bt.plugin_set_get_plugin_count(self._ptr)
        assert(count >= 0)
        return count

    def __getitem__(self, index):
        utils._check_uint64(index)

        if index >= len(self):
            raise IndexError

        plugin_ptr = native_bt.plugin_set_borrow_plugin_by_index_const(self._ptr, index)
        assert plugin_ptr is not None
        return _Plugin._create_from_ptr_and_get_ref(plugin_ptr)


class _PluginVersion:
    def __init__(self, major, minor, patch, extra):
        self._major = major
        self._minor = minor
        self._patch = patch
        self._extra = extra

    @property
    def major(self):
        return self._major

    @property
    def minor(self):
        return self._minor

    @property
    def patch(self):
        return self._patch

    @property
    def extra(self):
        return self._extra

    def __str__(self):
        extra = ''

        if self._extra is not None:
            extra = self._extra

        return '{}.{}.{}{}'.format(self._major, self._minor, self._patch, extra)


class _PluginComponentClassesIterator(collections.abc.Iterator):
    def __init__(self, plugin_comp_cls):
        self._plugin_comp_cls = plugin_comp_cls
        self._at = 0

    def __next__(self):
        plugin_ptr = self._plugin_comp_cls._plugin._ptr
        total = self._plugin_comp_cls._component_class_count(plugin_ptr)

        if self._at == total:
            raise StopIteration

        comp_cls_ptr = self._plugin_comp_cls._borrow_component_class_by_index(plugin_ptr, self._at)
        assert comp_cls_ptr is not None
        self._at += 1

        comp_cls_type = self._plugin_comp_cls._comp_cls_type
        comp_cls_pycls = bt2.component._COMP_CLS_TYPE_TO_GENERIC_COMP_CLS_PYCLS[comp_cls_type]
        comp_cls_ptr = comp_cls_pycls._as_component_class_ptr(comp_cls_ptr)
        name = native_bt.component_class_get_name(comp_cls_ptr)
        assert name is not None
        return name


class _PluginComponentClasses(collections.abc.Mapping):
    def __init__(self, plugin):
        self._plugin = plugin

    def __getitem__(self, key):
        utils._check_str(key)
        cc_ptr = self._borrow_component_class_by_name(self._plugin._ptr, key)

        if cc_ptr is None:
            raise KeyError(key)

        return bt2.component._create_component_class_from_ptr_and_get_ref(cc_ptr, self._comp_cls_type)

    def __len__(self):
        return self._component_class_count(self._plugin._ptr)

    def __iter__(self):
        return _PluginComponentClassesIterator(self)


class _PluginSourceComponentClasses(_PluginComponentClasses):
    _component_class_count = native_bt.plugin_get_source_component_class_count
    _borrow_component_class_by_name = native_bt.plugin_borrow_source_component_class_by_name_const
    _borrow_component_class_by_index = native_bt.plugin_borrow_source_component_class_by_index_const
    _comp_cls_type = native_bt.COMPONENT_CLASS_TYPE_SOURCE


class _PluginFilterComponentClasses(_PluginComponentClasses):
    _component_class_count = native_bt.plugin_get_filter_component_class_count
    _borrow_component_class_by_name = native_bt.plugin_borrow_filter_component_class_by_name_const
    _borrow_component_class_by_index = native_bt.plugin_borrow_filter_component_class_by_index_const
    _comp_cls_type = native_bt.COMPONENT_CLASS_TYPE_FILTER


class _PluginSinkComponentClasses(_PluginComponentClasses):
    _component_class_count = native_bt.plugin_get_sink_component_class_count
    _borrow_component_class_by_name = native_bt.plugin_borrow_sink_component_class_by_name_const
    _borrow_component_class_by_index = native_bt.plugin_borrow_sink_component_class_by_index_const
    _comp_cls_type = native_bt.COMPONENT_CLASS_TYPE_SINK


class _Plugin(object._SharedObject):
    _put_ref = native_bt.plugin_put_ref
    _get_ref = native_bt.plugin_get_ref

    @property
    def name(self):
        name = native_bt.plugin_get_name(self._ptr)
        assert(name is not None)
        return name

    @property
    def author(self):
        return native_bt.plugin_get_author(self._ptr)

    @property
    def license(self):
        return native_bt.plugin_get_license(self._ptr)

    @property
    def description(self):
        return native_bt.plugin_get_description(self._ptr)

    @property
    def path(self):
        return native_bt.plugin_get_path(self._ptr)

    @property
    def version(self):
        status, major, minor, patch, extra = native_bt.plugin_get_version(self._ptr)

        if status == native_bt.PROPERTY_AVAILABILITY_NOT_AVAILABLE:
            return

        return _PluginVersion(major, minor, patch, extra)

    @property
    def source_component_classes(self):
        return _PluginSourceComponentClasses(self)

    @property
    def filter_component_classes(self):
        return _PluginFilterComponentClasses(self)

    @property
    def sink_component_classes(self):
        return _PluginSinkComponentClasses(self)
