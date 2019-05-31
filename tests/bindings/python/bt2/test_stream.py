import unittest
from utils import run_in_component_init


class StreamTestCase(unittest.TestCase):
    def setUp(self):
        def f(comp_self):
            return comp_self._create_trace_class()

        self._tc = run_in_component_init(f)
        self._sc = self._tc.create_stream_class(assigns_automatic_stream_id=True)
        self._tr = self._tc()

    def test_create_default(self):
        stream = self._tr.create_stream(self._sc)
        self.assertIsNone(stream.name)

    def test_name(self):
        stream = self._tr.create_stream(self._sc, name='équidistant')
        self.assertEqual(stream.name, 'équidistant')

    def test_invalid_name(self):
        with self.assertRaises(TypeError):
            self._tr.create_stream(self._sc, name=22)

    def test_stream_class(self):
        stream = self._tr.create_stream(self._sc)
        self.assertEqual(stream.stream_class, self._sc)

    def test_invalid_id(self):
        sc = self._tc.create_stream_class(assigns_automatic_stream_id=False)

        with self.assertRaises(TypeError):
            self._tr.create_stream(sc, id='string')
