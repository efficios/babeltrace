#
# Copyright (C) 2019 EfficiOS Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; only version 2
# of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#

import unittest
import bt2


class _DummySink(bt2._UserSinkComponent):
    def _user_consume(self):
        pass


class ComponentDescriptorTestCase(unittest.TestCase):
    def setUp(self):
        self._obj = object()
        self._comp_descr = bt2.ComponentDescriptor(_DummySink, {'zoom': -23}, self._obj)

    def _get_comp_cls_from_plugin(self):
        plugin = bt2.find_plugin('text', find_in_user_dir=False, find_in_sys_dir=False)
        assert plugin is not None
        cc = plugin.source_component_classes['dmesg']
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
        self.assertEqual(self._comp_descr.params, {'zoom': -23})

    def test_attr_obj(self):
        self.assertIs(self._comp_descr.obj, self._obj)
