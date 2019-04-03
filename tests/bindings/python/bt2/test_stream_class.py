from bt2 import values
import unittest
import copy
import bt2


@unittest.skip("this is broken")
class StreamClassTestCase(unittest.TestCase):
    def setUp(self):
        self._packet_context_fc = bt2.StructureFieldClass()
        self._packet_context_fc.append_field('menu', bt2.FloatingPointNumberFieldClass())
        self._packet_context_fc.append_field('sticker', bt2.StringFieldClass())
        self._event_header_fc = bt2.StructureFieldClass()
        self._event_header_fc.append_field('id', bt2.IntegerFieldClass(19))
        self._event_context_fc = bt2.StructureFieldClass()
        self._event_context_fc.append_field('msg', bt2.StringFieldClass())
        self._ec1, self._ec2 = self._create_event_classes()
        self._sc = bt2.StreamClass(name='my_stream_class', id=12,
                                   packet_context_field_class=self._packet_context_fc,
                                   event_header_field_class=self._event_header_fc,
                                   event_context_field_class=self._event_context_fc,
                                   event_classes=(self._ec1, self._ec2))

    def tearDown(self):
        del self._packet_context_fc
        del self._event_header_fc
        del self._event_context_fc
        del self._ec1
        del self._sc

    def _create_event_classes(self):
        context_fc = bt2.StructureFieldClass()
        context_fc.append_field('allo', bt2.StringFieldClass())
        context_fc.append_field('zola', bt2.IntegerFieldClass(18))
        payload_fc = bt2.StructureFieldClass()
        payload_fc.append_field('zoom', bt2.StringFieldClass())
        ec1 = bt2.EventClass('event23', id=23, context_field_class=context_fc,
                             payload_field_class=payload_fc)
        ec2 = bt2.EventClass('event17', id=17, context_field_class=payload_fc,
                             payload_field_class=context_fc)
        return ec1, ec2

    def test_create(self):
        self.assertEqual(self._sc.name, 'my_stream_class')
        self.assertEqual(self._sc.id, 12)
        self.assertEqual(self._sc.packet_context_field_class, self._packet_context_fc)
        self.assertEqual(self._sc.event_header_field_class, self._event_header_fc)
        self.assertEqual(self._sc.event_context_field_class, self._event_context_fc)
        self.assertEqual(self._sc[23], self._ec1)
        self.assertEqual(self._sc[17], self._ec2)
        self.assertEqual(len(self._sc), 2)

    def test_assign_name(self):
        self._sc.name = 'lel'
        self.assertEqual(self._sc.name, 'lel')

    def test_assign_invalid_name(self):
        with self.assertRaises(TypeError):
            self._sc.name = 17

    def test_assign_id(self):
        self._sc.id = 1717
        self.assertEqual(self._sc.id, 1717)

    def test_assign_invalid_id(self):
        with self.assertRaises(TypeError):
            self._sc.id = 'lel'

    def test_no_id(self):
        sc = bt2.StreamClass()
        self.assertIsNone(sc.id)

    def test_assign_packet_context_field_class(self):
        self._sc.packet_context_field_class = self._event_context_fc
        self.assertEqual(self._sc.packet_context_field_class, self._event_context_fc)

    def test_assign_no_packet_context_field_class(self):
        self._sc.packet_context_field_class = None
        self.assertIsNone(self._sc.packet_context_field_class)

    def test_assign_invalid_packet_context_field_class(self):
        with self.assertRaises(TypeError):
            self._sc.packet_context_field_class = 'lel'

    def test_assign_event_header_field_class(self):
        self._sc.event_header_field_class = self._event_header_fc
        self.assertEqual(self._sc.event_header_field_class, self._event_header_fc)

    def test_assign_no_event_header_field_class(self):
        self._sc.event_header_field_class = None
        self.assertIsNone(self._sc.event_header_field_class)

    def test_assign_invalid_event_header_field_class(self):
        with self.assertRaises(TypeError):
            self._sc.event_header_field_class = 'lel'

    def test_assign_event_context_field_class(self):
        self._sc.event_context_field_class = self._packet_context_fc
        self.assertEqual(self._sc.event_context_field_class, self._packet_context_fc)

    def test_assign_no_event_context_field_class(self):
        self._sc.event_context_field_class = None
        self.assertIsNone(self._sc.event_context_field_class)

    def test_assign_invalid_event_context_field_class(self):
        with self.assertRaises(TypeError):
            self._sc.event_context_field_class = 'lel'

    def test_trace_prop_no_tc(self):
        self.assertIsNone(self._sc.trace)

    def test_trace_prop(self):
        tc = bt2.Trace()
        tc.add_stream_class(self._sc)
        self.assertEqual(self._sc.trace.addr, tc.addr)

    def _test_copy(self, cpy):
        self.assertIsNot(cpy, self._sc)
        self.assertNotEqual(cpy.addr, self._sc.addr)
        self.assertEqual(cpy, self._sc)

    def test_copy(self):
        cpy = copy.copy(self._sc)
        self._test_copy(cpy)
        self.assertEqual(self._sc.packet_context_field_class.addr, cpy.packet_context_field_class.addr)
        self.assertEqual(self._sc.event_header_field_class.addr, cpy.event_header_field_class.addr)
        self.assertEqual(self._sc.event_context_field_class.addr, cpy.event_context_field_class.addr)

    def test_deepcopy(self):
        cpy = copy.deepcopy(self._sc)
        self._test_copy(cpy)
        self.assertNotEqual(self._sc.packet_context_field_class.addr, cpy.packet_context_field_class.addr)
        self.assertNotEqual(self._sc.event_header_field_class.addr, cpy.event_header_field_class.addr)
        self.assertNotEqual(self._sc.event_context_field_class.addr, cpy.event_context_field_class.addr)

    def test_getitem(self):
        self.assertEqual(self._sc[23], self._ec1)
        self.assertEqual(self._sc[17], self._ec2)

    def test_getitem_wrong_key_type(self):
        with self.assertRaises(TypeError):
            self._sc['event23']

    def test_getitem_wrong_key(self):
        with self.assertRaises(KeyError):
            self._sc[19]

    def test_len(self):
        self.assertEqual(len(self._sc), 2)

    def test_iter(self):
        for ec_id, event_class in self._sc.items():
            self.assertIsInstance(event_class, bt2.EventClass)

            if ec_id == 23:
                self.assertEqual(event_class, self._ec1)
            elif ec_id == 17:
                self.assertEqual(event_class, self._ec2)

    def test_eq(self):
        ec1, ec2 = self._create_event_classes()
        sc1 = bt2.StreamClass(name='my_stream_class', id=12,
                              packet_context_field_class=self._packet_context_fc,
                              event_header_field_class=self._event_header_fc,
                              event_context_field_class=self._event_context_fc,
                              event_classes=(ec1, ec2))
        ec1, ec2 = self._create_event_classes()
        sc2 = bt2.StreamClass(name='my_stream_class', id=12,
                              packet_context_field_class=self._packet_context_fc,
                              event_header_field_class=self._event_header_fc,
                              event_context_field_class=self._event_context_fc,
                              event_classes=(ec1, ec2))
        self.assertEqual(sc1, sc2)

    def test_ne_name(self):
        ec1, ec2 = self._create_event_classes()
        sc1 = bt2.StreamClass(name='my_stream_class1', id=12,
                              packet_context_field_class=self._packet_context_fc,
                              event_header_field_class=self._event_header_fc,
                              event_context_field_class=self._event_context_fc,
                              event_classes=(ec1, ec2))
        ec1, ec2 = self._create_event_classes()
        sc2 = bt2.StreamClass(name='my_stream_class', id=12,
                              packet_context_field_class=self._packet_context_fc,
                              event_header_field_class=self._event_header_fc,
                              event_context_field_class=self._event_context_fc,
                              event_classes=(ec1, ec2))
        self.assertNotEqual(sc1, sc2)

    def test_ne_id(self):
        ec1, ec2 = self._create_event_classes()
        sc1 = bt2.StreamClass(name='my_stream_class', id=13,
                              packet_context_field_class=self._packet_context_fc,
                              event_header_field_class=self._event_header_fc,
                              event_context_field_class=self._event_context_fc,
                              event_classes=(ec1, ec2))
        ec1, ec2 = self._create_event_classes()
        sc2 = bt2.StreamClass(name='my_stream_class', id=12,
                              packet_context_field_class=self._packet_context_fc,
                              event_header_field_class=self._event_header_fc,
                              event_context_field_class=self._event_context_fc,
                              event_classes=(ec1, ec2))
        self.assertNotEqual(sc1, sc2)

    def test_ne_packet_context_field_class(self):
        ec1, ec2 = self._create_event_classes()
        sc1 = bt2.StreamClass(name='my_stream_class', id=12,
                              packet_context_field_class=self._event_context_fc,
                              event_header_field_class=self._event_header_fc,
                              event_context_field_class=self._event_context_fc,
                              event_classes=(ec1, ec2))
        ec1, ec2 = self._create_event_classes()
        sc2 = bt2.StreamClass(name='my_stream_class', id=12,
                              packet_context_field_class=self._packet_context_fc,
                              event_header_field_class=self._event_header_fc,
                              event_context_field_class=self._event_context_fc,
                              event_classes=(ec1, ec2))
        self.assertNotEqual(sc1, sc2)

    def test_ne_event_header_field_class(self):
        ec1, ec2 = self._create_event_classes()
        sc1 = bt2.StreamClass(name='my_stream_class', id=12,
                              packet_context_field_class=self._packet_context_fc,
                              event_context_field_class=self._event_context_fc,
                              event_classes=(ec1, ec2))
        ec1, ec2 = self._create_event_classes()
        sc2 = bt2.StreamClass(name='my_stream_class', id=12,
                              packet_context_field_class=self._packet_context_fc,
                              event_header_field_class=self._event_header_fc,
                              event_context_field_class=self._event_context_fc,
                              event_classes=(ec1, ec2))
        self.assertNotEqual(sc1, sc2)

    def test_ne_event_context_field_class(self):
        ec1, ec2 = self._create_event_classes()
        sc1 = bt2.StreamClass(name='my_stream_class', id=12,
                              packet_context_field_class=self._packet_context_fc,
                              event_header_field_class=self._event_header_fc,
                              event_context_field_class=self._packet_context_fc,
                              event_classes=(ec1, ec2))
        ec1, ec2 = self._create_event_classes()
        sc2 = bt2.StreamClass(name='my_stream_class', id=12,
                              packet_context_field_class=self._packet_context_fc,
                              event_header_field_class=self._event_header_fc,
                              event_context_field_class=self._event_context_fc,
                              event_classes=(ec1, ec2))
        self.assertNotEqual(sc1, sc2)

    def test_ne_event_class(self):
        ec1, ec2 = self._create_event_classes()
        sc1 = bt2.StreamClass(name='my_stream_class', id=12,
                              packet_context_field_class=self._packet_context_fc,
                              event_header_field_class=self._event_header_fc,
                              event_context_field_class=self._event_context_fc,
                              event_classes=(ec1,))
        ec1, ec2 = self._create_event_classes()
        sc2 = bt2.StreamClass(name='my_stream_class', id=12,
                              packet_context_field_class=self._packet_context_fc,
                              event_header_field_class=self._event_header_fc,
                              event_context_field_class=self._event_context_fc,
                              event_classes=(ec1, ec2))
        self.assertNotEqual(sc1, sc2)

    def test_eq_invalid(self):
        self.assertFalse(self._sc == 23)
