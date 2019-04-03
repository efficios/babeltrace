from bt2 import values
import unittest
import copy
import uuid
import bt2


@unittest.skip("this is broken")
class TraceTestCase(unittest.TestCase):
    def setUp(self):
        self._sc = self._create_stream_class('sc1', 3)
        self._tc = bt2.Trace()

    def tearDown(self):
        del self._sc
        del self._tc

    def _create_stream_class(self, name, id):
        ec1, ec2 = self._create_event_classes()
        packet_context_fc = bt2.StructureFieldClass()
        packet_context_fc.append_field('menu', bt2.FloatingPointNumberFieldClass())
        packet_context_fc.append_field('sticker', bt2.StringFieldClass())
        event_header_fc = bt2.StructureFieldClass()
        event_header_fc.append_field('id', bt2.IntegerFieldClass(19))
        event_context_fc = bt2.StructureFieldClass()
        event_context_fc.append_field('msg', bt2.StringFieldClass())
        return bt2.StreamClass(name=name, id=id,
                               packet_context_field_class=packet_context_fc,
                               event_header_field_class=event_header_fc,
                               event_context_field_class=event_context_fc,
                               event_classes=(ec1, ec2))

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

    def test_create_default(self):
        self.assertEqual(len(self._tc), 0)

    def _get_std_header(self):
        header_fc = bt2.StructureFieldClass()
        header_fc.append_field('magic', bt2.IntegerFieldClass(32))
        header_fc.append_field('stream_id', bt2.IntegerFieldClass(32))
        return header_fc

    def test_create_full(self):
        clock_classes = bt2.ClockClass('cc1', 1000), bt2.ClockClass('cc2', 30)
        sc = self._create_stream_class('sc1', 3)
        tc = bt2.Trace(name='my name',
                       native_byte_order=bt2.ByteOrder.LITTLE_ENDIAN,
                       env={'the_string': 'value', 'the_int': 23},
                       packet_header_field_class=self._get_std_header(),
                       clock_classes=clock_classes,
                       stream_classes=(sc,))
        self.assertEqual(tc.name, 'my name')
        self.assertEqual(tc.native_byte_order, bt2.ByteOrder.LITTLE_ENDIAN)
        self.assertEqual(tc.env['the_string'], 'value')
        self.assertEqual(tc.env['the_int'], 23)
        self.assertEqual(tc.packet_header_field_class, self._get_std_header())
        self.assertEqual(tc.clock_classes['cc1'], clock_classes[0])
        self.assertEqual(tc.clock_classes['cc2'], clock_classes[1])
        self.assertEqual(tc[3], sc)

    def test_assign_name(self):
        self._tc.name = 'lel'
        self.assertEqual(self._tc.name, 'lel')

    def test_assign_invalid_name(self):
        with self.assertRaises(TypeError):
            self._tc.name = 17

    def test_assign_static(self):
        self._tc.set_is_static()
        self.assertTrue(self._tc.is_static)

    def test_assign_native_byte_order(self):
        self._tc.native_byte_order = bt2.ByteOrder.BIG_ENDIAN
        self.assertEqual(self._tc.native_byte_order, bt2.ByteOrder.BIG_ENDIAN)

    def test_assign_invalid_native_byte_order(self):
        with self.assertRaises(TypeError):
            self._tc.native_byte_order = 'lel'

    def test_assign_packet_header_field_class(self):
        header_fc = bt2.StructureFieldClass()
        header_fc.append_field('magic', bt2.IntegerFieldClass(32))
        self._tc.packet_header_field_class = header_fc
        self.assertEqual(self._tc.packet_header_field_class, header_fc)

    def test_assign_no_packet_header_field_class(self):
        self._tc.packet_header_field_class = None
        self.assertIsNone(self._tc.packet_header_field_class)

    def _test_copy(self, cpy):
        self.assertIsNot(cpy, self._tc)
        self.assertNotEqual(cpy.addr, self._tc.addr)
        self.assertEqual(cpy, self._tc)
        self.assertEqual(len(self._tc), len(cpy))

    def _pre_copy(self):
        self._tc.packet_header_field_class = self._get_std_header()
        self._tc.name = 'the trace class'
        sc1 = self._create_stream_class('sc1', 3)
        sc2 = self._create_stream_class('sc2', 9)
        sc3 = self._create_stream_class('sc3', 17)
        self._tc.add_clock_class(bt2.ClockClass('cc1', 1000))
        self._tc.add_clock_class(bt2.ClockClass('cc2', 30))
        self._tc.env['allo'] = 'bateau'
        self._tc.env['bateau'] = 'cart'
        self._tc.add_stream_class(sc1)
        self._tc.add_stream_class(sc2)
        self._tc.add_stream_class(sc3)

    def test_copy(self):
        self._pre_copy()
        cpy = copy.copy(self._tc)
        self._test_copy(cpy)
        self.assertEqual(self._tc.packet_header_field_class.addr, cpy.packet_header_field_class.addr)
        self.assertEqual(self._tc.clock_classes['cc1'].addr, cpy.clock_classes['cc1'].addr)
        self.assertEqual(self._tc.clock_classes['cc2'].addr, cpy.clock_classes['cc2'].addr)
        self.assertEqual(self._tc.env['allo'].addr, cpy.env['allo'].addr)
        self.assertEqual(self._tc.env['bateau'].addr, cpy.env['bateau'].addr)

    def test_deepcopy(self):
        self._pre_copy()
        cpy = copy.deepcopy(self._tc)
        self._test_copy(cpy)
        self.assertNotEqual(self._tc.packet_header_field_class.addr, cpy.packet_header_field_class.addr)
        self.assertNotEqual(self._tc.clock_classes['cc1'].addr, cpy.clock_classes['cc1'].addr)
        self.assertNotEqual(self._tc.clock_classes['cc2'].addr, cpy.clock_classes['cc2'].addr)
        self.assertNotEqual(self._tc.env['allo'].addr, cpy.env['allo'].addr)
        self.assertNotEqual(self._tc.env['bateau'].addr, cpy.env['bateau'].addr)

    def test_getitem(self):
        self._tc.add_stream_class(self._sc)
        self.assertEqual(self._tc[3].addr, self._sc.addr)

    def test_getitem_wrong_key_type(self):
        self._tc.add_stream_class(self._sc)
        with self.assertRaises(TypeError):
            self._tc['hello']

    def test_getitem_wrong_key(self):
        self._tc.add_stream_class(self._sc)
        with self.assertRaises(KeyError):
            self._tc[4]

    def test_len(self):
        self.assertEqual(len(self._tc), 0)
        self._tc.add_stream_class(self._sc)
        self.assertEqual(len(self._tc), 1)

    def test_iter(self):
        self._tc.packet_header_field_class = self._get_std_header()
        sc1 = self._create_stream_class('sc1', 3)
        sc2 = self._create_stream_class('sc2', 9)
        sc3 = self._create_stream_class('sc3', 17)
        self._tc.add_stream_class(sc1)
        self._tc.add_stream_class(sc2)
        self._tc.add_stream_class(sc3)

        for sid, stream_class in self._tc.items():
            self.assertIsInstance(stream_class, bt2.StreamClass)

            if sid == 3:
                self.assertEqual(stream_class.addr, sc1.addr)
            elif sid == 9:
                self.assertEqual(stream_class.addr, sc2.addr)
            elif sid == 17:
                self.assertEqual(stream_class.addr, sc3.addr)

    def test_env_getitem_wrong_key(self):
        with self.assertRaises(KeyError):
            self._tc.env['lel']

    def test_clock_classes_getitem_wrong_key(self):
        with self.assertRaises(KeyError):
            self._tc.clock_classes['lel']

    def test_streams_none(self):
        self.assertEqual(len(self._tc.streams), 0)

    def test_streams_len(self):
        self._tc.add_stream_class(self._create_stream_class('sc1', 3))
        stream0 = self._tc[3]()
        stream1 = self._tc[3]()
        stream2 = self._tc[3]()
        self.assertEqual(len(self._tc.streams), 3)

    def test_streams_iter(self):
        self._tc.add_stream_class(self._create_stream_class('sc1', 3))
        stream0 = self._tc[3](id=12)
        stream1 = self._tc[3](id=15)
        stream2 = self._tc[3](id=17)
        sids = set()

        for stream in self._tc.streams:
            sids.add(stream.id)

        self.assertEqual(len(sids), 3)
        self.assertTrue(12 in sids and 15 in sids and 17 in sids)

    def _test_eq_create_objects(self):
        cc1_uuid = uuid.UUID('bc7f2f2d-2ee4-4e03-ab1f-2e0e1304e94f')
        cc1 = bt2.ClockClass('cc1', 1000, uuid=cc1_uuid)
        cc2_uuid = uuid.UUID('da7d6b6f-3108-4706-89bd-ab554732611b')
        cc2 = bt2.ClockClass('cc2', 30, uuid=cc2_uuid)
        sc1 = self._create_stream_class('sc1', 3)
        sc2 = self._create_stream_class('sc2', 9)
        return cc1, cc2, sc1, sc2, self._get_std_header()

    def test_eq(self):
        cc1, cc2, sc1, sc2, header_fc = self._test_eq_create_objects()
        tc1 = bt2.Trace(name='my name',
                        native_byte_order=bt2.ByteOrder.LITTLE_ENDIAN,
                        env={'the_string': 'value', 'the_int': 23},
                        packet_header_field_class=header_fc,
                        clock_classes=(cc1, cc2),
                        stream_classes=(sc1, sc2))
        cc1, cc2, sc1, sc2, header_fc = self._test_eq_create_objects()
        tc2 = bt2.Trace(name='my name',
                        native_byte_order=bt2.ByteOrder.LITTLE_ENDIAN,
                        env={'the_string': 'value', 'the_int': 23},
                        packet_header_field_class=header_fc,
                        clock_classes=(cc1, cc2),
                        stream_classes=(sc1, sc2))
        self.assertEqual(tc1, tc2)

    def test_ne_name(self):
        cc1, cc2, sc1, sc2, header_fc = self._test_eq_create_objects()
        tc1 = bt2.Trace(name='my name2',
                        native_byte_order=bt2.ByteOrder.LITTLE_ENDIAN,
                        env={'the_string': 'value', 'the_int': 23},
                        packet_header_field_class=header_fc,
                        clock_classes=(cc1, cc2),
                        stream_classes=(sc1, sc2))
        cc1, cc2, sc1, sc2, header_fc = self._test_eq_create_objects()
        tc2 = bt2.Trace(name='my name',
                        native_byte_order=bt2.ByteOrder.LITTLE_ENDIAN,
                        env={'the_string': 'value', 'the_int': 23},
                        packet_header_field_class=header_fc,
                        clock_classes=(cc1, cc2),
                        stream_classes=(sc1, sc2))
        self.assertNotEqual(tc1, tc2)

    def test_ne_packet_header_field_class(self):
        cc1, cc2, sc1, sc2, header_fc = self._test_eq_create_objects()
        tc1 = bt2.Trace(name='my name',
                        native_byte_order=bt2.ByteOrder.LITTLE_ENDIAN,
                        env={'the_string': 'value', 'the_int': 23},
                        packet_header_field_class=header_fc,
                        clock_classes=(cc1, cc2),
                        stream_classes=(sc1, sc2))
        cc1, cc2, sc1, sc2, header_fc = self._test_eq_create_objects()
        header_fc.append_field('yes', bt2.StringFieldClass())
        tc2 = bt2.Trace(name='my name',
                        native_byte_order=bt2.ByteOrder.LITTLE_ENDIAN,
                        env={'the_string': 'value', 'the_int': 23},
                        packet_header_field_class=header_fc,
                        clock_classes=(cc1, cc2),
                        stream_classes=(sc1, sc2))
        self.assertNotEqual(tc1, tc2)

    def test_ne_native_byte_order(self):
        cc1, cc2, sc1, sc2, header_fc = self._test_eq_create_objects()
        tc1 = bt2.Trace(name='my name',
                        native_byte_order=bt2.ByteOrder.LITTLE_ENDIAN,
                        env={'the_string': 'value', 'the_int': 23},
                        packet_header_field_class=header_fc,
                        clock_classes=(cc1, cc2),
                        stream_classes=(sc1, sc2))
        cc1, cc2, sc1, sc2, header_fc = self._test_eq_create_objects()
        tc2 = bt2.Trace(name='my name',
                        native_byte_order=bt2.ByteOrder.BIG_ENDIAN,
                        env={'the_string': 'value', 'the_int': 23},
                        packet_header_field_class=header_fc,
                        clock_classes=(cc1, cc2),
                        stream_classes=(sc1, sc2))
        self.assertNotEqual(tc1, tc2)

    def test_ne_env(self):
        cc1, cc2, sc1, sc2, header_fc = self._test_eq_create_objects()
        tc1 = bt2.Trace(name='my name',
                        native_byte_order=bt2.ByteOrder.LITTLE_ENDIAN,
                        env={'the_string': 'value', 'the_int2': 23},
                        packet_header_field_class=header_fc,
                        clock_classes=(cc1, cc2),
                        stream_classes=(sc1, sc2))
        cc1, cc2, sc1, sc2, header_fc = self._test_eq_create_objects()
        tc2 = bt2.Trace(name='my name',
                        native_byte_order=bt2.ByteOrder.LITTLE_ENDIAN,
                        env={'the_string': 'value', 'the_int': 23},
                        packet_header_field_class=header_fc,
                        clock_classes=(cc1, cc2),
                        stream_classes=(sc1, sc2))
        self.assertNotEqual(tc1, tc2)

    def test_ne_clock_classes(self):
        cc1, cc2, sc1, sc2, header_fc = self._test_eq_create_objects()
        tc1 = bt2.Trace(name='my name',
                        native_byte_order=bt2.ByteOrder.LITTLE_ENDIAN,
                        env={'the_string': 'value', 'the_int': 23},
                        packet_header_field_class=header_fc,
                        clock_classes=(cc1, cc2),
                        stream_classes=(sc1, sc2))
        cc1, cc2, sc1, sc2, header_fc = self._test_eq_create_objects()
        cc2.frequency = 1234
        tc2 = bt2.Trace(name='my name',
                        native_byte_order=bt2.ByteOrder.LITTLE_ENDIAN,
                        env={'the_string': 'value', 'the_int': 23},
                        packet_header_field_class=header_fc,
                        clock_classes=(cc1, cc2),
                        stream_classes=(sc1, sc2))
        self.assertNotEqual(tc1, tc2)

    def test_ne_stream_classes(self):
        cc1, cc2, sc1, sc2, header_fc = self._test_eq_create_objects()
        tc1 = bt2.Trace(name='my name',
                        native_byte_order=bt2.ByteOrder.LITTLE_ENDIAN,
                        env={'the_string': 'value', 'the_int': 23},
                        packet_header_field_class=header_fc,
                        clock_classes=(cc1, cc2),
                        stream_classes=(sc1, sc2))
        cc1, cc2, sc1, sc2, header_fc = self._test_eq_create_objects()
        sc2.id = 72632
        tc2 = bt2.Trace(name='my name',
                        native_byte_order=bt2.ByteOrder.LITTLE_ENDIAN,
                        env={'the_string': 'value', 'the_int': 23},
                        packet_header_field_class=header_fc,
                        clock_classes=(cc1, cc2),
                        stream_classes=(sc1, sc2))
        self.assertNotEqual(tc1, tc2)

    def test_eq_invalid(self):
        self.assertFalse(self._tc == 23)
