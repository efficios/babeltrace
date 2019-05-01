from bt2 import value
import unittest
import copy
import bt2


class UserComponentTestCase(unittest.TestCase):
    @staticmethod
    def _create_comp(comp_cls, name=None):
        graph = bt2.Graph()

        if name is None:
            name = 'comp'

        return graph.add_sink_component(comp_cls, name)

    def test_name(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                self.assertEqual(comp_self.name, 'yaes')

            def _consume(self):
                pass

        comp = self._create_comp(MySink, 'yaes')

    def test_class(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                self.assertEqual(comp_self.component_class, MySink)

            def _consume(self):
                pass

        self._create_comp(MySink)

    def test_addr(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                self.assertIsInstance(comp_self.addr, int)
                self.assertNotEqual(comp_self.addr, 0)

            def _consume(self):
                pass

        self._create_comp(MySink)

    def test_finalize(self):
        finalized = False

        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

            def _finalize(comp_self):
                nonlocal finalized
                finalized = True

        graph = bt2.Graph()
        comp = graph.add_sink_component(MySink, 'lel')

        del graph
        del comp
        self.assertTrue(finalized)


class GenericComponentTestCase(unittest.TestCase):
    @staticmethod
    def _create_comp(comp_cls, name=None):
        graph = bt2.Graph()

        if name is None:
            name = 'comp'

        return graph.add_sink_component(comp_cls, name)

    def test_name(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

        comp = self._create_comp(MySink, 'yaes')
        self.assertEqual(comp.name, 'yaes')

    def test_class(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

        comp = self._create_comp(MySink)
        self.assertEqual(comp.component_class, MySink)

    def test_addr(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

        comp = self._create_comp(MySink)
        self.assertIsInstance(comp.addr, int)
        self.assertNotEqual(comp.addr, 0)
