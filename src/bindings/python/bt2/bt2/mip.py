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

from bt2 import native_bt, utils
import bt2


def get_greatest_operative_mip_version(
    component_descriptors, log_level=bt2.LoggingLevel.NONE
):
    utils._check_log_level(log_level)
    comp_descr_set_ptr = native_bt.component_descriptor_set_create()

    if comp_descr_set_ptr is None:
        raise bt2._MemoryError('cannot create component descriptor set object')

    if len(component_descriptors) == 0:
        raise ValueError('no component descriptors')

    try:
        for descr in component_descriptors:
            if type(descr) is not bt2.ComponentDescriptor:
                raise TypeError("'{}' is not a component descriptor".format(descr))

            base_cc_ptr = descr.component_class._bt_component_class_ptr()
            params_ptr = None

            if descr.params is not None:
                params_ptr = descr.params._ptr

            status = native_bt.bt2_component_descriptor_set_add_descriptor_with_initialize_method_data(
                comp_descr_set_ptr, base_cc_ptr, params_ptr, descr.obj
            )
            utils._handle_func_status(
                status, 'cannot add descriptor to component descriptor set'
            )

        status, version = native_bt.get_greatest_operative_mip_version(
            comp_descr_set_ptr, log_level
        )

        if status == native_bt.__BT_FUNC_STATUS_NO_MATCH:
            return None

        utils._handle_func_status(status, 'cannot get greatest operative MIP version')
        return version
    finally:
        native_bt.component_descriptor_set_put_ref(comp_descr_set_ptr)


def get_maximal_mip_version():
    return native_bt.get_maximal_mip_version()
