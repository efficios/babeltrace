# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

import os.path
import collections.abc

from bt2 import utils as bt2_utils
from bt2 import object as bt2_object
from bt2 import component as bt2_component
from bt2 import native_bt


def find_plugins_in_path(path, recurse=True, fail_on_load_error=False):
    bt2_utils._check_str(path)
    bt2_utils._check_bool(recurse)
    bt2_utils._check_bool(fail_on_load_error)
    plugin_set_ptr = None

    if os.path.isfile(path):
        status, plugin_set_ptr = native_bt.bt2_plugin_find_all_from_file(
            path, fail_on_load_error
        )
    elif os.path.isdir(path):
        status, plugin_set_ptr = native_bt.bt2_plugin_find_all_from_dir(
            path, int(recurse), int(fail_on_load_error)
        )
    else:
        raise ValueError("invalid path: '{}'".format(path))

    if status == native_bt.__BT_FUNC_STATUS_NOT_FOUND:
        return

    bt2_utils._handle_func_status(status, "failed to find plugins")
    assert plugin_set_ptr is not None
    return _PluginSet._create_from_ptr(plugin_set_ptr)


def find_plugins(
    find_in_std_env_var=True,
    find_in_user_dir=True,
    find_in_sys_dir=True,
    find_in_static=True,
    fail_on_load_error=False,
):
    bt2_utils._check_bool(find_in_std_env_var)
    bt2_utils._check_bool(find_in_user_dir)
    bt2_utils._check_bool(find_in_sys_dir)
    bt2_utils._check_bool(find_in_static)
    bt2_utils._check_bool(fail_on_load_error)
    plugin_set_ptr = None

    status, plugin_set_ptr = native_bt.bt2_plugin_find_all(
        int(find_in_std_env_var),
        int(find_in_user_dir),
        int(find_in_sys_dir),
        int(find_in_static),
        int(fail_on_load_error),
    )

    if status == native_bt.__BT_FUNC_STATUS_NOT_FOUND:
        return

    bt2_utils._handle_func_status(status, "failed to find plugins")
    assert plugin_set_ptr is not None
    return _PluginSet._create_from_ptr(plugin_set_ptr)


def find_plugin(
    name,
    find_in_std_env_var=True,
    find_in_user_dir=True,
    find_in_sys_dir=True,
    find_in_static=True,
    fail_on_load_error=False,
):
    bt2_utils._check_str(name)
    bt2_utils._check_bool(fail_on_load_error)
    status, ptr = native_bt.bt2_plugin_find(
        name,
        int(find_in_std_env_var),
        int(find_in_user_dir),
        int(find_in_sys_dir),
        int(find_in_static),
        int(fail_on_load_error),
    )

    if status == native_bt.__BT_FUNC_STATUS_NOT_FOUND:
        return

    bt2_utils._handle_func_status(status, "failed to find plugin")
    assert ptr is not None
    return _Plugin._create_from_ptr(ptr)


class _PluginSet(bt2_object._SharedObject, collections.abc.Sequence):
    @staticmethod
    def _put_ref(ptr):
        native_bt.plugin_set_put_ref(ptr)

    @staticmethod
    def _get_ref(ptr):
        native_bt.plugin_set_get_ref(ptr)

    def __len__(self):
        count = native_bt.plugin_set_get_plugin_count(self._ptr)
        assert count >= 0
        return count

    def __getitem__(self, index):
        bt2_utils._check_uint64(index)

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
        extra = ""

        if self._extra is not None:
            extra = self._extra

        return "{}.{}.{}{}".format(self._major, self._minor, self._patch, extra)


class _PluginComponentClassesIterator(collections.abc.Iterator):
    def __init__(self, plugin_comp_cls):
        self._plugin_comp_cls = plugin_comp_cls
        self._at = 0

    def __next__(self):
        plugin_ptr = self._plugin_comp_cls._plugin._ptr
        total = self._plugin_comp_cls._component_class_count(plugin_ptr)

        if self._at == total:
            raise StopIteration

        comp_cls_ptr = self._plugin_comp_cls._borrow_component_class_by_index(
            plugin_ptr, self._at
        )
        assert comp_cls_ptr is not None
        self._at += 1

        comp_cls_type = self._plugin_comp_cls._comp_cls_type
        comp_cls_pycls = bt2_component._COMP_CLS_TYPE_TO_GENERIC_COMP_CLS_PYCLS[
            comp_cls_type
        ]
        comp_cls_ptr = comp_cls_pycls._bt_as_component_class_ptr(comp_cls_ptr)
        name = native_bt.component_class_get_name(comp_cls_ptr)
        assert name is not None
        return name


class _PluginComponentClasses(collections.abc.Mapping):
    def __init__(self, plugin):
        self._plugin = plugin

    def __getitem__(self, key):
        bt2_utils._check_str(key)
        cc_ptr = self._borrow_component_class_by_name(self._plugin._ptr, key)

        if cc_ptr is None:
            raise KeyError(key)

        return bt2_component._create_component_class_from_const_ptr_and_get_ref(
            cc_ptr, self._comp_cls_type
        )

    def __len__(self):
        return self._component_class_count(self._plugin._ptr)

    def __iter__(self):
        return _PluginComponentClassesIterator(self)


class _PluginSourceComponentClasses(_PluginComponentClasses):
    _component_class_count = staticmethod(
        native_bt.plugin_get_source_component_class_count
    )
    _borrow_component_class_by_name = staticmethod(
        native_bt.plugin_borrow_source_component_class_by_name_const
    )
    _borrow_component_class_by_index = staticmethod(
        native_bt.plugin_borrow_source_component_class_by_index_const
    )
    _comp_cls_type = native_bt.COMPONENT_CLASS_TYPE_SOURCE


class _PluginFilterComponentClasses(_PluginComponentClasses):
    _component_class_count = staticmethod(
        native_bt.plugin_get_filter_component_class_count
    )
    _borrow_component_class_by_name = staticmethod(
        native_bt.plugin_borrow_filter_component_class_by_name_const
    )
    _borrow_component_class_by_index = staticmethod(
        native_bt.plugin_borrow_filter_component_class_by_index_const
    )
    _comp_cls_type = native_bt.COMPONENT_CLASS_TYPE_FILTER


class _PluginSinkComponentClasses(_PluginComponentClasses):
    _component_class_count = staticmethod(
        native_bt.plugin_get_sink_component_class_count
    )
    _borrow_component_class_by_name = staticmethod(
        native_bt.plugin_borrow_sink_component_class_by_name_const
    )
    _borrow_component_class_by_index = staticmethod(
        native_bt.plugin_borrow_sink_component_class_by_index_const
    )
    _comp_cls_type = native_bt.COMPONENT_CLASS_TYPE_SINK


class _Plugin(bt2_object._SharedObject):
    @staticmethod
    def _put_ref(ptr):
        native_bt.plugin_put_ref(ptr)

    @staticmethod
    def _get_ref(ptr):
        native_bt.plugin_get_ref(ptr)

    @property
    def name(self):
        name = native_bt.plugin_get_name(self._ptr)
        assert name is not None
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
        status, major, minor, patch, extra = native_bt.bt2_plugin_get_version(self._ptr)

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
