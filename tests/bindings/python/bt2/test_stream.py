from collections import OrderedDict
from bt2 import value
import unittest
import copy
import bt2


@unittest.skip("this is broken")
class StreamTestCase(unittest.TestCase):
    def setUp(self):
        self._stream = self._create_stream(stream_id=23)

    def tearDown(self):
        del self._stream

    def _create_stream(self, name='my_stream', stream_id=None):
        # event header
        eh = bt2.StructureFieldClass()
        eh += OrderedDict((
            ('id', bt2.IntegerFieldClass(8)),
            ('ts', bt2.IntegerFieldClass(32)),
        ))

        # stream event context
        sec = bt2.StructureFieldClass()
        sec += OrderedDict((
            ('cpu_id', bt2.IntegerFieldClass(8)),
            ('stuff', bt2.FloatingPointNumberFieldClass()),
        ))

        # packet context
        pc = bt2.StructureFieldClass()
        pc += OrderedDict((
            ('something', bt2.IntegerFieldClass(8)),
            ('something_else', bt2.FloatingPointNumberFieldClass()),
        ))

        # stream class
        sc = bt2.StreamClass()
        sc.packet_context_field_class = pc
        sc.event_header_field_class = eh
        sc.event_context_field_class = sec

        # event context
        ec = bt2.StructureFieldClass()
        ec += OrderedDict((
            ('ant', bt2.IntegerFieldClass(16, is_signed=True)),
            ('msg', bt2.StringFieldClass()),
        ))

        # event payload
        ep = bt2.StructureFieldClass()
        ep += OrderedDict((
            ('giraffe', bt2.IntegerFieldClass(32)),
            ('gnu', bt2.IntegerFieldClass(8)),
            ('mosquito', bt2.IntegerFieldClass(8)),
        ))

        # event class
        event_class = bt2.EventClass('ec')
        event_class.context_field_class = ec
        event_class.payload_field_class = ep
        sc.add_event_class(event_class)

        # packet header
        ph = bt2.StructureFieldClass()
        ph += OrderedDict((
            ('magic', bt2.IntegerFieldClass(32)),
            ('stream_id', bt2.IntegerFieldClass(16)),
        ))

        # trace c;ass
        tc = bt2.Trace()
        tc.packet_header_field_class = ph
        tc.add_stream_class(sc)

        # stream
        return sc(name=name, id=stream_id)

    def test_attr_stream_class(self):
        self.assertIsNotNone(self._stream.stream_class)

    def test_attr_name(self):
        self.assertEqual(self._stream.name, 'my_stream')

    def test_eq(self):
        stream1 = self._create_stream(stream_id=17)
        stream2 = self._create_stream(stream_id=17)
        self.assertEqual(stream1, stream2)

    def test_ne_name(self):
        stream1 = self._create_stream(stream_id=17)
        stream2 = self._create_stream('lel', 17)
        self.assertNotEqual(stream1, stream2)

    def test_ne_id(self):
        stream1 = self._create_stream(stream_id=17)
        stream2 = self._create_stream(stream_id=23)
        self.assertNotEqual(stream1, stream2)

    def test_eq_invalid(self):
        self.assertFalse(self._stream == 23)

    def _test_copy(self, func):
        stream = self._create_stream()
        cpy = func(stream)
        self.assertIsNot(stream, cpy)
        self.assertNotEqual(stream.addr, cpy.addr)
        self.assertEqual(stream, cpy)

    def test_copy(self):
        self._test_copy(copy.copy)

    def test_deepcopy(self):
        self._test_copy(copy.deepcopy)
