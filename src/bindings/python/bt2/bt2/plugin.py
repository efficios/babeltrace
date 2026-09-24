# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

import typing
import os.path

from bt2 import error as bt2_error
from bt2 import utils as bt2_utils
from bt2 import object as bt2_object
from bt2 import component as bt2_component
from bt2 import native_bt


class _Version:
    def __init__(self, major, minor, patch, extra):
        self._major = major
        self._minor = minor
        self._patch = patch
        self._extra = extra

    @property
    def major(self) -> int:
        return self._major

    @property
    def minor(self) -> int:
        return self._minor

    @property
    def patch(self) -> int:
        return self._patch

    @property
    def extra(self) -> typing.Optional[str]:
        return self._extra

    def __str__(self) -> str:
        return f"{self._major}.{self._minor}.{self._patch}{self._extra if self._extra is not None else ''}"


class _PluginVersion(_Version):
    pass


class _PluginProviderVersion(_Version):
    pass


class _PluginProvider(bt2_object._BaseObject):
    # A plugin provider is not reference-counted: it remains valid for the
    # lifetime of libbabeltrace2, so this is a plain wrapper around the
    # borrowed pointer.

    @property
    def name(self) -> str:
        return native_bt.plugin_provider_get_name(self._ptr)

    @property
    def description(self) -> typing.Optional[str]:
        return native_bt.plugin_provider_get_description(self._ptr)

    @property
    def author(self) -> typing.Optional[str]:
        return native_bt.plugin_provider_get_author(self._ptr)

    @property
    def license(self) -> typing.Optional[str]:
        return native_bt.plugin_provider_get_license(self._ptr)

    @property
    def path(self) -> typing.Optional[str]:
        return native_bt.plugin_provider_get_path(self._ptr)

    @property
    def version(self) -> typing.Optional[_PluginProviderVersion]:
        version = native_bt.plugin_provider_get_version(self._ptr)

        if version is None:
            return

        return _PluginProviderVersion(*version)


class _PluginProviderSet(bt2_object._BaseObject, typing.Sequence[_PluginProvider]):
    # The plugin provider set is not reference-counted: it remains valid
    # for the lifetime of libbabeltrace2.

    def __len__(self) -> int:
        return native_bt.plugin_provider_set_get_plugin_provider_count(self._ptr)

    def __getitem__(self, index: int) -> _PluginProvider:
        bt2_utils._check_uint64(index)

        if index >= len(self):
            raise IndexError

        return _PluginProvider(
            native_bt.plugin_provider_set_borrow_plugin_provider_by_index(
                self._ptr, index
            )
        )


def plugin_providers() -> _PluginProviderSet:
    return _PluginProviderSet(native_bt.plugin_provider_set_borrow())


class _PluginComponentClassesIterator(typing.Iterator[str]):
    def __init__(self, plugin_comp_cls):
        self._plugin_comp_cls = plugin_comp_cls
        self._at = 0

    def __next__(self) -> str:
        plugin_ptr = self._plugin_comp_cls._plugin._ptr

        if self._at == self._plugin_comp_cls._component_class_count(plugin_ptr):
            raise StopIteration

        comp_cls_ptr = self._plugin_comp_cls._borrow_component_class_by_index(
            plugin_ptr, self._at
        )
        self._at += 1

        return native_bt.component_class_get_name(
            bt2_component._COMP_CLS_TYPE_TO_GENERIC_COMP_CLS_PYCLS[
                self._plugin_comp_cls._comp_cls_type
            ]._bt_as_component_class_ptr(comp_cls_ptr)
        )


_ComponentClassT = typing.TypeVar(
    "_ComponentClassT", bound=bt2_component._ComponentClassConst
)


class _PluginComponentClasses(typing.Mapping[str, _ComponentClassT]):
    def __init__(self, plugin):
        self._plugin = plugin

    # items() and values() are not provided by typing.Mapping as of Python 3.5
    # so we provide them ourselves.
    def items(self) -> typing.ItemsView[str, _ComponentClassT]:
        return ((key, self[key]) for key in self)

    def values(self) -> typing.ValuesView[_ComponentClassT]:
        return {key: self[key] for key in self}.values()

    def __getitem__(self, key: str) -> _ComponentClassT:
        bt2_utils._check_str(key)
        cc_ptr = self._borrow_component_class_by_name(self._plugin._ptr, key)

        if cc_ptr is None:
            raise KeyError(key)

        return bt2_component._create_component_class_from_const_ptr_and_get_ref(
            cc_ptr, self._comp_cls_type
        )

    def __len__(self) -> int:
        return self._component_class_count(self._plugin._ptr)

    def __iter__(self) -> typing.Iterator[str]:
        return _PluginComponentClassesIterator(self)

    # __conrains__ is not provided by typing.Mapping as of Python 3.5
    # so we provide it ourselves.
    def __contains__(self, key: str) -> bool:
        try:
            return self.__getitem__(key) is not None
        except KeyError:
            return False


class _PluginSourceComponentClasses(
    _PluginComponentClasses[bt2_component._SourceComponentClassConst]
):
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


class _PluginFilterComponentClasses(
    _PluginComponentClasses[bt2_component._FilterComponentClassConst]
):
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


class _PluginSinkComponentClasses(
    _PluginComponentClasses[bt2_component._SinkComponentClassConst]
):
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
    def name(self) -> str:
        return native_bt.plugin_get_name(self._ptr)

    @property
    def author(self) -> typing.Optional[str]:
        return native_bt.plugin_get_author(self._ptr)

    @property
    def license(self) -> typing.Optional[str]:
        return native_bt.plugin_get_license(self._ptr)

    @property
    def description(self) -> typing.Optional[str]:
        return native_bt.plugin_get_description(self._ptr)

    @property
    def path(self) -> typing.Optional[str]:
        return native_bt.plugin_get_path(self._ptr)

    @property
    def version(self) -> typing.Optional[_PluginVersion]:
        version = native_bt.plugin_get_version(self._ptr)

        if version is None:
            return

        return _PluginVersion(*version)

    @property
    def provider(self) -> typing.Optional[_PluginProvider]:
        ptr = native_bt.plugin_borrow_provider(self._ptr)

        if ptr is None:
            return

        return _PluginProvider(ptr)

    @property
    def source_component_classes(
        self,
    ) -> typing.Mapping[str, bt2_component._SourceComponentClassConst]:
        return _PluginSourceComponentClasses(self)

    @property
    def filter_component_classes(
        self,
    ) -> typing.Mapping[str, bt2_component._FilterComponentClassConst]:
        return _PluginFilterComponentClasses(self)

    @property
    def sink_component_classes(
        self,
    ) -> typing.Mapping[str, bt2_component._SinkComponentClassConst]:
        return _PluginSinkComponentClasses(self)


# `_UserPlugin` is only meant to be used by the Python plugin provider,
# and is therefore not exported by the `bt2` module.
#
# We use `assert` instead of functions like `bt2_utils._check_str()` to
# check inputs, because this is all internal code.
class _UserPlugin(_Plugin):
    def __init__(self, name):
        assert isinstance(name, str)
        ptr = native_bt.plugin_create(name)

        if ptr is None:
            raise bt2_error._MemoryError("could not create plugin object")

        super().__init__(ptr)

    def _set_author(self, author):
        assert isinstance(author, str)
        bt2_utils._handle_func_status(
            native_bt.plugin_set_author(self._ptr, author),
            "cannot set plugin object's author",
        )

    def _set_license(self, license):
        assert isinstance(license, str)
        bt2_utils._handle_func_status(
            native_bt.plugin_set_license(self._ptr, license),
            "cannot set plugin object's license",
        )

    def _set_description(self, description):
        assert isinstance(description, str)
        bt2_utils._handle_func_status(
            native_bt.plugin_set_description(self._ptr, description),
            "cannot set plugin object's description",
        )

    def _set_path(self, path):
        assert isinstance(path, str)
        bt2_utils._handle_func_status(
            native_bt.plugin_set_path(self._ptr, path),
            "cannot set plugin object's path",
        )

    def _set_version(self, major, minor, patch, extra=None):
        assert isinstance(major, int) and major >= 0
        assert isinstance(minor, int) and minor >= 0
        assert isinstance(patch, int) and patch >= 0

        if extra is not None:
            assert isinstance(extra, str)

        bt2_utils._handle_func_status(
            native_bt.plugin_set_version(self._ptr, major, minor, patch, extra),
            "cannot set plugin object's version",
        )

    def _add_component_class(self, comp_class):
        assert isinstance(comp_class, type)
        assert issubclass(comp_class, bt2_component._UserComponent)
        assert hasattr(comp_class, "_bt_plugin_component_class")

        bt2_utils._handle_func_status(
            native_bt.plugin_add_component_class(
                self._ptr,
                comp_class._bt_as_component_class_ptr(comp_class._bt_cc_ptr),
            ),
            "cannot add component class to plugin",
        )


class _PluginSet(bt2_object._SharedObject, typing.Sequence[_Plugin]):
    @staticmethod
    def _put_ref(ptr):
        native_bt.plugin_set_put_ref(ptr)

    @staticmethod
    def _get_ref(ptr):
        native_bt.plugin_set_get_ref(ptr)

    def __len__(self) -> int:
        return native_bt.plugin_set_get_plugin_count(self._ptr)

    def __getitem__(self, index: int) -> _Plugin:
        bt2_utils._check_uint64(index)

        if index >= len(self):
            raise IndexError

        return _Plugin._create_from_ptr_and_get_ref(
            native_bt.plugin_set_borrow_plugin_by_index_const(self._ptr, index)
        )


def find_plugins_in_path(
    path: str, recurse: bool = True, fail_on_load_error: bool = False
) -> typing.Optional[_PluginSet]:
    bt2_utils._check_str(path)
    bt2_utils._check_bool(recurse)
    bt2_utils._check_bool(fail_on_load_error)

    if os.path.isfile(path):
        plugin_set_ptr = native_bt.plugin_find_all_from_file(path, fail_on_load_error)
    elif os.path.isdir(path):
        plugin_set_ptr = native_bt.plugin_find_all_from_dir(
            path, recurse, fail_on_load_error
        )
    else:
        raise ValueError(f"invalid path: '{path}'")

    if plugin_set_ptr is None:
        return

    return _PluginSet._create_from_ptr(plugin_set_ptr)


def find_plugins(
    find_in_std_env_var: bool = True,
    find_in_user_dir: bool = True,
    find_in_sys_dir: bool = True,
    find_in_static: bool = True,
    fail_on_load_error: bool = False,
) -> typing.Optional[_PluginSet]:
    bt2_utils._check_bool(find_in_std_env_var)
    bt2_utils._check_bool(find_in_user_dir)
    bt2_utils._check_bool(find_in_sys_dir)
    bt2_utils._check_bool(find_in_static)
    bt2_utils._check_bool(fail_on_load_error)
    plugin_set_ptr = native_bt.plugin_find_all(
        find_in_std_env_var,
        find_in_user_dir,
        find_in_sys_dir,
        find_in_static,
        fail_on_load_error,
    )

    if plugin_set_ptr is None:
        return

    return _PluginSet._create_from_ptr(plugin_set_ptr)


def find_plugin(
    name: str,
    find_in_std_env_var: bool = True,
    find_in_user_dir: bool = True,
    find_in_sys_dir: bool = True,
    find_in_static: bool = True,
    fail_on_load_error: bool = False,
) -> typing.Optional[_Plugin]:
    bt2_utils._check_str(name)
    bt2_utils._check_bool(fail_on_load_error)
    ptr = native_bt.plugin_find(
        name,
        bool(find_in_std_env_var),
        bool(find_in_user_dir),
        bool(find_in_sys_dir),
        bool(find_in_static),
        fail_on_load_error,
    )

    if ptr is None:
        return

    return _Plugin._create_from_ptr(ptr)
