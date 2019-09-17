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
from bt2 import component as bt2_component


class UserComponentTestCase(unittest.TestCase):
    @staticmethod
    def _create_comp(comp_cls, name=None, log_level=bt2.LoggingLevel.NONE):
        graph = bt2.Graph()

        if name is None:
            name = 'comp'

        return graph.add_component(comp_cls, name, logging_level=log_level)

    def test_name(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                self.assertEqual(comp_self.name, 'yaes')

            def _user_consume(self):
                pass

        self._create_comp(MySink, 'yaes')

    def test_logging_level(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                self.assertEqual(comp_self.logging_level, bt2.LoggingLevel.INFO)

            def _user_consume(self):
                pass

        self._create_comp(MySink, 'yaes', bt2.LoggingLevel.INFO)

    def test_graph_mip_version(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                self.assertEqual(comp_self._graph_mip_version, 0)

            def _user_consume(self):
                pass

        self._create_comp(MySink, 'yaes', bt2.LoggingLevel.INFO)

    def test_class(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                self.assertEqual(comp_self.cls, MySink)

            def _user_consume(self):
                pass

        self._create_comp(MySink)

    def test_addr(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                self.assertIsInstance(comp_self.addr, int)
                self.assertNotEqual(comp_self.addr, 0)

            def _user_consume(self):
                pass

        self._create_comp(MySink)

    def test_finalize(self):
        finalized = False

        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            def _user_finalize(comp_self):
                nonlocal finalized
                finalized = True

        graph = bt2.Graph()
        comp = graph.add_component(MySink, 'lel')

        del graph
        del comp
        self.assertTrue(finalized)

    def test_source_component_config(self):
        class MySource(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                nonlocal cfg_type
                cfg_type = type(config)

        cfg_type = None
        self._create_comp(MySource)
        self.assertIs(cfg_type, bt2_component._UserSourceComponentConfiguration)

    def test_filter_component_config(self):
        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                nonlocal cfg_type
                cfg_type = type(config)

        cfg_type = None
        self._create_comp(MyFilter)
        self.assertIs(cfg_type, bt2_component._UserFilterComponentConfiguration)

    def test_sink_component_config(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                nonlocal cfg_type
                cfg_type = type(config)

            def _user_consume(self):
                pass

        cfg_type = None
        self._create_comp(MySink)
        self.assertIs(cfg_type, bt2_component._UserSinkComponentConfiguration)


class GenericComponentTestCase(unittest.TestCase):
    @staticmethod
    def _create_comp(comp_cls, name=None, log_level=bt2.LoggingLevel.NONE):
        graph = bt2.Graph()

        if name is None:
            name = 'comp'

        return graph.add_component(comp_cls, name, logging_level=log_level)

    def test_name(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

        comp = self._create_comp(MySink, 'yaes')
        self.assertEqual(comp.name, 'yaes')

    def test_logging_level(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

        comp = self._create_comp(MySink, 'yaes', bt2.LoggingLevel.WARNING)
        self.assertEqual(comp.logging_level, bt2.LoggingLevel.WARNING)

    def test_class(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

        comp = self._create_comp(MySink)
        self.assertEqual(comp.cls, MySink)

    def test_addr(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

        comp = self._create_comp(MySink)
        self.assertIsInstance(comp.addr, int)
        self.assertNotEqual(comp.addr, 0)


if __name__ == '__main__':
    unittest.main()
