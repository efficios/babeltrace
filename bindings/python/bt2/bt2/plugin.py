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
        plugin_set_ptr = native_bt.plugin_create_all_from_file(path)
    elif os.path.isdir(path):
        plugin_set_ptr = native_bt.plugin_create_all_from_dir(path, int(recurse))

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
    def __len__(self):
        count = native_bt.plugin_set_get_plugin_count(self._ptr)
        assert(count >= 0)
        return count

    def __getitem__(self, index):
        utils._check_uint64(index)

        if index >= len(self):
            raise IndexError

        plugin_ptr = native_bt.plugin_set_get_plugin(self._ptr, index)
        assert(plugin_ptr)
        return _Plugin._create_from_ptr(plugin_ptr)


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
        comp_cls_type = self._plugin_comp_cls._comp_cls_type
        total = native_bt.plugin_get_component_class_count(plugin_ptr)

        while True:
            if self._at == total:
                raise StopIteration

            comp_cls_ptr = native_bt.plugin_get_component_class_by_index(plugin_ptr,
                                                                         self._at)
            assert(comp_cls_ptr)
            cc_type = native_bt.component_class_get_type(comp_cls_ptr)
            self._at += 1

            if cc_type == comp_cls_type:
                break

            native_bt.put(comp_cls_ptr)

        name = native_bt.component_class_get_name(comp_cls_ptr)
        native_bt.put(comp_cls_ptr)
        assert(name is not None)
        return name


class _PluginComponentClasses(collections.abc.Mapping):
    def __init__(self, plugin, comp_cls_type):
        self._plugin = plugin
        self._comp_cls_type = comp_cls_type

    def __getitem__(self, key):
        utils._check_str(key)
        cc_ptr = native_bt.plugin_get_component_class_by_name_and_type(self._plugin._ptr,
                                                                       key,
                                                                       self._comp_cls_type)

        if cc_ptr is None:
            raise KeyError(key)

        return bt2.component._create_generic_component_class_from_ptr(cc_ptr)

    def __len__(self):
        count = 0
        total = native_bt.plugin_get_component_class_count(self._plugin._ptr)

        for at in range(total):
            comp_cls_ptr = native_bt.plugin_get_component_class_by_index(self._plugin._ptr,
                                                                         at)
            assert(comp_cls_ptr)
            cc_type = native_bt.component_class_get_type(comp_cls_ptr)

            if cc_type == self._comp_cls_type:
                count += 1

            native_bt.put(comp_cls_ptr)

        return count

    def __iter__(self):
        return _PluginComponentClassesIterator(self)


class _Plugin(object._SharedObject):
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

        if status < 0:
            return

        return _PluginVersion(major, minor, patch, extra)

    @property
    def source_component_classes(self):
        return _PluginComponentClasses(self, native_bt.COMPONENT_CLASS_TYPE_SOURCE)

    @property
    def filter_component_classes(self):
        return _PluginComponentClasses(self, native_bt.COMPONENT_CLASS_TYPE_FILTER)

    @property
    def sink_component_classes(self):
        return _PluginComponentClasses(self, native_bt.COMPONENT_CLASS_TYPE_SINK)
