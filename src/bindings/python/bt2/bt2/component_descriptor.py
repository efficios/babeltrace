# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

from bt2 import native_bt
from bt2 import component as bt2_component
import bt2


def _is_source_comp_cls(comp_cls):
    if isinstance(comp_cls, bt2_component._SourceComponentClassConst):
        return True

    try:
        return issubclass(comp_cls, bt2_component._UserSourceComponent)
    except Exception:
        return False


def _is_filter_comp_cls(comp_cls):
    if isinstance(comp_cls, bt2_component._FilterComponentClassConst):
        return True

    try:
        return issubclass(comp_cls, bt2_component._UserFilterComponent)
    except Exception:
        return False


def _is_sink_comp_cls(comp_cls):
    if isinstance(comp_cls, bt2_component._SinkComponentClassConst):
        return True

    try:
        return issubclass(comp_cls, bt2_component._UserSinkComponent)
    except Exception:
        return False


class ComponentDescriptor:
    def __init__(self, component_class, params=None, obj=None):
        if (
            not _is_source_comp_cls(component_class)
            and not _is_filter_comp_cls(component_class)
            and not _is_sink_comp_cls(component_class)
        ):
            raise TypeError(
                "'{}' is not a component class".format(
                    component_class.__class__.__name__
                )
            )

        base_cc_ptr = component_class._bt_component_class_ptr()

        if obj is not None and not native_bt.bt2_is_python_component_class(base_cc_ptr):
            raise ValueError("cannot pass a Python object to a non-Python component")

        self._comp_cls = component_class
        self._params = bt2.create_value(params)
        self._obj = obj

    @property
    def component_class(self):
        return self._comp_cls

    @property
    def params(self):
        return self._params

    @property
    def obj(self):
        return self._obj
