# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 EfficiOS Inc.
#

import unittest

import bt2


class _DummySink(bt2._UserSinkComponent):
    def _user_consume(self):
        pass


class ComponentDescriptorTestCase(unittest.TestCase):
    def setUp(self):
        self._obj = object()
        self._comp_descr = bt2.ComponentDescriptor(_DummySink, {"zoom": -23}, self._obj)

    def _get_comp_cls_from_plugin(self):
        plugin = bt2.find_plugin("text", find_in_user_dir=False, find_in_sys_dir=False)
        assert plugin is not None
        cc = plugin.source_component_classes["dmesg"]
        assert cc is not None
        return cc

    def test_init_invalid_cls_type(self):
        with self.assertRaises(TypeError):
            bt2.ComponentDescriptor(int)

    def test_init_invalid_params_type(self):
        with self.assertRaises(TypeError):
            bt2.ComponentDescriptor(_DummySink, object())

    def test_init_invalid_obj_non_python_comp_cls(self):
        cc = self._get_comp_cls_from_plugin()

        with self.assertRaises(ValueError):
            bt2.ComponentDescriptor(cc, obj=57)

    def test_init_with_user_comp_cls(self):
        bt2.ComponentDescriptor(_DummySink)

    def test_init_with_gen_comp_cls(self):
        cc = self._get_comp_cls_from_plugin()
        bt2.ComponentDescriptor(cc)

    def test_attr_component_class(self):
        self.assertIs(self._comp_descr.component_class, _DummySink)

    def test_attr_params(self):
        self.assertEqual(self._comp_descr.params, {"zoom": -23})

    def test_attr_obj(self):
        self.assertIs(self._comp_descr.obj, self._obj)


if __name__ == "__main__":
    unittest.main()
