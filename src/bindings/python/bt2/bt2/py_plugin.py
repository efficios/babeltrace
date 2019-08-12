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

from bt2 import utils
from bt2 import component as bt2_component
import sys


# Python plugin path to `_PluginInfo` (cache)
_plugin_infos = {}


def plugin_component_class(component_class):
    if not issubclass(component_class, bt2_component._UserComponent):
        raise TypeError('component class is not a subclass of a user component class')

    component_class._bt_plugin_component_class = None
    return component_class


def register_plugin(
    module_name, name, description=None, author=None, license=None, version=None
):
    if module_name not in sys.modules:
        raise RuntimeError(
            "cannot find module '{}' in loaded modules".format(module_name)
        )

    utils._check_str(name)

    if description is not None:
        utils._check_str(description)

    if author is not None:
        utils._check_str(author)

    if license is not None:
        utils._check_str(license)

    if version is not None:
        if not _validate_version(version):
            raise ValueError(
                'wrong version: expecting a tuple: (major, minor, patch) or (major, minor, patch, extra)'
            )

    sys.modules[module_name]._bt_plugin_info = _PluginInfo(
        name, description, author, license, version
    )


def _validate_version(version):
    if version is None:
        return True

    if not isinstance(version, tuple):
        return False

    if len(version) < 3 or len(version) > 4:
        return False

    if not isinstance(version[0], int):
        return False

    if not isinstance(version[1], int):
        return False

    if not isinstance(version[2], int):
        return False

    if len(version) == 4:
        if not isinstance(version[3], str):
            return False

    return True


class _PluginInfo:
    def __init__(self, name, description, author, license, version):
        self.name = name
        self.description = description
        self.author = author
        self.license = license
        self.version = version
        self.comp_class_addrs = None


# called by the BT plugin system
def _try_load_plugin_module(path):
    if path in _plugin_infos:
        # do not load module and create plugin info twice for this path
        return _plugin_infos[path]

    import importlib.machinery
    import inspect
    import hashlib

    if path is None:
        raise TypeError('missing path')

    # In order to load the module uniquely from its path, even from
    # different files which have the same basename, we hash the path
    # and prefix with `bt_plugin_`. This is its key in sys.modules.
    h = hashlib.sha256()
    h.update(path.encode())
    module_name = 'bt_plugin_{}'.format(h.hexdigest())
    assert module_name not in sys.modules
    # try loading the module: any raised exception is catched by the caller
    mod = importlib.machinery.SourceFileLoader(module_name, path).load_module()

    # we have the module: look for its plugin info first
    if not hasattr(mod, '_bt_plugin_info'):
        raise RuntimeError("missing '_bt_plugin_info' module attribute")

    plugin_info = mod._bt_plugin_info

    # search for user component classes
    def is_user_comp_class(obj):
        if not inspect.isclass(obj):
            return False

        if not hasattr(obj, '_bt_plugin_component_class'):
            return False

        return True

    comp_class_entries = inspect.getmembers(mod, is_user_comp_class)
    plugin_info.comp_class_addrs = [entry[1].addr for entry in comp_class_entries]
    _plugin_infos[path] = plugin_info
    return plugin_info
