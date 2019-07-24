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
from utils import run_in_component_init, get_default_trace_class


class TraceClassTestCase(unittest.TestCase):
    def test_create_default(self):
        def f(comp_self):
            return comp_self._create_trace_class()

        tc = run_in_component_init(f)

        self.assertEqual(len(tc), 0)
        self.assertTrue(tc.assigns_automatic_stream_class_id)

    def test_automatic_stream_class_id(self):
        def f(comp_self):
            return comp_self._create_trace_class(assigns_automatic_stream_class_id=True)

        tc = run_in_component_init(f)
        self.assertTrue(tc.assigns_automatic_stream_class_id)

        # This should not throw.
        sc1 = tc.create_stream_class()
        sc2 = tc.create_stream_class()

        self.assertNotEqual(sc1.id, sc2.id)

    def test_automatic_stream_class_id_raises(self):
        def f(comp_self):
            return comp_self._create_trace_class(assigns_automatic_stream_class_id=True)

        tc = run_in_component_init(f)
        self.assertTrue(tc.assigns_automatic_stream_class_id)

        with self.assertRaises(ValueError):
            sc1 = tc.create_stream_class(23)

    def test_no_assigns_automatic_stream_class_id(self):
        def f(comp_self):
            return comp_self._create_trace_class(
                assigns_automatic_stream_class_id=False
            )

        tc = run_in_component_init(f)
        self.assertFalse(tc.assigns_automatic_stream_class_id)

        sc = tc.create_stream_class(id=28)
        self.assertEqual(sc.id, 28)

    def test_no_assigns_automatic_stream_class_id_raises(self):
        def f(comp_self):
            return comp_self._create_trace_class(
                assigns_automatic_stream_class_id=False
            )

        tc = run_in_component_init(f)
        self.assertFalse(tc.assigns_automatic_stream_class_id)

        # In this mode, it is required to pass an explicit id.
        with self.assertRaises(ValueError):
            tc.create_stream_class()

    @staticmethod
    def _create_trace_class_with_some_stream_classes():
        def f(comp_self):
            return comp_self._create_trace_class(
                assigns_automatic_stream_class_id=False
            )

        tc = run_in_component_init(f)
        sc1 = tc.create_stream_class(id=12)
        sc2 = tc.create_stream_class(id=54)
        sc3 = tc.create_stream_class(id=2018)
        return tc, sc1, sc2, sc3

    def test_getitem(self):
        tc, _, _, sc3 = self._create_trace_class_with_some_stream_classes()
        self.assertEqual(tc[2018].addr, sc3.addr)

    def test_getitem_wrong_key_type(self):
        tc, _, _, _ = self._create_trace_class_with_some_stream_classes()
        with self.assertRaises(TypeError):
            tc['hello']

    def test_getitem_wrong_key(self):
        tc, _, _, _ = self._create_trace_class_with_some_stream_classes()
        with self.assertRaises(KeyError):
            tc[4]

    def test_len(self):
        tc = get_default_trace_class()
        self.assertEqual(len(tc), 0)
        tc.create_stream_class()
        self.assertEqual(len(tc), 1)

    def test_iter(self):
        tc, sc1, sc2, sc3 = self._create_trace_class_with_some_stream_classes()

        for sc_id, stream_class in tc.items():
            if sc_id == 12:
                self.assertEqual(stream_class.addr, sc1.addr)
            elif sc_id == 54:
                self.assertEqual(stream_class.addr, sc2.addr)
            elif sc_id == 2018:
                self.assertEqual(stream_class.addr, sc3.addr)

    def test_destruction_listener(self):
        def on_trace_class_destruction(trace_class):
            nonlocal trace_class_destroyed
            trace_class_destroyed = True

        trace_class_destroyed = False

        trace_class = get_default_trace_class()
        trace_class.add_destruction_listener(on_trace_class_destruction)

        self.assertFalse(trace_class_destroyed)

        del trace_class

        self.assertTrue(trace_class_destroyed)
