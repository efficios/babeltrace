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
import bt2


def _plugin_ptrs_to_plugins(plugin_ptrs):
    plugins = []

    for plugin_ptr in plugin_ptrs:
        plugin = _Plugin._create_from_ptr(plugin_ptr)
        plugins.append(plugin)

    return plugins


def create_plugins_from_file(path):
    utils._check_str(path)
    plugin_ptrs = native_bt.py3_plugin_create_all_from_file(path)

    if plugin_ptrs is None:
        raise bt2.Error('cannot get plugin objects from file')

    return _plugin_ptrs_to_plugins(plugin_ptrs)


def create_plugins_from_dir(path, recurse=True):
    utils._check_str(path)
    plugin_ptrs = native_bt.py3_plugin_create_all_from_dir(path, recurse)

    if plugin_ptrs is None:
        raise bt2.Error('cannot get plugin objects from directory')

    return _plugin_ptrs_to_plugins(plugin_ptrs)


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


class _Plugin(object._Object, collections.abc.Sequence):
    @property
    def name(self):
        name = native_bt.plugin_get_name(self._ptr)
        utils._handle_ptr(name, "cannot get plugin object's name")
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

    def source_component_class(self, name):
        utils._check_str(name)
        cc_ptr = native_bt.plugin_get_component_class_by_name_and_type(self._ptr,
                                                                       name,
                                                                       native_bt.COMPONENT_CLASS_TYPE_SOURCE)

        if cc_ptr is None:
            return

        return bt2.component._create_generic_component_class_from_ptr(cc_ptr)

    def filter_component_class(self, name):
        utils._check_str(name)
        cc_ptr = native_bt.plugin_get_component_class_by_name_and_type(self._ptr,
                                                                       name,
                                                                       native_bt.COMPONENT_CLASS_TYPE_FILTER)

        if cc_ptr is None:
            return

        return bt2.component._create_generic_component_class_from_ptr(cc_ptr)

    def sink_component_class(self, name):
        utils._check_str(name)
        cc_ptr = native_bt.plugin_get_component_class_by_name_and_type(self._ptr,
                                                                       name,
                                                                       native_bt.COMPONENT_CLASS_TYPE_SINK)

        if cc_ptr is None:
            return

        return bt2.component._create_generic_component_class_from_ptr(cc_ptr)

    def __len__(self):
        count = native_bt.plugin_get_component_class_count(self._ptr)
        utils._handle_ret(count, "cannot get plugin object's component class count")
        return count

    def __getitem__(self, index):
        utils._check_uint64(index)

        if index >= len(self):
            raise IndexError

        cc_ptr = native_bt.plugin_get_component_class(self._ptr, index)
        utils._handle_ptr(cc_ptr, "cannot get plugin object's component class object")
        return bt2.component._create_generic_component_class_from_ptr(cc_ptr)
