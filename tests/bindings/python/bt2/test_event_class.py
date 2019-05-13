import unittest
import bt2
from utils import get_default_trace_class


class EventClassTestCase(unittest.TestCase):
    def setUp(self):
        self._tc = get_default_trace_class()

        self._context_fc = self._tc.create_structure_field_class()
        self._context_fc.append_field('allo', self._tc.create_string_field_class())
        self._context_fc.append_field('zola', self._tc.create_signed_integer_field_class(18))

        self._payload_fc = self._tc.create_structure_field_class()
        self._payload_fc.append_field('zoom', self._tc.create_string_field_class())

        self._stream_class = self._tc.create_stream_class(assigns_automatic_event_class_id=True)

    def test_create_default(self):
        ec = self._stream_class.create_event_class()

        self.assertIsNone(ec.name, 'my_event')
        self.assertTrue(type(ec.id), int)
        self.assertIsNone(ec.specific_context_field_class)
        self.assertIsNone(ec.payload_field_class)
        self.assertIsNone(ec.emf_uri)
        self.assertIsNone(ec.log_level)

    def test_create_invalid_id(self):
        sc = self._tc.create_stream_class(assigns_automatic_event_class_id=False)
        with self.assertRaises(TypeError):
            sc.create_event_class(id='lel')

    def test_create_specific_context_field_class(self):
        fc = self._tc.create_structure_field_class()
        ec = self._stream_class.create_event_class(specific_context_field_class=fc)
        self.assertEqual(ec.specific_context_field_class.addr, fc.addr)

    def test_create_invalid_specific_context_field_class(self):
        with self.assertRaises(TypeError):
            self._stream_class.create_event_class(specific_context_field_class='lel')

    def test_create_payload_field_class(self):
        fc = self._tc.create_structure_field_class()
        ec = self._stream_class.create_event_class(payload_field_class=fc)
        self.assertEqual(ec.payload_field_class.addr, fc.addr)

    def test_create_invalid_payload_field_class(self):
        with self.assertRaises(TypeError):
            self._stream_class.create_event_class(payload_field_class='lel')

    def test_create_name(self):
        ec = self._stream_class.create_event_class(name='viande à chien')
        self.assertEqual(ec.name, 'viande à chien')

    def test_create_invalid_name(self):
        with self.assertRaises(TypeError):
            self._stream_class.create_event_class(name=2)

    def test_emf_uri(self):
        ec = self._stream_class.create_event_class(emf_uri='salut')
        self.assertEqual(ec.emf_uri, 'salut')

    def test_create_invalid_emf_uri(self):
        with self.assertRaises(TypeError):
            self._stream_class.create_event_class(emf_uri=23)

    def test_create_log_level(self):
        ec = self._stream_class.create_event_class(log_level=bt2.EventClassLogLevel.EMERGENCY)
        self.assertEqual(ec.log_level, bt2.EventClassLogLevel.EMERGENCY)

    def test_create_invalid_log_level(self):
        with self.assertRaises(ValueError):
            self._stream_class.create_event_class(log_level='zoom')

    def test_stream_class(self):
        ec = self._stream_class.create_event_class()
        self.assertEqual(ec.stream_class.addr, self._stream_class.addr)
