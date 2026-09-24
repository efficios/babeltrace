# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

import typing

from bt2 import error as bt2_error
from bt2 import utils as bt2_utils
from bt2 import logging as bt2_logging
from bt2 import native_bt
from bt2 import component_descriptor as bt2_component_descriptor


def get_greatest_operative_mip_version(
    component_descriptors: typing.Sequence[
        bt2_component_descriptor.ComponentDescriptor
    ],
    log_level: bt2_logging.LoggingLevel = bt2_logging.LoggingLevel.NONE,
) -> typing.Optional[int]:
    bt2_utils._check_type(log_level, bt2_logging.LoggingLevel)
    comp_descr_set_ptr = native_bt.component_descriptor_set_create()

    if comp_descr_set_ptr is None:
        raise bt2_error._MemoryError("cannot create component descriptor set object")

    if len(component_descriptors) == 0:
        raise ValueError("no component descriptors")

    try:
        for descr in component_descriptors:
            if type(descr) is not bt2_component_descriptor.ComponentDescriptor:
                raise TypeError(f"'{descr}' is not a component descriptor")

            native_bt.component_descriptor_set_add_descriptor(
                comp_descr_set_ptr,
                descr.component_class._bt_component_class_ptr(),
                descr.params._ptr if descr.params is not None else None,
                descr.obj,
            )

        return native_bt.get_greatest_operative_mip_version(
            comp_descr_set_ptr, log_level.value
        )
    finally:
        native_bt.component_descriptor_set_put_ref(comp_descr_set_ptr)


def get_maximal_mip_version():
    return native_bt.get_maximal_mip_version()
